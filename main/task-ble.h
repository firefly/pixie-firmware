#ifndef __BLE_H__
#define __BLE_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>


// 16kb
#define MAX_MESSAGE_SIZE        (1 << 14)

uint32_t ble_init();

void taskBleFunc(void* pvParameter);

typedef void* TransportContext;

typedef void (*MessageReceived)(TransportContext context,
    uint32_t command, uint8_t *data, size_t length);

bool panel_isMessageEnabled();
void panel_enableMessage(bool enable);

//void transport_task(void* pvParameter);
//uint32_t transport_send(TransportContext context,
//    uint32_t command, uint8_t *data, size_t length);

//uint32_t transport_wait(TransportContext context);

size_t panel_copyMessage(uint32_t messageId, uint8_t *output);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __BLE_H__ */
