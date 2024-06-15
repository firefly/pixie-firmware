#include <stdio.h>

#include "esp_efuse.h"
#include "nvs_flash.h"

#include <soc/soc.h>

#include "./device-info.h"


typedef enum AttestState {
    AttestStateNotLoaded = 0,
    AttestStateValid = 1,
    AttestStateBadInit = -1,
    AttestStateBadOpen = -2,
    AttestStateBadRead = -3,
    AttestStateBadVerify = -4,
} AttestState;


static uint32_t modelNumber = 0;
static uint32_t serialNumber = 0;

static int32_t attestState = AttestStateNotLoaded;
static uint8_t attestSig[32];


static uint32_t load_efuse() {
    uint32_t deviceInfo = esp_efuse_read_reg(EFUSE_BLK3, 0);

    uint32_t zeros = (deviceInfo >> 27) + 1;
    for (int i = 0; i < 27; i++) {
        if (!(deviceInfo & (1 << i))) { zeros--; }
    }

    if (zeros) {
        printf("[device] invalid device info: info=%08lx\n", deviceInfo);
        return 0;
    }

    serialNumber = deviceInfo & 0x1ffff;
    modelNumber = (deviceInfo >> 21) & 0x3f;

    uint32_t rev = serialNumber >> 10;
    if (modelNumber == 1 && rev) {
        serialNumber &= 0x3ff;
        printf("[device] detected: model=prototype:rev.%ld, serial=%ld\n", rev, serialNumber);
    } else {
        printf("[device] detected: model=%ld, serial=%ld\n", modelNumber, serialNumber);
    }

    return 1;
}

static uint32_t load_nvs() {
    esp_err_t err = nvs_flash_init_partition("attest");
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        printf("[device] attest init failure: status=%d\n", err);
        attestState = AttestStateBadInit;
        return 0;
    }

    nvs_handle_t nvs;
    err = nvs_open_from_partition("attest", "root", NVS_READONLY, &nvs);
    if (err) {
        printf("[device] attest open failure: status=%d", err);
        attestState = AttestStateBadOpen;
        return 0;
    }

    size_t length = sizeof(attestSig);
    err = nvs_get_blob(nvs, "sig", &attestSig, &length);
    if (err || length != 32) {
        printf("[device] attest read failure: status=%d length=%d", err, length);
        attestState = AttestStateBadRead;
        return 0;
    }

    // @TODO: Validate attest

    printf("[device] attestation: sig=");
    for (int i = 0; i < length; i++) { printf("%02x", attestSig[i]); }
    printf(" (%d)\n", length);

    attestState = AttestStateValid;

    return 1;
}

uint32_t device_init() {
    if (attestState == AttestStateNotLoaded) {
        load_efuse();
        load_nvs();
    }

    //int r = esp_efuse_write_reg(EFUSE_BLK3, 1, 0x9820140f);
    //printf("WROTE: %d", r);
    //printf("AA\n");
    //REG_WRITE(0x3fc94d80 + 4, 0x42434445);
    //printf("BB\n");

    return (serialNumber > 0 && modelNumber > 0 && attestState > 0) ? 1: 0;
}

uint32_t device_modelNumber() { return modelNumber; }
uint32_t device_serialNumber() { return serialNumber; }

uint32_t device_canAttest() {
    return (attestState > 0) ? 1: 0;;
}

uint32_t device_attest(uint32_t timestamp, uint32_t challenge, uint8_t *resp) {
    return 0;
}
