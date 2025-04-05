#ifndef __PIXELS_H__
#define __PIXELS_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>

#include "firefly-color.h"

#define PIXEL_COUNT   (4)


// Pixels Context Object (opaque; do not inspect or rely on internals)
typedef void* PixelsContext;

typedef void (*PixelAnimationFunc)(color_ffxt *output, size_t count,
  fixed_ffxt t, void *arg);

/**
 *  Initialize a Pixel Context connected to the data %%pin%%.
 */
PixelsContext pixels_init(size_t pixelCount, uint32_t pin);

/**
 *  Release the Pixel Context.
 */
void pixels_free(PixelsContext context);

/**
 *  Advance all animations to the current time.
 */
void pixels_tick(PixelsContext context);

/**
 *  Set the %%pixel%% to %%color%%.
 */
void pixels_setPixel(PixelsContext context, uint32_t pixel, color_ffxt color);

/**
 *  Animate the %%pixel%% with the %%pixelFunc%%, over %%duration%%,
 *  with optionally %%repeat%%.
 */
void pixels_animatePixel(PixelsContext context, uint32_t pixel,
  PixelAnimationFunc pixelFunc, uint32_t duration, uint32_t repeat,
  void *arg);

// Create ffx_color_lerpColorRamp(color_ffxt *colors, uint32_t count,
//   fixed_ffxt t)

/**
 *  Run the %%pixelFunc%% from time 0 until duration, allowing the
 *  entire pixel array to be animated as a single output.
 */
void pixels_animate(PixelsContext context, PixelAnimationFunc pixelFunc,
  uint32_t duration, uint32_t repeat, void *arg);

/**
 *  Stop any animations. If %%final%% is non-zero, then the
 *  animate will get called with a ``t=1``, otherwise the
 *  animation stops an the current color is preserved.
 */
void pixels_stopAnimation(PixelsContext context, uint32_t final);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PIXELS_H__ */
