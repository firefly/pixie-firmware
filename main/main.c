
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "events-private.h"
#include "task-ble.h"
#include "task-io.h"

#include "device-info.h"
#include "utils.h"

#include "panel.h"
#include "panel-connect.h"
#include "./panel-menu.h"
#include "./panel-space.h"
//#include "./panel-keyboard.h"
//#include "./panel-game.h"


#define MAX_EVENT_BACKLOG  (8)

///////////////////////////////
// Panels

typedef struct _PanelInit {
    PanelInit init;
    int id;
    size_t stateSize;
    void *arg;
    uint32_t ready;
    PanelStyle style;
} _PanelInit;

// @TODO: fix active panel for events

PanelContext *activePanel = NULL;


extern FfxScene scene;

static void _panelFocus(FfxNode node, FfxSceneActionStop stopType, void *arg) {
    panel_emitEvent(EventNamePanelFocus, (EventPayloadProps){
        .panel = { .id = activePanel->id }
    });
}

static void _panelInit(void *_arg) {
    // Copy the PanelInit so we can unblock the caller and it can
    // free this from its stack (by returning)

    _PanelInit *panelInit = _arg;

    // Create the panel state
    size_t stateSize = panelInit->stateSize ? panelInit->stateSize: 1;
    uint8_t state[stateSize];
    memset(state, 0, stateSize);

    // Create an incoming event queue
    StaticQueue_t eventQueue;
    uint8_t eventStore[MAX_EVENT_BACKLOG * sizeof(EventDispatch)];
    QueueHandle_t events = xQueueCreateStatic(MAX_EVENT_BACKLOG,
      sizeof(EventDispatch), eventStore, &eventQueue);
    assert(events != NULL);

    FfxPoint pNewStart = { 0 };
    FfxPoint pNewEnd = { 0 };
    FfxPoint pOldEnd = { 0 };
    switch (panelInit->style) {
        case PanelStyleInstant:
            break;
        case PanelStyleCoverUp:
            pNewStart.y = 240;
            break;
        case PanelStyleSlideLeft:
            pOldEnd.x = -240;
            pNewStart.x = 240;
            break;
    }

    FfxNode node = ffx_scene_createGroup(scene);

    if (pNewStart.x != 0 || pNewStart.y != 0) {
        ffx_sceneNode_setPosition(node, pNewStart);
    }

    PanelContext *oldPanel = activePanel;

    // Create the Panel context (attached to the task tag)
    PanelContext panel = { 0 };
    panel.id = panelInit->id;;
    panel.state = state;
    panel.events = events;
    panel.node = node;
    panel.parent = activePanel;
    panel.style = panelInit->style;
    vTaskSetApplicationTaskTag(NULL, (void*)&panel);

    activePanel = &panel;

    panelInit->ready = 1;

    // Initialize the Panel with the callback
    panelInit->init(scene, panel.node, panel.state, panelInit->arg);

    ffx_sceneGroup_appendChild(ffx_scene_root(scene), node);

    if (oldPanel && (pOldEnd.x != 0 || pOldEnd.y != 0)) {
        if (PanelStyleInstant) {
            ffx_sceneNode_setPosition(oldPanel->node, pOldEnd);
        } else {
            ffx_sceneNode_animatePosition(oldPanel->node, pOldEnd, 0, 300,
              FfxCurveEaseOutQuad, NULL, NULL);
        }
    }

    if (pNewStart.x != pNewEnd.x || pNewStart.y != pNewEnd.y) {
        if (PanelStyleInstant) {
            ffx_sceneNode_setPosition(node, pNewEnd);
            panel_emitEvent(EventNamePanelFocus, (EventPayloadProps){
                .panel = { .id = activePanel->id }
            });
        } else {
            ffx_sceneNode_animatePosition(node, pNewEnd, 0, 300,
              FfxCurveEaseOutQuad, _panelFocus, NULL);
        }
    } else {
        panel_emitEvent(EventNamePanelFocus, (EventPayloadProps){
            .panel = { .id = activePanel->id }
        });
    }

    // Begin the event loop
    EventDispatch dispatch = { 0 };
    while (1) {
        BaseType_t result = xQueueReceive(events, &dispatch, 1000);
        if (result != pdPASS) { continue; }

        dispatch.callback(dispatch.payload, dispatch.arg);
    }
}

void panel_push(PanelInit init, size_t stateSize, PanelStyle style, void *arg) {

    if (activePanel) {
        panel_emitEvent(EventNamePanelBlur, (EventPayloadProps){
            .panel = { .id = activePanel->id }
        });
    }

    static int nextPanelId = 1;
    int panelId = nextPanelId++;

    char name[configMAX_TASK_NAME_LEN];
    snprintf(name, sizeof(name), "panel-%d", panelId);

    TaskHandle_t handle = NULL;
    _PanelInit panelInit = { 0 };
    panelInit.id = panelId;
    panelInit.init = init;
    panelInit.style = style;
    panelInit.stateSize = stateSize;
    panelInit.arg = arg;

    BaseType_t status = xTaskCreatePinnedToCore(&_panelInit, name,
      4096 + stateSize, &panelInit, 1, &handle, 0);
        printf("[main] init panel task: status=%d\n", status);
        assert(handle != NULL);

    while (!panelInit.ready) { delay(2); } // ?? Maybe use event?
}

static void _panelBlur(FfxNode node, FfxSceneActionStop stopType, void *arg) {

    // Remove the node from the scene graph
    ffx_sceneNode_remove(node, true);

    panel_emitEvent(EventNamePanelFocus, (EventPayloadProps){
        .panel = { .id = activePanel->id }
    });
}

void panel_pop() {
    PanelContext *panel = (void*)xTaskGetApplicationTaskTag(NULL);

    // Remove all existing events
    deregisterPanel(panel);

    activePanel = panel->parent;

    if (panel->style == PanelStyleInstant) {
        ffx_sceneNode_setPosition(activePanel->node, (FfxPoint){
            .x = 0, .y = 0
        });
        _panelBlur(scene, FfxSceneActionStopFinal, panel->node);

    } else {
        FfxPoint pNewStart = ffx_sceneNode_getPosition(activePanel->node);

        FfxPoint pOldEnd = { 0 };
        switch (panel->style) {
            case PanelStyleInstant:
                assert(0);
                break;
            case PanelStyleCoverUp:
                pOldEnd.y = 240;
                break;
            case PanelStyleSlideLeft:
                pOldEnd.x = 240;
                break;
        }

        // Animate the popped active reverse how it arrived
        FfxPoint pOldStart = ffx_sceneNode_getPosition(panel->node);
        if (pOldStart.x != pOldEnd.x || pOldStart.y != pOldEnd.y) {
            ffx_sceneNode_animatePosition(panel->node, pOldEnd, 0, 300,
              FfxCurveEaseInQuad, _panelBlur, NULL);
        } else {
            _panelBlur(scene, FfxSceneActionStopFinal, panel->node);
        }

        if (pNewStart.x != 0 || pNewStart.y != 0) {
            ffx_sceneNode_animatePosition(activePanel->node, ffx_point(0, 0),
              0, 300, FfxCurveEaseInQuad, NULL, NULL);
        }
    }
    // @TODO: move to blur and set a flag to ensure no new events go to
    //        the dead task

    vTaskDelete(NULL);
}

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
//            uxTaskGetStackHighWaterMark(taskAppHandle),
            portTICK_PERIOD_MS);
        delay(60000);
    }
}





