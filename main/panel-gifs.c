#include <stdio.h>

#include "firefly-scene.h"

#include "utils.h"

#include "panel.h"
#include "panel-gifs.h"

#include "images/video-fox.h"
#include "images/video-rroll.h"
#include "images/video-shiba.h"
#include "images/video-nyan.h"

typedef struct State {
    int video;
    int menuHidden;
    FfxScene scene;
    FfxNode gif;
    FfxNode menu;
} State;

static void animateMenu(FfxNode menu, FfxNodeAnimation *animation, void *arg) {
    int32_t x = *(int*)arg;
    animation->duration = 1000;
    animation->curve = FfxCurveEaseOutQuad;

    ffx_sceneNode_setPosition(menu, ffx_point(x, 0));
}

static void keyChanged(EventPayload event, void *_state) {
    State *state = _state;

    switch(event.props.keys.down) {
        case KeyCancel:
            state->video = 0;
            if (state->menuHidden) {
                ffx_sceneNode_stopAnimations(state->menu,
                   FfxSceneActionStopCurrent);
                int32_t x = 0;
                ffx_sceneNode_animate(state->menu, animateMenu, &x);
                state->menuHidden = false;
            } else {
                panel_pop();
            }
            break;
        case KeyOk:
            state->video = 1;
            if (!state->menuHidden) {
                ffx_sceneNode_stopAnimations(state->menu,
                  FfxSceneActionStopCurrent);
                int32_t x = 240;
                ffx_sceneNode_animate(state->menu, animateMenu, &x);
                state->menuHidden = true;
            }
            break;
        case KeyNorth:
            state->video = 2;
            if (!state->menuHidden) {
                ffx_sceneNode_stopAnimations(state->menu,
                  FfxSceneActionStopCurrent);
                int32_t x = 240;
                ffx_sceneNode_animate(state->menu, animateMenu, &x);
                state->menuHidden = true;
            }
            break;
        case KeySouth:
            state->video = 3;
            if (!state->menuHidden) {
                ffx_sceneNode_stopAnimations(state->menu,
                  FfxSceneActionStopCurrent);
                int32_t x = 240;
                ffx_sceneNode_animate(state->menu, animateMenu, &x);
                state->menuHidden = true;
            }
            break;
    }
}

static void setFoxFrame(FfxNode node) {
    const size_t frameCount = 47;

    // 20 fps
    uint32_t frame = (ticks() / 50) % frameCount;

    const uint16_t *frames[] = {
        image_fox_0, image_fox_1, image_fox_2, image_fox_3, image_fox_4,
        image_fox_5, image_fox_6, image_fox_7, image_fox_8, image_fox_9,
        image_fox_10, image_fox_11, image_fox_12, image_fox_13,
        image_fox_14, image_fox_15, image_fox_16, image_fox_17,
        image_fox_18, image_fox_19, image_fox_20, image_fox_21,
        image_fox_22, image_fox_23, image_fox_24, image_fox_25,
        image_fox_26, image_fox_27, image_fox_28, image_fox_29,
        image_fox_30, image_fox_31, image_fox_32, image_fox_33,
        image_fox_34, image_fox_35, image_fox_36, image_fox_37,
        image_fox_38, image_fox_39, image_fox_40, image_fox_41,
        image_fox_42, image_fox_43, image_fox_44, image_fox_45,
        image_fox_46
    };

    const uint16_t *imgData = frames[frame];
    ffx_sceneImage_setData(node, imgData, sizeof(image_fox_0));
}

static void setNyanFrame(FfxNode node) {
    const size_t frameCount = 8;

    // 10 fps
    uint32_t frame = (ticks() / 100) % frameCount;

    const uint16_t *frames[] = {
        image_nyan_0, image_nyan_1, image_nyan_2, image_nyan_3,
        image_nyan_4, image_nyan_5, image_nyan_6, image_nyan_7
    };

    const uint16_t *imgData = frames[frame];
    ffx_sceneImage_setData(node, imgData, sizeof(image_nyan_0));
}

static void setShibaFrame(FfxNode node) {

    const size_t frameCount = 13;

    // 10 fps
    uint32_t frame = (ticks() / 100) % frameCount;

    const uint16_t *frames[] = {
        image_shiba_0, image_shiba_1, image_shiba_2, image_shiba_3,
        image_shiba_4, image_shiba_5, image_shiba_6, image_shiba_7,
        image_shiba_8, image_shiba_9, image_shiba_10, image_shiba_11,
        image_shiba_12
    };

    const uint16_t *imgData = frames[frame];
    ffx_sceneImage_setData(node, imgData, sizeof(image_shiba_0));
}

static void setRrollFrame(FfxNode node) {
    const size_t frameCount = 27;

    // 12.5 fps
    uint32_t frame = (ticks() / 80) % frameCount;

    const uint16_t *frames[] = {
        image_rroll_0, image_rroll_1, image_rroll_2, image_rroll_3,
        image_rroll_4, image_rroll_5, image_rroll_6, image_rroll_7,
        image_rroll_8, image_rroll_9, image_rroll_10, image_rroll_11,
        image_rroll_12, image_rroll_13, image_rroll_14, image_rroll_15,
        image_rroll_16, image_rroll_17, image_rroll_18, image_rroll_19,
        image_rroll_20, image_rroll_21, image_rroll_22, image_rroll_23,
        image_rroll_24, image_rroll_25, image_rroll_26
    };

    const uint16_t *imgData = frames[frame];
    ffx_sceneImage_setData(node, imgData, sizeof(image_rroll_0));
}


static void render(EventPayload event, void *_state) {
    State *state = _state;

    switch (state->video) {
        case 0:
            setShibaFrame(state->gif);
            break;
        case 1:
            setNyanFrame(state->gif);
            break;
        case 2:
            setFoxFrame(state->gif);
            break;
        case 3:
            setRrollFrame(state->gif);
            break;
    }
}

static int _init(FfxScene scene, FfxNode node, void *_state, void *arg) {
    State *state = _state;
    state->scene = scene;

    FfxNode gif = ffx_scene_createImage(scene, image_nyan_0,
      sizeof(image_nyan_0));
    state->gif = gif;
    ffx_sceneGroup_appendChild(node, gif);
    setShibaFrame(gif);

    FfxNode menu = ffx_scene_createGroup(scene);
    state->menu = menu;
    ffx_sceneGroup_appendChild(node, menu);
    ffx_sceneNode_setPosition(menu, ffx_point(0, 0));

    FfxNode text;

    text = ffx_scene_createLabel(scene, FfxFontLarge, "< back");
    ffx_sceneGroup_appendChild(menu, text);
    ffx_sceneNode_setPosition(text, (FfxPoint){ .x = 230, .y = 14 });
    ffx_sceneLabel_setAlign(text, FfxTextAlignRight | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(text, ffx_color_rgb(0, 0, 0));

    text = ffx_scene_createLabel(scene, FfxFontLarge, "  Nyan");
    ffx_sceneGroup_appendChild(menu, text);
    ffx_sceneNode_setPosition(text, (FfxPoint){ .x = 230, .y = 84 });
    ffx_sceneLabel_setAlign(text, FfxTextAlignRight | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(text, ffx_color_rgb(0, 0, 0));

    text = ffx_scene_createLabel(scene, FfxFontLarge, "   Fox");
    ffx_sceneGroup_appendChild(menu, text);
    ffx_sceneNode_setPosition(text, (FfxPoint){ .x = 230, .y = 156 });
    ffx_sceneLabel_setAlign(text, FfxTextAlignRight | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(text, ffx_color_rgb(0, 0, 0));

    text = ffx_scene_createLabel(scene, FfxFontLarge, "R-Roll");
    ffx_sceneGroup_appendChild(menu, text);
    ffx_sceneNode_setPosition(text, (FfxPoint){ .x = 230, .y = 226 });
    ffx_sceneLabel_setAlign(text, FfxTextAlignRight | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(text, ffx_color_rgb(0, 0, 0));

    panel_onEvent(EventNameKeysChanged | KeyNorth | KeySouth | KeyOk | KeyCancel,
      keyChanged, state);

    panel_onEvent(EventNameRenderScene, render, state);

    return 0;
}

void pushPanelGifs(void *arg) {
    panel_push(_init, sizeof(State), PanelStyleSlideLeft, arg);
}
