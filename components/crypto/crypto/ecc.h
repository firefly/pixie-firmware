
/**
 * A huge thanks to Ken Mackay for the micro-ecc library.
 * See: https://github.com/kmackay/micro-ecc
 *
 * A few changes have been made and are released under the same
 * terms of the original license.
 *
 * Changes include:
 *  - Signatures are normalized to the canonical S value
 *  - The recid (27 or 28) is appended to signatures
 *  - Fully compliant with RFC6979 (based on PR#51)
 *  - Dropped support for secp160r1 (conflicts with RFC6979) and all non-secp256k1 curves
 *  - secp256k1_ prefix added to exported function to protect from esp32 aliasing
 *  - merged into a single implementation file
 *
 * ~ RicMoo <me@ricmoo.com> (https://www.ricmoo.com)
 *
 * ------- Original License Body --------
 *
 * Copyright (c) 2014, Kenneth MacKay
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *  * Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __ECC_H__
#define __ECC_H__

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#include <stdint.h>


#define ECC_SIGNATURE_SIZE             (65)

#define ECC_PRIVATEKEY_SIZE            (32)
#define ECC_DIGEST_SIZE                (32)

#define ECC_PUBLICKEY_SIZE             (65)
#define ECC_COMPRESSED_PUBLICKEY_SIZE  (33)

#define ECC_SHARED_SECRET_SIZE         (32)

#define ECC_SUCCESS  (1)
#define ECC_ERROR    (0)


int32_t ecc_signSecp256k1(uint8_t *privateKey, uint8_t *digest, uint8_t *signature);

int32_t ecc_recoverPublicKeySecp256k1(uint8_t *digest, uint8_t *signature, uint8_t *publicKey);

int32_t ecc_computePublicKeySecp256k1(uint8_t *privateKey, uint8_t *publicKey);

void ecc_compressPublicKeySecp256k1(uint8_t *publicKey, uint8_t *compressedPublicKey);
void ecc_decompressPublicKeySecp256k1(uint8_t *compressedPublicKey, uint8_t *publicKey);

int32_t ecc_computeSharedSecretSecp256k1(uint8_t *privateKey, uint8_t *otherPublicKey, uint8_t *sharedSecret);


int32_t ecc_signP256(uint8_t *privateKey, uint8_t *digest, uint8_t *signature);

int32_t ecc_recoverPublicKeyP256(uint8_t *digest, uint8_t *signature, uint8_t *publicKey);

int32_t ecc_computePublicKeyP256(uint8_t *privateKey, uint8_t *publicKey);

void ecc_compressPublicKeyP256(uint8_t *publicKey, uint8_t *compressedPublicKey);
void ecc_decompressPublicKeyP256(uint8_t *compressedPublicKey, uint8_t *publicKey);

int32_t ecc_computeSharedSecretP256(uint8_t *privateKey, uint8_t *otherPublicKey, uint8_t *sharedSecret);


#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif  /* __ECC_H__ */
