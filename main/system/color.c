#include <stdio.h>

#include "./color.h"

#define COLOR_HSV          (0x80000000)
#define NEGATIVE_HUE       (0x40000000)

// Color Format:
//  - RGBA = 00AA AAAA  RRRR RRRR  GGGG GGGG  BBBB BBBB
//  - HSVA = 1NAA AAAA  HHHH HHHH  HHHH SSSS  SSVV VVVV   (N => negative hue)
//  - Alpha is 0 for fully opaque and 0x3f for fully transparent


#define CLAMP(v,m)     (((v) < 0) ? 0: ((v) > (m)) ? (m): (v))

color_t color_rgb(int32_t r, int32_t g, int32_t b, int32_t alpha) {
    color_t result = 0;

    result |= CLAMP(r, 0xff) << 16;
    result |= CLAMP(g, 0xff) << 8;
    result |= CLAMP(b, 0xff);
    result |= (0x3f - CLAMP(alpha, 0x3f)) << 24;

    return result;
}

color_t color_hsv(int32_t h, int32_t s, int32_t v, int32_t alpha) {
    color_t result = COLOR_HSV;

    if (h < 0) {
        result |= NEGATIVE_HUE;
        h *= -1;
    }

    result |= CLAMP(h, 0xfff) << 12;
    result |= CLAMP(s, 0x3f) << 6;
    result |= CLAMP(v, 0x3f);
    result |= (0x3f - CLAMP(alpha, 0x3f)) << 24;

    return result;
}

// Stores RGB565 as a uint16_t
#define RGB16(r,g,b)     ((((r) & 0xf8) << 8) | (((g) & 0xfc) << 3) | (((b) & 0xf8) >> 3))

// Stores RGB888 as a uint32_t
#define RGB24(r,g,b)    ((rgb24_t)(((r) << 16) | ((g) << 8) | (b)))

#define B6     (0x3f)

// Heavily inspired by: https://stackoverflow.com/questions/3018313/algorithm-to-convert-rgb-to-hsv-and-hsv-to-rgb-in-range-0-255-for-both
static color_t _fromHSV(color_t color) {

    int32_t h = ((color >> 12) & 0xfff);
    int32_t s = (color >> 6) & B6;
    int32_t v = color & B6;
    if (color & NEGATIVE_HUE) { h *= -1; }

    // Normalize h to [0, 359]  // @TODO: Scale up!
    h %= 360;
    if (h < 0) { h += 360; }

    int32_t region = h / 60;
    int32_t rem = (h - (region * 60));

    // Normalize value to a byte
    v = (v * 255) / B6;

    // Notes:
    //  - v, p, and q MUST all be [0, 255] for the RGB mapping
    //  - s is [0, 63]
    //  - rem is [0, 59]
    //  - v, s and rem represent values [0, 1] multiplied by their multiplier
    //    - so (63 - s) is the equivlent of (1.0 - s_)
    //    - (v * s) / (255 * 63) is equivalent of (v_ * s_)

    // Mask out the two HSV-specific bits
    color_t result = color & 0x3f000000;

    int32_t p = (v * (63 - s)) / 63, q;

    // Map the RGB value based on the region
    switch (region) {
        case 0:
            q = (v * ((63 * 59) - (s * (59 - rem)))) / (63 * 59);
            result |= (v << 16) | (q << 8) | p;
            break;
        case 1:
            q = (v * ((63 * 59) - (s * rem))) / (63 * 59);
            result |= (q << 16) | (v << 8) | p;
            break;
        case 2:
            q = (v * ((63 * 59) - (s * (59 - rem)))) / (63 * 59);
            result |= (p << 16) | (v << 8) | q;
            break;
        case 3:
            q = (v * ((63 * 59) - (s * rem))) / (63 * 59);
            result |= (p << 16) | (q << 8) | v;
            break;
        case 4:
            q = (v * ((63 * 59) - (s * (59 - rem)))) / (63 * 59);
            result |= (q << 16) | (p << 8) | v;
            break;
        case 5:
            q = (v * ((63 * 59) - (s * rem))) / (63 * 59);
            result |= (v << 16) | (p << 8) | q;
            break;
    }

    return result;
}

rgb16_t color_rgb16(color_t color) {
    if (color & COLOR_HSV) { color = _fromHSV(color); }
    return RGB16((color >> 24) & 0xff, (color >> 16) & 0xff, color & 0xff);
}


//rgba16_t color_rgba16(color_t color) {
//    if (color & COLOR_HSV) { color = _fromHSV(color); }
//    return RGBA16((color >> 24) & 0xff, (color >> 16) & 0xff, color & 0xff, color >> 24);
//}

rgb24_t color_rgb24(color_t color) {
    if (color & COLOR_HSV) { color = _fromHSV(color); }
    return color;
}

static int32_t lerp(int32_t a, int32_t b, int32_t top, int32_t bottom) {
    return ((top * b) / bottom) + (((bottom - top) * a) / bottom);
}

color_t color_lerpQuotient(color_t c0, color_t c1, int32_t num, int32_t denom) {
    int32_t a = lerp(ALPHA(c0), ALPHA(c1), num, denom);

    if (((c0 >> 31) + (c1 >> 31)) != 2) {
        c0 = color_rgb24(c0);
        c1 = color_rgb24(c1);

        int32_t r = lerp(RGB_RED(c0), RGB_RED(c1), num, denom);
        int32_t g = lerp(RGB_BLUE(c0), RGB_BLUE(c1), num, denom);
        int32_t b = lerp(RGB_GREEN(c0), RGB_GREEN(c1), num, denom);

        return color_rgb(r, g, b, a);
    }

    int32_t h = lerp(HSV_HUE(c0), HSV_HUE(c1), num, denom);
    int32_t s = lerp(HSV_SAT(c0), HSV_SAT(c1), num, denom);
    int32_t v = lerp(HSV_VAL(c0), HSV_VAL(c1), num, denom);

    return color_hsv(h, s, v, a);
}


// Test program to validate HSV conversion
/*
int main() {
    for (uint32_t h  = 0; h < 360; h++) {
        for (uint32_t s  = 0; s <= 0x3f; s++) {
            for (uint32_t v = 0; v <= 0x3f; v++) {
                color_t hsv = color_hsv(h, s, v, 0xff);
                color_t rgb = _fromHSV(hsv);
                printf("h=%d, s=%d, v=%d, hsv=%u, rgb=%u\n", h,s, v, hsv, rgb);
            }
        }
    }
}
*/
