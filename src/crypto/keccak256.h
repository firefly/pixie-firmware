#ifndef __KECCAK256_H__
#define __KECCAK256_H__

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */


#define KECCAK256_DIGEST_SIZE (32)

#define sha3_max_permutation_size 25
#define sha3_max_rate_in_qwords 24

typedef struct Keccak256Context {
    /* 1600 bits algorithm hashing state */
    uint64_t hash[sha3_max_permutation_size];

    /* 1536-bit buffer for leftovers */
    uint64_t message[sha3_max_rate_in_qwords];

    /* count of bytes in the message[] buffer */
    uint16_t rest;
} Keccak256Context;


void keccak256_init(Keccak256Context *context);
void keccak256_update(Keccak256Context *context, const uint8_t *data, uint32_t dataLength);
void keccak256_final(Keccak256Context *context, uint8_t* digest);

void keccak256_hash(const uint8_t *data, uint32_t dataLength, uint8_t *digest);



#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif  /* __KECCAK256__ */
