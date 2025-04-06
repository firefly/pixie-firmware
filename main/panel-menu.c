#include <stdio.h>

#include "firefly-scene.h"

#include "panel.h"
#include "./panel-attest.h"
#include "./panel-gifs.h"
#include "./panel-menu.h"
#include "./panel-space.h"

#include "images/image-arrow.h"


typedef struct MenuState {
    size_t cursor;
    FfxScene scene;
    FfxNode nodeCursor;
} MenuState;


static void keyChanged(EventPayload event, void *_app) {
    MenuState *app = _app;

    switch(event.props.keys.down) {
        case KeyOk:
            switch(app->cursor) {
                case 0:
                    pushPanelAttest(NULL);
                    break;
                case 1:
                    pushPanelGifs(NULL);
                    break;
                case 2:
                    pushPanelSpace(NULL);
                    break;
            }
            return;
        case KeyNorth:
            if (app->cursor == 0) { return; }
            app->cursor--;
            break;
        case KeySouth:
            if (app->cursor == 2) { return; }
            app->cursor++;
            break;
        default:
            return;
    }

    ffx_sceneNode_stopAnimations(app->nodeCursor, FfxSceneActionStopCurrent);
    ffx_sceneNode_animatePosition(app->nodeCursor,
      ffx_point(25, 58 + (app->cursor * 40)), 0, 150,
      FfxCurveEaseOutQuad, NULL, NULL);
}

static int _init(FfxScene scene, FfxNode node, void *_app, void *arg) {
    MenuState *app = _app;
    app->scene = scene;

    FfxNode box = ffx_scene_createBox(scene, ffx_size(200, 180));
    ffx_sceneBox_setColor(box, RGBA_DARKER75);
    ffx_sceneGroup_appendChild(node, box);
    ffx_sceneNode_setPosition(box, (FfxPoint){ .x = 20, .y = 30 });

    FfxNode text;

    text = ffx_scene_createLabel(scene, FfxFontLarge, "Device");
    ffx_sceneGroup_appendChild(node, text);
    ffx_sceneNode_setPosition(text, (FfxPoint){ .x = 70, .y = 63 });

    text = ffx_scene_createLabel(scene, FfxFontLarge, "GIFs");
    ffx_sceneGroup_appendChild(node, text);
    ffx_sceneNode_setPosition(text, (FfxPoint){ .x = 70, .y = 103 });

    text = ffx_scene_createLabel(scene, FfxFontLarge, "Le Space");
    ffx_sceneGroup_appendChild(node, text);
    ffx_sceneNode_setPosition(text, (FfxPoint){ .x = 70, .y = 143 });

    FfxNode cursor = ffx_scene_createImage(scene, image_arrow,
      sizeof(image_arrow));
    ffx_sceneGroup_appendChild(node, cursor);
    ffx_sceneNode_setPosition(cursor, (FfxPoint){ .x = 25, .y = 58 });

    app->nodeCursor = cursor;

    panel_onEvent(EventNameKeysChanged | KeyNorth | KeySouth | KeyOk,
      keyChanged, app);

    return 0;
}

void pushPanelMenu(void *arg) {
    panel_push(_init, sizeof(MenuState), PanelStyleCoverUp, arg);
}
