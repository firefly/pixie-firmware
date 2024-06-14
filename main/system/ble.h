#ifndef __BLE_H__
#define __BLE_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>


uint32_t ble_init();


typedef void* TransportContext;

typedef void (*MessageReceived)(TransportContext context,
    uint32_t command, uint8_t *data, size_t length);


void transport_task(void* pvParameter);
uint32_t transport_send(TransportContext context,
    uint32_t command, uint8_t *data, size_t length);

//uint32_t transport_wait(TransportContext context);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __BLE_H__ */
