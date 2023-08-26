/**
 * Copyright (c) 2000-2001 Aaron D. Gifford
 * Copyright (c) 2013-2014 Pavol Rusnak
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTOR(S) ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTOR(S) BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

// See: https://aarongifford.com/computers/sha.html

#include <math.h>

#include <string.h>
// #include <stdint.h>

#include "./sha2.h"

#define LITTLE_ENDIAN    1

#if LITTLE_ENDIAN

static void reverseBuffer(uint32_t *values, uint_fast8_t length) {
    uint32_t tmp;
    for (uint_fast8_t j = 0; j < length; j++) {
        tmp = values[j];
        tmp = (tmp >> 16) | (tmp << 16);
        values[j] = ((tmp & 0xff00ff00UL) >> 8) | ((tmp & 0x00ff00ffUL) << 8);
    }
}

#endif /* LITTLE_ENDIAN */

#define SHA256_BLOCK_LENGTH 64
#define SHA256_DIGEST_LENGTH 32
#define SHA256_DIGEST_STRING_LENGTH (SHA256_DIGEST_LENGTH * 2 + 1)
#define SHA256_SHORT_BLOCK_LENGTH (SHA256_BLOCK_LENGTH - 8)

/* Shift-right (used in SHA-256, SHA-384, and SHA-512): */
#define SHR(b, x) ((x) >> (b))

/* 32-bit Rotate-right (used in SHA-256): */
#define ROTR32(b, x) (((x) >> (b)) | ((x) << (32 - (b))))

/* Two of six logical functions used in SHA-1, SHA-256, SHA-384, and SHA-512: */
#define Ch(x, y, z) (((x) & (y)) ^ ((~(x)) & (z)))
#define Maj(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

/* Four of six logical functions used in SHA-256: */
#define Sigma0_256(x) (ROTR32(2, (x)) ^ ROTR32(13, (x)) ^ ROTR32(22, (x)))
#define Sigma1_256(x) (ROTR32(6, (x)) ^ ROTR32(11, (x)) ^ ROTR32(25, (x)))
#define sigma0_256(x) (ROTR32(7, (x)) ^ ROTR32(18, (x)) ^ SHR(3, (x)))
#define sigma1_256(x) (ROTR32(17, (x)) ^ ROTR32(19, (x)) ^ SHR(10, (x)))

// https://github.com/jedisct1/libsodium/blob/1647f0d53ae0e370378a9195477e3df0a792408f/src/libsodium/sodium/utils.c#L102-L130
static void memzero(uint8_t *dst, uint32_t length) {
    memset(dst, 0, length);
    /*
    volatile unsigned char *volatile dst = (volatile unsigned char *volatile) dst;
    size_t i = (size_t) 0U;

    while (i < length) {
        dst[i++] = 0U;
    }
    */
}

// Returns true if value is prime
/*
uint8_t isPrime(uint16_t value) {
    if ((value % 2) == 0) { return 0; }
    for (uint_fast16_t i = 3; i < value; i += 2) {
        if (value % i == 0) { return 0; }
    }
    return 1;
}

// Returns the index-th prime number
uint16_t getPrime(uint8_t index) {
    if (index == 0) { return 2; }
    if (index == 1) { return 3; }

    index -= 2;

    for (uint_fast16_t i = 6; i <= 312; i += 6) {
        if (isPrime(i - 1)) {
            if (index == 0) { return i - 1; }
            index--;
        }
        if (isPrime(i + 1)) {
            if (index == 0) { return i + 1; }
            index--;
        }
    }

    return 0;
}
*/
/*
const uint8_t lowPrimes[] = {
    2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67,
    71, 73, 79, 83, 89, 97, 101, 103, 107, 109, 113, 127,
    131, 137, 139, 149, 151, 157, 163, 167, 173, 179, 181, 191, 193, 197, 199, 211, 223, 227, 229,
    233, 239, 241, 251};

const uint16_t highPrimes[] = {
    257, 263, 269, 271, 277, 281, 283, 293, 307, 311
};

static uint16_t getPrime(uint8_t index) {
    if (index < 54) { return lowPrimes[index]; }
    return highPrimes[index - 54];
}
*/

static const uint32_t K256_2[] = {
    0x6a09e667UL,
	0xbb67ae85UL,
	0x3c6ef372UL,
	0xa54ff53aUL,
	0x510e527fUL,
	0x9b05688cUL,
	0x1f83d9abUL,
	0x5be0cd19UL
};

static const uint32_t K256_3[64] = {
	0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL,
	0x3956c25bUL, 0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL,
	0xd807aa98UL, 0x12835b01UL, 0x243185beUL, 0x550c7dc3UL,
	0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL, 0xc19bf174UL,
	0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL,
	0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL,
	0x983e5152UL, 0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL,
	0xc6e00bf3UL, 0xd5a79147UL, 0x06ca6351UL, 0x14292967UL,
	0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL, 0x53380d13UL,
	0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
	0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL,
	0xd192e819UL, 0xd6990624UL, 0xf40e3585UL, 0x106aa070UL,
	0x19a4c116UL, 0x1e376c08UL, 0x2748774cUL, 0x34b0bcb5UL,
	0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL, 0x682e6ff3UL,
	0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL,
	0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL
};

static uint32_t getConstant(uint8_t primeIndex, uint8_t cubeRoot) {
    if (cubeRoot) { return K256_3[primeIndex]; }
    return K256_2[primeIndex];
}
/*
static uint32_t getConstant(uint8_t primeIndex, uint8_t cubeRoot) {
    double v = getPrime(primeIndex);
    if (cubeRoot) {
        v = pow(v, 1.0 / 3.0);
    } else {
        v = pow(v, 0.5);
    }
    v -= (int)v;
    v *= 0x100000000;
    return (uint32_t)v;
}
*/

void sha256_init(Sha256Context *context) {
    for (int_fast8_t i = 0; i < 8; i++) {
        context->state[i] = getConstant(i, 0);
    }
    for (uint_fast8_t i = 0; i < (64 / sizeof(uint32_t)); i++) {
        context->buffer[i] = 0;
    }
    context->bitCount = 0;
}

static void sha256_Transform(const uint32_t *state_in, const uint32_t *data, uint32_t *state_out) {
    uint32_t working[8];
    uint32_t s0, s1;
    uint32_t T1, T2, W256[16];

    /* Initialize registers with the prev. intermediate value */
    for (int_fast8_t i = 7; i >= 0; i--) {
        working[i] = state_in[i];
    }

    for (uint_fast8_t j = 0; j < 16; j++) {
        /* Apply the SHA-256 compression function to update a..h with copy */
        T1 = working[7] + Sigma1_256(working[4]) + Ch(working[4], working[5], working[6]) + getConstant(j, 1) + (W256[j] = *data++);
        T2 = Sigma0_256(working[0]) + Maj(working[0], working[1], working[2]);
        for (int_fast8_t i = 7; i > 0; i--) {
            working[i] = working[i - 1];
        }
        working[4] += T1;
        working[0] = T1 + T2;
    }

    for (uint8_t j = 16; j < 64; j++) {
        /* Part of the message block expansion: */
        s0 = W256[(j + 1) & 0x0f];
        s0 = sigma0_256(s0);
        s1 = W256[(j + 14) & 0x0f];
        s1 = sigma1_256(s1);

        /* Apply the SHA-256 compression function to update a..h */
        T1 = working[7] + Sigma1_256(working[4]) + Ch(working[4], working[5], working[6]) + getConstant(j, 1) +
             (W256[j & 0x0f] += s1 + W256[(j + 9) & 0x0f] + s0);
        T2 = Sigma0_256(working[0]) + Maj(working[0], working[1], working[2]);

        for (int_fast8_t i = 7; i > 0; i--) {
            working[i] = working[i - 1];
        }
        working[4] += T1;
        working[0] = T1 + T2;
    }

    /* Compute the current intermediate hash value */
    for (int_fast8_t i = 7; i >= 0; i--) {
        state_out[i] = state_in[i] + working[i];
        working[i] = 0;
    }

    /* Clean up */
    T1 = T2 = 0;
}

#define MAX_UINT32 (0xffffffff)
static void increment_bitcount(Sha256Context *context, uint32_t count) {
    if (context->bitCount >= MAX_UINT32 - count) {
        //FFCrash(CrashReasonOutOfBounds);
        while(1); //@TODO: return an error
    }
    context->bitCount += count;
}

void sha256_update(Sha256Context *context, const uint8_t *data, uint32_t dataLength) {
    if (dataLength == 0) { return; }

    unsigned int freespace, usedspace;

    usedspace = (context->bitCount >> 3) % SHA256_BLOCK_LENGTH;
    if (usedspace > 0) {
        /* Calculate how much free space is available in the buffer */
        freespace = SHA256_BLOCK_LENGTH - usedspace;

        if (dataLength >= freespace) {
            /* Fill the buffer completely and process it */
            memcpy(((uint8_t *)context->buffer) + usedspace, data, freespace);
            // context->bitCount += freespace << 3;
            increment_bitcount(context, freespace << 3);
            dataLength -= freespace;
            data += freespace;
#if LITTLE_ENDIAN
            /* Convert TO host byte order */
            reverseBuffer(context->buffer, 16);
#endif
            sha256_Transform(context->state, context->buffer, context->state);
        } else {
            /* The buffer is not yet full */
            memcpy(((uint8_t *)context->buffer) + usedspace, data, dataLength);
            // context->bitCount += dataLength << 3;
            increment_bitcount(context, dataLength << 3);
            /* Clean up: */
            usedspace = freespace = 0;
            return;
        }
    }
    while (dataLength >= SHA256_BLOCK_LENGTH) {
        /* Process as many complete blocks as we can */
        memcpy(context->buffer, data, SHA256_BLOCK_LENGTH);
#if LITTLE_ENDIAN
        /* Convert TO host byte order */
        reverseBuffer(context->buffer, 16);
#endif
        sha256_Transform(context->state, context->buffer, context->state);
        // context->bitCount += SHA256_BLOCK_LENGTH << 3;
        increment_bitcount(context, SHA256_BLOCK_LENGTH << 3);
        dataLength -= SHA256_BLOCK_LENGTH;
        data += SHA256_BLOCK_LENGTH;
    }
    if (dataLength > 0) {
        /* There's left-overs, so save 'em */
        memcpy(context->buffer, data, dataLength);
        // context->bitCount += dataLength << 3;
        increment_bitcount(context, dataLength << 3);
    }
    /* Clean up: */
    usedspace = freespace = 0;
}

void sha256_final(Sha256Context *context, uint8_t *digest) {
    unsigned int usedspace;

    usedspace = (context->bitCount >> 3) % SHA256_BLOCK_LENGTH;

    /* Begin padding with a 1 bit: */
    ((uint8_t *)context->buffer)[usedspace++] = 0x80;

    if (usedspace > SHA256_SHORT_BLOCK_LENGTH) {
        memzero(((uint8_t *)context->buffer) + usedspace, SHA256_BLOCK_LENGTH - usedspace);

#if LITTLE_ENDIAN
        /* Convert TO host byte order */
        reverseBuffer(context->buffer, 16);
#endif
        /* Do second-to-last transform: */
        sha256_Transform(context->state, context->buffer, context->state);

        /* And prepare the last transform: */
        usedspace = 0;
    }
    /* Set-up for the last transform: */
    memzero(((uint8_t *)context->buffer) + usedspace, SHA256_SHORT_BLOCK_LENGTH - usedspace);

#if LITTLE_ENDIAN
    /* Convert TO host byte order */
    reverseBuffer(context->buffer, 14);
#endif
    /* Set the bit count: */
    // context->buffer[14] = context->bitCount >> 32;
    context->buffer[14] = 0;
    context->buffer[15] = context->bitCount & 0xffffffff;

    /* Final transform: */
    sha256_Transform(context->state, context->buffer, context->state);

#if LITTLE_ENDIAN
    /* Convert FROM host byte order */
    reverseBuffer(context->state, 8);
#endif
    memcpy(digest, context->state, SHA256_DIGEST_LENGTH);

    /* Clean up state data: */
    memzero((uint8_t*)context, sizeof(Sha256Context));
    usedspace = 0;
}

/*
void ethers_sha256(uint8_t *data, uint32_t dataLength, uint8_t *digest) {
    Sha256Context context;
    ethers_sha256Init(&context);
    ethers_sha256Update(&context, data, dataLength);
    ethers_sha256Finalize(&context, digest);
}
*/
