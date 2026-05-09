// Top-down dungeon crawler. 12x12 grid, classic turn-and-step controls
// (N=turn left, S=turn right, OK=step forward, Cancel=exit). Find the
// goal cell to win.

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "firefly-hollows.h"
#include "firefly-scene.h"

#include "panels.h"
#include "utils.h"


#define DUNGEON_W       (12)
#define DUNGEON_H       (12)
#define CELL_SIZE       (18)
#define GRID_PIXELS     (DUNGEON_W * CELL_SIZE)
#define GRID_OFFSET_X   ((240 - GRID_PIXELS) / 2)
#define GRID_OFFSET_Y   (24)

#define DIR_N           (0)
#define DIR_E           (1)
#define DIR_S           (2)
#define DIR_W           (3)


typedef struct CrawlState {
    FfxScene scene;
    FfxNode bg;
    FfxNode walls[DUNGEON_W * DUNGEON_H];
    FfxNode goalMarker;
    FfxNode player;
    FfxNode facingDot;
    FfxNode titleLabel;
    FfxNode statusLabel;
    FfxNode hint;

    int8_t px, py;
    int8_t pdir;
    int8_t goalX, goalY;
    int steps;
    bool won;
} CrawlState;


static const char dungeon[DUNGEON_H][DUNGEON_W + 1] = {
    "############",
    "#P.....#...#",
    "#.###..#.#.#",
    "#.#.#..#.#.#",
    "#.#......#.#",
    "#.######.#.#",
    "#........#.#",
    "###.######.#",
    "#..........#",
    "#.######.#.#",
    "#........#G#",
    "############"
};


static void placePlayer(CrawlState *state) {
    int x = GRID_OFFSET_X + state->px * CELL_SIZE + 2;
    int y = GRID_OFFSET_Y + state->py * CELL_SIZE + 2;
    ffx_sceneNode_setPosition(state->player, ffx_point(x, y));

    int cx = GRID_OFFSET_X + state->px * CELL_SIZE + CELL_SIZE / 2 - 2;
    int cy = GRID_OFFSET_Y + state->py * CELL_SIZE + CELL_SIZE / 2 - 2;
    int dx = 0, dy = 0;
    switch (state->pdir) {
        case DIR_N: dy = -CELL_SIZE / 2 + 2; break;
        case DIR_E: dx =  CELL_SIZE / 2 - 2; break;
        case DIR_S: dy =  CELL_SIZE / 2 - 2; break;
        case DIR_W: dx = -CELL_SIZE / 2 + 2; break;
    }
    ffx_sceneNode_setPosition(state->facingDot, ffx_point(cx + dx, cy + dy));
}

static bool isWall(int x, int y) {
    if (x < 0 || x >= DUNGEON_W || y < 0 || y >= DUNGEON_H) { return true; }
    return dungeon[y][x] == '#';
}

static void resetGame(CrawlState *state) {
    state->steps = 0;
    state->won = false;
    state->pdir = DIR_E;
    for (int y = 0; y < DUNGEON_H; y++) {
        for (int x = 0; x < DUNGEON_W; x++) {
            char c = dungeon[y][x];
            if (c == 'P') { state->px = x; state->py = y; }
            if (c == 'G') { state->goalX = x; state->goalY = y; }
        }
    }
    placePlayer(state);
    ffx_sceneNode_setHidden(state->statusLabel, true);
}

static void onKeys(FfxEvent event, FfxEventProps props, void *_state) {
    CrawlState *state = _state;

    if (props.keys.down & FfxKeyCancel) {
        ffx_popPanel(0);
        return;
    }
    if (state->won) {
        if (props.keys.down & FfxKeyOk) { resetGame(state); }
        return;
    }
    if (props.keys.down & FfxKeyNorth) {
        state->pdir = (state->pdir + 3) & 3;
        placePlayer(state);
    } else if (props.keys.down & FfxKeySouth) {
        state->pdir = (state->pdir + 1) & 3;
        placePlayer(state);
    } else if (props.keys.down & FfxKeyOk) {
        int nx = state->px;
        int ny = state->py;
        switch (state->pdir) {
            case DIR_N: ny--; break;
            case DIR_E: nx++; break;
            case DIR_S: ny++; break;
            case DIR_W: nx--; break;
        }
        if (!isWall(nx, ny)) {
            state->px = nx;
            state->py = ny;
            state->steps++;
            placePlayer(state);
            if (nx == state->goalX && ny == state->goalY) {
                state->won = true;
                ffx_sceneLabel_setTextFormat(state->statusLabel,
                  "ESCAPED IN %d STEPS - OK", state->steps);
                ffx_sceneNode_setHidden(state->statusLabel, false);
            } else {
                ffx_sceneLabel_setTextFormat(state->titleLabel,
                  "DUNGEON  STEPS:%d", state->steps);
            }
        }
    }
}

static int initFunc(FfxScene scene, FfxNode panel, void *_state, void *arg) {
    CrawlState *state = _state;
    state->scene = scene;

    state->bg = ffx_scene_createBox(scene, ffx_size(240, 240));
    ffx_sceneBox_setColor(state->bg, ffx_color_rgb(8, 8, 16));
    ffx_sceneGroup_appendChild(panel, state->bg);
    ffx_sceneNode_setPosition(state->bg, ffx_point(0, 0));

    state->titleLabel = ffx_scene_createLabel(scene, FfxFontLargeBold,
      "DUNGEON");
    ffx_sceneGroup_appendChild(panel, state->titleLabel);
    ffx_sceneNode_setPosition(state->titleLabel, ffx_point(120, 12));
    ffx_sceneLabel_setAlign(state->titleLabel,
      FfxTextAlignCenter | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(state->titleLabel, COLOR_BLACK);

    for (int y = 0; y < DUNGEON_H; y++) {
        for (int x = 0; x < DUNGEON_W; x++) {
            int i = y * DUNGEON_W + x;
            FfxNode cell = ffx_scene_createBox(scene,
              ffx_size(CELL_SIZE - 1, CELL_SIZE - 1));
            ffx_sceneGroup_appendChild(panel, cell);
            ffx_sceneNode_setPosition(cell, ffx_point(
              GRID_OFFSET_X + x * CELL_SIZE,
              GRID_OFFSET_Y + y * CELL_SIZE));
            if (dungeon[y][x] == '#') {
                ffx_sceneBox_setColor(cell, ffx_color_rgb(80, 70, 110));
            } else {
                ffx_sceneBox_setColor(cell, ffx_color_rgb(20, 20, 30));
            }
            state->walls[i] = cell;
        }
    }

    state->goalMarker = ffx_scene_createBox(scene,
      ffx_size(CELL_SIZE - 6, CELL_SIZE - 6));
    ffx_sceneBox_setColor(state->goalMarker, ffx_color_rgb(0, 255, 80));
    ffx_sceneGroup_appendChild(panel, state->goalMarker);

    state->player = ffx_scene_createBox(scene,
      ffx_size(CELL_SIZE - 4, CELL_SIZE - 4));
    ffx_sceneBox_setColor(state->player, ffx_color_rgb(255, 200, 0));
    ffx_sceneGroup_appendChild(panel, state->player);

    state->facingDot = ffx_scene_createBox(scene, ffx_size(4, 4));
    ffx_sceneBox_setColor(state->facingDot, ffx_color_rgb(255, 255, 255));
    ffx_sceneGroup_appendChild(panel, state->facingDot);

    state->hint = ffx_scene_createLabel(scene, FfxFontMedium,
      "N:LEFT  S:RIGHT  OK:STEP  X:EXIT");
    ffx_sceneGroup_appendChild(panel, state->hint);
    ffx_sceneNode_setPosition(state->hint, ffx_point(120, 230));
    ffx_sceneLabel_setAlign(state->hint,
      FfxTextAlignCenter | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(state->hint, COLOR_BLACK);

    state->statusLabel = ffx_scene_createLabel(scene, FfxFontLargeBold,
      "ESCAPED");
    ffx_sceneGroup_appendChild(panel, state->statusLabel);
    ffx_sceneNode_setPosition(state->statusLabel, ffx_point(120, 120));
    ffx_sceneLabel_setAlign(state->statusLabel,
      FfxTextAlignCenter | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(state->statusLabel, COLOR_BLACK);
    ffx_sceneNode_setHidden(state->statusLabel, true);

    resetGame(state);

    ffx_sceneNode_setPosition(state->goalMarker, ffx_point(
      GRID_OFFSET_X + state->goalX * CELL_SIZE + 3,
      GRID_OFFSET_Y + state->goalY * CELL_SIZE + 3));

    ffx_onEvent(FfxEventKeys, onKeys, state);

    return 0;
}

int pushPanelCrawl() {
    return ffx_pushPanel(initFunc, sizeof(CrawlState), NULL);
}
