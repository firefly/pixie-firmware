// LED button-press feedback + on-screen button legend.
//
// Each press flashes the LED next to the pressed button briefly at
// low brightness. Uses the firefly-hollows pixels API; symbols are
// forward-declared inline since they live in the component's
// private src/ header. (Same pattern as panel-leds.c.)

#include <stdint.h>
#include <stdio.h>

#include "firefly-color.h"
#include "firefly-fixed.h"
#include "firefly-scene.h"

#include "feedback.h"


typedef void* PixelsContext;
typedef void (*PixelAnimationFunc)(color_ffxt *output, size_t count,
  fixed_ffxt t, void *arg);
extern PixelsContext pixels;
extern void pixels_animatePixel(PixelsContext context, uint32_t pixel,
  PixelAnimationFunc fn, uint32_t duration, uint32_t repeat, void *arg);


#define FEEDBACK_DURATION_MS    (140)
#define FEEDBACK_PEAK           (5)   // ~10% of the original 48-step peak


static bool ledOffMode = true;  // assume off until the LED panel says otherwise


static void flashAnim(color_ffxt *out, size_t count, fixed_ffxt t,
  void *arg) {
    // First half holds at the (very dim) peak, second half fades to
    // off so the LED clears itself if no other animation runs after.
    if (t < FM_1 / 2) {
        out[0] = ffx_color_rgb(FEEDBACK_PEAK, FEEDBACK_PEAK,
          FEEDBACK_PEAK + 1);
    } else {
        int32_t v = scalarfx(FEEDBACK_PEAK, FM_1 - t);
        if (v < 0) { v = 0; }
        out[0] = ffx_color_rgb((uint8_t)v, (uint8_t)v, (uint8_t)(v + 1));
    }
}


void feedback_setLedOffMode(bool off) {
    ledOffMode = off;
}


void feedback_onKey(FfxKeys keys) {
    if (!ledOffMode) { return; }
    // The four on-board LEDs are numbered right-to-left when the
    // device is held with the screen up - LED 0 is above SW1 (the
    // rightmost button), LED 3 is above SW4 (the leftmost). With
    // the button row left-to-right being SW4..SW1 = DOWN UP OK ESC
    // that gives the per-key mapping:
    //     Cancel (ESC, SW1)  -> LED 0
    //     OK            (SW2)-> LED 1
    //     North (UP,    SW3) -> LED 2
    //     South (DOWN,  SW4) -> LED 3
    int led = -1;
    if      (keys & FfxKeyCancel) { led = 0; }
    else if (keys & FfxKeyOk)     { led = 1; }
    else if (keys & FfxKeyNorth)  { led = 2; }
    else if (keys & FfxKeySouth)  { led = 3; }
    if (led < 0) { return; }
    // repeat=0 makes this a one-shot animation; the LED lands on the
    // animation's final color and stays there until something else
    // overrides it.
    pixels_animatePixel(pixels, (uint32_t)led, flashAnim,
      FEEDBACK_DURATION_MS, 0, NULL);
}


void feedback_addButtonLegend(FfxNode panel,
  const char *upText, const char *downText,
  const char *okText, const char *cancelText) {

    if (!upText)     { upText = ""; }
    if (!downText)   { downText = ""; }
    if (!okText)     { okText = ""; }
    if (!cancelText) { cancelText = ""; }

    FfxScene scene = ffx_sceneNode_getScene(panel);

    // Just space-join the four labels in physical button order
    // (left-to-right SW4..SW1: DOWN UP OK ESC).
    char buf[48];
    snprintf(buf, sizeof(buf), "%s %s %s %s",
      downText, upText, okText, cancelText);

    FfxNode label = ffx_scene_createLabel(scene, FfxFontSmall, buf);
    ffx_sceneGroup_appendChild(panel, label);
    ffx_sceneNode_setPosition(label, ffx_point(120, 232));
    ffx_sceneLabel_setAlign(label,
      FfxTextAlignCenter | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(label, COLOR_BLACK);
}


////////////////////////////////
// FPS counter

#include "utils.h"

void feedback_addFpsCounter(FpsCounter *fps, FfxNode panel) {
    FfxScene scene = ffx_sceneNode_getScene(panel);
    fps->label = ffx_scene_createLabel(scene, FfxFontSmall, "");
    ffx_sceneGroup_appendChild(panel, fps->label);
    ffx_sceneNode_setPosition(fps->label, ffx_point(4, 10));
    ffx_sceneLabel_setAlign(fps->label,
      FfxTextAlignLeft | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(fps->label, COLOR_BLACK);
    fps->windowStart = ticks();
    fps->frames = 0;
}

void feedback_tickFps(FpsCounter *fps) {
    if (!fps || !fps->label) { return; }
    fps->frames++;
    uint32_t t = ticks();
    uint32_t window = t - fps->windowStart;
    if (window >= 1000) {
        uint32_t fpsValue = (fps->frames * 1000) / window;
        ffx_sceneLabel_setTextFormat(fps->label, "%lu fps",
          (unsigned long)fpsValue);
        fps->windowStart = t;
        fps->frames = 0;
    }
}
