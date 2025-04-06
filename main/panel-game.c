/**
 *
 * Useful sources:
 *  - https://github.com/NesHacker/PlatformerMovement/blob/main/src/state/Player.s
 *  - https://simplistic6502.github.io/smb1_tll/smbpedia_movement.html
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdbool.h>
#include <stdio.h>

#include "firefly-fixed.h"
#include "firefly-scene.h"

#include "utils.h"

#include "panel.h"

#include "panel-game.h"

typedef enum Motion {
    MotionStill = 0,
    MotionWalk,
    MotionPivot,
    MotionAirborne
} Motion;

typedef struct State {
    bool running;

    FfxScene scene;
    FfxNode panel;

    FfxNode hero;
    fixed_ffxt posx, posy;
    fixed_ffxt vx, vy;
    fixed_ffxt tvx;
    Motion motion;
    int runningTimer;

    uint32_t ticks;

    Keys keysDown;
    Keys keysPressed;
} State;

#define FM_STEP    (FM_1_16)
#define WALK_MAXV  (FM_1 + FM_1_2)
#define RUN_MAXV   ((2 * FM_1) + FM_1_2)

#define JUMP_INIT  ((3 * FM_1) + FM_1_2)
#define FALL_SLOW  (-FM_1_16)
#define FALL_FAST  (-5 * FM_1_16)
#define FALL_MAXV  (-4 * FM_1)

static const Keys KEY_LEFT = KeySouth;
static const Keys KEY_RIGHT = KeyNorth;
static const Keys KEY_RUN = KeyOk;
static const Keys KEY_JUMP = KeyCancel;

static void updateHero(State *state) {

    // If KEY_RUN is held (or was held in the last 10 frames), run
    fixed_ffxt target = WALK_MAXV;
    if (state->keysDown & KEY_RUN) {
        target = RUN_MAXV;
        state->runningTimer = 10;
    } else if (state->runningTimer) {
        target = RUN_MAXV;
        state->runningTimer--;
    }

    // Set the target horizontal velocity depending on KEY_LEFT or KEY_RIGHT
    if (state->keysDown & KEY_LEFT) {
        state->tvx = -target;
    } else if (state->keysDown & KEY_RIGHT) {
        state->tvx = target;
    } else {
        state->tvx = 0;
    }

    if (state->motion == MotionAirborne) {
        // Jumping
        state->vy += (state->keysDown & KEY_JUMP) ? FALL_SLOW: FALL_FAST;
        if (state->vy < FALL_MAXV) { state->vy = FALL_MAXV; }
    } else if (state->keysPressed & KEY_JUMP) {
        // Start jump
        state->vy = JUMP_INIT;
        state->motion = MotionAirborne;
    } else {
        if (state->keysDown & KEY_LEFT && state->vx > 0) {
            state->motion = MotionPivot;
        } else if (state->keysDown & KEY_RIGHT && state->vx < 0) {
            state->motion = MotionPivot;
        } else {
            state->motion = MotionWalk;
        }
    }

    // Update the horizontal velocity towards the target velocity
    if (state->vx < state->tvx) {
        state->vx += FM_STEP;
        if (state->vx > state->tvx) { state->vx = state->tvx; }
    } else if (state->vx > state->tvx) {
        state->vx -= FM_STEP;
        if (state->vx < state->tvx) { state->vx = state->tvx; }
    }

    // Update the positions
    state->posx += state->vx;
    state->posy += state->vy;

    // @TODO: real collisions
    if (state->posx > tofx(200)) { state->posx = tofx(200); }
    if (state->posx < tofx(40)) { state->posx = tofx(40); }
    if (state->posy < tofx(100)) {
        state->posy = tofx(100);
        state->motion = MotionStill;
    }

    ffx_sceneNode_setPosition(state->hero, ffx_point(240 - (state->posy >> 16),
      240 - (state->posx >> 16)));
}

static void focus(EventPayload event, void *_state) {
    State *state = _state;
    state->running = true;
}

static void render(EventPayload event, void *_state) {
    State *state = _state;
    state->ticks = event.props.render.ticks;

    static uint32_t ticks = 0;
    static int frames = 0;

    frames++;

    uint32_t now = state->ticks;
    if (now - ticks > 1000) {
        uint32_t fps = 1000 * frames / (now - ticks);
        printf("FPS: %ld\n", fps);

        frames = 0;
        ticks = now;
    }

    updateHero(state);
    state->keysPressed = 0;
}

static void keyDown(EventPayload event, void *_state) {
    State *state = _state;

    Keys down = event.props.keys.down;
    Keys changed = event.props.keys.changed;

    state->keysPressed = down & changed;;
    state->keysDown = down;
}

static void keyUp(EventPayload event, void *_state) {
    State *state = _state;
    state->keysDown = event.props.keys.down;
}

static int _init(FfxScene scene, FfxNode panel, void* panelState, void* arg) {
    State *state = panelState;
    state->scene = scene;
    state->panel = panel;

    FfxNode hero = ffx_scene_createBox(scene, ffx_size(32, 16));
    state->hero = hero;
    ffx_sceneGroup_appendChild(panel, hero);
    ffx_sceneNode_setPosition(hero, ffx_point(100, 100));
    state->posx = tofx(100);
    state->posy = tofx(100);

    panel_onEvent(EventNameKeysDown | KeyNorth | KeySouth | KeyOk | KeyCancel,
      keyDown, state);

    panel_onEvent(EventNameKeysUp | KeyNorth | KeySouth | KeyOk | KeyCancel,
      keyUp, state);

    panel_onEvent(EventNameRenderScene, render, state);

    panel_onEvent(EventNamePanelFocus, focus, state);

    return 0;
}

void pushPanelGame(void* arg) {
    panel_push(_init, sizeof(State), PanelStyleSlideLeft, arg);
}
