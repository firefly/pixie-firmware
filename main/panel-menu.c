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

    switch(event.props.keyEvent.down) {
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

    ffx_scene_stopAnimations(app->nodeCursor, FfxSceneActionStopCurrent);
    ffx_scene_nodeAnimatePosition(app->scene, app->nodeCursor,
      (FfxPoint){ .x = 25, .y = 58 + (app->cursor * 40) }, 150,
      FfxCurveEaseOutQuad, NULL);
}

static int _init(FfxScene scene, FfxNode node, void *_app, void *arg) {
    MenuState *app = _app;
    app->scene = scene;

    const char* const phraseOption1   = "Device";
    const char* const phraseOption2   = "GIFs";
    const char* const phraseOption3   = "Le Space";

    FfxNode box = ffx_scene_createBox(scene, (FfxSize){ .width = 200, .height = 180 }, RGB_DARK75);
    ffx_scene_appendChild(node, box);
    printf("box p=%p\n", box);
    ffx_scene_nodeSetPosition(box, (FfxPoint){ .x = 20, .y = 30 });

    FfxNode text;

    text = ffx_scene_createTextStr(scene, phraseOption1);
    ffx_scene_appendChild(node, text);
    ffx_scene_nodeSetPosition(text, (FfxPoint){ .x = 70, .y = 85 });

    text = ffx_scene_createTextStr(scene, phraseOption2);
    ffx_scene_appendChild(node, text);
    ffx_scene_nodeSetPosition(text, (FfxPoint){ .x = 70, .y = 125 });

    text = ffx_scene_createTextStr(scene, phraseOption3);
    ffx_scene_appendChild(node, text);
    ffx_scene_nodeSetPosition(text, (FfxPoint){ .x = 70, .y = 165 });

    FfxNode cursor = ffx_scene_createImage(scene, image_arrow, sizeof(image_arrow));
    ffx_scene_appendChild(node, cursor);
    ffx_scene_nodeSetPosition(cursor, (FfxPoint){ .x = 25, .y = 58 });

    app->nodeCursor = cursor;

    panel_onEvent(EventNameKeysChanged | KeyNorth | KeySouth | KeyOk,
      keyChanged, app);

    return 0;
}

void pushPanelMenu(void *arg) {
    panel_push(_init, sizeof(MenuState), PanelStyleCoverUp, arg);
}
