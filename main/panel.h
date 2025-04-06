#ifndef __PANEL_H__
#define __PANEL_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdbool.h>
#include <stdint.h>

#include "firefly-cbor.h"
#include "firefly-scene.h"

#include "events.h"


typedef enum PanelStyle {
    PanelStyleInstant = 0,
    PanelStyleCoverUp,
//    PanelStyleSlideOver,
    PanelStyleSlideLeft, // SlideFromRight?
//    PanelStyleCoverFromRight,
//    PanelStyleSlideSlideOut,
} PanelStyle;


///////////////////////////////
// Panel Managment API

typedef int (*PanelInit)(FfxScene scene, FfxNode node, void* state, void* arg);

void panel_push(PanelInit init, size_t stateSize, PanelStyle style, void* arg);

// Deletes the current panel task, returning control to previous panel in stack
void panel_pop();


///////////////////////////////
// Pixel API

void panel_setPixel(uint32_t pixel, color_ffxt color);


///////////////////////////////
// Message API

bool panel_acceptMessage(uint32_t id, FfxCborCursor *params);
bool panel_sendErrorReply(uint32_t id, uint32_t code, char *message);
bool panel_sendReply(uint32_t id, FfxCborBuilder *result);

// @TODO: Remvoe this and automatically register messages
//        on message events
bool panel_isMessageEnabled();
void panel_enableMessage(bool enable);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PANEL_H__ */
