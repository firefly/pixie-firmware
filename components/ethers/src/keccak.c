/* sha3 - an implementation of Secure Hash Algorithm 3 (Keccak).
 * based on the
 * The Keccak SHA-3 submission. Submission to NIST (Round 3), 2011
 * by Guido Bertoni, Joan Daemen, Michaël Peeters and Gilles Van Assche
 *
 * Copyright: 2013 Aleksey Kravchenko <rhash.admin@gmail.com>
 *
 * Permission is hereby granted,  free of charge,  to any person  obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction,  including without limitation
 * the rights to  use, copy, modify,  merge, publish, distribute, sublicense,
 * and/or sell copies  of  the Software,  and to permit  persons  to whom the
 * Software is furnished to do so.
 *
 * This program  is  distributed  in  the  hope  that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  Use this program  at  your own risk!
 */

//#if BYTE_ORDER == BIG_ENDIAN
//#error "Big edian support was removed; re-add"
//#endif

#include <string.h>
#include <stdint.h>

#include "firefly-hash.h"

// RicMoo: we only care about Keccak256, so we hardcode the blocksize
#define KECCAK256_BITS  (256)
#define KECCAK256_BLOCK_SIZE     ((1600 - KECCAK256_BITS * 2) / 8)

#define I64(x) x##LL
#define ROTL64(qword, n) ((qword) << (n) ^ ((qword) >> (64 - (n))))
#define le2me_64(x) (x)
#define IS_ALIGNED_64(p) (0 == (7 & ((const char*)(p) - (const char*)0)))
#define me64_to_le_str(to, from, length) memcpy((to), (from), (length))

/* constants */

const uint8_t constants[]  = {
    // round constants; compressed, see get_round_constant for decompression
    1, 26, 94, 112, 31, 33, 121, 85, 14, 12, 53, 38, 63, 79, 93, 83, 82, 72, 22, 102, 121, 88, 33, 116,

    // pi transform
    1, 6, 9, 22, 14, 20, 2, 12, 13, 19, 23, 15, 4, 24, 21, 8, 16, 5, 3, 18, 17, 11, 7, 10,

    // rho transforms
    1, 62, 28, 27, 36, 44, 6, 55, 20, 3, 10, 43, 25, 39, 41, 45, 15, 21, 8, 18, 2, 61, 56, 14,
};

#define TYPE_ROUND_INFO      0
#define TYPE_PI_TRANSFORM   24
#define TYPE_RHO_TRANSFORM  48

static uint8_t getConstant(uint32_t type, uint32_t index) {
    return constants[type + index];
}

static uint64_t get_round_constant(uint8_t round) {
    uint64_t result = 0;

    const uint8_t roundInfo = getConstant(TYPE_ROUND_INFO, round);
    for (uint_fast8_t i = 0; i <= 6; i++) {
        uint64_t bit = (roundInfo >> i) & 1;
        result |= bit << ((1 << i) - 1);
    }

    return result;
}


/* Initializing a sha3 context for given number of output bits */
void ffx_hash_initKeccak256(FfxKeccak256Context *context) {
    memset(context, 0, sizeof(FfxKeccak256Context));
}

/* Keccak theta() transformation */
static void keccak_theta(uint64_t *A) {
    uint64_t C[5], D[5];

    for (uint_fast8_t i = 0; i < 5; i++) {
        C[i] = A[i];
        for (uint8_t j = 5; j < 25; j += 5) { C[i] ^= A[i + j]; }
    }

    for (uint_fast8_t i = 0; i < 5; i++) {
        D[i] = ROTL64(C[(i + 1) % 5], 1) ^ C[(i + 4) % 5];
    }

    for (uint_fast8_t i = 0; i < 5; i++) {
        for (uint_fast8_t j = 0; j < 25; j += 5) { A[i + j] ^= D[i]; }
    }
}


/* Keccak pi() transformation */
static void keccak_pi(uint64_t *A) {
    uint64_t A1 = A[1];
    for (uint8_t i = 1; i < 24; i++) {
        A[getConstant(TYPE_PI_TRANSFORM, i - 1)] = A[getConstant(TYPE_PI_TRANSFORM, i)];
    }
    A[10] = A1;
    /* note: A[ 0] is left as is */
}

/* Keccak chi() transformation */
static void keccak_chi(uint64_t *A) {
    for (uint_fast8_t i = 0; i < 25; i += 5) {
        uint64_t A0 = A[0 + i], A1 = A[1 + i];
        A[0 + i] ^= ~A1 & A[2 + i];
        A[1 + i] ^= ~A[2 + i] & A[3 + i];
        A[2 + i] ^= ~A[3 + i] & A[4 + i];
        A[3 + i] ^= ~A[4 + i] & A0;
        A[4 + i] ^= ~A0 & A1;
    }
}


static void sha3_permutation(uint64_t *state) {
    for (uint_fast8_t round = 0; round < 24; round++) {
        keccak_theta(state);

        /* apply Keccak rho() transformation */
        for (uint8_t i = 1; i < 25; i++) {
            state[i] = ROTL64(state[i], getConstant(TYPE_RHO_TRANSFORM, i - 1));
        }

        keccak_pi(state);
        keccak_chi(state);

        /* apply iota(state, round) */
        *state ^= get_round_constant(round);
    }
}

/**
 * The core transformation. Process the specified block of data.
 *
 * @param hash the algorithm state
 * @param block the message block to process
 * @param block_size the size of the processed block in bytes
 */
static void sha3_process_block(uint64_t hash[25], const uint64_t *block) {
    for (uint8_t i = 0; i < 17; i++) {
        hash[i] ^= le2me_64(block[i]);
    }

    /* make a permutation of the hash */
    sha3_permutation(hash);
}

//#define SHA3_FINALIZED 0x80000000
//#define SHA3_FINALIZED 0x8000

/**
 * Calculate message hash.
 * Can be called repeatedly with chunks of the message to be hashed.
 *
 * @param context the algorithm context containing current hashing state
 * @param data message chunk
 * @param dataLength length of the message chunk
 */
void ffx_hash_updateKeccak256(FfxKeccak256Context *context,
  const uint8_t *data, size_t dataLength) {
    uint16_t idx = (uint16_t)context->rest;

    //if (context->rest & SHA3_FINALIZED) return; /* too late for additional input */
    context->rest = (unsigned)((context->rest + dataLength) % KECCAK256_BLOCK_SIZE);

    /* fill partial block */
    if (idx) {
        uint16_t left = KECCAK256_BLOCK_SIZE - idx;
        memcpy((char*)context->message + idx, data, (dataLength < left ? dataLength : left));
        if (dataLength < left) return;

        /* process partial block */
        sha3_process_block(context->hash, context->message);
        data  += left;
        dataLength -= left;
    }

    while (dataLength >= KECCAK256_BLOCK_SIZE) {
        uint64_t* aligned_message_block;
        if (IS_ALIGNED_64(data)) {
            // the most common case is processing of an already aligned message without copying it
            aligned_message_block = (uint64_t*)(void*)data;
        } else {
            memcpy(context->message, data, KECCAK256_BLOCK_SIZE);
            aligned_message_block = context->message;
        }

        sha3_process_block(context->hash, aligned_message_block);
        data  += KECCAK256_BLOCK_SIZE;
        dataLength -= KECCAK256_BLOCK_SIZE;
    }

    if (dataLength) {
        memcpy(context->message, data, dataLength); /* save leftovers */
    }
}

/**
* Store calculated hash into the given array.
*
* @param context the algorithm context containing current hashing state
* @param result calculated hash in binary form
*/
void ffx_hash_finalKeccak256(FfxKeccak256Context *context, uint8_t* result) {
    uint16_t digest_length = 100 - KECCAK256_BLOCK_SIZE / 2;

//    if (!(context->rest & SHA3_FINALIZED)) {
        /* clear the rest of the data queue */
        memset((char*)context->message + context->rest, 0, KECCAK256_BLOCK_SIZE - context->rest);
        ((char*)context->message)[context->rest] |= 0x01;
        ((char*)context->message)[KECCAK256_BLOCK_SIZE - 1] |= 0x80;

        /* process final block */
        sha3_process_block(context->hash, context->message);
//        context->rest = SHA3_FINALIZED; /* mark context as finalized */
//    }

    if (result) {
         me64_to_le_str(result, context->hash, digest_length);
    }
}

void ffx_hash_keccak256(uint8_t *digest, const uint8_t *data, size_t length) {
    FfxKeccak256Context ctx;
    ffx_hash_initKeccak256(&ctx);
    ffx_hash_updateKeccak256(&ctx, data, length);
    ffx_hash_finalKeccak256(&ctx, digest);
}
