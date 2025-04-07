#include <stdio.h>

#include "freertos/FreeRTOS.h"

#include "events-private.h"

#include "panel.h"


#define MAX_EVENT_FILTERS  (32)

typedef struct EventFilter {
    int id;

    PanelContext *panel;

    EventName event;
    EventCallback callback;
    void *arg;
} EventFilter;

// List of filters
static EventFilter eventFilters[MAX_EVENT_FILTERS] = { 0 };

// Lock to acquire before modifying eventFilters.
static StaticSemaphore_t lockEventsBuffer;
static SemaphoreHandle_t lockEvents;


///////////////////////////////
// Private API (events-private.h)

void events_init() {
    lockEvents = xSemaphoreCreateBinaryStatic(&lockEventsBuffer);
    xSemaphoreGive(lockEvents);
}

void events_clearFilters(PanelContext *panel) {
    xSemaphoreTake(lockEvents, portMAX_DELAY);

    for (int i = 0; i < MAX_EVENT_FILTERS; i++) {
        EventFilter *filter = &eventFilters[i];
        if (filter->event == 0 || filter->panel != panel) { continue; }
        filter->event = 0;
    }

    xSemaphoreGive(lockEvents);
}


///////////////////////////////
// API

void panel_emitEvent(EventName eventName, EventPayloadProps props) {

     EventDispatch event = { 0 };

     xSemaphoreTake(lockEvents, portMAX_DELAY);

    static uint32_t countOk = 0, countFail = 0;
    for (int i = 0; i < MAX_EVENT_FILTERS; i++) {
        EventFilter *filter = &eventFilters[i];

        // Filter events (and munge/set payload props)

        if (filter->event == 0 || filter->panel != activePanel) {
            // Not an event or not an event for the active panel
            continue;

        } else if (eventName == EventNameRenderScene) {
            if (filter->event != EventNameRenderScene) { continue; }

            event.payload.props = props; // Does this work?

        } else if (eventName == EventNameMessage) {
            if (filter->event != EventNameMessage) { continue; }

            event.payload.props = props; // Does this work?

        } else if (eventName & EventNamePanel) {
            if (filter->event != eventName) { continue; }

            event.payload.props = props; // Does this work?

        } else if (eventName & EventNameKeys) {
            if (!(filter->event & EventNameKeys)) { continue; }

            // This keys this event cares about
            Keys keys = filter->event & KeyAll;
            Keys changed = keys & props.keys.changed;
            Keys down = props.keys.down;

            // No keys for this event changed
            if (!changed) { continue; }

            switch (filter->event & EventNameMask) {
                case EventNameKeysDown:
                    if (!(keys & changed & down)) { continue; }
                    break;
                case EventNameKeysUp:
                    if (!(keys & changed & ~down)) { continue; }
                    break;
                case EventNameKeysChanged:
                    break;
            }

            event.payload.props.keys.down = down;
            event.payload.props.keys.changed = changed;

        } else if (eventName & EventNameCustom) {
            if (filter->event != eventName) { continue; }

            event.payload.props = props; // Does this work?

        } else {
            printf("emit: unknown event: 0x%08x\n", eventName);
            continue;
        }

        event.callback = filter->callback;
        event.arg = filter->arg;
        event.payload.event = filter->event;
        event.payload.eventId = filter->id;

        QueueHandle_t events = filter->panel->events;
        BaseType_t status = xQueueSendToBack(events, &event, 0);
        if (status != pdTRUE) {
            printf("FAILED TO QUEUE EVENT: %02x", eventName);
        /*
            countFail++;
            if (countFail == 1 || countFail == 100) {
                printf("[%s] emit:RenderScene failed: to=panel-%d id=%d ok=%ld>
                  taskName(NULL), filter->panel->id, filter->id, countOk,
                  countFail, status);
                if (countFail == 100) { countOk = countFail = 0; }
            }
        } else {
            countOk++;
            */
        }
    }

    xSemaphoreGive(lockEvents);
}

// Caller must own the lockEvents mutex
static EventFilter* getEmptyFilter() {
    for (int i = 0; i < MAX_EVENT_FILTERS; i++) {
        EventFilter *filter = &eventFilters[i];

        // Find an empty DispatchWait entry
        if (filter->event != 0) { continue; }

        return filter;
    }

    return NULL;
}

int panel_onEvent(EventName event, EventCallback callback, void *arg) {
    PanelContext *ctx = (void*)xTaskGetApplicationTaskTag(NULL);
    if (ctx == NULL) { return 0; }

    static int nextFilterId = 1;

    int filterId = nextFilterId++;

    xSemaphoreTake(lockEvents, portMAX_DELAY);

    EventFilter *filter = getEmptyFilter(event);
    if (filter == NULL) {
        // @TODO: panic?
        printf("No filter\n");
        xSemaphoreGive(lockEvents);
        return -1;
    }

    filter->id = filterId;
    filter->panel = ctx;

    filter->event = event;
    filter->callback = callback;
    filter->arg = arg;

    xSemaphoreGive(lockEvents);

    return filterId;
}

void panel_offEvent(int filterId) {
    PanelContext *ctx = (void*)xTaskGetApplicationTaskTag(NULL);
    if (ctx == NULL) { return; }

    xSemaphoreTake(lockEvents, portMAX_DELAY);
    for (int i = 0; i < MAX_EVENT_FILTERS; i++) {
        EventFilter *filter = &eventFilters[i];
        if (filter->id == filterId) {
            // Only a task can offEvent its own tasks
            if (filter->panel == ctx) { filter->event = 0; }
            break;
        }
    }

    xSemaphoreGive(lockEvents);
}

