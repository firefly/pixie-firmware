// Asteroids-flavored side-scroll dodger. Ship on the left, drifting
// rocks come from the right; tap OK to fire bullets, hold N/S to move.

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "firefly-hollows.h"
#include "firefly-scene.h"

#include "panels.h"
#include "utils.h"


#define MAX_ROCKS       (10)
#define MAX_BULLETS     (6)
#define SHIP_X          (16)
#define SHIP_W          (16)
#define SHIP_H          (10)
#define BULLET_W        (8)
#define BULLET_H        (3)
#define BULLET_SPEED    (5)
#define ROCK_SIZE       (16)


typedef struct Rock {
    int16_t x, y;
    int8_t  vx, vy;
    uint8_t alive;
} Rock;

typedef struct RoidsState {
    FfxScene scene;
    FfxNode bg;
    FfxNode ship;
    FfxNode rocks[MAX_ROCKS];
    FfxNode bullets[MAX_BULLETS];
    FfxNode scoreLabel;
    FfxNode hint;
    FfxNode gameOverLabel;

    int16_t shipY;
    Rock rocks_state[MAX_ROCKS];
    int16_t bulletX[MAX_BULLETS];
    int16_t bulletY[MAX_BULLETS];
    bool bulletAlive[MAX_BULLETS];

    FfxKeys keys;
    uint32_t score;
    uint32_t spawnAt;
    uint32_t fireCooldown;
    uint32_t rng;
    bool gameOver;
} RoidsState;


static uint32_t rngNext(uint32_t *s) {
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x ? x : 0xA5A5C0DE;
    return *s;
}

static void spawnRock(RoidsState *state) {
    for (int i = 0; i < MAX_ROCKS; i++) {
        if (state->rocks_state[i].alive) { continue; }
        Rock *r = &state->rocks_state[i];
        r->x = 240 + ROCK_SIZE;
        r->y = (int16_t)(rngNext(&state->rng) % (240 - ROCK_SIZE));
        r->vx = -2 - (int8_t)(rngNext(&state->rng) % 3);
        r->vy = (int8_t)((rngNext(&state->rng) % 3) - 1);
        r->alive = 1;
        ffx_sceneNode_setHidden(state->rocks[i], false);
        ffx_sceneNode_setPosition(state->rocks[i], ffx_point(r->x, r->y));
        return;
    }
}

static void fireBullet(RoidsState *state) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (state->bulletAlive[i]) { continue; }
        state->bulletAlive[i] = true;
        state->bulletX[i] = SHIP_X + SHIP_W;
        state->bulletY[i] = state->shipY + SHIP_H / 2;
        ffx_sceneNode_setHidden(state->bullets[i], false);
        ffx_sceneNode_setPosition(state->bullets[i],
          ffx_point(state->bulletX[i], state->bulletY[i]));
        return;
    }
}

static void resetGame(RoidsState *state) {
    state->shipY = 120 - SHIP_H / 2;
    state->score = 0;
    state->gameOver = false;
    state->spawnAt = ticks() + 500;
    state->fireCooldown = 0;
    for (int i = 0; i < MAX_ROCKS; i++) {
        state->rocks_state[i].alive = 0;
        ffx_sceneNode_setHidden(state->rocks[i], true);
    }
    for (int i = 0; i < MAX_BULLETS; i++) {
        state->bulletAlive[i] = false;
        ffx_sceneNode_setHidden(state->bullets[i], true);
    }
    ffx_sceneNode_setHidden(state->gameOverLabel, true);
    ffx_sceneNode_setPosition(state->ship, ffx_point(SHIP_X, state->shipY));
}

static void onKeys(FfxEvent event, FfxEventProps props, void *_state) {
    RoidsState *state = _state;
    state->keys = props.keys.down;
    if (props.keys.down & FfxKeyCancel) {
        ffx_popPanel(0);
        return;
    }
    if (state->gameOver && (props.keys.down & FfxKeyOk)) {
        resetGame(state);
    }
}

static void onRender(FfxEvent event, FfxEventProps props, void *_state) {
    RoidsState *state = _state;
    if (state->gameOver) { return; }

    uint32_t t = ticks();
    state->score++;

    if (state->keys & FfxKeyNorth) { state->shipY -= 3; }
    if (state->keys & FfxKeySouth) { state->shipY += 3; }
    if (state->shipY < 0)               { state->shipY = 0; }
    if (state->shipY > 240 - SHIP_H)    { state->shipY = 240 - SHIP_H; }
    ffx_sceneNode_setPosition(state->ship, ffx_point(SHIP_X, state->shipY));

    if ((state->keys & FfxKeyOk) && t > state->fireCooldown) {
        fireBullet(state);
        state->fireCooldown = t + 180;
    }

    if (t > state->spawnAt) {
        spawnRock(state);
        uint32_t gap = 600 - (state->score / 4);
        if (gap < 200) { gap = 200; }
        state->spawnAt = t + gap;
    }

    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!state->bulletAlive[i]) { continue; }
        state->bulletX[i] += BULLET_SPEED;
        if (state->bulletX[i] > 240) {
            state->bulletAlive[i] = false;
            ffx_sceneNode_setHidden(state->bullets[i], true);
            continue;
        }
        ffx_sceneNode_setPosition(state->bullets[i],
          ffx_point(state->bulletX[i], state->bulletY[i]));
    }

    int16_t shipL = SHIP_X;
    int16_t shipR = SHIP_X + SHIP_W;
    int16_t shipT = state->shipY;
    int16_t shipB = state->shipY + SHIP_H;

    for (int i = 0; i < MAX_ROCKS; i++) {
        Rock *r = &state->rocks_state[i];
        if (!r->alive) { continue; }
        r->x += r->vx;
        r->y += r->vy;
        if (r->y < 0)                  { r->y = 0;             r->vy = -r->vy; }
        if (r->y > 240 - ROCK_SIZE)    { r->y = 240 - ROCK_SIZE; r->vy = -r->vy; }
        if (r->x < -ROCK_SIZE) {
            r->alive = 0;
            ffx_sceneNode_setHidden(state->rocks[i], true);
            continue;
        }

        for (int j = 0; j < MAX_BULLETS; j++) {
            if (!state->bulletAlive[j]) { continue; }
            int16_t bx = state->bulletX[j];
            int16_t by = state->bulletY[j];
            if (bx + BULLET_W >= r->x && bx <= r->x + ROCK_SIZE &&
                by + BULLET_H >= r->y && by <= r->y + ROCK_SIZE) {
                r->alive = 0;
                ffx_sceneNode_setHidden(state->rocks[i], true);
                state->bulletAlive[j] = false;
                ffx_sceneNode_setHidden(state->bullets[j], true);
                state->score += 50;
                break;
            }
        }
        if (!r->alive) { continue; }

        if (r->x < shipR && r->x + ROCK_SIZE > shipL &&
            r->y < shipB && r->y + ROCK_SIZE > shipT) {
            state->gameOver = true;
            ffx_sceneNode_setHidden(state->gameOverLabel, false);
        }

        ffx_sceneNode_setPosition(state->rocks[i], ffx_point(r->x, r->y));
    }

    ffx_sceneLabel_setTextFormat(state->scoreLabel, "SCORE %lu",
      (unsigned long)state->score);
}

static int initFunc(FfxScene scene, FfxNode panel, void *_state, void *arg) {
    RoidsState *state = _state;
    state->scene = scene;
    state->rng = 0x13371337 ^ ticks();

    state->bg = ffx_scene_createBox(scene, ffx_size(240, 240));
    ffx_sceneBox_setColor(state->bg, ffx_color_rgb(2, 2, 14));
    ffx_sceneGroup_appendChild(panel, state->bg);
    ffx_sceneNode_setPosition(state->bg, ffx_point(0, 0));

    state->ship = ffx_scene_createBox(scene, ffx_size(SHIP_W, SHIP_H));
    ffx_sceneBox_setColor(state->ship, ffx_color_rgb(0, 255, 200));
    ffx_sceneGroup_appendChild(panel, state->ship);

    for (int i = 0; i < MAX_ROCKS; i++) {
        FfxNode r = ffx_scene_createBox(scene, ffx_size(ROCK_SIZE, ROCK_SIZE));
        ffx_sceneBox_setColor(r, ffx_color_rgb(160, 80, 40));
        ffx_sceneGroup_appendChild(panel, r);
        ffx_sceneNode_setHidden(r, true);
        state->rocks[i] = r;
    }
    for (int i = 0; i < MAX_BULLETS; i++) {
        FfxNode b = ffx_scene_createBox(scene, ffx_size(BULLET_W, BULLET_H));
        ffx_sceneBox_setColor(b, ffx_color_rgb(255, 255, 0));
        ffx_sceneGroup_appendChild(panel, b);
        ffx_sceneNode_setHidden(b, true);
        state->bullets[i] = b;
    }

    state->scoreLabel = ffx_scene_createLabel(scene, FfxFontMedium, "SCORE 0");
    ffx_sceneGroup_appendChild(panel, state->scoreLabel);
    ffx_sceneNode_setPosition(state->scoreLabel, ffx_point(8, 14));
    ffx_sceneLabel_setAlign(state->scoreLabel,
      FfxTextAlignLeft | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(state->scoreLabel, COLOR_BLACK);

    state->hint = ffx_scene_createLabel(scene, FfxFontMedium,
      "N/S:MOVE  OK:FIRE  X:EXIT");
    ffx_sceneGroup_appendChild(panel, state->hint);
    ffx_sceneNode_setPosition(state->hint, ffx_point(120, 230));
    ffx_sceneLabel_setAlign(state->hint,
      FfxTextAlignCenter | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(state->hint, COLOR_BLACK);

    state->gameOverLabel = ffx_scene_createLabel(scene, FfxFontLargeBold,
      "GAME OVER - OK");
    ffx_sceneGroup_appendChild(panel, state->gameOverLabel);
    ffx_sceneNode_setPosition(state->gameOverLabel, ffx_point(120, 120));
    ffx_sceneLabel_setAlign(state->gameOverLabel,
      FfxTextAlignCenter | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(state->gameOverLabel, COLOR_BLACK);
    ffx_sceneNode_setHidden(state->gameOverLabel, true);

    resetGame(state);

    ffx_onEvent(FfxEventKeys, onKeys, state);
    ffx_onEvent(FfxEventRenderScene, onRender, state);

    return 0;
}

int pushPanelRoids() {
    return ffx_pushPanel(initFunc, sizeof(RoidsState), NULL);
}
