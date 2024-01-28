#ifndef __COLOR_H__
#define __COLOR_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>

#include "./curves.h"


typedef uint16_t rgb16_t;
typedef uint32_t rgba16_t;

typedef uint32_t rgb24_t;


// General color format that can be used to indicate RGBA and HSVA.
// - RGB: 8-bits per color, 6-bits alpha
// - HSV: hue=[-4095,4095], 6-bits for saturation, value and alpha
typedef uint32_t color_t;

// Stores RGB565 as a uint16_t
#define RGB16(r,g,b)     ((((r) & 0xf8) << 8) | (((g) & 0xfc) << 3) | (((b) & 0xf8) >> 3))

// Stores RGB888 as a uint32_t
#define RGB24(r,g,b)    ((rgb24_t)(((r) << 16) | ((g) << 8) | (b)))

// Extract the alpha of a color_t
#define ALPHA(c)       (0x3f - (((c) >> 24) & 0x3f))
#define MAX_ALPHA      (0x3f)



// Stores the 0ARGB8565 as a uint32_t
//#define RGBA16(r,g,b,a)  (((uint32_t)RGB16((r),(g),(b))) | ((0xff - ((a) & 0xff)) << 16))

// A color which darkens pixels under it by 50%. It can be
// used for a more optimized algorithm
//#define RGB_DARK50       (RGBA16(0,0,0,128))


//color_t colorHSV(int32_t hue, int32_t saturation, int32_t value);
//color_t colorHSVA(int32_t hue, int32_t saturation, int32_t value, int32_t alpha);

//color_t color16HSV(int32_t hue, int32_t saturation, int32_t value);
//color_r color16HSVA(int32_t hue, int32_t saturation, int32_t value, uint8_t alpha);

//rgb24_t color24HSV(int32_t hue, int32_t saturation, int32_t value);

// Convert an RGBA to a color_t. All values are clamped.
color_t color_rgb(int32_t red, int32_t gren, int32_t blue, int32_t alpha);

// Convert an HSV to a color_t. All values are clamped; this means hue may
// result in an earlier rotation.
color_t color_hsv(int32_t hue, int32_t saturation, int32_t value, int32_t alpha);

// Comvert a color to RGB space
rgb16_t color_rgb16(color_t color);
rgba16_t color_rgba16(color_t color);
rgb24_t color_rgb24(color_t color);

// Linear-interpolate from color a to b at parametric value t. When
// the color-spaces (RGB vs HSV) differ, values are coerced to RGBA.
color_t color_lerpfx(color_t a, color_t b, fixed_t t);

color_t color_blend(color_t foreground, color_t background);

//color_t color_lerp(color_t a, color_t b, uint32_t top, uint32_t bottom);

//rgb16_t color_rgb16Lerp(color_t a, color_t b, uint32_t top, uint32_t bottom);
//rgba16_t color_rgba16Lerp(color_t a, color_t b, uint32_t top, uint32_t bottom);
//rgb16_t color_rgb16Lerpfx(color_t a, color_t b, fixed_t alpha);
//rgba16_t color_rgba16Lerpfx(color_t a, color_t b, fixed_t alpha);

//rgb24_t color_rgb24Lerp(color_t a, color_t b, uint32_t top, uint32_t bottom);
//rgb24_t color_rgb24Lerpfx(color_t a, color_t b, fixed_t alpha);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __COLOR_H__ */
