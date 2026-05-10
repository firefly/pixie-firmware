#include <stdio.h>

#include "firefly-hollows.h"
#include "firefly-scene.h"

#include "panels.h"


#include "feedback.h"
#include "image-data.h"


#define ITEM_COUNT      (13)
#define VISIBLE_ROWS    (3)
#define ROW_HEIGHT      (40)
#define ROW_FIRST_Y     (63)
#define ARROW_X         (25)
#define ARROW_OFFSET_Y  (-5)
#define LABEL_X         (70)


typedef struct State {
    int cursor;
    int scrollOffset;
    FfxScene scene;
    FfxNode arrow;
    FfxNode labels[ITEM_COUNT];
    FpsCounter fps;
} State;


static const char *menuItems[ITEM_COUNT] = {
    "Wallet",
    "GIFs",
    "Le Space",
    "Cyber Pulse",
    "Life Grid",
    "Byte Stream",
    "Sys Stats",
    "LED Mode",
    "Raycast",
    "Asteroids",
    "Breakout",
    "Dungeon",
    "Snake",
};


static void launchItem(int idx) {
    switch (idx) {
        case 0:  pushPanelConnect(); break;
        case 1:  pushPanelGifs();    break;
        case 2:  pushPanelSpace();   break;
        case 3:  pushPanelCyber();   break;
        case 4:  pushPanelLife();    break;
        case 5:  pushPanelBytes();   break;
        case 6:  pushPanelStats();   break;
        case 7:  pushPanelLeds();    break;
        case 8:  pushPanelRaycast(); break;
        case 9:  pushPanelRoids();   break;
        case 10: pushPanelBrick();   break;
        case 11: pushPanelCrawl();   break;
        case 12: pushPanelSnake();   break;
    }
}

static int rowYForSlot(int slot) {
    return ROW_FIRST_Y + slot * ROW_HEIGHT;
}

static void layoutItems(State *app) {
    for (int i = 0; i < ITEM_COUNT; i++) {
        int slot = i - app->scrollOffset;
        if (slot < 0 || slot >= VISIBLE_ROWS) {
            ffx_sceneNode_setHidden(app->labels[i], true);
            continue;
        }
        ffx_sceneNode_setHidden(app->labels[i], false);
        ffx_sceneNode_setPosition(app->labels[i],
          ffx_point(LABEL_X, rowYForSlot(slot)));
    }
}

static void scrollToCursor(State *app) {
    if (app->cursor < app->scrollOffset) {
        app->scrollOffset = app->cursor;
    } else if (app->cursor >= app->scrollOffset + VISIBLE_ROWS) {
        app->scrollOffset = app->cursor - (VISIBLE_ROWS - 1);
    }
}

static void moveArrow(State *app, bool animated) {
    int slot = app->cursor - app->scrollOffset;
    int y = rowYForSlot(slot) + ARROW_OFFSET_Y;
    ffx_sceneNode_stopAnimations(app->arrow, FfxSceneActionStopCurrent);
    if (animated) {
        ffx_sceneNode_animatePosition(app->arrow, ffx_point(ARROW_X, y),
          0, 150, FfxCurveEaseOutQuad, NULL, NULL);
    } else {
        ffx_sceneNode_setPosition(app->arrow, ffx_point(ARROW_X, y));
    }
}

static void onRender(FfxEvent event, FfxEventProps props, void *_app) {
    State *app = _app;
    feedback_tickFps(&app->fps);
}

static void onKeys(FfxEvent event, FfxEventProps props, void *_app) {
    State *app = _app;
    feedback_onKey(props.keys.down);

    switch (props.keys.down) {
        case FfxKeyOk:
            launchItem(app->cursor);
            return;
        case FfxKeyNorth:
            if (app->cursor == 0) { return; }
            app->cursor--;
            break;
        case FfxKeySouth:
            if (app->cursor == ITEM_COUNT - 1) { return; }
            app->cursor++;
            break;
        default:
            return;
    }

    scrollToCursor(app);
    layoutItems(app);
    moveArrow(app, true);
}

static int initFunc(FfxScene scene, FfxNode node, void *_app, void *arg) {
    State *app = _app;
    app->scene = scene;
    app->cursor = 0;
    app->scrollOffset = 0;

    FfxNode box = ffx_scene_createBox(scene, ffx_size(200, 180));
    ffx_sceneBox_setColor(box, RGBA_DARKER75);
    ffx_sceneGroup_appendChild(node, box);
    ffx_sceneNode_setPosition(box, ffx_point(20, 30));

    FfxNode topRule = ffx_scene_createBox(scene, ffx_size(200, 1));
    ffx_sceneBox_setColor(topRule, ffx_color_rgb(0, 255, 65));
    ffx_sceneGroup_appendChild(node, topRule);
    ffx_sceneNode_setPosition(topRule, ffx_point(20, 30));

    FfxNode bottomRule = ffx_scene_createBox(scene, ffx_size(200, 1));
    ffx_sceneBox_setColor(bottomRule, ffx_color_rgb(0, 255, 65));
    ffx_sceneGroup_appendChild(node, bottomRule);
    ffx_sceneNode_setPosition(bottomRule, ffx_point(20, 209));

    for (int i = 0; i < ITEM_COUNT; i++) {
        FfxNode label = ffx_scene_createLabel(scene, FfxFontLarge,
          menuItems[i]);
        ffx_sceneGroup_appendChild(node, label);
        ffx_sceneNode_setHidden(label, true);
        app->labels[i] = label;
    }

    FfxNode cursor = ffx_scene_createImage(scene, image_arrow,
      image_arrow_len);
    ffx_sceneGroup_appendChild(node, cursor);
    app->arrow = cursor;

    layoutItems(app);
    moveArrow(app, false);

    feedback_addButtonLegend(node, "UP", "DOWN", "OK", "ESC");
    feedback_addFpsCounter(&app->fps, node);

    ffx_onEvent(FfxEventKeys, onKeys, app);
    ffx_onEvent(FfxEventRenderScene, onRender, app);

    return 0;
}

int pushPanelMenu() {
    return ffx_pushPanel(initFunc, sizeof(State), NULL);
}
