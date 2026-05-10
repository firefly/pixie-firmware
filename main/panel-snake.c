// Snake - classic grid-based snake game on a 16x16 board.
// Tap a direction to turn; the snake auto-advances on a tick.
// Eat the food to grow longer; hitting a wall or your own tail ends
// the game.

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "firefly-hollows.h"
#include "firefly-scene.h"

#include "feedback.h"
#include "panels.h"
#include "utils.h"


#define GRID_DIM        (16)
#define GRID_CELLS      (GRID_DIM * GRID_DIM)
#define CELL_SIZE       (14)
#define CELL_GAP        (0)
#define GRID_PIXELS     (GRID_DIM * (CELL_SIZE + CELL_GAP))
#define GRID_OFFSET_X   ((240 - GRID_PIXELS) / 2)
#define GRID_OFFSET_Y   (24)

#define MAX_SNAKE       (96)
#define INITIAL_LEN     (4)
#define MOVE_INITIAL_MS (180)
#define MOVE_MIN_MS     (70)

#define DIR_N           (0)
#define DIR_E           (1)
#define DIR_S           (2)
#define DIR_W           (3)


typedef struct Cell { int8_t x, y; } Cell;

typedef struct SnakeState {
    FfxScene scene;
    FfxNode bg;
    FfxNode boardBg;
    FfxNode segments[MAX_SNAKE];
    FfxNode food;
    FfxNode hud;
    FfxNode scoreLabel;
    FfxNode bigLabel;
    FfxNode subLabel;

    Cell body[MAX_SNAKE];
    int length;
    int8_t dir;
    int8_t pendingDir;

    Cell foodCell;

    uint32_t score;
    uint32_t lastMoveAt;
    uint32_t moveIntervalMs;

    uint32_t okHeldAt;
    bool gameOver;
    uint32_t rng;
    FfxKeys keys;
} SnakeState;


static uint32_t rngNext(uint32_t *s) {
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x ? x : 0xC0DEFEED;
    return *s;
}

static void hideOverlay(SnakeState *s) {
    ffx_sceneNode_setHidden(s->bigLabel, true);
    ffx_sceneNode_setHidden(s->subLabel, true);
}

static void showOverlay(SnakeState *s, const char *big, const char *sub) {
    ffx_sceneLabel_setText(s->bigLabel, big);
    ffx_sceneLabel_setText(s->subLabel, sub);
    ffx_sceneNode_setHidden(s->bigLabel, false);
    ffx_sceneNode_setHidden(s->subLabel, false);
}

static FfxPoint cellToPoint(int8_t x, int8_t y) {
    return ffx_point(GRID_OFFSET_X + x * (CELL_SIZE + CELL_GAP),
                     GRID_OFFSET_Y + y * (CELL_SIZE + CELL_GAP));
}

static bool snakeOccupies(SnakeState *s, int8_t x, int8_t y) {
    for (int i = 0; i < s->length; i++) {
        if (s->body[i].x == x && s->body[i].y == y) { return true; }
    }
    return false;
}

static void placeFood(SnakeState *s) {
    // Try random spots; bail to a deterministic scan after a few tries.
    for (int attempt = 0; attempt < 64; attempt++) {
        int8_t x = (int8_t)(rngNext(&s->rng) % GRID_DIM);
        int8_t y = (int8_t)(rngNext(&s->rng) % GRID_DIM);
        if (!snakeOccupies(s, x, y)) {
            s->foodCell.x = x;
            s->foodCell.y = y;
            ffx_sceneNode_setPosition(s->food, cellToPoint(x, y));
            return;
        }
    }
    for (int8_t y = 0; y < GRID_DIM; y++) {
        for (int8_t x = 0; x < GRID_DIM; x++) {
            if (!snakeOccupies(s, x, y)) {
                s->foodCell.x = x;
                s->foodCell.y = y;
                ffx_sceneNode_setPosition(s->food, cellToPoint(x, y));
                return;
            }
        }
    }
}

static void renderSnake(SnakeState *s) {
    for (int i = 0; i < MAX_SNAKE; i++) {
        if (i < s->length) {
            ffx_sceneNode_setHidden(s->segments[i], false);
            ffx_sceneNode_setPosition(s->segments[i],
              cellToPoint(s->body[i].x, s->body[i].y));
            // Head a bit brighter.
            if (i == 0) {
                ffx_sceneBox_setColor(s->segments[i],
                  ffx_color_rgb(120, 255, 120));
            } else {
                ffx_sceneBox_setColor(s->segments[i],
                  ffx_color_rgb(0, 200, 80));
            }
        } else {
            ffx_sceneNode_setHidden(s->segments[i], true);
        }
    }
}

static void resetGame(SnakeState *s) {
    s->length = INITIAL_LEN;
    s->dir = DIR_E;
    s->pendingDir = DIR_E;
    s->score = 0;
    s->moveIntervalMs = MOVE_INITIAL_MS;
    s->lastMoveAt = ticks();
    s->gameOver = false;

    int cx = GRID_DIM / 2;
    int cy = GRID_DIM / 2;
    for (int i = 0; i < s->length; i++) {
        s->body[i].x = (int8_t)(cx - i);
        s->body[i].y = (int8_t)cy;
    }

    hideOverlay(s);
    placeFood(s);
    renderSnake(s);
    ffx_sceneLabel_setTextFormat(s->scoreLabel, "SCORE %lu",
      (unsigned long)s->score);
}

static void requestDir(SnakeState *s, int8_t newDir) {
    // Block U-turns - they'd instantly self-collide.
    if ((s->dir == DIR_N && newDir == DIR_S) ||
        (s->dir == DIR_S && newDir == DIR_N) ||
        (s->dir == DIR_E && newDir == DIR_W) ||
        (s->dir == DIR_W && newDir == DIR_E)) {
        return;
    }
    s->pendingDir = newDir;
}

static void stepSnake(SnakeState *s) {
    s->dir = s->pendingDir;

    int8_t dx = 0, dy = 0;
    switch (s->dir) {
        case DIR_N: dy = -1; break;
        case DIR_E: dx =  1; break;
        case DIR_S: dy =  1; break;
        case DIR_W: dx = -1; break;
    }

    int8_t nx = s->body[0].x + dx;
    int8_t ny = s->body[0].y + dy;

    if (nx < 0 || nx >= GRID_DIM || ny < 0 || ny >= GRID_DIM) {
        s->gameOver = true;
        showOverlay(s, "GAME OVER", "OK = AGAIN");
        return;
    }
    // Self-collision: ignore the tail cell because it'll move out of
    // the way unless we're growing.
    bool eating = (nx == s->foodCell.x && ny == s->foodCell.y);
    int checkLen = eating ? s->length : s->length - 1;
    for (int i = 0; i < checkLen; i++) {
        if (s->body[i].x == nx && s->body[i].y == ny) {
            s->gameOver = true;
            showOverlay(s, "GAME OVER", "OK = AGAIN");
            return;
        }
    }

    if (eating) {
        if (s->length < MAX_SNAKE) {
            for (int i = s->length; i > 0; i--) { s->body[i] = s->body[i - 1]; }
            s->body[0].x = nx;
            s->body[0].y = ny;
            s->length++;
        } else {
            for (int i = s->length - 1; i > 0; i--) {
                s->body[i] = s->body[i - 1];
            }
            s->body[0].x = nx;
            s->body[0].y = ny;
        }
        s->score += 10;
        ffx_sceneLabel_setTextFormat(s->scoreLabel, "SCORE %lu",
          (unsigned long)s->score);
        if (s->moveIntervalMs > MOVE_MIN_MS) { s->moveIntervalMs -= 4; }
        placeFood(s);
    } else {
        for (int i = s->length - 1; i > 0; i--) {
            s->body[i] = s->body[i - 1];
        }
        s->body[0].x = nx;
        s->body[0].y = ny;
    }

    renderSnake(s);

    if (s->length >= GRID_CELLS) {
        s->gameOver = true;
        showOverlay(s, "PERFECT!", "OK = AGAIN");
    }
}

static void onKeys(FfxEvent event, FfxEventProps props, void *_state) {
    SnakeState *s = _state;
    s->keys = props.keys.down;
    feedback_onKey(props.keys.down);

    if (s->gameOver) {
        if (props.keys.down & FfxKeyOk)     { resetGame(s); return; }
        if (props.keys.down & FfxKeyCancel) { ffx_popPanel(0); return; }
        return;
    }

    s->okHeldAt = (props.keys.down == FfxKeyOk) ? ticks() : 0;

    if (props.keys.down & FfxKeyNorth)  { requestDir(s, DIR_N); }
    if (props.keys.down & FfxKeySouth)  { requestDir(s, DIR_S); }
    // OK = right, Cancel = left so all four directions are reachable
    // and a long-press OK can still exit (handled in onRender).
    if (props.keys.down & FfxKeyOk)     { requestDir(s, DIR_E); }
    if (props.keys.down & FfxKeyCancel) { requestDir(s, DIR_W); }
}

static void onRender(FfxEvent event, FfxEventProps props, void *_state) {
    SnakeState *s = _state;
    uint32_t t = ticks();

    if (!s->gameOver && s->keys == FfxKeyOk &&
        s->okHeldAt && t - s->okHeldAt > 3000) {
        ffx_popPanel(0);
        return;
    }

    if (s->gameOver) { return; }

    if (t - s->lastMoveAt >= s->moveIntervalMs) {
        s->lastMoveAt = t;
        stepSnake(s);
    }
}

static int initFunc(FfxScene scene, FfxNode panel, void *_state, void *arg) {
    SnakeState *s = _state;
    s->scene = scene;
    s->rng = 0xDEADBEEF ^ ticks();

    s->bg = ffx_scene_createBox(scene, ffx_size(240, 240));
    ffx_sceneBox_setColor(s->bg, ffx_color_rgb(0, 4, 0));
    ffx_sceneGroup_appendChild(panel, s->bg);
    ffx_sceneNode_setPosition(s->bg, ffx_point(0, 0));

    s->boardBg = ffx_scene_createBox(scene,
      ffx_size(GRID_PIXELS + 4, GRID_PIXELS + 4));
    ffx_sceneBox_setColor(s->boardBg, ffx_color_rgb(8, 18, 8));
    ffx_sceneGroup_appendChild(panel, s->boardBg);
    ffx_sceneNode_setPosition(s->boardBg,
      ffx_point(GRID_OFFSET_X - 2, GRID_OFFSET_Y - 2));

    s->food = ffx_scene_createBox(scene, ffx_size(CELL_SIZE, CELL_SIZE));
    ffx_sceneBox_setColor(s->food, ffx_color_rgb(255, 60, 80));
    ffx_sceneGroup_appendChild(panel, s->food);

    for (int i = 0; i < MAX_SNAKE; i++) {
        FfxNode seg = ffx_scene_createBox(scene, ffx_size(CELL_SIZE, CELL_SIZE));
        ffx_sceneBox_setColor(seg, ffx_color_rgb(0, 200, 80));
        ffx_sceneGroup_appendChild(panel, seg);
        ffx_sceneNode_setHidden(seg, true);
        s->segments[i] = seg;
    }

    s->hud = ffx_scene_createBox(scene, ffx_size(240, 16));
    ffx_sceneBox_setColor(s->hud, RGBA_DARKER75);
    ffx_sceneGroup_appendChild(panel, s->hud);
    ffx_sceneNode_setPosition(s->hud, ffx_point(0, 0));

    s->scoreLabel = ffx_scene_createLabel(scene, FfxFontMedium, "SCORE 0");
    ffx_sceneGroup_appendChild(panel, s->scoreLabel);
    ffx_sceneNode_setPosition(s->scoreLabel, ffx_point(120, 8));
    ffx_sceneLabel_setAlign(s->scoreLabel,
      FfxTextAlignCenter | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(s->scoreLabel, COLOR_BLACK);

    s->bigLabel = ffx_scene_createLabel(scene, FfxFontLargeBold, "");
    ffx_sceneGroup_appendChild(panel, s->bigLabel);
    ffx_sceneNode_setPosition(s->bigLabel, ffx_point(120, 116));
    ffx_sceneLabel_setAlign(s->bigLabel,
      FfxTextAlignCenter | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(s->bigLabel, COLOR_BLACK);
    ffx_sceneNode_setHidden(s->bigLabel, true);

    s->subLabel = ffx_scene_createLabel(scene, FfxFontMedium, "");
    ffx_sceneGroup_appendChild(panel, s->subLabel);
    ffx_sceneNode_setPosition(s->subLabel, ffx_point(120, 146));
    ffx_sceneLabel_setAlign(s->subLabel,
      FfxTextAlignCenter | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(s->subLabel, COLOR_BLACK);
    ffx_sceneNode_setHidden(s->subLabel, true);

    feedback_addButtonLegend(panel, "UP", "DOWN", "RIGHT", "LEFT");

    resetGame(s);

    ffx_onEvent(FfxEventKeys, onKeys, s);
    ffx_onEvent(FfxEventRenderScene, onRender, s);

    return 0;
}

int pushPanelSnake() {
    return ffx_pushPanel(initFunc, sizeof(SnakeState), NULL);
}
