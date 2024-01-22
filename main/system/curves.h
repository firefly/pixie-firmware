
#ifndef __CURVES_H__
#define __CURVES_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


typedef int32_t fixed_t;


/**
 *  Fixed-point constants
 */

extern const fixed_t FM_1;

extern const fixed_t FM_PI_2;
extern const fixed_t FM_PI;
extern const fixed_t FM_3PI_2;
extern const fixed_t FM_2PI;

extern const fixed_t FM_E;

extern const fixed_t FM_MAX;
extern const fixed_t FM_MIN;


/**
 *  Fixed-point maths
 */

fixed_t tofx(int32_t value);

fixed_t divfx(fixed_t x, fixed_t y);
fixed_t mulfx(fixed_t x, fixed_t y);
int32_t scalarfx(int32_t scalar, fixed_t x);

fixed_t log2fx(fixed_t arg);
fixed_t exp2fx(fixed_t arg);
fixed_t powfx(fixed_t a, fixed_t b);

fixed_t sinfx(fixed_t x);
fixed_t cosfx(fixed_t x);


/**
 *  Fixed-point curves
 */

typedef fixed_t (*CurveFunc)(fixed_t t);

fixed_t CurveLinear(fixed_t t);

fixed_t CurveEaseInSine(fixed_t t);
fixed_t CurveEaseOutSine(fixed_t t);
fixed_t CurveEaseInOutSine(fixed_t t);

fixed_t CurveEaseInQuad(fixed_t t);
fixed_t CurveEaseOutQuad(fixed_t t);
fixed_t CurveEaseInOutQuad(fixed_t t);

fixed_t CurveEaseInCubic(fixed_t t);
fixed_t CurveEaseOutCubic(fixed_t t);
fixed_t CurveEaseInOutCubic(fixed_t t);

fixed_t CurveEaseInQuart(fixed_t t);
fixed_t CurveEaseOutQuart(fixed_t t);
fixed_t CurveEaseInOutQuart(fixed_t t);

fixed_t CurveEaseInQuint(fixed_t t);
fixed_t CurveEaseOutQuint(fixed_t t);
fixed_t CurveEaseInOutQuint(fixed_t t);

fixed_t CurveEaseInExpo(fixed_t t);
fixed_t CurveEaseOutExpo(fixed_t t);
fixed_t CurveEaseInOutExpo(fixed_t t);

fixed_t CurveEaseInBack(fixed_t t);
fixed_t CurveEaseOutBack(fixed_t t);
// fixed_t CurveEaseInOutBack(fixed_t t)

fixed_t CurveEaseInElastic(fixed_t t);
fixed_t CurveEaseOutElastic(fixed_t t);
// fixed_t CurveEaseInOutElastic(fixed_t t)

fixed_t CurveEaseInBounce(fixed_t t);
fixed_t CurveEaseOutBounce(fixed_t t);
fixed_t CurveEaseInOutBounce(fixed_t t);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CURVES_H__ */
