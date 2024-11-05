#include "esp_log.h"
#include "esp_random.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOSConfig.h"

// BLE
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "crypto/sha2.h"
#include "./task-ble.h"
#include "./system/build-defs.h"
#include "./system/device-info.h"
#include "./system/utils.h"
#include "./utils/cbor.h"

typedef struct Payload {
    size_t length;
    uint8_t *data;
} Payload;

// 16kb
#define MAX_MESSAGE_SIZE        (1 << 14)

#define STATE_CONNECTED         (1 << 0)
#define STATE_SUBSCRIBED        (1 << 1)
#define STATE_ENCRYPTED         (1 << 2)
typedef struct Connection {
    uint32_t state;

    uint8_t address[6];
    uint8_t own_addr_type;

    uint16_t conn_handle;
    uint16_t content;
    uint16_t logger;
    uint16_t battery_handle;

    // The buffer to hold an incoming message
    uint8_t msg[MAX_MESSAGE_SIZE];

    // Next expected offset for the incoming message
    size_t msgOffset;

    // Total expected message size
    size_t msgLen;
} Connection;

static Connection connection = { 0 };

static void print_addr(const char *prefix, const void *addr) {
    const uint8_t *u8p = addr;
    printf("%s%02x:%02x:%02x:%02x:%02x:%02x\n", prefix,
      u8p[5], u8p[4], u8p[3], u8p[2], u8p[1], u8p[0]);
}

static void dumpBuffer(char *header, uint8_t *buffer, size_t length) {
    printf("%s (length=%d)", header, length);
    for (int i = 0; i < length; i++) {
        if ((i % 16) == 0) { printf("\n    "); }
        printf("%02x", buffer[i]);
        if ((i % 4) == 3) { printf("  "); }
        //if ((i % 16) == 7) { printf("  "); }
        //if ((i % 16) == 11) { printf("  "); }
    }
    printf("\n");
}

// SIG membership pending...
#define VENDOR_ID       (0x5432)
#define PRODUCT_ID      (0x0001)
#define PRODUCT_VERSION (0x0006)

// Device Information Service
// https://www.bluetooth.com/specifications/specs/device-information-service-1-1/
#define UUID_SVC_DEVICE_INFO                        (0x180A)
#define UUID_CHR_MANUFACTURER_NAME_STRING           (0x2A29)
#define UUID_CHR_MODEL_NUMBER_STRING                (0x2A24)
#define UUID_CHR_FIRMWARE_REVISION_STRING           (0x2A26)
#define UUID_CHR_PNP                                (0x2A50)

// Battery Service
#define UUID_SVC_BATTERY_LEVEL                      (0x180f)
#define UUID_CHR_BATTERY_LEVEL                      (0x2a19)
#define UUID_DSC_BATTERY_LEVEL                      (0x2904)

// Firefly Serial Protocol
// UUID: https://github.com/espressif/esp-idf/blob/master/examples/bluetooth/nimble/ble_spp/spp_server/main/ble_spp_server.h#L19
#define UUID_SVC_FSP                                (0xabf0)
#define UUID_CHR_FSP_CONTENT                        (0xabf1)
#define UUID_CHR_FSP_LOGGER                         (0xabf2)

#define CMD_QUERY                                   (0b0001)
#define CMD_RESET                                   (0b0100)
#define CMD_START_MESSAGE                           (0b0110)
#define CMD_CONTINUE_MESSAGE                        (0b0111)

#define ERROR_UNKNOWN                               (0b1000)
#define ERROR_UNSUPPORTED_VERSION                   (0b1001)
#define ERROR_BAD_COMMAND                           (0b1010)
#define ERROR_BUFFER_OVERRUN                        (0b1011)
#define ERROR_MISSING_MESSAGE                       (0b1100)
#define ERROR_BAD_CHECKSUM                          (0b1101)


static int notify(uint16_t handle, uint8_t *data, size_t length) {
    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, length);
    //int rc = ble_gatts_notify_custom(connection.conn_handle, handle, om);
    int rc = ble_gatts_indicate_custom(connection.conn_handle, handle, om);
    if (rc) { printf("[ble] indicate fail: handle=%d rc=%d\n", handle, rc); }
    return rc;
}

static void processMessage() {
    dumpBuffer("Process Message", connection.msg, connection.msgLen);

    uint64_t value;

    CborCursor cursor;
    cbor_init(&cursor, connection.msg, connection.msgLen);

    CborStatus status = cbor_followKey(&cursor, "test");
    printf("S: status=%d\n", status);
    status = cbor_getValue(&cursor, &value);
    printf("S: status=%d\n", status);
    printf("  value=%lld err=%d\n", value, status);

    connection.msgOffset = 0;
    connection.msgLen = 0;
}

// @TODO: Renamte to gattAccess
static int _gattAccess(uint16_t conn_handle, uint16_t attr_handle,
  struct ble_gatt_access_ctxt *ctx, void *arg) {

    bool isWrite = false;
    uint16_t uuid = 0;
    switch (ctx->op) {
        case BLE_GATT_ACCESS_OP_WRITE_CHR:
            isWrite = true;
            // Falls through
        case BLE_GATT_ACCESS_OP_READ_CHR:
            uuid = ble_uuid_u16(ctx->chr->uuid);
            break;
        case BLE_GATT_ACCESS_OP_WRITE_DSC:
            isWrite = true;
            // Falls through
        case BLE_GATT_ACCESS_OP_READ_DSC:
            uuid = ble_uuid_u16(ctx->dsc->uuid);
            break;
    }

    if (isWrite) {

        ////////////////////
        // Write operation (host-to-device)

        // @TODO: Check that length doesn't exceed 512 bytes

        uint16_t length = os_mbuf_len(ctx->om);
        uint8_t req[length];
        int rc = os_mbuf_copydata(ctx->om, 0, length, req);
        if (rc) { printf("[ble] write fail: rc=%d\n", rc); }

        // Response; maximum length is 13 bytes (include space for hash)
        uint8_t resp[33] = { 0 };
        size_t respLen = 1;

        do {
            // Error copying request
            if (rc) {
                resp[0] = (ERROR_UNKNOWN << 4);
                break;
            }

            // No data to work with at all
            if (length < 1) {
                resp[0] = (ERROR_BUFFER_OVERRUN << 4);
                break;
            }

            resp[0] = req[0];
            // @TODO: Check seqno is the expected value

            uint8_t cmd = req[0] >> 4;
            uint8_t seqno = req[0] & 0x0f;

            if (cmd == CMD_QUERY) {
                // Missing version parameter
                if (length < 2) {
                    resp[0] = (ERROR_BUFFER_OVERRUN << 4) | seqno;
                    break;
                }

                // Only support version 1
                int version = req[1];
                if (version != 0x01) {
                    resp[0] = (ERROR_UNSUPPORTED_VERSION << 4) | seqno;
                    break;
                }

                // OK
                resp[1] = 0x01;
                resp[2] = connection.msgOffset >> 8;
                resp[3] = connection.msgOffset & 0xff;
                respLen = 4;

            } else if (cmd == CMD_RESET) {
                connection.msgOffset = 0;
                connection.msgLen = 0;

            } else if (cmd == CMD_START_MESSAGE) {

                // Missing length parameter
                if (length < 3) {
                    resp[0] = (ERROR_BUFFER_OVERRUN << 4) | seqno;
                    break;
                }

                uint16_t msgLen = (req[1] << 8) | req[2];

                if (msgLen > 509) {
                    // The message could consume entire req but did not
                    if (length < 1 + 2 + 509) {
                        resp[0] = (ERROR_MISSING_MESSAGE << 4) | seqno;
                        break;
                    }
                } else {
                    // The message can fit in the req but was not
                    if (msgLen == 0 || length != 1 + 2 + msgLen) {
                        resp[0] = (ERROR_MISSING_MESSAGE << 4) | seqno;
                        break;
                    }
                }

                // A message is already started
                if (connection.msgOffset != 0) {
                    resp[0] = (ERROR_MISSING_MESSAGE << 4) | seqno;
                    break;
                }

                // Update the message
                connection.msgLen = msgLen;
                connection.msgOffset = length - 1 - 2;
                memcpy(connection.msg, &req[3], length - 1 - 2);

                // Response include the current hash
                Sha256Context ctx;
                sha2_initSha256(&ctx);
                sha2_updateSha256(&ctx, &req[3], length - 1 - 2);
                sha2_finalSha256(&ctx, &resp[1]);

                respLen = 13;

            } else if (cmd == CMD_CONTINUE_MESSAGE) {

                // Missing length parameter
                if (length < 3) {
                    resp[0] = (ERROR_BUFFER_OVERRUN << 4) | seqno;
                    break;
                }

                // No message to continue
                if (connection.msgOffset == 0) {
                    resp[0] = (ERROR_MISSING_MESSAGE << 4) | seqno;
                    break;
                }

                uint16_t msgOffset = (req[1] << 8) | req[2];

                // Message offset is out of sync
                if (msgOffset != connection.msgOffset) {
                    resp[0] = (ERROR_MISSING_MESSAGE << 4) | seqno;
                    break;
                }

                size_t remaining = connection.msgLen - msgOffset;

                if (remaining > 509) {
                    // The message could consume entire req but did not
                    if (length < 1 + 2 + 509) {
                        resp[0] = (ERROR_MISSING_MESSAGE << 4) | seqno;
                        break;
                    }
                } else {
                    // The message can fit in the req but was not
                    if (length != 1 + 2 + remaining) {
                        resp[0] = (ERROR_MISSING_MESSAGE << 4) | seqno;
                        break;
                    }
                }

                // Update the message
                connection.msgOffset += length - 1 - 2;
                memcpy(&connection.msg[msgOffset], &req[3], length - 1 - 2);

                // Response include the current hash
                Sha256Context ctx;
                sha2_initSha256(&ctx);
                sha2_updateSha256(&ctx, &req[3], length - 1 - 2);
                sha2_finalSha256(&ctx, &resp[1]);

                respLen = 13;

            } else {
                resp[0] |= (ERROR_BAD_COMMAND << 4) | seqno;
            }

        } while (0);

        notify(connection.content, resp, respLen);

        // Message ready to process!
        if (connection.msgOffset && connection.msgOffset == connection.msgLen) {
            processMessage();
        }

        return 0;
    }

    ////////////////////
    // Read operation (device-to-host)

    // Static content; just pass along the payload
    if (arg) {
        Payload *payload = arg;

        int rc = os_mbuf_append(ctx->om, payload->data, payload->length);
        if (rc) { printf("[ble] failed to send: rc=%d", rc); }
        return (rc == 0) ? 0: BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    // The bettery UUID; pass along the percentage
    if (uuid == UUID_CHR_BATTERY_LEVEL) {
        uint8_t data[] = { 100 };
        int rc = os_mbuf_append(ctx->om, data, sizeof(data));
        return (rc == 0) ? 0: BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    printf("send uuid=%04x\n", uuid);

    static int foo = 0;
    char buffer[9] = { 0 };
    buffer[0] = foo++;
    int rc = os_mbuf_append(ctx->om, &buffer, sizeof(buffer));

    return (rc == 0) ? 0: BLE_ATT_ERR_INSUFFICIENT_RES;
}

static void _svrRegister(struct ble_gatt_register_ctxt *ctxt, void *arg) {
    char buf[BLE_UUID_STR_LEN];

    switch (ctxt->op) {
        case BLE_GATT_REGISTER_OP_SVC:
            MODLOG_DFLT(DEBUG, "registered service %s with handle=%d\n",
                        ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                        ctxt->svc.handle);
            break;

        case BLE_GATT_REGISTER_OP_CHR:
            MODLOG_DFLT(DEBUG, "registering characteristic %s with "
                        "def_handle=%d val_handle=%d\n",
                        ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                        ctxt->chr.def_handle,
                        ctxt->chr.val_handle);
            break;

        case BLE_GATT_REGISTER_OP_DSC:
            MODLOG_DFLT(DEBUG, "registering descriptor %s with handle=%d\n",
                        ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                        ctxt->dsc.handle);
            break;

        default:
            assert(0);
            break;
    }
}

static int _gapEvent(struct ble_gap_event *event, void *arg);

static void _advertise() {
    printf("ble_advertise\n");

    struct ble_hs_adv_fields fields;
    memset(&fields, 0, sizeof(fields));

    // Advertise:
    //  - Discoverability in forthcoming advertisement (general)
    //  - BLE-only (BR/EDR unsupported)
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    // Include TX power
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    fields.tx_pwr_lvl_is_present = 1;

    // Set device name
    const char *device_name = DEVICE_NAME;
    fields.name = (uint8_t *)device_name;
    fields.name_len = strlen(device_name);
    fields.name_is_complete = 1;

    fields.uuids16 = (ble_uuid16_t[]) {
        BLE_UUID16_INIT(UUID_SVC_FSP),
    };
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    // Configure the data to advertise
    {
        int rc = ble_gap_adv_set_fields(&fields);
        if (rc != 0) {
            MODLOG_DFLT(ERROR, "error setting advertisement data; rc=%d\n", rc);
            return;
        }
    }

    // Advertisement
    //  - Undirected-connectable
    //  - general-discoverable
    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    // Begin advertising
    {
        int rc = ble_gap_adv_start(connection.own_addr_type, NULL,
          BLE_HS_FOREVER, &adv_params, _gapEvent, NULL);

        if (rc != 0) {
            MODLOG_DFLT(ERROR, "error enabling advertisement; rc=%d\n", rc);
            return;
        }
    }
}

static void _onSync(void) {
    int rc;

    rc = ble_hs_id_infer_auto(0, &connection.own_addr_type);
    assert(rc == 0);

    rc = ble_hs_id_copy_addr(connection.own_addr_type,
      connection.address, NULL);

    print_addr("[ble] sync addr=", connection.address);

    _advertise();
}

static void _onReset(int reason) {
    printf("[ble] reset=%d\n", reason);
}

// /Users/ricmoo/esp/esp-idf/components/bt//host/nimble/nimble/nimble/host/include/host/ble_gap.h
static int _gapEvent(struct ble_gap_event *event, void *_context) {

    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            // A new connection was established or a connection attempt failed
            printf("[ble] connect: status=%s (%d)\n",
              event->connect.status == 0 ? "established" : "failed",
              event->connect.status);

            //Connection failed; resume advertising
            if (event->connect.status != 0) { _advertise(); }
            connection.conn_handle = event->connect.conn_handle;
            connection.state = STATE_CONNECTED;

            return 0;

        case BLE_GAP_EVENT_DISCONNECT:
            printf("[ble] disconnect: reason=%d\n", event->disconnect.reason);

            connection.state = 0;
            connection.conn_handle = 0;

            // Connection terminated; resume advertising
            _advertise();
            return 0;

        case BLE_GAP_EVENT_CONN_UPDATE:
            printf("[ble] conn_update: status=%d\n", event->conn_update.status);
            return 0;

        case BLE_GAP_EVENT_CONN_UPDATE_REQ:
            printf("[ble] conn_update_req\n");
            return 0;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            printf("[ble] adv_complete: reason=%d\n",
              event->adv_complete.reason);

            _advertise();
            return 0;

        case BLE_GAP_EVENT_SUBSCRIBE:
            printf("[ble] subscribe: connHandle=%d, attrHandle=%d reason=%d prevNotify=%d curNotify=%d prevIndicate=%d curIndicate=%d\n",
              event->subscribe.conn_handle, event->subscribe.attr_handle,
              event->subscribe.reason, event->subscribe.prev_notify,
              event->subscribe.cur_notify, event->subscribe.prev_indicate,
              event->subscribe.cur_indicate);
            connection.state |= STATE_SUBSCRIBED;
            return 0;

        case BLE_GAP_EVENT_NOTIFY_TX:
            printf("[ble] notify_tx status=%d indication=%d\n",
              event->notify_tx.status, event->notify_tx.indication);
            return 0;

        case BLE_GAP_EVENT_MTU:
            printf("[ble] mtu: connHandle=%d channelId=%d mtu=%d\n",
              event->mtu.conn_handle, event->mtu.channel_id, event->mtu.value);
            return 0;

        case BLE_GAP_EVENT_REPEAT_PAIRING: {
            printf("[ble] repeat pairing: connHandle=%d\n",
              event->repeat_pairing.conn_handle);

            // @TODO: Should notify the user?

            struct ble_gap_conn_desc desc;
            int rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
            assert(rc == 0);
            ble_store_util_delete_peer(&desc.peer_id_addr);

            // Request the host to continue pairing
            return BLE_GAP_REPEAT_PAIRING_RETRY;
        }

        case BLE_GAP_EVENT_PASSKEY_ACTION:
            if (event->passkey.params.action == BLE_SM_IOACT_DISP) {
                struct ble_sm_io passkey = { 0 };
                passkey.action = event->passkey.params.action;
                passkey.passkey = 123456;
                printf("[ble] passkey action; display: passkey=%06ld\n",
                  passkey.passkey);
                int rc = ble_sm_inject_io(event->passkey.conn_handle, &passkey);
                if (rc) { printf("[ble] ble_sm_inject_io result: %d\n", rc); }

            } else if (event->passkey.params.action == BLE_SM_IOACT_NUMCMP) {
                printf("[ble] passkey action; numcmp: passkey=%06ld\n",
                  event->passkey.params.numcmp);

                struct ble_sm_io passkey = { 0 };
                passkey.action = event->passkey.params.action;
                passkey.numcmp_accept = 1;
                int rc = ble_sm_inject_io(event->passkey.conn_handle, &passkey);
                if (rc) { printf("[ble] ble_sm_inject_io result: %d\n", rc); }

            } else if (event->passkey.params.action == BLE_SM_IOACT_OOB) {
                printf("[ble] passkey action; oob\n");

                struct ble_sm_io passkey = { 0 };
                static uint8_t tem_oob[16] = { 0 };
                passkey.action = event->passkey.params.action;
                for (int i = 0; i < 16; i++) { passkey.oob[i] = tem_oob[i]; }
                int rc = ble_sm_inject_io(event->passkey.conn_handle, &passkey);
                if (rc) { printf("ble_sm_inject_io result: %d\n", rc); }

            } else if (event->passkey.params.action == BLE_SM_IOACT_INPUT) {
                printf("[ble] passkey action; input\n");

                struct ble_sm_io passkey = { 0 };
                passkey.action = event->passkey.params.action;
                passkey.passkey = 123456;
                int rc = ble_sm_inject_io(event->passkey.conn_handle, &passkey);
                if (rc) { printf("ble_sm_inject_io result: %d\n", rc); }
            }
            return 0;

        case BLE_GAP_EVENT_AUTHORIZE:
            printf("[ble] authorize: connHandle=%d attrHandle=%d isRead=%d outResponse=%d\n",
              event->authorize.conn_handle, event->authorize.attr_handle,
              event->authorize.is_read, event->authorize.out_response);
            return 0;

        case BLE_GAP_EVENT_PHY_UPDATE_COMPLETE:
            printf("[ble] phy update complete: status=%d connHandle=%d txPhy=%d rxPhy=%d\n",
              event->phy_updated.status, event->phy_updated.conn_handle,
              event->phy_updated.tx_phy, event->phy_updated.tx_phy);
            return 0;

        case BLE_GAP_EVENT_ENC_CHANGE:
            printf("[ble] enc change: status=%d connHandle=%d\n",
              event->enc_change.status, event->enc_change.conn_handle);
            return 0;

        default:
            printf("Unhandled: type=%d\n", event->type);
            return 0;
    }

    return 0;
}

static void _runTask() {
    printf("[ble] BLE Host Task Started\n");

    // Runs until nimble_port_stop() is called
    nimble_port_run();

    // Should this restart the nimble?
    nimble_port_freertos_deinit();

    printf("[ble] BLE Host Task Stopped\n");
}


// TEMP
void ble_store_config_init(void);

void taskBleFunc(void* pvParameter) {
    uint32_t *ready = (uint32_t*)pvParameter;
    vTaskSetApplicationTaskTag( NULL, (void*)NULL);

    // Device Information Service Data

    char disModelNumber[32];
    device_getModelName(disModelNumber, sizeof(disModelNumber));
    Payload payloadDisModelNumber = {
        .data = (uint8_t*)disModelNumber, .length = strlen(disModelNumber)
    };

    Payload payloadDisManufacturerName = {
        .data = (uint8_t*)MANUFACTURER_NAME,
        .length = strlen(MANUFACTURER_NAME)
    };

    char disFirmwareRevision[64];
    snprintf(disFirmwareRevision, sizeof(disFirmwareRevision),
      "v%d.%d.%d (%04d-%02d-%02d %02d:%02d)",
      (VERSION >> 16) & 0xff, (VERSION >> 8) & 0xff, VERSION & 0xff,
      BUILD_YEAR, BUILD_MONTH, BUILD_DAY, BUILD_HOUR, BUILD_SEC);
    Payload payloadDisFirmwareRevision = {
        .data = (uint8_t*)disFirmwareRevision,
        .length = strlen(disFirmwareRevision)
    };

    uint8_t disPnp[] = {
        0x01,                                          // Bluetooth SIG
        VENDOR_ID & 0xff, VENDOR_ID >> 8,              // Vendor ID
        PRODUCT_ID & 0xff, PRODUCT_ID >> 8,            // Product ID
        PRODUCT_VERSION & 0xff, PRODUCT_VERSION >> 8   // Product Version
    };
    Payload payloadDisPnp = { .data = disPnp, .length = sizeof(disPnp) };

    uint8_t batteryLevel[] = {
        0x04,        // BLE Format: uint8
        0x00,        // Exponent
        0x27, 0xad,  // BLE Unit (percentage)
        0x01,        // Namespace
        0x00, 0x00   // Description (??)
    };
    Payload payloadBatteryLevel = {
        .data = batteryLevel, .length = sizeof(batteryLevel)
    };

    // Definitions

    const struct ble_gatt_svc_def services[] = { {
        // Service: Device Information
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(UUID_SVC_DEVICE_INFO),
        .characteristics = (struct ble_gatt_chr_def[]) { {
            // Characteristic: * Manufacturer name
            .uuid = BLE_UUID16_DECLARE(UUID_CHR_MANUFACTURER_NAME_STRING),
            .access_cb = _gattAccess,
            .arg = &payloadDisManufacturerName,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            // Characteristic: Model number string
            .uuid = BLE_UUID16_DECLARE(UUID_CHR_MODEL_NUMBER_STRING),
            .access_cb = _gattAccess,
            .arg = &payloadDisModelNumber,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            // Characteristic: Model number string
            .uuid = BLE_UUID16_DECLARE(UUID_CHR_FIRMWARE_REVISION_STRING),
            .access_cb = _gattAccess,
            .arg = &payloadDisFirmwareRevision,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            // Characteristic: PNP
            .uuid = BLE_UUID16_DECLARE(UUID_CHR_PNP),
            .access_cb = _gattAccess,
            .arg = &payloadDisPnp,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            0, // No more characteristics in this service
        } }
    }, {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(UUID_SVC_BATTERY_LEVEL),
        .characteristics = (struct ble_gatt_chr_def[]) { {

            // Battery Level
            .uuid = BLE_UUID16_DECLARE(UUID_CHR_BATTERY_LEVEL),
            .access_cb = _gattAccess,
            .val_handle = &connection.battery_handle,
            .descriptors = (struct ble_gatt_dsc_def[]) { {
                .uuid = BLE_UUID16_DECLARE(UUID_DSC_BATTERY_LEVEL),
                .access_cb = _gattAccess,
                .arg = &payloadBatteryLevel,
                .att_flags = BLE_ATT_F_READ
            }, {
                .uuid = NULL
            } },
            .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
        }, {
            .uuid = NULL,
        } },
    }, {
        // Service: Firefly Serial Protocol
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(UUID_SVC_FSP),
        .characteristics = (struct ble_gatt_chr_def[]) { {
            // Characteristic: Data
            .uuid = BLE_UUID16_DECLARE(UUID_CHR_FSP_CONTENT),
            .access_cb = _gattAccess,
            .val_handle = &connection.content,
            .flags = BLE_GATT_CHR_F_READ | BLE_ATT_F_READ_ENC
              | BLE_ATT_F_WRITE | BLE_ATT_F_WRITE_ENC | BLE_GATT_CHR_F_INDICATE
        }, {
            // Characteristic: Log
            .uuid = BLE_UUID16_DECLARE(UUID_CHR_FSP_LOGGER),
            .access_cb = _gattAccess,
            .val_handle = &connection.logger,
            .flags = BLE_GATT_CHR_F_NOTIFY
        }, {
            0, // No more characteristics in this service
        } },
    }, {
        0, // No more services
    } };

    // Initialize NVS — it is used to store PHY calibration data
    {
        esp_err_t status = nvs_flash_init();
        if (status == ESP_ERR_NVS_NO_FREE_PAGES || status == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ESP_ERROR_CHECK(nvs_flash_erase());
            status = nvs_flash_init();
        }
        ESP_ERROR_CHECK(status);

        status = nimble_port_init();
        if (status != ESP_OK) {
            MODLOG_DFLT(ERROR, "Failed to init nimble %d \n", status);
            return;
        }
    }

    // Initialize the NimBLE host configuration
    ble_hs_cfg.gatts_register_cb = _svrRegister;
    ble_hs_cfg.reset_cb = _onReset;
    ble_hs_cfg.sync_cb = _onSync;

    //ble_hs_cfg.store_read_cb = _store_read;
    //ble_hs_cfg.store_write_cb = _store_write;
    //ble_hs_cfg.store_delete_cb = _store_delete;
    //ble_hs_cfg.store_status_cb = _store_status;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    //ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_DISP_ONLY;
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 1;
    ble_hs_cfg.sm_sc = 1;
    //ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC;
    //ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    assert(ble_gatts_count_cfg(services) == 0);
    assert(ble_gatts_add_svcs(services) == 0);

    // Set the default device name
    const char *device_name = DEVICE_NAME;
    assert(ble_svc_gap_device_name_set(device_name) == 0);

    // TEMP
    // See: components/bt//host/nimble/nimble/nimble/host/store/config/src/ble_store_config.c
    ble_store_config_init();

    // Run forever
    nimble_port_freertos_init(_runTask);

    // Unblock the bootstrap task
    *ready = 1;

    while (1) {
        delay(30000);
        if (connection.state & STATE_SUBSCRIBED) {
            char *ping = "ping";
            notify(connection.logger, (uint8_t*)ping, sizeof(ping));
        }
    }
}

