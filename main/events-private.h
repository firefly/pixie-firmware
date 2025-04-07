#ifndef __EVENTS_PRIVATE_H__
#define __EVENTS_PRIVATE_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 *  These are structs and functions used internally and not generally
 *  useful outside core functionality, mainly between Panels and Events.
 */


#include <stdint.h>

#include "events.h"
#include "panel.h"

/**
 *  The struct storing a Panels state. This is stored on the stack of
 *  the Panel and is reclaimed when the task exists. It should not have
 *  any references maintained to it as it could vanish at any time.
 */
typedef struct PanelContext {
    QueueHandle_t events;
    int id;
    uint8_t *state;
    FfxNode node;
    struct PanelContext *parent;
    PanelStyle style;
} PanelContext;

/**
 *  The struct stored in the Task Queue for a Panel to manage
 *  pending events.
 */
typedef struct EventDispatch {
    EventCallback callback;
    void* arg;
    EventPayload payload;
} EventDispatch;

/**
 *  Reference to the current activePanel. Do not store a reference to
 *  this as it could vanish.
 */
extern PanelContext *activePanel;

/**
 *  Initialize any necessary event data structures.
 */
void events_init();

/**
 *  Remove all filters for %%panel%%. Used when a Panel is destroyed.
 */
void events_clearFilters(PanelContext *panel);



#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EVENTS_PRIVATE_H__ */
