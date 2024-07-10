#include "esp_log.h"
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

#include "./ble.h"
#include "./build-defs.h"
#include "./utils.h"

#define MANUFACTURER_NAME    ("Firefly")
#define MODEL_NUMBER         ("Firefly DevKit (Pixie rev.6)")
#define DEVICE_NAME          ("Firefly")


typedef struct _TransportContext {
    MessageReceived onMessage;
    //uint32_t heartbeat;
    uint16_t transmit_handle;
    uint16_t conn_handle;
} _TransportContext;


// Utility function to log an array of bytes.
void print_bytes(const uint8_t *bytes, int len) {
    for (int i = 0; i < len; i++) {
        MODLOG_DFLT(INFO, "%s0x%02x", i != 0 ? ":" : "", bytes[i]);
    }
}

void print_addr(const char *prefix, const void *addr) {
    const uint8_t *u8p = addr;
    printf("%s%02x:%02x:%02x:%02x:%02x:%02x\n", prefix,
      u8p[5], u8p[4], u8p[3], u8p[2], u8p[1], u8p[0]);
}


// @TODO: What is this?
static uint8_t blehr_addr_type;

// Generalized onSync callback. This is mainly so we
// can pass and argument to the sync callback

typedef void (*SyncCallback)(void *contxt);

typedef struct PendingSync {
    SyncCallback callback;
    void *context;
} PendingSync;

static int isSync = 0;
QueueHandle_t syncQueue = NULL;

void initSyncCallback() {
    if (syncQueue != NULL) { return; }
    syncQueue = xQueueCreate(3, sizeof(PendingSync));
}

void drainSyncQueue() {
    while(isSync) {
        PendingSync pending;
        if (!xQueueReceive(syncQueue, &pending, 100)) { break; }
        pending.callback(pending.context);
    }
}

uint32_t onSync(SyncCallback callback, void *context) {
    if (isSync) {
        callback(context);
        return true;
    }
    PendingSync pending = { .callback = callback, .context = context };
    return xQueueSend(syncQueue, &pending, 100);
}

static void ble_onSync(void) {
    int rc;

    rc = ble_hs_id_infer_auto(0, &blehr_addr_type);
    assert(rc == 0);

    uint8_t addr[6] = {0};
    rc = ble_hs_id_copy_addr(blehr_addr_type, addr, NULL);

    print_addr("[ble] sync addr=", addr);

    isSync = 1;

    drainSyncQueue();
}

static void ble_onReset(int reason) {
    isSync = 0;
    printf("[ble] reset=%d\n", reason);
}


// Device Information Service
#define UUID_SVC_DEVICE_INFO                        (0x180A)
#define UUID_CHR_MANUFACTURER_NAME_STRING           (0x2A29)
#define UUID_CHR_MODEL_NUMBER_STRING                (0x2A24)
#define UUID_CHR_FIRMWARE_REVISION_STRING           (0x2A26)


// Heart-rate configuration
//#define GATT_HRS_UUID                           0x180D
//#define GATT_HRS_MEASUREMENT_UUID               0x2A37
//#define GATT_HRS_BODY_SENSOR_LOC_UUID           0x2A38

#define UUID_SVC_SPP                            (0xABF0)
#define UUID_CHR_TRANSMIT                       (0xABF1)


static int ble_chrAccessTransmit(uint16_t conn_handle,
    uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {

    // Sensor location, set to "Chest"
    //uint16_t uuid;
    //int rc;
    //uint16_t uuid = ble_uuid_u16(ctxt->chr->uuid);

    switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR: {
            MODLOG_DFLT(INFO, "Callback for read");

            char buffer[] = "Hello";

            int rc = os_mbuf_append(ctxt->om, &buffer, sizeof(buffer));

            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }

        case BLE_GATT_ACCESS_OP_WRITE_CHR: {
            MODLOG_DFLT(INFO, "Data received in write event,conn_handle = %x,attr_handle = %x", conn_handle, attr_handle);
            uint16_t length = os_mbuf_len(ctxt->om);
            uint8_t buffer[length];
            os_mbuf_copydata(ctxt->om, 0, length, buffer);
            printf("READ: ");
            for (int i = 0; i < length; i++) {
                printf("%02x", buffer[i]);
                if ((i % 8) == 0) { printf(" "); }
            }
            printf("\n");

            return 0;
        }

        default:
            MODLOG_DFLT(INFO, "\nDefault Callback");
            break;
    }

    return 0;
/*
    if (uuid == GATT_HRS_BODY_SENSOR_LOC_UUID) {
        rc = os_mbuf_append(ctxt->om, &body_sens_loc, sizeof(body_sens_loc));

        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    assert(0);
    return BLE_ATT_ERR_UNLIKELY;
*/
}

static int gatt_svr_chr_access_device_info(uint16_t conn_handle,
  uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {

    uint16_t uuid;
    int rc;

    uuid = ble_uuid_u16(ctxt->chr->uuid);

    if (uuid == UUID_CHR_MODEL_NUMBER_STRING) {
        const char *value = MODEL_NUMBER;
        rc = os_mbuf_append(ctxt->om, value, strlen(value));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    if (uuid == UUID_CHR_MANUFACTURER_NAME_STRING) {
        const char *value = MANUFACTURER_NAME;
        rc = os_mbuf_append(ctxt->om, value, strlen(value));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    if (uuid == UUID_CHR_FIRMWARE_REVISION_STRING) {
        int major = 0, minor = 1, patch = 1;
        char revisionStr[64];
        snprintf(revisionStr, sizeof(revisionStr), "v%d.%d.%d (%04d-%02d-%02d %02d:%02d)",
          major, minor, patch, BUILD_YEAR, BUILD_MONTH,
          BUILD_DAY, BUILD_HOUR, BUILD_SEC);
        rc = os_mbuf_append(ctxt->om, revisionStr, strlen(revisionStr));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    assert(0);
    return BLE_ATT_ERR_UNLIKELY;
}

void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg) {
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

int gatt_server_init(const struct ble_gatt_svc_def *services) {
    int rc;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    rc = ble_gatts_count_cfg(services);
    if (rc != 0) {
        return rc;
    }

    rc = ble_gatts_add_svcs(services);
    if (rc != 0) {
        return rc;
    }

    return 0;
}


static bool notify_state;

static int ble_gap_event(struct ble_gap_event *event, void *arg);


static void ble_advertise(void *context) {

    struct ble_hs_adv_fields fields;
    memset(&fields, 0, sizeof(fields));

    // Advertise:
    //  - Discoverability in forthcoming advertisement (general)
    //  - BLE-only (BR/EDR unsupported)
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    // Include TX power
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    // Set device name
    const char *device_name = DEVICE_NAME;
    fields.name = (uint8_t *)device_name;
    fields.name_len = strlen(device_name);
    fields.name_is_complete = 1;

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
        int rc = ble_gap_adv_start(blehr_addr_type, NULL, BLE_HS_FOREVER,
          &adv_params, ble_gap_event, context);

        if (rc != 0) {
            MODLOG_DFLT(ERROR, "error enabling advertisement; rc=%d\n", rc);
            return;
        }
    }
}

// This function simulates heart beat and notifies it to the client
/*
static void blehr_tx_hrate(TimerHandle_t ev) {
    static uint8_t hrm[2];
    int rc;
    struct os_mbuf *om;

    _TransportContext *context = (_TransportContext*)pvTimerGetTimerID(ev);

    if (!notify_state) {
        context->heartbeat = 90;
        return;
    }

    hrm[0] = 0x06; // contact of a sensor
    hrm[1] = context->heartbeat; // storing dummy data

    // Simulation of heart beats
    context->heartbeat++;
    if (context->heartbeat == 160) {
        context->heartbeat = 90;
    }

    om = ble_hs_mbuf_from_flat(hrm, sizeof(hrm));
    rc = ble_gatts_notify_custom(context->conn_handle, context->hrs_hrm_handle, om);

    assert(rc == 0);
}
*/

static int ble_gap_event(struct ble_gap_event *event, void *_context) {
    _TransportContext *context = (_TransportContext*)_context;

    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            // A new connection was established or a connection attempt failed
            MODLOG_DFLT(INFO, "connection %s; status=%d\n",
                        event->connect.status == 0 ? "established" : "failed",
                        event->connect.status);

            if (event->connect.status != 0) {
                //Connection failed; resume advertising
                ble_advertise(context);
            }
            context->conn_handle = event->connect.conn_handle;
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            MODLOG_DFLT(INFO, "disconnect; reason=%d\n", event->disconnect.reason);

            // Connection terminated; resume advertising
            ble_advertise(context);
            break;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            MODLOG_DFLT(INFO, "adv complete\n");
            ble_advertise(context);
            break;

        case BLE_GAP_EVENT_SUBSCRIBE:
            MODLOG_DFLT(INFO, "subscribe event; cur_notify=%d\n value handle; "
                        "val_handle=%d\n",
                        event->subscribe.cur_notify, context->transmit_handle);
            if (event->subscribe.attr_handle == context->transmit_handle) {
                notify_state = event->subscribe.cur_notify;
                //blehr_tx_hrate_reset();
            } else if (event->subscribe.attr_handle != context->transmit_handle) {
                notify_state = event->subscribe.cur_notify;
                //blehr_tx_hrate_stop();
            }
            ESP_LOGI("BLE_GAP_SUBSCRIBE_EVENT", "conn_handle from subscribe=%d", context->conn_handle);
            break;

        case BLE_GAP_EVENT_MTU:
            MODLOG_DFLT(INFO, "mtu update event; conn_handle=%d mtu=%d\n",
                        event->mtu.conn_handle,
                        event->mtu.value);
            break;

    }

    return 0;
}

void ble_runTask() {
    printf("[ble] BLE Host Task Started\n");

    // Runs until nimble_port_stop() is called
    nimble_port_run();

    // Should this restart the nimble?
    nimble_port_freertos_deinit();

    printf("[ble] BLE Host Task Stopped\n");
}
/*
#define RW(v)      ((((v) + 3)) / 4)

typedef struct BleCounts {
    size_t svcCount;
    size_t chrCount;
    size_t uuid16Count;
    size_t uuid32Count;
    size_t uuid128Count;
    size_t badCount;
} BleCounts;

static void ble_computeUuidCounts(ble_uuid_t *uuid, BleCounts *counts) {
    switch (uuid->u) {
        case BLE_UUID_TYPE_16:
            counts->uuid16Count++;
            break;
        case BLE_UUID_TYPE_32:
            counts->uuid32Count++;
            break;
        case BLE_UUID_TYPE_128:
            counts->uuid128Count++;
            break;
        default:
            counts->badCount++;
            break;
    }
}

static void ble_computeCounts(const struct ble_gatt_svc_def *services, BleCounts *counts) {
    memset(counts, 0, sizeof(BleCounts);

    int s = 0;
    while (true) {
        counts->svcCount++;

        const struct ble_gatt_svc_def *svc = &services[s++];
        if (svc->type == 0) { break; }

        bleComnputeUuidCounts(svc->uuid, counts);

        int c = 0;
        while (true) {
            counts->chrCount++;

            const struct ble_gatt_chr_def *chr = &svc->characteristics[c++];
            if (chr->uuid == NULL) { break; }

            bleComnputeUuidCounts(chr->uuid, counts);
        }
    }
}

static ble_gatt_svc_def* ble_copyServices(void* data, size_t length, ble_gatt_svc_def *services) {
    BleCounts counts;
    ble_computeCounts(services, &counts);

    const length = counts->svcCount * RW(sizeof(ble_gatt_svc_def)) +
        counts->chrCount * RW(sizeof(ble_gatt_chr_def)) +
        counts->uuid16Count * RW(sizeof(ble_uuid16_t)) +
        counts->uuid32Count * RW(sizeof(ble_uuid32_t)) +
        counts->uuid128Count * RW(sizeof(ble_uuid128_t));

    const uint8_t *data = (uint8_t*)malloc(length);
    memset(data, 0, length);

    uint32_t offset = 0;

    ble_gatt_svc_def *svcCopy = &data[offset];
    offset += counts->svcCount * RW(sizeof(ble_gatt_svc_def));
    ble_gatt_chr_def *chrCopy = &data[offset];
    offset += counts->chrCount * RW(sizeof(ble_gatt_chr_def));
    ble_uuid16_t uuid16Copy = &data[offset];
    offset += counts->uuid16Count * RW(sizeof(ble_uuid16_t));
    ble_uuid32_t uuid16Copy = &data[offset];
    offset += counts->uuid16Count * RW(sizeof(ble_uuid32_t));
    ble_uuid128_t uuid16Copy = &data[offset];

    int s = 0;
    while (true) {
        const struct ble_gatt_svc_def *svc = &services[s];
        const struct ble_gatt_svc_def *svcDst = &svcCopy[s++];

        if (svc->type == 0) { break; }

        svcDst->type = svc->type;


        //bleComnputeUuidCounts(svc->uuid, counts);

        int c = 0;
        while (true) {
            const struct ble_gatt_chr_def *chr = &svc->characteristics[c];
            const struct ble_gatt_chr_def *chr = &svc->characteristics[c++];

            if (chr->uuid == NULL) { break; }

            bleComnputeUuidCounts(chr->uuid, counts);
        }
    }
}
*/

void transport_task(void* pvParameter) {
    initSyncCallback();

    _TransportContext context;
    //context->onMessage = onMessage;
    context.transmit_handle = 0;

    const struct ble_gatt_svc_def services[] = {
        {
            // Service: Heart-rate
            .type = BLE_GATT_SVC_TYPE_PRIMARY,
            //.uuid = BLE_UUID16_DECLARE(GATT_HRS_UUID),
            .uuid = BLE_UUID16_DECLARE(UUID_SVC_SPP),
            .characteristics = (struct ble_gatt_chr_def[])
            { {
                    // Characteristic: Heart-rate measurement
                    .uuid = BLE_UUID16_DECLARE(UUID_CHR_TRANSMIT),
                    .access_cb = ble_chrAccessTransmit,
                    .val_handle = &context.transmit_handle,
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
/*
                    // Characteristic: Heart-rate measurement
                    .uuid = BLE_UUID16_DECLARE(GATT_HRS_MEASUREMENT_UUID),
                    .access_cb = ble_chrAccessTransmit,
                    .val_handle = &context.hrs_hrm_handle,
                    .flags = BLE_GATT_CHR_F_NOTIFY,
                }, {
                    // Characteristic: Body sensor location
                    .uuid = BLE_UUID16_DECLARE(GATT_HRS_BODY_SENSOR_LOC_UUID),
                    .access_cb = gatt_svr_chr_access_heart_rate,
                    .flags = BLE_GATT_CHR_F_READ,
*/
                }, {
                    0, // No more characteristics in this service
                },
            }
        },

        {
            // Service: Device Information
            .type = BLE_GATT_SVC_TYPE_PRIMARY,
            .uuid = BLE_UUID16_DECLARE(UUID_SVC_DEVICE_INFO),
            .characteristics = (struct ble_gatt_chr_def[])
            { {
                    // Characteristic: * Manufacturer name
                    .uuid = BLE_UUID16_DECLARE(UUID_CHR_MANUFACTURER_NAME_STRING),
                    .access_cb = gatt_svr_chr_access_device_info,
                    .flags = BLE_GATT_CHR_F_READ,
                }, {
                    // Characteristic: Model number string
                    .uuid = BLE_UUID16_DECLARE(UUID_CHR_MODEL_NUMBER_STRING),
                    .access_cb = gatt_svr_chr_access_device_info,
                    .flags = BLE_GATT_CHR_F_READ,
                }, {
                    // Characteristic: Model number string
                    .uuid = BLE_UUID16_DECLARE(UUID_CHR_FIRMWARE_REVISION_STRING),
                    .access_cb = gatt_svr_chr_access_device_info,
                    .flags = BLE_GATT_CHR_F_READ,
                }, {
                    0, // No more characteristics in this service
                },
            }
        },

        {
            0, // No more services
        },
    };

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

    //printf("MOO: %d %d %d %d\n", ble_computeLength(services), sizeof(struct ble_gatt_svc_def), sizeof(struct ble_gatt_chr_def), sizeof(ble_uuid128_t));

    // Initialize the NimBLE host configuration
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
    ble_hs_cfg.gatts_register_arg = &context;

    ble_hs_cfg.sync_cb = ble_onSync;
    ble_hs_cfg.reset_cb = ble_onReset;

    onSync(ble_advertise, &context);

    // name, period/time,  auto reload, timer ID, callback
    //TimerHandle_t blehr_tx_timer = xTimerCreate("blehr_tx_timer", pdMS_TO_TICKS(1000), pdTRUE, &context, blehr_tx_hrate);
    //xTimerStart(blehr_tx_timer, 0);

    assert(gatt_server_init(services) == 0);

    // Set the default device name
    const char *device_name = DEVICE_NAME;
    assert(ble_svc_gap_device_name_set(device_name) == 0);

    // Run forever
    nimble_port_freertos_init(ble_runTask);

    while (1) {
        delay(1000);
    }
}

