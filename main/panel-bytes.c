#include <stdint.h>
#include <stdio.h>

#include "firefly-hollows.h"
#include "firefly-scene.h"

#include "panels.h"
#include "utils.h"


#define COL_COUNT       (12)
#define TRAIL_LEN       (10)
#define DOT_SIZE        (10)
#define COL_STRIDE      (20)
#define COL_OFFSET      ((240 - (COL_COUNT * COL_STRIDE)) / 2)
#define DOT_STRIDE      (16)

#define TOTAL_RANGE     (240 + TRAIL_LEN * DOT_STRIDE)


typedef struct ByteCol {
    uint16_t period;
    uint16_t phase;
} ByteCol;

typedef struct BytesState {
    FfxScene scene;
    FfxNode dots[COL_COUNT * TRAIL_LEN];
    FfxNode glyphs[COL_COUNT];
    ByteCol cols[COL_COUNT];
    uint32_t rngState;
} BytesState;


static uint32_t rngNext(uint32_t *s) {
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x ? x : 0xC0FFEE42;
    return *s;
}

static void onRender(FfxEvent event, FfxEventProps props, void *_state) {
    BytesState *state = _state;
    uint32_t t = ticks();

    for (int c = 0; c < COL_COUNT; c++) {
        ByteCol *col = &state->cols[c];
        int32_t headY = (int32_t)(((t + col->phase) / col->period) % TOTAL_RANGE);
        int32_t x = COL_OFFSET + c * COL_STRIDE;

        for (int i = 0; i < TRAIL_LEN; i++) {
            int32_t y = headY - i * DOT_STRIDE;
            FfxNode dot = state->dots[c * TRAIL_LEN + i];
            if (y < -DOT_SIZE || y > 240) {
                ffx_sceneNode_setHidden(dot, true);
                continue;
            }
            ffx_sceneNode_setHidden(dot, false);
            ffx_sceneNode_setPosition(dot, ffx_point(x, y));
        }

        FfxNode glyph = state->glyphs[c];
        int32_t gy = headY - 5;
        if (gy < 0 || gy > 240) {
            ffx_sceneNode_setHidden(glyph, true);
        } else {
            ffx_sceneNode_setHidden(glyph, false);
            ffx_sceneNode_setPosition(glyph, ffx_point(x + DOT_SIZE / 2, gy));
        }
    }
}

static void onKeys(FfxEvent event, FfxEventProps props, void *_state) {
    if (props.keys.down & FfxKeyCancel) {
        ffx_popPanel(0);
    }
}

static int initFunc(FfxScene scene, FfxNode panel, void *_state, void *arg) {
    BytesState *state = _state;
    state->scene = scene;
    state->rngState = 0x13371337 ^ ticks();

    FfxNode bg = ffx_scene_createBox(scene, ffx_size(240, 240));
    ffx_sceneBox_setColor(bg, COLOR_BLACK);
    ffx_sceneGroup_appendChild(panel, bg);
    ffx_sceneNode_setPosition(bg, ffx_point(0, 0));

    static const char *glyphChars[8] = { "0", "1", "X", "F", "A", "Z", "7", "$" };

    for (int c = 0; c < COL_COUNT; c++) {
        ByteCol *col = &state->cols[c];
        col->period = 6 + (rngNext(&state->rngState) % 12);
        col->phase = rngNext(&state->rngState) % 4096;

        for (int i = 0; i < TRAIL_LEN; i++) {
            FfxNode dot = ffx_scene_createBox(scene,
              ffx_size(DOT_SIZE, DOT_SIZE));
            uint8_t opacity;
            color_ffxt color;
            if (i == 0) {
                color = ffx_color_rgb(220, 255, 220);
                opacity = MAX_OPACITY;
            } else {
                color = ffx_color_rgb(0, 255, 65);
                int fade = MAX_OPACITY - (i * MAX_OPACITY / TRAIL_LEN);
                if (fade < 2) { fade = 2; }
                opacity = (uint8_t)fade;
            }
            ffx_sceneBox_setColor(dot, color);
            ffx_sceneBox_setOpacity(dot, opacity);
            ffx_sceneGroup_appendChild(panel, dot);
            ffx_sceneNode_setPosition(dot, ffx_point(0, -32));
            state->dots[c * TRAIL_LEN + i] = dot;
        }

        const char *glyph = glyphChars[rngNext(&state->rngState) & 7];
        FfxNode g = ffx_scene_createLabel(scene, FfxFontMedium, glyph);
        ffx_sceneGroup_appendChild(panel, g);
        ffx_sceneNode_setPosition(g, ffx_point(0, -32));
        ffx_sceneLabel_setAlign(g, FfxTextAlignCenter | FfxTextAlignMiddle);
        ffx_sceneLabel_setOutlineColor(g, COLOR_BLACK);
        state->glyphs[c] = g;
    }

    FfxNode title = ffx_scene_createLabel(scene, FfxFontLargeBold,
      "BYTE//STREAM");
    ffx_sceneGroup_appendChild(panel, title);
    ffx_sceneNode_setPosition(title, ffx_point(120, 18));
    ffx_sceneLabel_setAlign(title, FfxTextAlignCenter | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(title, COLOR_BLACK);

    FfxNode hint = ffx_scene_createLabel(scene, FfxFontMedium, "[CANCEL] EXIT");
    ffx_sceneGroup_appendChild(panel, hint);
    ffx_sceneNode_setPosition(hint, ffx_point(120, 228));
    ffx_sceneLabel_setAlign(hint, FfxTextAlignCenter | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(hint, COLOR_BLACK);

    ffx_onEvent(FfxEventKeys, onKeys, state);
    ffx_onEvent(FfxEventRenderScene, onRender, state);

    return 0;
}

int pushPanelBytes() {
    return ffx_pushPanel(initFunc, sizeof(BytesState), NULL);
}
