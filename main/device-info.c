#include <stdio.h>
#include <string.h>

#include "esp_ds.h"
#include "esp_efuse.h"
#include "esp_heap_caps.h"
#include "esp_random.h"
#include "nvs_flash.h"

#include "firefly-hash.h"

#include "device-info.h"
#include "utils.h"


#define DEVICE_INFO_BLOCK   (EFUSE_BLK3)
#define ATTEST_SLOT         (2)
#define ATTEST_KEY_BLOCK    (EFUSE_BLK_KEY2)
#define ATTEST_HMAC_KEY     (HMAC_KEY2)


static uint32_t modelNumber = 0;
static uint32_t serialNumber = 0;

static DeviceStatus ready = DeviceStatusNotInitialized;

static uint8_t attestProof[64] = { 0 };
static uint8_t pubkeyN[384] = { 0 };
esp_ds_data_t *cipherdata = NULL;

static void reverseBytes(uint8_t *data, size_t length) {
    for (int i = 0; i < length / 2; i++) {
        uint8_t tmp = data[i];
        data[i] = data[length - 1 - i];
        data[length - 1 - i] = tmp;
    }
}

uint32_t device_modelNumber() { return modelNumber; }
uint32_t device_serialNumber() { return serialNumber; }

DeviceStatus device_canAttest() { return ready; }

DeviceStatus device_getModelName(char *output, size_t length) {
    if (length == 0) { return DeviceStatusTruncated; }

    if (ready == DeviceStatusNotInitialized) {
        int l = snprintf(output, length, "[uninitialized]");
        if (l >= length) { return DeviceStatusTruncated; }
        return ready;
    }

    if (ready != DeviceStatusOk) {
        int l = snprintf(output, length, "[failed]");
        if (l >= length) { return DeviceStatusTruncated; }
        return ready;
    }

    if ((modelNumber >> 8) == 1) {
        int l = snprintf(output, length, "Firefly Pixie (DevKit rev.%ld)",
          modelNumber & 0xff);
        if (l >= length) { return DeviceStatusTruncated; }
        return ready;
    }

    int l = snprintf(output, length, "Unknown model: 0x%lx", modelNumber);
    if (l >= length) { return DeviceStatusTruncated; }
    return ready;
}

DeviceStatus device_init() {
    // Already loaded or failed to laod
    if (ready == DeviceStatusOk || ready != DeviceStatusNotInitialized) {
        return ready;
    }

    // Read eFuse info
    uint32_t version = esp_efuse_read_reg(EFUSE_BLK3, 0);
    uint32_t _modelNumber = esp_efuse_read_reg(EFUSE_BLK3, 1);
    uint32_t _serialNumber = esp_efuse_read_reg(EFUSE_BLK3, 2);

    // Invalid eFuse info
    if (version != 0x00000001 || _modelNumber == 0 || _serialNumber == 0) {
        ready = DeviceStatusMissingEfuse;
        return ready;
    }

    // Open the NVS partition
    nvs_handle_t nvs;
    {
        int ret = nvs_flash_init_partition("attest");
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ready = DeviceStatusMissingNvs;
            return ready;
        }

        ret = nvs_open_from_partition("attest", "secure", NVS_READONLY, &nvs);
        if (ret) {
            ready = DeviceStatusMissingNvs;
            return ready;
        }
    }

    // Load the cipherdata
    {
        size_t olen = sizeof(esp_ds_data_t);
        cipherdata = heap_caps_malloc(sizeof(esp_ds_data_t), MALLOC_CAP_DMA);
        memset(cipherdata, 0, sizeof(esp_ds_data_t));
        int ret = nvs_get_blob(nvs, "cipherdata", cipherdata, &olen);

        if (ret || olen != sizeof(esp_ds_data_t)) {
            free(cipherdata);
            cipherdata = NULL;
            ready = DeviceStatusMissingNvs;
            return ready;
        }
    }

    // Load the attest proof
    {
        size_t olen = 64;
        int ret = nvs_get_blob(nvs, "attest", attestProof, &olen);
        if (ret || olen != 64) {
            ready = DeviceStatusMissingNvs;
            return ready;
        }
    }

    // Load the RSA public key
    {
        size_t olen = 384;
        int ret = nvs_get_blob(nvs, "pubkey-n", pubkeyN, &olen);
        if (ret || olen != 384) {
            ready = DeviceStatusMissingNvs;
            return ready;
        }
    }

    serialNumber = _serialNumber;
    modelNumber = _modelNumber;

    ready = DeviceStatusOk;
    return ready;
}

DeviceStatus device_attest(uint8_t *challenge, DeviceAttestation *attest) {
    if (!ready) { return ready; }

    size_t offset = 0;
    uint8_t attestation[sizeof(DeviceAttestation) - 384] = { 0 };

    attest->version = 1;
    attestation[offset++] = 1;

    esp_fill_random(attest->nonce, 16);
    memcpy(&attestation[offset], attest->nonce, 16);
    offset += 16;

    memcpy(attest->challenge, challenge, 32);
    memcpy(&attestation[offset], attest->challenge, 32);
    offset += 32;

    attest->modelNumber = modelNumber;
    attestation[offset++] = (modelNumber >> 24) & 0xff;
    attestation[offset++] = (modelNumber >> 16) & 0xff;
    attestation[offset++] = (modelNumber >> 8) & 0xff;
    attestation[offset++] = (modelNumber >> 0) & 0xff;

    attest->serialNumber = serialNumber;
    attestation[offset++] = (serialNumber >> 24) & 0xff;
    attestation[offset++] = (serialNumber >> 16) & 0xff;
    attestation[offset++] = (serialNumber >> 8) & 0xff;
    attestation[offset++] = (serialNumber >> 0) & 0xff;

    memcpy(&attest->pubkeyN, pubkeyN, sizeof(pubkeyN));
    memcpy(&attestation[offset], pubkeyN, sizeof(pubkeyN));
    offset += sizeof(pubkeyN);

    memcpy(&attest->attestProof, attestProof, sizeof(attestProof));
    memcpy(&attestation[offset], attestProof, sizeof(attestProof));
    offset += sizeof(attestProof);

    uint8_t hash[384] = { 0 };
    memset(hash, 0x42, sizeof(hash));

    FfxSha256Context ctx;
    ffx_hash_initSha256(&ctx);
    ffx_hash_updateSha256(&ctx, attestation, sizeof(attestation));
    ffx_hash_finalSha256(&ctx, hash);
    reverseBytes(hash, 32);

    uint8_t sig[384] = { 0 };

    // Sync version
    int ret = esp_ds_sign(hash, cipherdata, ATTEST_HMAC_KEY, sig);
    if (ret) { return DeviceStatusFailed; }

    // Async version
    /*
    esp_ds_context_t *signCtx = NULL;
    int ret = esp_ds_start_sign(hash, cipherdata, ATTEST_HMAC_KEY, &signCtx);
    if (ret) { return DeviceStatusFailed; }
    while (esp_ds_is_busy()) { delay(1); }
    ret = esp_ds_finish_sign(sig, signCtx);
    if (ret) { return DeviceStatusFailed; }
    */

    reverseBytes(sig, sizeof(sig));

    memcpy(attest->signature, sig, sizeof(sig));

    return DeviceStatusOk;
}
