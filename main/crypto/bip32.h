#ifndef __BIP32_H__
#define __BIP32_H__

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#include <stddef.h>
#include <stdint.h>

typedef struct Bip32Node {
    uint8_t privateKey[32];
    uint8_t publicKey[33];
    uint8_t chainCode[32];
    uint32_t index;
} Bip32Node;

typedef struct Bip32NueteredNode {
    uint8_t publicKey[33];
    uint8_t chainCode[32];
    uint32_t index;
} Bip32NueteredNode;


int32_t bip32_initWithPhrase(const char* phrase, const char *password, Bip32Node *node);
int32_t bip32_initWithSeed(uint8_t *seed, size_t seedLength, Bip32Node *node);
int32_t bip32_initWithXpriv(const char *xpriv, size_t xpubLength, Bip32Node *node);
int32_t bip32_initWithXpub(const char *xpub, size_t xpubLength, Bip32NueteredNode *node);

int32_t bip32_deriveChild(Bip32Node *parent, Bip32Node *child, uint32_t index);
int32_t bip32_deriveNueteredChild(Bip32NueteredNode *parent, Bip32NueteredNode *child, uint32_t index);

int32_t bip32_nueter(Bip32Node *node, Bip32NueteredNode *nueteredNode);

int32_t bip32_getXpub(Bip32NueteredNode *node, const char *xpub, size_t xpubLength);
int32_t bip32_getXpriv(Bip32Node *node, const char *xpriv, size_t xprivLength);


#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif  /* __BIP32__ */
