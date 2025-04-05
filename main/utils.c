
#include "./utils.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


uint32_t ticks() {
    return xTaskGetTickCount();
}

void delay(uint32_t duration) {
    vTaskDelay((duration + portTICK_PERIOD_MS - 1) / portTICK_PERIOD_MS);
}

char* taskName() {
    TaskStatus_t xTaskDetails;
    vTaskGetInfo(NULL, &xTaskDetails, pdFALSE, eInvalid);
    return xTaskDetails.pcTaskName;
}
