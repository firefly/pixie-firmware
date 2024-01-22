
// See: https://easings.net/#easeInOutExpo


#include "./curves.h"


const fixed_t FM_PI_2   = 0x19220;
const fixed_t FM_PI     = 2 * FM_PI_2;
const fixed_t FM_3PI_2  = 3 * FM_PI_2;
const fixed_t FM_2PI    = 4 * FM_PI_2;

const fixed_t FM_E      = 0x2b7e1;

const fixed_t FM_1_2    =     0x8000;
const fixed_t FM_1_4    =     0x4000;

const fixed_t FM_MAX    = 0x7fffffff;
const fixed_t FM_MIN    = 0x80000000;

const fixed_t FM_0      =    0;
const fixed_t FM_1      =    0x10000;

const fixed_t FM_2      =    0x20000;
const fixed_t FM_10     =    0xa0000;
const fixed_t FM_20     =   0x140000;

fixed_t tofx(int32_t value) {
    return value << 16;
}

static uint32_t umul32hi(uint32_t a, uint32_t b) {
    return (uint32_t)(((uint64_t)a * b) >> 32);
}

/* compute log2() with s15.16 fixed-point argument and result */
fixed_t log2fx(fixed_t arg) {
    const uint32_t a0 = (uint32_t)((1.44269476063 - 1)* (1LL << 32) + 0.5);
    const uint32_t a1 = (uint32_t)(7.2131008654833e-1 * (1LL << 32) + 0.5);
    const uint32_t a2 = (uint32_t)(4.8006370104849e-1 * (1LL << 32) + 0.5);
    const uint32_t a3 = (uint32_t)(3.5339481476694e-1 * (1LL << 32) + 0.5);
    const uint32_t a4 = (uint32_t)(2.5600972794928e-1 * (1LL << 32) + 0.5);
    const uint32_t a5 = (uint32_t)(1.5535182948224e-1 * (1LL << 32) + 0.5);
    const uint32_t a6 = (uint32_t)(6.3607925549150e-2 * (1LL << 32) + 0.5);
    const uint32_t a7 = (uint32_t)(1.2319647939876e-2 * (1LL << 32) + 0.5);
    int32_t lz;
    uint32_t approx, h, m, l, z, y, x = arg;
    lz = __builtin_clz(x);

    // Shift off integer bits and normalize fraction 0 <= f <= 1
    x = x << (lz + 1);
    y = umul32hi(x, x); // f**2
    z = umul32hi(y, y); // f**4

    // Evaluate minimax polynomial to compute log2(1+f)
    h = a0 - umul32hi(a1, x);
    m = umul32hi(a2 - umul32hi(a3, x), y);
    l = umul32hi(a4 - umul32hi(a5, x) + umul32hi(a6 - umul32hi(a7, x), y), z);
    approx = x + umul32hi (x, h + m + l);

    // Combine integer and fractional parts of result; round result
    approx = ((15 - lz) << 16) + ((((approx) >> 15) + 1) >> 1);
    return approx;
}

/* compute exp2() with s15.16 fixed-point argument and result */
fixed_t exp2fx(fixed_t arg) {
    const uint32_t a0 = (uint32_t)(6.9314718107e-1 * (1LL << 32) + 0.5);
    const uint32_t a1 = (uint32_t)(2.4022648809e-1 * (1LL << 32) + 0.5);
    const uint32_t a2 = (uint32_t)(5.5504413787e-2 * (1LL << 32) + 0.5);
    const uint32_t a3 = (uint32_t)(9.6162736882e-3 * (1LL << 32) + 0.5);
    const uint32_t a4 = (uint32_t)(1.3386828359e-3 * (1LL << 32) + 0.5);
    const uint32_t a5 = (uint32_t)(1.4629773796e-4 * (1LL << 32) + 0.5);
    const uint32_t a6 = (uint32_t)(2.0663021132e-5 * (1LL << 32) + 0.5);
    int32_t i;
    uint32_t approx, h, m, l, s, q, f, x = arg;

    // Extract integer portion; 2**i is realized as a shift at the end
    i = ((x >> 16) ^ 0x8000) - 0x8000;

    // Extract and normalize fraction f to compute 2**(f)-1, 0 <= f < 1
    f = x << 16;

    // Evaluate minimax polynomial to compute exp2(f)-1
    s = umul32hi(f, f); // f**2
    q = umul32hi(s, s); // f**4
    h = a0 + umul32hi(a1, f);
    m = umul32hi(a2 + umul32hi(a3, f), s);
    l = umul32hi(a4 + umul32hi(a5, f) + umul32hi(a6, s), q);
    approx = umul32hi(f, h + m + l);

    // combine integer and fractional parts of result; round result
    approx = ((approx >> (15 - i)) + (0x80000000 >> (14 - i)) + 1) >> 1;

    // handle underflow to 0
    approx = ((i < -16) ? 0: approx);
    return approx;
}

/* s15.16 division without rounding */
fixed_t divfx(fixed_t x, fixed_t y) {
    return ((int64_t)x * 65536) / y;
}

/* s15.16 multiplication with rounding */
fixed_t mulfx(fixed_t x, fixed_t y) {
    int32_t r;
    int64_t t = (int64_t)x * (int64_t)y;
    r = (int32_t)(uint32_t)(((uint64_t)t + (1 << 15)) >> 16);
    return r;
}


int32_t scalarfx(int32_t _scalar, fixed_t x) {
    int64_t scalar = _scalar;
    return (scalar * x) >> 16;
}

/* compute a**b for a >= 0 */
fixed_t powfx(fixed_t a, fixed_t b) {
    return exp2fx(mulfx(b, log2fx(a)));
}

// See: http://www.coranac.com/2009/07/sines/
fixed_t sinfx(fixed_t x) {

    // Normalize i to [0, 2pi)
    x = (((x % FM_2PI) + FM_2PI) % FM_2PI);

    // Account for the quadrant reflecting in the x or y axis
    int32_t ymul = 1;
    if (x >= FM_3PI_2) {
        x -= FM_3PI_2;
        x = FM_PI_2 - x;
        ymul = -1;
    } else if (x >= FM_PI) {
        x -= FM_PI;
        ymul = -1;
    } else if (x >= FM_PI_2) {
        x -= FM_PI_2;
        x = FM_PI_2 - x;
    }

    // 3rd order approximation;
    // Note: tweaked 0xf476 to 0xf475 so that FM_PI and FM_3PI_2
    // line up perfectly with 1 and -1 respectively.
    fixed_t result = mulfx(0xf475, x) - mulfx(0x2106, mulfx(mulfx(x, x), x));

    return ymul * result;
}

fixed_t cosfx(fixed_t x) {
    return sinfx(x + FM_PI_2);
}


fixed_t CurveLinear(fixed_t t) { return t; }


fixed_t CurveEaseInSine(fixed_t t) {
    // return 1.0f - cosf((t * M_PI) / 2.0f);
    return FM_1 - cosfx(mulfx(t, FM_PI) >> 1);
}

fixed_t CurveEaseOutSine(fixed_t t) {
    // return sinf((t * M_PI) / 2.0f);
    return sinfx(mulfx(t, FM_PI) >> 1);
}

fixed_t CurveEaseInOutSine(fixed_t t) {
    // return -(cosf(M_PI * t) - 1.0f) / 2.0f;
    return -(cosfx(mulfx(FM_PI, t) - FM_1) >> 1);
}


fixed_t CurveEaseInQuad(fixed_t t) {
    // return t * t; 
    return mulfx(t, t);
}

fixed_t CurveEaseOutQuad(fixed_t t) {
    // return 1.0f - (1.0f - t) * (1.0f - t);
    fixed_t alpha1 = FM_1 - t;
    return FM_1 - mulfx(alpha1, alpha1);
}

fixed_t CurveEaseInOutQuad(fixed_t t) {
    // return (t < 0.5f) ? 2.0f * t * t:
    //     1.0f - powf(-2.0 * t + 2.0f, 2.0f) / 2.0f;
    if (t < FM_1_2) { return mulfx(t, t) << 1; }
    fixed_t alpha1 = -(t << 1) + FM_2;
    return FM_1 - (mulfx(alpha1, alpha1) >> 1);
}


fixed_t CurveEaseInCubic(fixed_t t) {
    // return t * t * t;
    return mulfx(t, mulfx(t, t));
}

fixed_t CurveEaseOutCubic(fixed_t t) {
    //return 1.0f - powf(1.0f - t, 3.0f);
    return FM_1 - CurveEaseInCubic(FM_1 - t);

}

fixed_t CurveEaseInOutCubic(fixed_t t) {
    // return (t < 0.5f) ? (4 * t * t * t): 1.0f - powf(-2.0f * t + 2.0f, 3.0f) / 2.0f;
    if (t < FM_1_2) { return CurveEaseInCubic(t) << 2; }
    return FM_1 - (CurveEaseInCubic(-(t << 1) + FM_2) >> 1);
}


fixed_t CurveEaseInQuart(fixed_t t) {
    //return t * t * t * t;
    fixed_t t2 = mulfx(t, t);
    return mulfx(t2, t2);
}

fixed_t CurveEaseOutQuart(fixed_t t) {
    // return 1.0f - powf(1.0f - t, 4.0f);
    return FM_1 - CurveEaseInQuart(FM_1 - t);
}

fixed_t CurveEaseInOutQuart(fixed_t t) {
    // return (t < 0.5f) ? 8.0f * t * t * t * t: 1.0 - powf(-2.0f * t + 2.0f, 4.0f) / 2.0f;
    if (t < FM_1_2) { return CurveEaseInQuart(t) << 3; }
    return FM_1 - (CurveEaseInQuart(-(t << 1) + FM_2) >> 1);
}


fixed_t CurveEaseInQuint(fixed_t t) {
    // return t * t * t * t * t;
    fixed_t t2 = mulfx(t, t);
    return mulfx(t, mulfx(t2, t2));
}

fixed_t CurveEaseOutQuint(fixed_t t) {
    // return 1.0f - powf(1.0f - t, 5);
    return FM_1 - CurveEaseInQuint(FM_1 - t);
}

fixed_t CurveEaseInOutQuint(fixed_t t) {
    // return (t < 0.5f) ? 16.0f * t * t * t * t * t: 1.0f - powf(-2.0f * t + 2.0f, 5.0f) / 2.0f;
    if (t < FM_1_2) {
        return CurveEaseInQuint(t) << 4;
    }
    return FM_1 - (CurveEaseInQuint(-(t << 1) + FM_2) >> 1);

}

fixed_t CurveEaseInExpo(fixed_t t) {
    // return (t == 0.0f) ? 0.0f: powf(2.0f, 10.0f * t - 10.0f);
    if (t == 0) { return 0; }
    return powfx(FM_2, mulfx(FM_10, t - FM_10));
}

fixed_t CurveEaseOutExpo(fixed_t t) {
    // return (t == 1.0f) ? 1.0f: 1.0f - powf(2.0f, -10.0f * t);
    if (t == FM_1) { return FM_1; }
    return FM_1 - powfx(FM_2, mulfx(-FM_10, t));
}

fixed_t CurveEaseInOutExpo(fixed_t t) {
    // return (t == 0.0f) ? 0.0f:
    //     (t == 1.0f) ? 1.0f:
    //     (t < 0.5f) ? powf(2.0f, 20.0f * t - 10.0f) / 2.0f:
    //     (2.0f - powf(2.0f, -20.0f * t + 10.0f)) / 2.0f;
    if (t == 0) { return 0; }
    if (t == FM_1) { return FM_1; }
    if (t < FM_1_2) { return powfx(FM_2, mulfx(-FM_20, t) + FM_10) >> 1; }
    return (FM_2 - powfx(FM_2, mulfx(-FM_20, t) + FM_10)) >> 1;
}

// float CurveEaseInCirc(float t) { return 1.0f - sqrtf(1.0f - powf(t, 2.0f)); }
// float CurveEaseOutCirc(float t) { return sqrtf(1.0f - powf(t - 1.0f, 2.0f)); }
// float CurveEaseInOutCirc(float t) {
//     return (t < 0.5f) ? (1.0f - sqrtf(1.0f - powf(2.0f * t, 2.0f))) / 2.0f:
//         (sqrtf(1.0f - powf(-2.0f * t + 2.0f, 2.0f)) + 1.0f) / 2.0f;
// }

fixed_t CurveEaseInBack(fixed_t t) {
    // const float c1 = 1.70158f;
    // const float c3 = c1 + 1.0f;
    // return c3 * t * t * t - c1 * t * t;

    const fixed_t c1 = 0x1b39b;
    const fixed_t c3 = c1 + FM_1;
    const fixed_t t2 = mulfx(t, t);
    return mulfx(mulfx(c3, t2), t) - mulfx(c1, t2);
}

fixed_t CurveEaseOutBack(fixed_t t) {
    // const float c1 = 1.70158f;
    // const float c3 = c1 + 1.0f;

    // return 1.0f + c3 * powf(t - 1.0f, 3.0f) + c1 * powf(t - 1.0f, 2.0f);
    // float t_1 = t - 1.0f
    // return 1.0f + c3 * t_1 * t_1 * t_1 + c1 * t_1 * t_1;

    const fixed_t c1 = 0x1b39b;
    const fixed_t c3 = c1 + FM_1;
    const fixed_t t_1 = t - FM_1;
    const fixed_t t2 = mulfx(t_1, t_1);

    return FM_1 + mulfx(c3, mulfx(t_1, t2)) - mulfx(c1, t2);
}

// fixed_t CurveEaseInOutBack(fixed_t t) {
    // return (t == 0.0f) ? 0.0f:
    //     (t == 1.0f) ? 1.0f:
    //     (t < 0.5f) ? powf(2.0, 20.0 * t - 10.0f) / 2.0f:
    //     (2.0f - powf(2.0, -20.0f * t + 10.0f)) / 2.0f;

//     if (t == 0) { return 0; }
//     if (t == 1) { return 1; }
//     if (tx < FixedHalf) {
//     }
// }

fixed_t CurveEaseInElastic(fixed_t t) {
    // const float c4 = (2.0f * M_PI) / 3.0f;
    // return (t == 0.0f) ? 0.0f:
    //     (t == 1.0f) ? 1.0f:
    //     -powf(2.0f, 10.0f * t - 10.0f) * sinf((t * 10.0f - 10.75f) * c4);

    const fixed_t c4 = 0x2182a;
    if (t == 0) { return 0; }
    if (t == 1) { return 1; }
    return -mulfx(powfx(FM_2, mulfx(FM_10, t) - FM_10), sinfx(mulfx(mulfx(FM_10, t) - 0xac000, c4)));
}

fixed_t CurveEaseOutElastic(fixed_t t) {
    // const float c4 = (2 * M_PI) / 3;
    // return (t == 0) ? 0.0f:
    //     (t == 1) ? 1.0f:
    //     powf(2.0f, -10.0f * t) * sinf((t * 10.0f - 0.75f) * c4) + 1.0f;

    const fixed_t c4 = 0x2182a;
    if (t == 0) { return 0; }
    if (t == 1) { return 1; }
    return mulfx(powfx(FM_2, mulfx(-FM_10, t)), sinfx(mulfx(mulfx(FM_10, t) - 0xc000, c4))) + FM_1;
}

// float CurveEaseInOutElastic(float t) {
//     const float c5 = (2.0f * M_PI) / 4.5f;
//     return (t == 0.0f) ? 0.0f:
//         (t == 1.0f) ? 1.0f:
//         (t < 0.5f) ? -(powf(2.0f, 20.0f * t - 10.0f) * sinf((20.0f * t - 11.125f) * c5)) / 2.0f:
//         (pow(2.0f, -20.0f * t + 10.0f) * sinf((20.0f * t - 11.125f) * c5)) / 2.0f + 1.0f;
// }

fixed_t CurveEaseInBounce(fixed_t t) {
    // return 1.0f - CurveEaseOutBounce(1.0f - t);
    return FM_1 - CurveEaseOutBounce(FM_1 - t);
}

fixed_t CurveEaseOutBounce(fixed_t t) {
    // const float n1 = 7.5625f;
    // const float d1 = 2.75f;
    // if (t < 1.0f / d1) {
    //     return n1 * t * t;
    // } else if (t < 2.0f / d1) {
    //     return n1 * (t -= 1.5f / d1) * t + 0.75f;
    // } else if (t < 2.5f / d1) {
    //     return n1 * (t -= 2.25f / d1) * t + 0.9375f;
    // } else {
    //     return n1 * (t -= 2.625f / d1) * t + 0.984375f;
    // }

    const fixed_t n1 = 0x79000;
    const fixed_t d1 = 0x2c000;
    const fixed_t d1t = mulfx(d1, t);

    if (d1t < FM_1) { return mulfx(n1, mulfx(t, t)); }

    if (d1t < FM_2) {
        t -= (FM_1 + FM_1_2);
        return mulfx(n1 * t / d1, t) + 0xc000;
    }

    if (d1t < (FM_2 + FM_1_2)) {
        t -= (FM_2 + FM_1_4);
        return mulfx(n1 * t / d1, t) + 0xf000;
    }

    t -= 0x2a000;
    return mulfx(n1 * t / d1, t) + 0xfc00;

}

fixed_t CurveEaseInOutBounce(fixed_t t) {
    // return t < 0.5f ? (1.0f - CurveEaseOutBounce(1.0f - 2.0f * t)) / 2.0f:
    //     (1.0f + CurveEaseOutBounce(2.0f * t - 1.0f)) / 2.0f;
    if (t < FM_1_2) { return (FM_1 - CurveEaseOutBounce(FM_1 - (t << 1))) >> 1; }
    return (FM_1 + CurveEaseOutBounce((t << 1) - FM_1)) >> 1;
}
