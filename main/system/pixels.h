#ifndef __PIXELS_H__
#define __PIXELS_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>

#define PIXEL_COUNT    (1)


// Keypad Context Object (opaque; do not inspect or rely on internals)
typedef void* PixelsContext;

PixelsContext pixels_init();
void pixels_free(PixelsContext context);

// #define encodeRGB(r,g,b)      ((r ))
// #define encodeHSV(h,s,v)      ((((h) & 0x7fff) << 16) | (((s) & 0xff) << 8) | (((v) & 0xff)))

// Set (and hold) the color
void pixels_setRGB(PixelsContext context, uint32_t index, uint32_t rgb);
void pixels_setHSV(PixelsContext context, uint32_t index, uint32_t hsv);

// Animate the color through the color ramp over duration millis, optionally repeating
void pixels_animateRGB(PixelsContext context, uint32_t index, uint32_t* colorRamp, uint32_t colorCount, uint32_t duration, uint32_t repeat);
void pixels_animateHSV(PixelsContext context, uint32_t index, uint32_t* colorRamp, uint32_t colorCount, uint32_t duration, uint32_t repeat);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PIXELS_H__ */
