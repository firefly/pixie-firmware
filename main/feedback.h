// Shared button-feedback helpers used by every panel.

#ifndef __PIXIE_FEEDBACK_H__
#define __PIXIE_FEEDBACK_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "firefly-hollows.h"
#include "firefly-scene.h"


// Brief low-brightness flash on the LED that corresponds to the
// pressed button: North=LED0, OK=LED1, Cancel=LED2, South=LED3.
// Safe to call from any panel's onKeys; a no-op if `keys` is 0.
//
// The flash only fires while the LED Mode panel's selected mode is
// OFF (the default at boot). Once the user picks any other LED mode
// the feedback stays out of its way; selecting OFF re-enables it.
void feedback_onKey(FfxKeys keys);


// Tell the feedback module whether the LED Mode panel is currently
// set to OFF. Called from panel-leds when the user picks a mode.
void feedback_setLedOffMode(bool off);


// Add a small bottom-of-screen legend with one short label per
// button. Order on screen matches the device's physical button
// row, left to right: SW4(DOWN) SW3(UP) SW2(OK) SW1(ESC).
//
// The strings can be either button names or action verbs - the
// helper just space-joins them. Pass NULL for an unused slot.
//
// Examples:
//   menu: feedback_addButtonLegend(panel, "UP", "DOWN", "OK", "ESC")
//         -> DOWN UP OK ESC
//   game: feedback_addButtonLegend(panel, "UP", "DOWN", "HOLD", "FIRE")
//         -> DOWN UP HOLD FIRE
void feedback_addButtonLegend(FfxNode panel,
  const char *upText, const char *downText,
  const char *okText, const char *cancelText);


// Lightweight running FPS counter pinned to the top-left of a panel.
// Each panel that wants one keeps the FpsCounter struct in its state,
// calls feedback_addFpsCounter() from its init function, and calls
// feedback_tickFps() once per render frame. The label updates ~once
// per second.
typedef struct FpsCounter {
    FfxNode label;
    uint32_t windowStart;
    uint32_t frames;
} FpsCounter;

void feedback_addFpsCounter(FpsCounter *fps, FfxNode panel);
void feedback_tickFps(FpsCounter *fps);


#ifdef __cplusplus
}
#endif

#endif  /* __PIXIE_FEEDBACK_H__ */
