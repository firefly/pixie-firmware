#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "panel.h"
#include "events-private.h"

#include "utils.h"


// See: task-io.c
extern FfxScene scene;


#define MAX_EVENT_BACKLOG  (8)

typedef struct _PanelInit {
    PanelInit init;
    int id;
    size_t stateSize;
    void *arg;
    uint32_t ready;
    PanelStyle style;
} _PanelInit;

PanelContext *activePanel = NULL;


static void _panelFocus(FfxNode node, FfxSceneActionStop stopType, void *arg) {
    panel_emitEvent(EventNamePanelFocus, (EventPayloadProps){
        .panel = { .id = activePanel->id }
    });
}

static void _panelBlur(FfxNode node, FfxSceneActionStop stopType, void *arg) {

    // Remove the node from the scene graph
    ffx_sceneNode_remove(node, true);

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


///////////////////////////////
// API

void panel_push(PanelInit init, size_t stateSize, PanelStyle style,
  void *arg) {

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

void panel_pop() {
    PanelContext *panel = (void*)xTaskGetApplicationTaskTag(NULL);

    // Remove all existing events
    events_clearFilters(panel);

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

