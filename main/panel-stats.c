#include <stdint.h>
#include <stdio.h>

#include "esp_chip_info.h"
#include "esp_idf_version.h"
#include "esp_system.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "firefly-hollows.h"
#include "firefly-scene.h"

#include "panels.h"
#include "utils.h"


#ifndef GIT_COMMIT
#define GIT_COMMIT  ("unknown")
#endif


#define HISTORY_LEN     (60)
#define BAR_AREA_LEFT   (20)
#define BAR_AREA_TOP    (170)
#define BAR_HEIGHT_MAX  (50)


typedef struct StatsState {
    FfxScene scene;

    FfxNode heapHistory[HISTORY_LEN];
    FfxNode heapValueLabel;
    FfxNode uptimeValueLabel;
    FfxNode fpsValueLabel;

    uint32_t bootTime;
    uint32_t nextSampleAt;
    int historyHead;
    uint32_t initialFreeHeap;

    uint32_t fpsWindowStart;
    uint32_t fpsFrames;
} StatsState;


static FfxNode addLabel(FfxScene scene, FfxNode parent, const char *text,
  int32_t x, int32_t y, FfxFont font, uint32_t align) {
    FfxNode label = ffx_scene_createLabel(scene, font, text);
    ffx_sceneGroup_appendChild(parent, label);
    ffx_sceneNode_setPosition(label, ffx_point(x, y));
    ffx_sceneLabel_setAlign(label, align);
    ffx_sceneLabel_setOutlineColor(label, COLOR_BLACK);
    return label;
}

static void renderHeapBar(StatsState *state, int slot, uint32_t freeBytes) {
    if (state->initialFreeHeap == 0) {
        state->initialFreeHeap = freeBytes;
    }
    uint32_t scale = state->initialFreeHeap;
    if (scale < freeBytes) { scale = freeBytes; }
    if (scale == 0) { scale = 1; }

    int32_t h = (int32_t)((freeBytes * BAR_HEIGHT_MAX) / scale);
    if (h < 1) { h = 1; }
    if (h > BAR_HEIGHT_MAX) { h = BAR_HEIGHT_MAX; }

    int32_t x = BAR_AREA_LEFT + slot * 3;
    int32_t y = BAR_AREA_TOP + (BAR_HEIGHT_MAX - h);
    ffx_sceneNode_setPosition(state->heapHistory[slot], ffx_point(x, y));
}

static void onRender(FfxEvent event, FfxEventProps props, void *_state) {
    StatsState *state = _state;

    state->fpsFrames++;

    uint32_t t = ticks();
    if (t < state->nextSampleAt) { return; }
    state->nextSampleAt = t + 250;

    uint32_t freeHeap = esp_get_free_heap_size();
    renderHeapBar(state, state->historyHead, freeHeap);
    state->historyHead = (state->historyHead + 1) % HISTORY_LEN;

    ffx_sceneLabel_setTextFormat(state->heapValueLabel, "%lu KB",
      (unsigned long)(freeHeap / 1024));

    uint32_t uptimeMs = t - state->bootTime;
    uint32_t secs = uptimeMs / 1000;
    ffx_sceneLabel_setTextFormat(state->uptimeValueLabel, "%02lu:%02lu:%02lu",
      (unsigned long)(secs / 3600),
      (unsigned long)((secs / 60) % 60),
      (unsigned long)(secs % 60));

    uint32_t windowMs = t - state->fpsWindowStart;
    if (windowMs >= 1000) {
        uint32_t fps = (state->fpsFrames * 1000) / windowMs;
        ffx_sceneLabel_setTextFormat(state->fpsValueLabel, "%lu FPS",
          (unsigned long)fps);
        state->fpsWindowStart = t;
        state->fpsFrames = 0;
    }
}

static void onKeys(FfxEvent event, FfxEventProps props, void *_state) {
    if (props.keys.down & FfxKeyCancel) {
        ffx_popPanel(0);
    }
}

static int initFunc(FfxScene scene, FfxNode panel, void *_state, void *arg) {
    StatsState *state = _state;
    state->scene = scene;
    state->bootTime = ticks();
    state->fpsWindowStart = state->bootTime;

    FfxNode bg = ffx_scene_createBox(scene, ffx_size(240, 240));
    ffx_sceneBox_setColor(bg, ffx_color_rgb(0, 8, 12));
    ffx_sceneGroup_appendChild(panel, bg);
    ffx_sceneNode_setPosition(bg, ffx_point(0, 0));

    addLabel(scene, panel, "SYS//STATS", 120, 16, FfxFontLargeBold,
      FfxTextAlignCenter | FfxTextAlignMiddle);

    addLabel(scene, panel, "CHIP", 14, 50, FfxFontMedium,
      FfxTextAlignLeft | FfxTextAlignMiddle);
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    static char chipBuf[32];
    snprintf(chipBuf, sizeof(chipBuf), "ESP32-C3 r%d  %dC",
      (int)chip.revision, (int)chip.cores);
    addLabel(scene, panel, chipBuf, 226, 50, FfxFontMedium,
      FfxTextAlignRight | FfxTextAlignMiddle);

    addLabel(scene, panel, "IDF", 14, 70, FfxFontMedium,
      FfxTextAlignLeft | FfxTextAlignMiddle);
    addLabel(scene, panel, esp_get_idf_version(), 226, 70, FfxFontMedium,
      FfxTextAlignRight | FfxTextAlignMiddle);

    addLabel(scene, panel, "BUILD", 14, 90, FfxFontMedium,
      FfxTextAlignLeft | FfxTextAlignMiddle);
    addLabel(scene, panel, GIT_COMMIT, 226, 90, FfxFontMedium,
      FfxTextAlignRight | FfxTextAlignMiddle);

    addLabel(scene, panel, "UPTIME", 14, 110, FfxFontMedium,
      FfxTextAlignLeft | FfxTextAlignMiddle);
    state->uptimeValueLabel = addLabel(scene, panel, "00:00:00", 226, 110,
      FfxFontMedium, FfxTextAlignRight | FfxTextAlignMiddle);

    addLabel(scene, panel, "HEAP", 14, 130, FfxFontMedium,
      FfxTextAlignLeft | FfxTextAlignMiddle);
    state->heapValueLabel = addLabel(scene, panel, "-- KB", 226, 130,
      FfxFontMedium, FfxTextAlignRight | FfxTextAlignMiddle);

    addLabel(scene, panel, "RENDER", 14, 150, FfxFontMedium,
      FfxTextAlignLeft | FfxTextAlignMiddle);
    state->fpsValueLabel = addLabel(scene, panel, "-- FPS", 226, 150,
      FfxFontMedium, FfxTextAlignRight | FfxTextAlignMiddle);

    for (int i = 0; i < HISTORY_LEN; i++) {
        FfxNode bar = ffx_scene_createBox(scene, ffx_size(2, BAR_HEIGHT_MAX));
        ffx_sceneBox_setColor(bar, ffx_color_rgb(0, 200, 80));
        ffx_sceneGroup_appendChild(panel, bar);
        ffx_sceneNode_setPosition(bar,
          ffx_point(BAR_AREA_LEFT + i * 3, BAR_AREA_TOP + BAR_HEIGHT_MAX));
        state->heapHistory[i] = bar;
    }

    addLabel(scene, panel, "[CANCEL] EXIT", 120, 232, FfxFontMedium,
      FfxTextAlignCenter | FfxTextAlignMiddle);

    state->nextSampleAt = state->bootTime;

    ffx_onEvent(FfxEventKeys, onKeys, state);
    ffx_onEvent(FfxEventRenderScene, onRender, state);

    return 0;
}

int pushPanelStats() {
    return ffx_pushPanel(initFunc, sizeof(StatsState), NULL);
}
