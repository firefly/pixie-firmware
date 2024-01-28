#include "crypto/bip32.h"

int32_t bip32_initWithPhrase(const char* phrase, const char *password, Bip32Node *node) {
    return 0;
}

int32_t bip32_initWithSeed(uint8_t *seed, size_t seedLength, Bip32Node *node) {
    return 0;
}

int32_t bip32_deriveChild(Bip32Node *parent, Bip32Node *child, uint32_t index) {
    return 0;
}

int32_t bip32_deriveNueteredChild(Bip32NueteredNode *parent, Bip32NueteredNode *child, uint32_t index) {
    return 0;
}

int32_t bip32_nueter(Bip32Node *node, Bip32NueteredNode *nueteredNode) {
    return 0;
}

int32_t bip32_extendedKey(Bip32Node *node, uint8_t *extendedKey) {
    return 0;
}
