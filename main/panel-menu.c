#include <stdio.h>

#include "images/image-arrow.h"

#include "./panel-menu.h"

//static void render(EventPayload event, void *_app) {
    //PanelMenuState *app = arg;
    //FfxPoint *pos = ffx_scene_nodePosition(app->box);
    //pos->x = (pos->x + 1) % 250;
//}

static void keyChanged(EventPayload event, void *_app) {
    PanelMenuState *app = _app;

    switch(event.props.keyEvent.down) {
        case KeyOk:
            printf("Launch: %d\n", app->cursor);
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

int panelMenuInit(FfxScene scene, FfxNode node, void *_app, void *arg) {
    printf("Init menu\n");

    PanelMenuState *app = _app;
    app->scene = scene;

    const char* const phrasePixies = "Show BG";
    const char* const phraseDevice = "Device";
    const char* const phraseSpace  = "Le Space";

    FfxNode box = ffx_scene_createBox(scene, (FfxSize){ .width = 200, .height = 180 }, RGB_DARK75);
    ffx_scene_appendChild(node, box);
    printf("box p=%p\n", box);
    ffx_scene_nodeSetPosition(box, (FfxPoint){ .x = 20, .y = 30 });

    FfxNode text;

    text = ffx_scene_createTextStr(scene, phrasePixies);
    printf("text p=%p\n", text);
    ffx_scene_appendChild(node, text);
    ffx_scene_nodeSetPosition(text, (FfxPoint){ .x = 70, .y = 85 });

    text = ffx_scene_createTextStr(scene, phraseDevice);
    ffx_scene_appendChild(node, text);
    ffx_scene_nodeSetPosition(text, (FfxPoint){ .x = 70, .y = 125 });

    text = ffx_scene_createTextStr(scene, phraseSpace);
    ffx_scene_appendChild(node, text);
    ffx_scene_nodeSetPosition(text, (FfxPoint){ .x = 70, .y = 165 });

    FfxNode cursor = ffx_scene_createImage(scene, image_arrow, sizeof(image_arrow));
    ffx_scene_appendChild(node, cursor);
    ffx_scene_nodeSetPosition(cursor, (FfxPoint){ .x = 25, .y = 58 });

    app->nodeCursor = cursor;

//    app_onEvent(EventNameRenderScene, render, app);
    app_onEvent(EventNameKeysChanged | KeyNorth | KeySouth | KeyOk,
      keyChanged, app);

    return 0;
}
