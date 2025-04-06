#ifndef __EVENTS_PRIVATE_H__
#define __EVENTS_PRIVATE_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include <stdint.h>

#include "events.h"
#include "panel.h"


typedef struct PanelContext {
    QueueHandle_t events;
    int id;
    uint8_t *state;
    FfxNode node;
    struct PanelContext *parent;
    PanelStyle style;
} PanelContext;


typedef struct EventDispatch {
    EventCallback callback;
    void* arg;
    EventPayload payload;
} EventDispatch;


void events_init();

// rename?
void deregisterPanel(PanelContext *panel);



#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EVENTS_PRIVATE_H__ */
