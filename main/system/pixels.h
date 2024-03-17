#ifndef __PIXELS_H__
#define __PIXELS_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>

#include "./color.h"

#define PIXEL_COUNT    (1)

// Keypad Context Object (opaque; do not inspect or rely on internals)
typedef void* PixelsContext;

PixelsContext pixels_init(uint32_t pin);
void pixels_free(PixelsContext context);

void pixels_tick(PixelsContext context);

// Set (and hold) the color
void pixels_setColor(PixelsContext context, uint32_t index, color_t color);

// Animate the color through the color ramp over duration millis, optionally repeating
void pixels_animateColor(PixelsContext context, uint32_t index, color_t* colorRamp, uint32_t colorCount, uint32_t duration, uint32_t repeat);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PIXELS_H__ */
