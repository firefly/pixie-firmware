// Extern declarations for every image-data array defined in `images.c`.
// Use these instead of including main/images/image-*.h directly, except
// from images.c itself.

#ifndef __PIXIE_IMAGE_DATA_H__
#define __PIXIE_IMAGE_DATA_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>


extern const uint16_t image_space[];
extern const uint16_t image_ship[];
extern const uint16_t image_bullet[];
extern const uint16_t image_alien1[];
extern const uint16_t image_alien2[];
extern const uint16_t image_alienboom[];
extern const uint16_t image_arrow[];

extern const size_t image_space_len;
extern const size_t image_ship_len;
extern const size_t image_bullet_len;
extern const size_t image_alien1_len;
extern const size_t image_alien2_len;
extern const size_t image_alienboom_len;
extern const size_t image_arrow_len;


// 90-degree clockwise-rotated copies of the directional game sprites,
// allocated once at boot via images_initRotated(). Use these from the
// portrait-layout games where the ship needs to face up rather than
// left.
extern const uint16_t *image_ship_cw;
extern const uint16_t *image_alien1_cw;
extern const uint16_t *image_alien2_cw;
extern const uint16_t *image_alienboom_cw;
extern size_t image_ship_cw_len;
extern size_t image_alien1_cw_len;
extern size_t image_alien2_cw_len;
extern size_t image_alienboom_cw_len;

void images_initRotated(void);


#ifdef __cplusplus
}
#endif

#endif  /* __PIXIE_IMAGE_DATA_H__ */
