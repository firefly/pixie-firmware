// Wolfenstein 3D-style raycaster, following Lode Vandevenne's tutorial
// (public domain): https://lodev.org/cgtutor/raycasting.html

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "firefly-hollows.h"
#include "firefly-scene.h"

#include "feedback.h"
#include "panels.h"
#include "utils.h"


#define STRIP_COUNT     (60)
#define STRIP_WIDTH     (4)
#define SCREEN_W        (240)
#define SCREEN_H        (240)
#define HUD_H           (28)
#define VIEW_H          (SCREEN_H - HUD_H)
#define MAP_W           (16)
#define MAP_H           (16)
#define FOV             (1.0472f)
#define MOVE_SPEED      (0.045f)
#define ROT_SPEED       (0.04f)
#define DDA_LIMIT       (32)


typedef struct RayState {
    FfxScene scene;
    FfxNode strips[STRIP_COUNT];
    FfxNode sky;
    FfxNode floor;
    FfxNode hudBg;
    FfxNode hudLabel;

    float px, py;
    float pa;

    FfxKeys keys;
} RayState;


static const char worldMap[MAP_H][MAP_W + 1] = {
    "################",
    "#..............#",
    "#.####....####.#",
    "#.#..........#.#",
    "#.#..######..#.#",
    "#.#..#....#..#.#",
    "#....#....#....#",
    "#....#....#....#",
    "#....#....#....#",
    "#.#..#....#..#.#",
    "#.#..######..#.#",
    "#.#..........#.#",
    "#.####....####.#",
    "#..............#",
    "#..............#",
    "################"
};


static bool blocked(float x, float y) {
    int mx = (int)x;
    int my = (int)y;
    if (mx < 0 || mx >= MAP_W || my < 0 || my >= MAP_H) { return true; }
    return worldMap[my][mx] == '#';
}

static void onKeys(FfxEvent event, FfxEventProps props, void *_state) {
    RayState *state = _state;
    state->keys = props.keys.down;
    feedback_onKey(props.keys.down);
    if (props.keys.down & FfxKeyCancel) {
        ffx_popPanel(0);
    }
}

static void onRender(FfxEvent event, FfxEventProps props, void *_state) {
    RayState *state = _state;
    FfxKeys keys = state->keys;

    if (keys & FfxKeyNorth) {
        float dx = cosf(state->pa) * MOVE_SPEED;
        float dy = sinf(state->pa) * MOVE_SPEED;
        if (!blocked(state->px + dx, state->py)) { state->px += dx; }
        if (!blocked(state->px, state->py + dy)) { state->py += dy; }
    }
    if (keys & FfxKeySouth) { state->pa -= ROT_SPEED; }
    if (keys & FfxKeyOk)    { state->pa += ROT_SPEED; }

    for (int i = 0; i < STRIP_COUNT; i++) {
        float cameraX = (2.0f * i + 1.0f) / STRIP_COUNT - 1.0f;
        float rayAngle = state->pa + cameraX * (FOV / 2.0f);
        float rdx = cosf(rayAngle);
        float rdy = sinf(rayAngle);

        int mapX = (int)state->px;
        int mapY = (int)state->py;

        float ddx = (rdx == 0.0f) ? 1e30f : fabsf(1.0f / rdx);
        float ddy = (rdy == 0.0f) ? 1e30f : fabsf(1.0f / rdy);

        int stepX, stepY;
        float sideX, sideY;
        if (rdx < 0) { stepX = -1; sideX = (state->px - mapX) * ddx; }
        else         { stepX =  1; sideX = (mapX + 1.0f - state->px) * ddx; }
        if (rdy < 0) { stepY = -1; sideY = (state->py - mapY) * ddy; }
        else         { stepY =  1; sideY = (mapY + 1.0f - state->py) * ddy; }

        bool hit = false;
        int side = 0;
        for (int n = 0; !hit && n < DDA_LIMIT; n++) {
            if (sideX < sideY) {
                sideX += ddx; mapX += stepX; side = 0;
            } else {
                sideY += ddy; mapY += stepY; side = 1;
            }
            if (mapX < 0 || mapX >= MAP_W || mapY < 0 || mapY >= MAP_H) { break; }
            if (worldMap[mapY][mapX] == '#') { hit = true; }
        }

        if (!hit) {
            ffx_sceneNode_setHidden(state->strips[i], true);
            continue;
        }
        float perp = (side == 0) ? (sideX - ddx) : (sideY - ddy);
        if (perp < 0.1f) { perp = 0.1f; }

        int wallH = (int)(VIEW_H / perp);
        if (wallH > VIEW_H) { wallH = VIEW_H; }
        if (wallH < 1)      { wallH = 1; }
        int wallY = (VIEW_H - wallH) / 2;

        ffx_sceneNode_setHidden(state->strips[i], false);
        ffx_sceneBox_setSize(state->strips[i], ffx_size(STRIP_WIDTH, wallH));
        ffx_sceneNode_setPosition(state->strips[i],
          ffx_point(i * STRIP_WIDTH, wallY));

        int b = (int)(220.0f / (1.0f + perp * 0.4f));
        if (b > 220) { b = 220; }
        if (b < 30)  { b = 30; }
        if (side == 1) { b = b * 2 / 3; }
        ffx_sceneBox_setColor(state->strips[i],
          ffx_color_rgb(b, b / 5, b / 7));
    }
}

static int initFunc(FfxScene scene, FfxNode panel, void *_state, void *arg) {
    RayState *state = _state;
    state->scene = scene;
    state->px = 8.0f;
    state->py = 7.5f;
    state->pa = 0.0f;
    state->keys = 0;

    state->sky = ffx_scene_createBox(scene, ffx_size(SCREEN_W, VIEW_H / 2));
    ffx_sceneBox_setColor(state->sky, ffx_color_rgb(15, 8, 30));
    ffx_sceneGroup_appendChild(panel, state->sky);
    ffx_sceneNode_setPosition(state->sky, ffx_point(0, 0));

    state->floor = ffx_scene_createBox(scene, ffx_size(SCREEN_W, VIEW_H / 2));
    ffx_sceneBox_setColor(state->floor, ffx_color_rgb(40, 20, 12));
    ffx_sceneGroup_appendChild(panel, state->floor);
    ffx_sceneNode_setPosition(state->floor, ffx_point(0, VIEW_H / 2));

    for (int i = 0; i < STRIP_COUNT; i++) {
        FfxNode strip = ffx_scene_createBox(scene, ffx_size(STRIP_WIDTH, 1));
        ffx_sceneBox_setColor(strip, ffx_color_rgb(180, 40, 30));
        ffx_sceneGroup_appendChild(panel, strip);
        ffx_sceneNode_setPosition(strip, ffx_point(i * STRIP_WIDTH, VIEW_H / 2));
        state->strips[i] = strip;
    }

    state->hudBg = ffx_scene_createBox(scene, ffx_size(SCREEN_W, HUD_H));
    ffx_sceneBox_setColor(state->hudBg, ffx_color_rgb(0, 0, 0));
    ffx_sceneGroup_appendChild(panel, state->hudBg);
    ffx_sceneNode_setPosition(state->hudBg, ffx_point(0, VIEW_H));

    state->hudLabel = NULL;
    feedback_addButtonLegend(panel, "FWD", "LEFT", "RIGHT", "EXIT");

    ffx_onEvent(FfxEventKeys, onKeys, state);
    ffx_onEvent(FfxEventRenderScene, onRender, state);

    return 0;
}

int pushPanelRaycast() {
    return ffx_pushPanel(initFunc, sizeof(RayState), NULL);
}
