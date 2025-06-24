
#include "./utils.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


uint32_t ticks() {
    return xTaskGetTickCount();
}

void delay(uint32_t duration) {
    vTaskDelay(pdMS_TO_TICKS(duration));
}

char* taskName() {
    TaskStatus_t xTaskDetails;
    vTaskGetInfo(NULL, &xTaskDetails, pdFALSE, eInvalid);
    return xTaskDetails.pcTaskName;
}
