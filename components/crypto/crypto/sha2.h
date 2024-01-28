#ifndef __SHA2_H__
#define __SHA2_H__

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#include <stdint.h>

#define SHA256_DIGEST_SIZE (32)
#define SHA512_DIGEST_SIZE (64)


#define SHA256_BLOCK_LENGTH		(64)
#define SHA512_BLOCK_LENGTH		(128)


typedef struct Sha256Context {
    uint32_t state[8];
    uint32_t bitCount;
    uint32_t buffer[SHA256_BLOCK_LENGTH / sizeof(uint32_t)];
} Sha256Context;

typedef struct Sha512Context {
	uint64_t	state[8];
	uint64_t	bitcount[2];
	uint64_t	buffer[SHA512_BLOCK_LENGTH / sizeof(uint64_t)];
} Sha512Context;

typedef struct HmacSha256Context {
  uint8_t o_key_pad[SHA256_BLOCK_LENGTH];
  Sha512Context ctx;
} HmacSha256Context;

typedef struct HmacSha512Context {
  uint8_t o_key_pad[SHA512_BLOCK_LENGTH];
  Sha512Context ctx;
} HmacSha512Context;


void sha2_initSha256(Sha256Context *context);
void sha2_updateSha256(Sha256Context *context, const uint8_t *data, uint32_t dataLength);
void sha2_finalSha256(Sha256Context *context, uint8_t *digest);

void sha2_initHmacSha256(HmacSha256Context *context, const uint8_t *key, const uint32_t keylen);
void sha2_updateHmacSha256(HmacSha256Context *context, const uint8_t *data, const uint32_t dataLength);
void sha2_finalHmacSha256(HmacSha256Context *context, uint8_t *hmac);

void sha2_initSha512(Sha512Context *context);
void sha2_updateSha512(Sha512Context *context, const uint8_t *data, uint32_t dataLength);
void sha2_finalSha512(Sha512Context *context, uint8_t *digest);

void sha2_initHmacSha512(HmacSha512Context *context, const uint8_t *key, const uint32_t keylen);
void sha2_updateHmacSha512(HmacSha512Context *context, const uint8_t *data, const uint32_t dataLength);
void sha2_finalHmacSha512(HmacSha512Context *context, uint8_t *hmac);


#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif  /* __SHA2_H__ */
