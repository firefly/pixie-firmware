#ifndef __PANEL_H__
#define __PANEL_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdbool.h>
#include <stdint.h>

#include "firefly-scene.h"

typedef enum PanelStyle {
    PanelStyleInstant = 0,
    PanelStyleCoverUp,
//    PanelStyleSlideOver,
    PanelStyleSlideLeft,
//    PanelStyleSlideSlideOut,
} PanelStyle;

typedef int (*PanelInit)(FfxScene scene, FfxNode node, void* state, void* arg);

void panel_push(PanelInit init, size_t stateSize, PanelStyle style, void* arg);

// Deletes the current panel task, returning control to previous panel in stack
void panel_pop();


// Disabling requests clears any pending message
//void panel_enableRequest(bool enabled);

// Blocks the current task until the BLE task accepts and sends
// the response
//bool panel_sendResponse(uint8_t *payload, size_t length);

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
    EventNameKeysDown       = ((0x11) << 24),
    EventNameKeysUp         = ((0x12) << 24),
    EventNameKeysPress      = ((0x13) << 24),
    EventNameKeysChanged    = ((0x14) << 24),
    EventNameCategoryKeys   = ((0x10) << 24),

    // Panel events
    EventNamePanelFocus     = ((0x21) << 24),
    EventNamePanelBlur      = ((0x22) << 24),
    EventNameCategoryPanel  = ((0x20) << 24),

    // Message events
    EventNameMessage        = ((0x51) << 24),

    // Custom event for panels to use (is this a good idea?)
    EventNameCustom         = ((0xf1) << 24),

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
    int panelId;
} EventPanelProps;

typedef struct EventMessageProps {
    uint8_t *data;
    size_t length;
} EventMessageProps;

typedef union EventPayloadProps {
    EventRenderSceneProps renderEvent;
    EventKeysProps keyEvent;
    EventPanelProps panelEvent;
    EventMessageProps messageEvent;
} EventPayloadProps;

typedef struct EventPayload {
    EventName event;
    int eventId;
    EventPayloadProps props;
} EventPayload;

typedef void (*EventCallback)(EventPayload event, void* arg);

int panel_onEvent(EventName event, EventCallback cb, void* arg);

void panel_offEvent(int eventId);

uint32_t panel_keys();

// @TODO: Remvoe this and automatically register messages
//        on message events
bool panel_isMessageEnabled();
void panel_enableMessage(bool enable);
int panel_sendReply(uint8_t *data, size_t length);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PANEL_H__ */
