#ifndef __SHA2_H__
#define __SHA2_H__

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#include <stdint.h>

#define SHA256_DIGEST_SIZE (32)

typedef struct Sha256Context {
    uint32_t state[8];
    uint32_t bitCount;
    uint32_t buffer[64 / sizeof(uint32_t)];
} Sha256Context;


// Streaming SHA2-256 hashing
void sha256_init(Sha256Context *context);
void sha256_update(Sha256Context *context, const uint8_t *data, uint32_t dataLength);
void sha256_final(Sha256Context *context, uint8_t *digest);

// Single-shot SHA2-256 hashing
//void ethers_sha256(uint8_t *data, uint32_t dataLength, uint8_t *digest);


#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif  /* __SHA2_H__ */
