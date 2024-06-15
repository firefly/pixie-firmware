#ifndef __DEVICE_INFO_H__
#define __DEVICE_INFO_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include <stdint.h>

uint32_t device_init();

uint32_t device_modelNumber();
uint32_t device_serialNumber();

uint32_t device_canAttest();

/**
 *  Response:
 *   - device info [4 bytes]
 *   - P_device.address [20 bytes]
 *   - P_firefly.sign(device info ++ P_device.address) [64 bytes]
 *   - P_device.sign(device info ++ timestamp ++ challenge) [64 bytes]
 *
 *  Verify:
 *   - ecrecover(deviceInfo ++ P_device.address) => P_firefly.address
 *   - ecrecover(deviceInfo ++ timestamp ++ challenge) => P_device.address
 *
 *  Note:
 *   - all signatures are EIP-2098 compressed signatures
 */
uint32_t device_attest(uint32_t timestamp, uint32_t challenge, uint8_t *resp);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __DEVICE_INFO_H__ */
