#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "firefly-hollows.h"
#include "firefly-scene.h"

#include "feedback.h"
#include "panels.h"
#include "utils.h"


#define GRID_DIM        (16)
#define CELL_SIZE       (14)
#define CELL_GAP        (1)
#define GRID_PIXELS     (GRID_DIM * (CELL_SIZE + CELL_GAP))
#define GRID_OFFSET     ((240 - GRID_PIXELS) / 2)
#define GRID_TOP        (32)

#define STEP_INTERVAL   (160)

#define PRESET_COUNT    (4)


typedef struct LifeState {
    FfxScene scene;
    FfxNode cells[GRID_DIM * GRID_DIM];
    FfxNode presetLabels[PRESET_COUNT];

    uint8_t board[GRID_DIM * GRID_DIM];
    uint8_t scratch[GRID_DIM * GRID_DIM];

    int preset;
    bool paused;
    uint32_t nextStepAt;
    uint32_t rngState;
} LifeState;


static const char *presetNames[PRESET_COUNT] = {
    "GLIDER", "PULSAR", "RPENTOMINO", "RANDOM"
};

static uint32_t rngNext(uint32_t *s) {
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x ? x : 0xCAFEBABE;
    return *s;
}

static void boardClear(LifeState *life) {
    memset(life->board, 0, sizeof(life->board));
}

static void boardSet(LifeState *life, int x, int y, uint8_t v) {
    if (x < 0 || x >= GRID_DIM || y < 0 || y >= GRID_DIM) { return; }
    life->board[y * GRID_DIM + x] = v ? 1 : 0;
}

static void seedGlider(LifeState *life) {
    boardClear(life);
    boardSet(life, 1, 0, 1);
    boardSet(life, 2, 1, 1);
    boardSet(life, 0, 2, 1);
    boardSet(life, 1, 2, 1);
    boardSet(life, 2, 2, 1);
}

static void seedPulsar(LifeState *life) {
    static const int8_t cells[][2] = {
        {2,0},{3,0},{4,0},{8,0},{9,0},{10,0},
        {0,2},{5,2},{7,2},{12,2},
        {0,3},{5,3},{7,3},{12,3},
        {0,4},{5,4},{7,4},{12,4},
        {2,5},{3,5},{4,5},{8,5},{9,5},{10,5},
        {2,7},{3,7},{4,7},{8,7},{9,7},{10,7},
        {0,8},{5,8},{7,8},{12,8},
        {0,9},{5,9},{7,9},{12,9},
        {0,10},{5,10},{7,10},{12,10},
        {2,12},{3,12},{4,12},{8,12},{9,12},{10,12}
    };
    boardClear(life);
    int ox = (GRID_DIM - 13) / 2;
    int oy = (GRID_DIM - 13) / 2;
    for (size_t i = 0; i < sizeof(cells) / sizeof(cells[0]); i++) {
        boardSet(life, ox + cells[i][0], oy + cells[i][1], 1);
    }
}

static void seedRPentomino(LifeState *life) {
    boardClear(life);
    int cx = GRID_DIM / 2;
    int cy = GRID_DIM / 2;
    boardSet(life, cx, cy - 1, 1);
    boardSet(life, cx + 1, cy - 1, 1);
    boardSet(life, cx - 1, cy, 1);
    boardSet(life, cx, cy, 1);
    boardSet(life, cx, cy + 1, 1);
}

static void seedRandom(LifeState *life) {
    boardClear(life);
    for (int i = 0; i < GRID_DIM * GRID_DIM; i++) {
        life->board[i] = (rngNext(&life->rngState) & 0x3) == 0 ? 1 : 0;
    }
}

static void applyPreset(LifeState *life) {
    switch (life->preset) {
        case 0: seedGlider(life); break;
        case 1: seedPulsar(life); break;
        case 2: seedRPentomino(life); break;
        default: seedRandom(life); break;
    }
}

static void stepBoard(LifeState *life) {
    for (int y = 0; y < GRID_DIM; y++) {
        for (int x = 0; x < GRID_DIM; x++) {
            int n = 0;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) { continue; }
                    int nx = (x + dx + GRID_DIM) % GRID_DIM;
                    int ny = (y + dy + GRID_DIM) % GRID_DIM;
                    n += life->board[ny * GRID_DIM + nx];
                }
            }
            uint8_t alive = life->board[y * GRID_DIM + x];
            life->scratch[y * GRID_DIM + x] =
              (alive && (n == 2 || n == 3)) || (!alive && n == 3);
        }
    }
    memcpy(life->board, life->scratch, sizeof(life->board));
}

static void renderBoard(LifeState *life) {
    for (int i = 0; i < GRID_DIM * GRID_DIM; i++) {
        ffx_sceneNode_setHidden(life->cells[i], !life->board[i]);
    }
}

static void updatePresetHighlight(LifeState *life) {
    for (int i = 0; i < PRESET_COUNT; i++) {
        ffx_sceneNode_setHidden(life->presetLabels[i], i != life->preset);
    }
}

static void onRender(FfxEvent event, FfxEventProps props, void *_state) {
    LifeState *life = _state;
    if (life->paused) { return; }

    uint32_t t = ticks();
    if (t < life->nextStepAt) { return; }
    life->nextStepAt = t + STEP_INTERVAL;

    stepBoard(life);
    renderBoard(life);
}

static void onKeys(FfxEvent event, FfxEventProps props, void *_state) {
    LifeState *life = _state;
    feedback_onKey(props.keys.down);

    switch (props.keys.down) {
        case FfxKeyCancel:
            ffx_popPanel(0);
            break;
        case FfxKeyOk:
            life->paused = !life->paused;
            life->nextStepAt = ticks();
            break;
        case FfxKeyNorth:
            life->preset = (life->preset + PRESET_COUNT - 1) % PRESET_COUNT;
            applyPreset(life);
            renderBoard(life);
            updatePresetHighlight(life);
            break;
        case FfxKeySouth:
            life->preset = (life->preset + 1) % PRESET_COUNT;
            applyPreset(life);
            renderBoard(life);
            updatePresetHighlight(life);
            break;
    }
}

static int initFunc(FfxScene scene, FfxNode panel, void *_state, void *arg) {
    LifeState *life = _state;
    life->scene = scene;
    life->preset = 0;
    life->paused = false;
    life->rngState = 0xDEADBEEF ^ ticks();

    FfxNode bg = ffx_scene_createBox(scene, ffx_size(240, 240));
    ffx_sceneBox_setColor(bg, ffx_color_rgb(2, 6, 4));
    ffx_sceneGroup_appendChild(panel, bg);
    ffx_sceneNode_setPosition(bg, ffx_point(0, 0));

    FfxNode title = ffx_scene_createLabel(scene, FfxFontLargeBold, "LIFE//GRID");
    ffx_sceneGroup_appendChild(panel, title);
    ffx_sceneNode_setPosition(title, ffx_point(8, 16));
    ffx_sceneLabel_setAlign(title, FfxTextAlignLeft | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(title, ffx_color_rgb(0, 0, 0));

    for (int i = 0; i < PRESET_COUNT; i++) {
        FfxNode label = ffx_scene_createLabel(scene, FfxFontMedium,
          presetNames[i]);
        ffx_sceneGroup_appendChild(panel, label);
        ffx_sceneNode_setPosition(label, ffx_point(232, 16));
        ffx_sceneLabel_setAlign(label, FfxTextAlignRight | FfxTextAlignMiddle);
        ffx_sceneLabel_setOutlineColor(label, ffx_color_rgb(0, 0, 0));
        life->presetLabels[i] = label;
    }
    updatePresetHighlight(life);

    for (int y = 0; y < GRID_DIM; y++) {
        for (int x = 0; x < GRID_DIM; x++) {
            FfxNode cell = ffx_scene_createBox(scene,
              ffx_size(CELL_SIZE, CELL_SIZE));
            uint8_t g = 120 + ((x + y) * 8) % 130;
            ffx_sceneBox_setColor(cell, ffx_color_rgb(0, g, 65));
            ffx_sceneGroup_appendChild(panel, cell);
            ffx_sceneNode_setPosition(cell, ffx_point(
              GRID_OFFSET + x * (CELL_SIZE + CELL_GAP),
              GRID_TOP + y * (CELL_SIZE + CELL_GAP)));
            ffx_sceneNode_setHidden(cell, true);
            life->cells[y * GRID_DIM + x] = cell;
        }
    }

    feedback_addButtonLegend(panel, "PREV", "NEXT", "PAUSE", "EXIT");

    applyPreset(life);
    renderBoard(life);
    life->nextStepAt = ticks() + STEP_INTERVAL;

    ffx_onEvent(FfxEventKeys, onKeys, life);
    ffx_onEvent(FfxEventRenderScene, onRender, life);

    return 0;
}

int pushPanelLife() {
    return ffx_pushPanel(initFunc, sizeof(LifeState), NULL);
}
