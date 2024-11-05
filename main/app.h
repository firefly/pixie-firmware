#ifndef __APP_H__
#define __APP_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include <stdint.h>

#include "firefly-scene.h"

#include "./system/keypad.h"


typedef int (*AppInit)(FfxScene scene, FfxNode node, void* app, void* arg);

void app_push(AppInit appInit, size_t stateSize, void* arg);

// Deletes the current app task, returning control to previous app in stack
void app_pop();


// Disabling requests clears any pending message
//void app_enableRequest(bool enabled);

// Blocks the current task until the BLE task accepts and sends
// the response
//bool app_sendResponse(uint8_t *payload, size_t length);

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
    EventNameRenderScene    = ((0x01) << 24),

    // Keypad events; bottom bits indicate keys
    EventNameKeysDown       = ((0x10) << 24),
    EventNameKeysUp         = ((0x11) << 24),
    EventNameKeysPress      = ((0x12) << 24),
    EventNameKeysChanged    = ((0x13) << 24),
    EventNameCategoryKeys   = ((0x10) << 24),

    // Message events
    EventNameMessage        = ((0x20) << 24),

    // Custom event for applications to use (is this a good idea?)
    EventNameCustom         = ((0xf0) << 24),

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

typedef struct EventMessageProps {
    uint8_t *data;
    size_t length;
} EventMessageProps;

typedef union EventPayloadProps {
    EventRenderSceneProps renderEvent;
    EventKeysProps keyEvent;
    EventMessageProps messageEvent;
} EventPayloadProps;

typedef struct EventPayload {
    EventName event;
    int eventId;
    EventPayloadProps props;
} EventPayload;

typedef void (*EventCallback)(EventPayload event, void* arg);

int app_onEvent(EventName event, EventCallback cb, void* arg);

void app_offEvent(uint32_t eventId);

uint32_t app_keys();



#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __APP_H__ */
