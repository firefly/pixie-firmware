// Single translation unit that pulls in every image-data header.
//
// The headers in main/images/ define their `const uint16_t image_X[] = {…}`
// arrays at file scope without `static`, so including them in two .c files
// produces duplicate-symbol link errors. Including them only here gives
// each array exactly one definition; other panels see them via
// `image-data.h`'s extern declarations.
//
// Also produces 90-degree clockwise-rotated copies of the directional
// game sprites at boot, since the originals were drawn for a horizontal
// (left-firing) layout but the games now run in portrait/up-firing
// mode. See images_initRotated() below.

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>


#include "images/image-alien-1.h"
#include "images/image-alien-2.h"
#include "images/image-alien-boom.h"
#include "images/image-arrow.h"
#include "images/image-bullet.h"
#include "images/image-ship.h"
#include "images/image-space.h"

// image-background.h and image-pixie.h are intentionally not included:
// firefly-hollows ships its own copy of those symbols in
// src/demo/background-pixies.c. Including ours would conflict at link
// time, and we don't currently use them from main/.


// Companion size constants, computed in this TU where sizeof() works.
const size_t image_space_len     = sizeof(image_space);
const size_t image_ship_len      = sizeof(image_ship);
const size_t image_bullet_len    = sizeof(image_bullet);
const size_t image_alien1_len    = sizeof(image_alien1);
const size_t image_alien2_len    = sizeof(image_alien2);
const size_t image_alienboom_len = sizeof(image_alienboom);
const size_t image_arrow_len     = sizeof(image_arrow);


////////////////////////////////
// Runtime sprite rotation.
//
// firefly-scene's RGB565+A4 image format (type byte 0x05) lays the data
// out as:
//   data[0]  = type/format flag
//   data[1]  = width
//   data[2]  = height
//   data[3]  = alphaCount  (number of uint16_t alpha words; 4-bit/pixel)
//   data[4 .. 4+alphaCount-1] = alpha bitmap (4 alpha values per word,
//                               packed high-nibble-first)
//   data[4+alphaCount .. end] = pixel data (RGB565, row-major)
//
// rotate90cw() rotates that structure 90 degrees clockwise into a fresh
// malloc'd buffer; width and height are swapped, alpha count stays the
// same. The resulting buffer is read-only after init and never freed.

const uint16_t *image_ship_cw      = NULL;
const uint16_t *image_alien1_cw    = NULL;
const uint16_t *image_alien2_cw    = NULL;
const uint16_t *image_alienboom_cw = NULL;
size_t image_ship_cw_len           = 0;
size_t image_alien1_cw_len         = 0;
size_t image_alien2_cw_len         = 0;
size_t image_alienboom_cw_len      = 0;


static const uint16_t *rotate90cw(const uint16_t *src, size_t *outBytes) {
    if ((src[0] & 0x0f) != 0x05) { return NULL; }

    int oldW = src[1];
    int oldH = src[2];
    int alphaCount = src[3];

    size_t totalWords = 4 + (size_t)alphaCount + (size_t)oldW * oldH;
    uint16_t *dst = calloc(totalWords, sizeof(uint16_t));
    if (!dst) { return NULL; }

    dst[0] = src[0];
    dst[1] = oldH;          // new width  = old height
    dst[2] = oldW;          // new height = old width
    dst[3] = alphaCount;    // unchanged: same total pixel count

    int newW = oldH;
    int newH = oldW;

    const uint16_t *srcPixels = &src[4 + alphaCount];
    uint16_t *dstPixels       = &dst[4 + alphaCount];

    for (int ny = 0; ny < newH; ny++) {
        for (int nx = 0; nx < newW; nx++) {
            // 90 deg CW: new(nx,ny) <- old(ox,oy) where ox=ny, oy=oldH-1-nx.
            int ox = ny;
            int oy = oldH - 1 - nx;
            int oldIdx = oy * oldW + ox;
            int newIdx = ny * newW + nx;

            dstPixels[newIdx] = srcPixels[oldIdx];

            uint16_t aWord = src[4 + (oldIdx / 4)];
            uint8_t a = (aWord >> (12 - 4 * (oldIdx % 4))) & 0x0f;
            dst[4 + (newIdx / 4)] |=
              (uint16_t)((uint16_t)a << (12 - 4 * (newIdx % 4)));
        }
    }

    *outBytes = totalWords * sizeof(uint16_t);
    return dst;
}

void images_initRotated(void) {
    if (image_ship_cw) { return; }   // idempotent
    image_ship_cw      = rotate90cw(image_ship,      &image_ship_cw_len);
    image_alien1_cw    = rotate90cw(image_alien1,    &image_alien1_cw_len);
    image_alien2_cw    = rotate90cw(image_alien2,    &image_alien2_cw_len);
    image_alienboom_cw = rotate90cw(image_alienboom, &image_alienboom_cw_len);
}
