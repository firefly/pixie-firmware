#ifndef __EVENTS_H__
#define __EVENTS_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>

#include "firefly-cbor.h"

// Needed for PanelContext... Move?
//#include "firefly-scene.h"
//#include "freertos/FreeRTOS.h"

typedef uint16_t Keys;

typedef enum Key {
    KeyNone          = 0,
    KeyNorth         = (1 << 0),
    KeyEast          = (1 << 1),
    KeySouth         = (1 << 2),
    KeyWest          = (1 << 3),
    KeyOk            = (1 << 4),
    KeyCancel        = (1 << 5),
    KeyStart         = (1 << 6),
    KeySelect        = (1 << 7),
    KeyAll           = (0xff),
} Key;

#define KeyReset     (KeyCancel | KeyNorth)


// Bottom 24 bits are reserved for filter info.
typedef enum EventName {
    // Render Scene (N.B. filter by equality, bottom bits unused)
    EventNameRenderScene    = ((0x01) << 24),

    // Message events (N.B. filter by equality, bottom bits unused)
    EventNameMessage        = ((0x02) << 24),

    // Keypad events; info=keys (N.B. filter by & EventNameKeys)
    EventNameKeysDown       = ((0x11) << 24),
    EventNameKeysUp         = ((0x12) << 24),
//    EventNameKeysPress      = ((0x13) << 24),  // @TODO: type repeat-rate
    EventNameKeysChanged    = ((0x14) << 24),
//    EventNameKeysCancelled    = ((0x15) << 24),
    EventNameKeys           = ((0x10) << 24),

    // Panel events; (N.B. filter by & EventNamePanel)
    EventNamePanelFocus     = ((0x21) << 24),
    EventNamePanelBlur      = ((0x22) << 24),
    EventNamePanel          = ((0x20) << 24),

    // Custom event; (N.B. filter by & EventNameCustom)
    EventNameCustom         = ((0x80) << 24),

    // Mask to isolate the event type
    EventNameMask           = ((0xff) << 24),

    // Mask to isolate the event category
    EventNameCategoryMask   = ((0xf0) << 24),
} EventName;

typedef struct EventRenderSceneProps {
    uint32_t ticks;
} EventRenderSceneProps;

typedef enum EventKeysFlags {
    EventKeysFlagsNone        = 0,
    EventKeysFlagsCancelled   = (1 << 0),
} EventKeysFlags;

typedef struct EventKeysProps {
    Keys down;
    Keys changed;
    uint16_t flags;
} EventKeysProps;

typedef struct EventPanelProps {
    int id;
} EventPanelProps;

//typedef struct EventIncomingMessageProps {
//    size_t complete;
//    size_t length;
//} EventIncomingMessageProps;

typedef struct EventMessageProps {
    uint32_t id;
    const char* method;
    FfxCborCursor params;
} EventMessageProps;

typedef struct EventCustomProps {
    uint8_t data[32];
} EventCustomProps;

// @TODO: rename; remove "Event" suffix;
typedef union EventPayloadProps {
    EventRenderSceneProps render;
    EventKeysProps keys;
    EventPanelProps panel;
//    EventIncomingMessageProps incoming;
    EventMessageProps message;
    EventCustomProps custom;
} EventPayloadProps;

typedef struct EventPayload {
    EventName event;
    int eventId;
    EventPayloadProps props;
} EventPayload;

typedef void (*EventCallback)(EventPayload event, void* arg);

void panel_emitEvent(EventName eventName, EventPayloadProps props);
int panel_onEvent(EventName event, EventCallback cb, void* arg);
void panel_offEvent(int eventId);



#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EVENTS_H__ */

