// LED mode picker. Drives the 4 WS2812B LEDs via firefly-hollows'
// internal pixels API. Each mode is a PixelAnimationFunc bound to
// each pixel via pixels_animatePixel; the most recently selected
// mode loops forever in the IO task and persists after the panel
// is popped.

#include <stdint.h>
#include <stdio.h>

#include "firefly-color.h"
#include "firefly-fixed.h"
#include "firefly-hollows.h"
#include "firefly-scene.h"

#include "panels.h"
#include "utils.h"


#define MODE_COUNT      (12)
#define LED_COUNT       (4)


// firefly-hollows' pixels API lives in src/ (private to the
// component). Forward-declare what we need; the linker resolves to
// the same symbols task-io.c uses.
//
// Note: pixels.h declares pixels_animate and pixels_stopAnimation but
// pixels.c at the pinned commit only implements pixels_animatePixel.
// Animation functions write only out[0] - the pixel index travels
// through `arg`. Pass repeat=1 so the animation loops forever
// (repeat=0 in the implementation means one-shot, which is the
// opposite of what the parameter name suggests).
typedef void* PixelsContext;
typedef void (*PixelAnimationFunc)(color_ffxt *output, size_t count,
  fixed_ffxt t, void *arg);
extern PixelsContext pixels;
extern void pixels_animatePixel(PixelsContext context, uint32_t pixel,
  PixelAnimationFunc fn, uint32_t duration, uint32_t repeat, void *arg);


typedef struct LedsState {
    FfxScene scene;
    FfxNode bg;
    FfxNode titleLabel;
    FfxNode modeLabel;
    FfxNode descLabel;
    FfxNode dots[MODE_COUNT];
    FfxNode hint;

    int mode;
} LedsState;


static const char *modeNames[MODE_COUNT] = {
    "OFF",     "CYAN",   "RAINBOW", "PULSE",
    "STROBE",  "POLICE", "MATRIX",  "FIRE",
    "COMET",   "OCEAN",  "NEON",    "SPARKLE"
};
static const char *modeDescs[MODE_COUNT] = {
    "all off",
    "solid neon cyan",
    "cycling hues",
    "violet breathing",
    "1 Hz white flash",
    "alternating red/blue",
    "green wave",
    "red/orange flicker",
    "yellow chaser",
    "blue/cyan ripple",
    "magenta/cyan/yellow",
    "random twinkle"
};
static const uint32_t modeDurations[MODE_COUNT] = {
    1000,  // OFF
    1000,  // CYAN
    4000,  // RAINBOW
    1800,  // PULSE
    1000,  // STROBE
    800,   // POLICE
    1200,  // MATRIX
    400,   // FIRE
    900,   // COMET
    3000,  // OCEAN
    2000,  // NEON
    600    // SPARKLE
};


static int pixIndex(void *arg) { return (int)(uintptr_t)arg; }


static void animOff(color_ffxt *out, size_t count, fixed_ffxt t, void *arg) {
    out[0] = COLOR_BLACK;
}

static void animCyan(color_ffxt *out, size_t count, fixed_ffxt t, void *arg) {
    out[0] = ffx_color_rgb(0, 220, 240);
}

static void animRainbow(color_ffxt *out, size_t count, fixed_ffxt t,
  void *arg) {
    int32_t base = scalarfx(3960, t);
    int32_t hue = (base + pixIndex(arg) * 990) % 3960;
    out[0] = ffx_color_hsv(hue, MAX_SAT, MAX_VAL);
}

static void animPulse(color_ffxt *out, size_t count, fixed_ffxt t, void *arg) {
    int32_t v;
    fixed_ffxt half = FM_1 / 2;
    if (t < half) { v = scalarfx(MAX_VAL, mulfx(t, tofx(2))); }
    else          { v = scalarfx(MAX_VAL, mulfx(FM_1 - t, tofx(2))); }
    out[0] = ffx_color_hsv(275, MAX_SAT, v);
}

static void animStrobe(color_ffxt *out, size_t count, fixed_ffxt t,
  void *arg) {
    bool on = t < (FM_1 / 4);
    out[0] = on ? ffx_color_rgb(255, 255, 255) : COLOR_BLACK;
}

static void animPolice(color_ffxt *out, size_t count, fixed_ffxt t,
  void *arg) {
    bool phase = t < (FM_1 / 2);
    bool side = (pixIndex(arg) & 1) ^ phase;
    out[0] = side ? ffx_color_rgb(255, 0, 0) : ffx_color_rgb(0, 0, 255);
}

static void animMatrix(color_ffxt *out, size_t count, fixed_ffxt t,
  void *arg) {
    int i = pixIndex(arg);
    int32_t head = scalarfx(LED_COUNT * 2, t);
    int32_t dist = (head - i + LED_COUNT * 2) % (LED_COUNT * 2);
    int32_t v = (dist < LED_COUNT)
      ? (MAX_VAL - dist * (MAX_VAL / LED_COUNT))
      : 0;
    if (v < 0) { v = 0; }
    out[0] = ffx_color_hsv(180, MAX_SAT, v);
}

// Cheap per-pixel pseudo-random hash for organic effects.
static uint32_t hash32(uint32_t x) {
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x ? x : 0xA5C0FFEE;
}

static void animFire(color_ffxt *out, size_t count, fixed_ffxt t, void *arg) {
    int i = pixIndex(arg);
    uint32_t phase = (uint32_t)(t >> 11);
    uint32_t s = hash32(phase * 31u + (uint32_t)i * 0x9E3779B9u);
    int32_t hue = (int32_t)(s % 350);
    int32_t v = MAX_VAL - (int32_t)(s & 0x1f);
    if (v < 12) { v = 12; }
    out[0] = ffx_color_hsv(hue, MAX_SAT, v);
}

static void animComet(color_ffxt *out, size_t count, fixed_ffxt t, void *arg) {
    int i = pixIndex(arg);
    int32_t head = scalarfx(LED_COUNT, t);
    int32_t dist = (head - i + LED_COUNT) % LED_COUNT;
    int32_t v;
    switch (dist) {
        case 0:  v = MAX_VAL; break;
        case 1:  v = MAX_VAL / 3; break;
        case 2:  v = MAX_VAL / 8; break;
        default: v = 0; break;
    }
    out[0] = ffx_color_hsv(660, MAX_SAT, v);
}

static void animOcean(color_ffxt *out, size_t count, fixed_ffxt t, void *arg) {
    int i = pixIndex(arg);
    int32_t base = scalarfx(700, t) + i * 200;
    int32_t hue = 1980 + (base % 700);
    out[0] = ffx_color_hsv(hue, MAX_SAT, MAX_VAL - 8);
}

static void animNeon(color_ffxt *out, size_t count, fixed_ffxt t, void *arg) {
    int i = pixIndex(arg);
    int phase = (int)((t * 4) >> 16);
    static const int32_t hues[4] = { 3300, 1980, 660, 1320 };
    int32_t hue = hues[(phase + i) & 3];
    out[0] = ffx_color_hsv(hue, MAX_SAT, MAX_VAL);
}

static void animSparkle(color_ffxt *out, size_t count, fixed_ffxt t,
  void *arg) {
    int i = pixIndex(arg);
    uint32_t phase = (uint32_t)(t >> 13);
    uint32_t s = hash32(phase * 17u + (uint32_t)i * 0x85EBCA77u);
    bool on = (s & 0xf) < 3;
    int32_t hue = (int32_t)(s % 3960);
    out[0] = on ? ffx_color_hsv(hue, MAX_SAT, MAX_VAL) : COLOR_BLACK;
}


static const PixelAnimationFunc modeFuncs[MODE_COUNT] = {
    animOff,    animCyan,   animRainbow, animPulse,
    animStrobe, animPolice, animMatrix,  animFire,
    animComet,  animOcean,  animNeon,    animSparkle
};


static void applyMode(LedsState *state) {
    PixelAnimationFunc fn = modeFuncs[state->mode];
    uint32_t dur = modeDurations[state->mode];
    // repeat=1 means loop forever (repeat=0 is one-shot in this impl)
    for (int i = 0; i < LED_COUNT; i++) {
        pixels_animatePixel(pixels, i, fn, dur, 1, (void*)(uintptr_t)i);
    }

    ffx_sceneLabel_setText(state->modeLabel, modeNames[state->mode]);
    ffx_sceneLabel_setText(state->descLabel, modeDescs[state->mode]);
    for (int i = 0; i < MODE_COUNT; i++) {
        ffx_sceneBox_setColor(state->dots[i],
          i == state->mode ? ffx_color_rgb(0, 255, 200)
                           : ffx_color_rgb(40, 40, 60));
    }
}

static void onKeys(FfxEvent event, FfxEventProps props, void *_state) {
    LedsState *state = _state;
    switch (props.keys.down) {
        case FfxKeyCancel:
            ffx_popPanel(0);
            return;
        case FfxKeyNorth:
            state->mode = (state->mode + MODE_COUNT - 1) % MODE_COUNT;
            applyMode(state);
            break;
        case FfxKeySouth:
            state->mode = (state->mode + 1) % MODE_COUNT;
            applyMode(state);
            break;
        case FfxKeyOk:
            applyMode(state);
            break;
    }
}

static int initFunc(FfxScene scene, FfxNode panel, void *_state, void *arg) {
    LedsState *state = _state;
    state->scene = scene;
    state->mode = 2;

    state->bg = ffx_scene_createBox(scene, ffx_size(240, 240));
    ffx_sceneBox_setColor(state->bg, ffx_color_rgb(4, 0, 12));
    ffx_sceneGroup_appendChild(panel, state->bg);
    ffx_sceneNode_setPosition(state->bg, ffx_point(0, 0));

    state->titleLabel = ffx_scene_createLabel(scene, FfxFontLargeBold,
      "LED MODE");
    ffx_sceneGroup_appendChild(panel, state->titleLabel);
    ffx_sceneNode_setPosition(state->titleLabel, ffx_point(120, 26));
    ffx_sceneLabel_setAlign(state->titleLabel,
      FfxTextAlignCenter | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(state->titleLabel, COLOR_BLACK);

    int dotW = 14;
    int dotH = 6;
    int gap  = 4;
    int totalW = MODE_COUNT * dotW + (MODE_COUNT - 1) * gap;
    int x0 = (240 - totalW) / 2;
    for (int i = 0; i < MODE_COUNT; i++) {
        FfxNode dot = ffx_scene_createBox(scene, ffx_size(dotW, dotH));
        ffx_sceneGroup_appendChild(panel, dot);
        ffx_sceneNode_setPosition(dot, ffx_point(x0 + i * (dotW + gap), 56));
        state->dots[i] = dot;
    }

    state->modeLabel = ffx_scene_createLabel(scene, FfxFontLargeBold, "");
    ffx_sceneGroup_appendChild(panel, state->modeLabel);
    ffx_sceneNode_setPosition(state->modeLabel, ffx_point(120, 116));
    ffx_sceneLabel_setAlign(state->modeLabel,
      FfxTextAlignCenter | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(state->modeLabel, COLOR_BLACK);

    state->descLabel = ffx_scene_createLabel(scene, FfxFontMedium, "");
    ffx_sceneGroup_appendChild(panel, state->descLabel);
    ffx_sceneNode_setPosition(state->descLabel, ffx_point(120, 156));
    ffx_sceneLabel_setAlign(state->descLabel,
      FfxTextAlignCenter | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(state->descLabel, COLOR_BLACK);

    state->hint = ffx_scene_createLabel(scene, FfxFontMedium,
      "N/S:MODE  X:EXIT (mode persists)");
    ffx_sceneGroup_appendChild(panel, state->hint);
    ffx_sceneNode_setPosition(state->hint, ffx_point(120, 230));
    ffx_sceneLabel_setAlign(state->hint,
      FfxTextAlignCenter | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(state->hint, COLOR_BLACK);

    applyMode(state);

    ffx_onEvent(FfxEventKeys, onKeys, state);

    return 0;
}

int pushPanelLeds() {
    return ffx_pushPanel(initFunc, sizeof(LedsState), NULL);
}
