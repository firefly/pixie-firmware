// Asteroids - vertical / portrait layout. Ship at the bottom, rocks
// fall from the top, bullets fire up. Uses the Le Space sprite set.

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "firefly-hollows.h"
#include "firefly-scene.h"

#include "feedback.h"
#include "image-data.h"
#include "panels.h"
#include "utils.h"


#define MAX_ROCKS       (10)
#define MAX_BULLETS     (6)

// Sprite dimensions after the 90-deg CW rotation in images.c:
// ship is 38 wide x 36 tall, rock is 26 wide x 20 tall.
#define SHIP_W          (38)
#define SHIP_H          (36)
#define SHIP_Y          (240 - SHIP_H - 2)

#define ROCK_W          (26)
#define ROCK_H          (20)
#define BULLET_W        (10)
#define BULLET_H        (8)
#define BULLET_SPEED    (5)

#define HUD_H           (16)


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

    FfxNode hud;
    FfxNode scoreLabel;
    FfxNode bigLabel;
    FfxNode subLabel;

    int16_t shipX;
    Rock rocks_state[MAX_ROCKS];
    int16_t bulletX[MAX_BULLETS];
    int16_t bulletY[MAX_BULLETS];
    bool bulletAlive[MAX_BULLETS];

    FfxKeys keys;
    uint32_t score;
    uint32_t spawnAt;
    uint32_t fireCooldown;
    uint32_t okHeldAt;
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

static void hideOverlay(RoidsState *s) {
    ffx_sceneNode_setHidden(s->bigLabel, true);
    ffx_sceneNode_setHidden(s->subLabel, true);
}

static void showOverlay(RoidsState *s, const char *big, const char *sub) {
    ffx_sceneLabel_setText(s->bigLabel, big);
    ffx_sceneLabel_setText(s->subLabel, sub);
    ffx_sceneNode_setHidden(s->bigLabel, false);
    ffx_sceneNode_setHidden(s->subLabel, false);
}

static void spawnRock(RoidsState *state) {
    for (int i = 0; i < MAX_ROCKS; i++) {
        if (state->rocks_state[i].alive) { continue; }
        Rock *r = &state->rocks_state[i];
        // Spawn off the TOP edge, drifting down toward the ship.
        r->y = -ROCK_H;
        r->x = (int16_t)(rngNext(&state->rng) % (240 - ROCK_W));
        r->vy = 2 + (int8_t)(rngNext(&state->rng) % 3);
        r->vx = (int8_t)((rngNext(&state->rng) % 3) - 1);
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
        // Bullet starts above the ship and travels up.
        state->bulletX[i] = state->shipX + SHIP_W / 2 - BULLET_W / 2;
        state->bulletY[i] = SHIP_Y - BULLET_H - 2;
        ffx_sceneNode_setHidden(state->bullets[i], false);
        ffx_sceneNode_setPosition(state->bullets[i],
          ffx_point(state->bulletX[i], state->bulletY[i]));
        return;
    }
}

static void resetGame(RoidsState *state) {
    state->shipX = 120 - SHIP_W / 2;
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
    hideOverlay(state);
    ffx_sceneNode_setPosition(state->ship, ffx_point(state->shipX, SHIP_Y));
}

static void onKeys(FfxEvent event, FfxEventProps props, void *_state) {
    RoidsState *state = _state;
    state->keys = props.keys.down;
    feedback_onKey(props.keys.down);

    state->okHeldAt = (props.keys.down == FfxKeyOk) ? ticks() : 0;

    if (props.keys.down & FfxKeyCancel) {
        if (state->gameOver) {
            ffx_popPanel(0);
        } else {
            uint32_t t = ticks();
            if (t > state->fireCooldown) {
                fireBullet(state);
                state->fireCooldown = t + 180;
            }
        }
        return;
    }
    if (state->gameOver && (props.keys.down & FfxKeyOk)) {
        resetGame(state);
    }
}

static void onRender(FfxEvent event, FfxEventProps props, void *_state) {
    RoidsState *state = _state;

    if (!state->gameOver && state->keys == FfxKeyOk &&
        state->okHeldAt && ticks() - state->okHeldAt > 3000) {
        ffx_popPanel(0);
        return;
    }

    if (state->gameOver) { return; }

    uint32_t t = ticks();
    state->score++;

    // Ship moves horizontally.
    if (state->keys & FfxKeyNorth) { state->shipX -= 3; }
    if (state->keys & FfxKeySouth) { state->shipX += 3; }
    if (state->shipX < 0)              { state->shipX = 0; }
    if (state->shipX > 240 - SHIP_W)   { state->shipX = 240 - SHIP_W; }
    ffx_sceneNode_setPosition(state->ship, ffx_point(state->shipX, SHIP_Y));

    if (t > state->spawnAt) {
        spawnRock(state);
        uint32_t gap = 600 - (state->score / 4);
        if (gap < 200) { gap = 200; }
        state->spawnAt = t + gap;
    }

    // Bullets travel UP.
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!state->bulletAlive[i]) { continue; }
        state->bulletY[i] -= BULLET_SPEED;
        if (state->bulletY[i] + BULLET_H < HUD_H) {
            state->bulletAlive[i] = false;
            ffx_sceneNode_setHidden(state->bullets[i], true);
            continue;
        }
        ffx_sceneNode_setPosition(state->bullets[i],
          ffx_point(state->bulletX[i], state->bulletY[i]));
    }

    int16_t shipL = state->shipX;
    int16_t shipR = state->shipX + SHIP_W;
    int16_t shipT = SHIP_Y;
    int16_t shipB = SHIP_Y + SHIP_H;

    for (int i = 0; i < MAX_ROCKS; i++) {
        Rock *r = &state->rocks_state[i];
        if (!r->alive) { continue; }
        r->x += r->vx;
        r->y += r->vy;
        if (r->x < 0)               { r->x = 0; r->vx = -r->vx; }
        if (r->x > 240 - ROCK_W)    { r->x = 240 - ROCK_W; r->vx = -r->vx; }
        if (r->y > 240) {
            r->alive = 0;
            ffx_sceneNode_setHidden(state->rocks[i], true);
            continue;
        }

        for (int j = 0; j < MAX_BULLETS; j++) {
            if (!state->bulletAlive[j]) { continue; }
            int16_t bx = state->bulletX[j];
            int16_t by = state->bulletY[j];
            if (bx + BULLET_W >= r->x && bx <= r->x + ROCK_W &&
                by + BULLET_H >= r->y && by <= r->y + ROCK_H) {
                r->alive = 0;
                ffx_sceneNode_setHidden(state->rocks[i], true);
                state->bulletAlive[j] = false;
                ffx_sceneNode_setHidden(state->bullets[j], true);
                state->score += 50;
                break;
            }
        }
        if (!r->alive) { continue; }

        if (r->x < shipR && r->x + ROCK_W > shipL &&
            r->y < shipB && r->y + ROCK_H > shipT) {
            state->gameOver = true;
            showOverlay(state, "GAME OVER", "OK = AGAIN");
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

    state->bg = ffx_scene_createImage(scene, image_space, image_space_len);
    ffx_sceneGroup_appendChild(panel, state->bg);
    ffx_sceneNode_setPosition(state->bg, ffx_point(0, 0));

    for (int i = 0; i < MAX_ROCKS; i++) {
        FfxNode r = ffx_scene_createImage(scene, image_alienboom_cw,
          image_alienboom_cw_len);
        ffx_sceneGroup_appendChild(panel, r);
        ffx_sceneNode_setHidden(r, true);
        state->rocks[i] = r;
    }
    for (int i = 0; i < MAX_BULLETS; i++) {
        FfxNode b = ffx_scene_createImage(scene, image_bullet,
          image_bullet_len);
        ffx_sceneGroup_appendChild(panel, b);
        ffx_sceneNode_setHidden(b, true);
        state->bullets[i] = b;
    }

    state->ship = ffx_scene_createImage(scene, image_ship_cw, image_ship_cw_len);
    ffx_sceneGroup_appendChild(panel, state->ship);

    state->hud = ffx_scene_createBox(scene, ffx_size(240, HUD_H));
    ffx_sceneBox_setColor(state->hud, RGBA_DARKER75);
    ffx_sceneGroup_appendChild(panel, state->hud);
    ffx_sceneNode_setPosition(state->hud, ffx_point(0, 0));

    state->scoreLabel = ffx_scene_createLabel(scene, FfxFontMedium, "SCORE 0");
    ffx_sceneGroup_appendChild(panel, state->scoreLabel);
    ffx_sceneNode_setPosition(state->scoreLabel, ffx_point(120, 8));
    ffx_sceneLabel_setAlign(state->scoreLabel,
      FfxTextAlignCenter | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(state->scoreLabel, COLOR_BLACK);

    state->bigLabel = ffx_scene_createLabel(scene, FfxFontLargeBold, "");
    ffx_sceneGroup_appendChild(panel, state->bigLabel);
    ffx_sceneNode_setPosition(state->bigLabel, ffx_point(120, 110));
    ffx_sceneLabel_setAlign(state->bigLabel,
      FfxTextAlignCenter | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(state->bigLabel, COLOR_BLACK);
    ffx_sceneNode_setHidden(state->bigLabel, true);

    state->subLabel = ffx_scene_createLabel(scene, FfxFontMedium, "");
    ffx_sceneGroup_appendChild(panel, state->subLabel);
    ffx_sceneNode_setPosition(state->subLabel, ffx_point(120, 140));
    ffx_sceneLabel_setAlign(state->subLabel,
      FfxTextAlignCenter | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(state->subLabel, COLOR_BLACK);
    ffx_sceneNode_setHidden(state->subLabel, true);

    feedback_addButtonLegend(panel, "UP", "DOWN", "HOLD", "FIRE");

    resetGame(state);

    ffx_onEvent(FfxEventKeys, onKeys, state);
    ffx_onEvent(FfxEventRenderScene, onRender, state);

    return 0;
}

int pushPanelRoids() {
    return ffx_pushPanel(initFunc, sizeof(RoidsState), NULL);
}
