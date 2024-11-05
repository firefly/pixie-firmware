#ifndef __PANEL_MENU_H__
#define __PANEL_MENU_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "firefly-scene.h"

#include "app.h"

typedef struct PanelMenuState {
    size_t cursor;
    FfxScene scene;
    FfxNode nodeCursor;
} PanelMenuState;

int panelMenuInit(FfxScene scene, FfxNode node, void* panelState, void* arg);



#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PANEL_MENU_H__ */
