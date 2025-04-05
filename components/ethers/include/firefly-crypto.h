#ifndef __FIREFLY_CRYPTO_H__
#define __FIREFLY_CRYPTO_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#define FFX_PRIVKEY_LENGTH                    (32)
#define FFX_PUBKEY_LENGTH                     (65)
#define FFX_COMP_PUBKEY_LENGTH                (33)

#define FFX_SECP256K1_SIGNATURE_LENGTH        (65)
//#define FFX_P256_SIGNATURE_LENGTH             (65)

#define FFX_SECP256K1_DIGEST_LENGTH           (32)
#define FFX_P256_DIGEST_LENGTH                (32)

#define FFX_SHARED_SECRET_LENGTH              (32)


bool ffx_pk_signSecp256k1(uint8_t *privkey, uint8_t *digest,
  uint8_t *signature);

bool ffx_pk_recoverPubkeySecp256k1(uint8_t *digest,
  uint8_t *signature, uint8_t *pubkey);

bool ffx_pk_computePubkeySecp256k1(uint8_t *privkey,
  uint8_t *pubkey);

void ffx_pk_compressPubkeySecp256k1(uint8_t *pubkey, uint8_t *compPubkey);
void ffx_pk_decompressPubkeySecp256k1(uint8_t *compPubkey, uint8_t *pubkey);

bool ffx_pk_computeSharedSecretSecp256k1(uint8_t *privkey,
  uint8_t *otherPubkey, uint8_t *sharedSecret);


bool ffx_pk_signP256(uint8_t *privkey, uint8_t *digest,
  uint8_t *signature);

bool ecc_recoverPubkeyP256(uint8_t *digest, uint8_t *signature,
  uint8_t *pubkey);

bool ffx_pk_computePubkeyP256(uint8_t *privkey, uint8_t *pubkey);

void ffx_pk_compressPubkeyP256(uint8_t *pubkey, uint8_t *compPubkey);
void ffx_pk_decompressPubkeyP256(uint8_t *compPubkey, uint8_t *pubkey);

bool ffx_pk_computeSharedSecretP256(uint8_t *privkey,
  uint8_t *otherPubkey, uint8_t *sharedSecret);



#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __FIREFLY_CRYPTO_H__ */
