
#include "firefly-address.h"


void ffx_address_checksumAddress(uint8_t *address, char *checksumed) {

    // Add the "0x" prefix and advance the pointer (so we can ignore it)
    checksumed[0] = '0';
    checksumed[1] = 'x';
    checksumed += 2;

    // Place the ASCII representation of the address in checksumed
    const char * const HexNibbles = "0123456789abcdef";
    int offset = 0;
    for (int i = 0; i < 20; i++) {
        checksumed[offset++] = HexNibbles[address[i] >> 4];
        checksumed[offset++] = HexNibbles[address[i] & 0xf];
    }

    // Hash the ASCII representation
    uint8_t digest[KECCAK256_DIGEST_SIZE] = { 0 };
    keccak256_hash(digest, (uint8_t*)checksumed, 40);

    // Uppercase any (alpha) nibble if the coresponding hash nibble >= 8
    for (int i = 0; i < 40; i += 2) {
        uint8_t c = digest[i >> 1];

        if (checksumed[i] >= 'a' && ((c >> 4) >= 8)) {
            checksumed[i] -= 0x20;
        }

        if (checksumed[i + 1] >= 'a' && ((c & 0x0f) >= 8)) {
            checksumed[i + 1] -= 0x20;
        }
    }
}
