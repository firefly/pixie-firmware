
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "events-private.h"
#include "task-ble.h"
#include "task-io.h"

#include "device-info.h"
#include "utils.h"

#include "panel-connect.h"
#include "./panel-menu.h"
#include "./panel-space.h"
//#include "./panel-keyboard.h"
//#include "./panel-game.h"


void app_main() {
    vTaskSetApplicationTaskTag( NULL, (void*)NULL);

    TaskHandle_t taskIoHandle = NULL;
    TaskHandle_t taskBleHandle = NULL;

    // Initialie the events
    events_init();

    // Load NVS and eFuse provision data
    {
        DeviceStatus status = device_init();
        printf("[main] device initialized: status=%d serial=%ld model=%ld\n",
          status, device_serialNumber(), device_modelNumber());
    }

    // Start the IO task (handles the display, LEDs and keypad)
    {
        // Pointer passed to taskIoFunc to notify us when IO is ready
        uint32_t ready = 0;

        BaseType_t status = xTaskCreatePinnedToCore(&taskIoFunc, "io", 8192, &ready, 3, &taskIoHandle, 0);
        printf("[main] start IO task: status=%d\n", status);
        assert(taskIoHandle != NULL);

        // Wait for the IO task to complete setup
        while (!ready) { delay(1); }
        printf("[main] IO ready\n");
    }

    // Start the Message task (handles BLE messages)
    {
        // Pointer passed to taskReplFunc to notify us when REPL is ready
        uint32_t ready = 0; // @TODO: set this to 0 and set in the task

        BaseType_t status = xTaskCreatePinnedToCore(&taskBleFunc, "ble", 5 * 1024, &ready, 2, &taskBleHandle, 0);
        printf("[main] start BLE task: status=%d\n", status);
        assert(taskBleHandle != NULL);

        // Wait for the REPL task to complete setup
        while (!ready) { delay(1); }
        printf("[main] BLE ready\n");
    }

    // Start the App Process; this is started in the main task, so
    // has high-priority. Don't doddle.
    // @TODO: should we start a short-lived low-priority task to start this?
    //app_push(panelMenuInit, sizeof(PanelMenuState), NULL);
    pushPanelMenu(NULL);
    //pushPanelSpace(NULL);
    //pushPanelKeyboard(NULL);
    //pushPanelGame(NULL);
    //pushPanelConnect(NULL);

    while (1) {
        printf("[main] high-water: boot=%d io=%d, ble=%d freq=%ld\n",
            uxTaskGetStackHighWaterMark(NULL),
            uxTaskGetStackHighWaterMark(taskIoHandle),
            uxTaskGetStackHighWaterMark(taskBleHandle),
            portTICK_PERIOD_MS);
        delay(60000);
    }
}





