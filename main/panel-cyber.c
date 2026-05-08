#include <stdint.h>
#include <stdio.h>

#include "firefly-hollows.h"
#include "firefly-scene.h"

#include "panels.h"
#include "utils.h"


#define BAR_COUNT       (8)
#define BAR_WIDTH       (22)
#define BAR_GAP         (4)
#define BAR_AREA_LEFT   (16)
#define BAR_FLOOR_Y     (220)
#define BAR_HEIGHT_MAX  (150)

#define SCAN_SPEED_MS   (12)


typedef struct CyberState {
    FfxScene scene;
    FfxNode bars[BAR_COUNT];
    FfxNode floor;
    FfxNode scanline;
    FfxNode tickerDot;
} CyberState;


static const uint16_t barPeriods[BAR_COUNT] = {
    1100, 870, 1450, 720, 1230, 980, 1610, 830
};

static const uint16_t barPhases[BAR_COUNT] = {
    0, 200, 450, 730, 1000, 1300, 1500, 1850
};


static uint32_t triangleWave(uint32_t t, uint32_t period, uint32_t phase,
  uint32_t maxVal) {
    uint32_t pos = (t + phase) % period;
    uint32_t half = period / 2;
    if (pos < half) { return (pos * maxVal) / half; }
    return maxVal - ((pos - half) * maxVal) / half;
}

static void neonBarColor(int idx, uint8_t *r, uint8_t *g, uint8_t *b) {
    static const uint8_t palette[BAR_COUNT][3] = {
        {   0, 255,  65 }, // matrix green
        {   0, 255, 255 }, // cyan
        { 255,   0, 200 }, // hot pink
        { 255, 220,   0 }, // amber
        { 120,   0, 255 }, // violet
        { 255,  80,   0 }, // neon orange
        {   0, 140, 255 }, // electric blue
        { 255,   0,  80 }, // blood red
    };
    *r = palette[idx][0];
    *g = palette[idx][1];
    *b = palette[idx][2];
}

static void onRender(FfxEvent event, FfxEventProps props, void *_state) {
    CyberState *state = _state;
    uint32_t t = ticks();

    for (int i = 0; i < BAR_COUNT; i++) {
        uint32_t h = triangleWave(t, barPeriods[i], barPhases[i],
          BAR_HEIGHT_MAX);
        int32_t x = BAR_AREA_LEFT + i * (BAR_WIDTH + BAR_GAP);
        int32_t y = BAR_FLOOR_Y - h;
        ffx_sceneNode_setPosition(state->bars[i], ffx_point(x, y));
    }

    int32_t sy = (t / SCAN_SPEED_MS) % 240;
    ffx_sceneNode_setPosition(state->scanline, ffx_point(0, sy));

    int32_t tx = (t / 20) % 240;
    ffx_sceneNode_setPosition(state->tickerDot, ffx_point(tx, 218));
}

static void onKeys(FfxEvent event, FfxEventProps props, void *_state) {
    if (props.keys.down & FfxKeyCancel) {
        ffx_popPanel(0);
    }
}

static int initFunc(FfxScene scene, FfxNode panel, void *_state, void *arg) {
    CyberState *state = _state;
    state->scene = scene;

    FfxNode bg = ffx_scene_createBox(scene, ffx_size(240, 240));
    ffx_sceneBox_setColor(bg, ffx_color_rgb(8, 0, 16));
    ffx_sceneGroup_appendChild(panel, bg);
    ffx_sceneNode_setPosition(bg, ffx_point(0, 0));

    FfxNode title = ffx_scene_createLabel(scene, FfxFontLargeBold,
      "CYBER//PULSE");
    ffx_sceneGroup_appendChild(panel, title);
    ffx_sceneNode_setPosition(title, ffx_point(120, 22));
    ffx_sceneLabel_setAlign(title, FfxTextAlignCenter | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(title, ffx_color_rgb(0, 0, 0));

    FfxNode subtitle = ffx_scene_createLabel(scene, FfxFontMedium,
      "BPM 128 // SYNC OK");
    ffx_sceneGroup_appendChild(panel, subtitle);
    ffx_sceneNode_setPosition(subtitle, ffx_point(120, 42));
    ffx_sceneLabel_setAlign(subtitle, FfxTextAlignCenter | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(subtitle, ffx_color_rgb(0, 0, 0));

    FfxNode floor = ffx_scene_createBox(scene, ffx_size(240, 2));
    ffx_sceneBox_setColor(floor, ffx_color_rgb(0, 255, 65));
    ffx_sceneGroup_appendChild(panel, floor);
    ffx_sceneNode_setPosition(floor, ffx_point(0, BAR_FLOOR_Y));
    state->floor = floor;

    for (int i = 0; i < BAR_COUNT; i++) {
        FfxNode bar = ffx_scene_createBox(scene,
          ffx_size(BAR_WIDTH, BAR_HEIGHT_MAX));
        uint8_t r, g, b;
        neonBarColor(i, &r, &g, &b);
        ffx_sceneBox_setColor(bar, ffx_color_rgb(r, g, b));
        ffx_sceneGroup_appendChild(panel, bar);
        int32_t x = BAR_AREA_LEFT + i * (BAR_WIDTH + BAR_GAP);
        ffx_sceneNode_setPosition(bar, ffx_point(x, BAR_FLOOR_Y));
        state->bars[i] = bar;
    }

    FfxNode scanline = ffx_scene_createBox(scene, ffx_size(240, 2));
    ffx_sceneBox_setColor(scanline, ffx_color_rgb(180, 255, 220));
    ffx_sceneBox_setOpacity(scanline, 8);
    ffx_sceneGroup_appendChild(panel, scanline);
    state->scanline = scanline;

    FfxNode tickerDot = ffx_scene_createBox(scene, ffx_size(3, 3));
    ffx_sceneBox_setColor(tickerDot, ffx_color_rgb(0, 255, 255));
    ffx_sceneGroup_appendChild(panel, tickerDot);
    state->tickerDot = tickerDot;

    FfxNode hint = ffx_scene_createLabel(scene, FfxFontMedium, "[CANCEL] EXIT");
    ffx_sceneGroup_appendChild(panel, hint);
    ffx_sceneNode_setPosition(hint, ffx_point(120, 232));
    ffx_sceneLabel_setAlign(hint, FfxTextAlignCenter | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(hint, ffx_color_rgb(0, 0, 0));

    ffx_onEvent(FfxEventKeys, onKeys, state);
    ffx_onEvent(FfxEventRenderScene, onRender, state);

    return 0;
}

int pushPanelCyber() {
    return ffx_pushPanel(initFunc, sizeof(CyberState), NULL);
}
