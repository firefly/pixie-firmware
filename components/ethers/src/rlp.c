#include <string.h>

#include "firefly-rlp.h"


#define TAG_ARRAY         (0xc0)
#define TAG_DATA          (0x80)

// This mask can be used to check the value type
#define TAG_MASK          (0xc0)

// This is not stored, just used as a hint to appendHeader that
// a non-compact 4-byte size should be used regardless
#define TAG_RESERVE       (0x00)

/**
 *  RLP
 *
 *  If the top two bits are set, then it is an Array. If the top
 *  bit is set, then it is a data.
 *  If the bottom 6 bits are less than 55, that is the length in
 *  bytes. Otherwise, that is the number of bytes to additionally
 *  consume to get the length.
 */

static size_t getByteCount(size_t value) {
    if (value < 0x100) { return 1; }
    if (value < 0x10000) { return 2; }
    if (value < 0x1000000) { return 3; }
    return 4;
}

static FfxRlpStatus appendByte(FfxRlpBuilder *rlp, uint8_t byte) {
    size_t remaining = rlp->length - rlp->offset;
    if (remaining < 1) { return FfxRlpStatusBufferOverrun; }
    rlp->data[rlp->offset++] = byte;
    return FfxRlpStatusOK;
}

static FfxRlpStatus appendBytes(FfxRlpBuilder *rlp, uint8_t *data, size_t length) {
    size_t remaining = rlp->length - rlp->offset;
    if (remaining < length) { return FfxRlpStatusBufferOverrun; }
    memmove(&rlp->data[rlp->offset], data, length);
    rlp->offset += length;
    return FfxRlpStatusOK;
}

// The TAG_RESERVE indicates we are leaving space in the Array. It
// currently has the number of items in the array, which will need
// to be swapped out for number of bytes in the final RLP.
static FfxRlpStatus appendHeader(FfxRlpBuilder *rlp, uint8_t tag, size_t length) {

    if (tag != TAG_RESERVE && length <= 55) {
        return appendByte(rlp, tag + length);
    }

    size_t byteCount = 0;
    if (tag == TAG_RESERVE) {
        byteCount = 4;
        tag = TAG_ARRAY;
    } else {
        byteCount = getByteCount(length);
    }

    FfxRlpStatus status = appendByte(rlp, tag + 55 + byteCount);
    if (status) { return status; }

    for (int i = byteCount - 1; i >= 0; i--) {
        FfxRlpStatus status = appendByte(rlp, length >> (8 * i));
        if (status) { return status; }
    }

    return FfxRlpStatusOK;
}


///////////////////////////////
// API

void ffx_rlp_build(FfxRlpBuilder *rlp, uint8_t *data, size_t length) {
    rlp->data = data;
    rlp->offset = 0;
    rlp->length = length;
}

FfxRlpStatus ffx_rlp_appendData(FfxRlpBuilder *rlp, uint8_t *data, size_t length) {
    if (length == 1 && data[0] <= 127) {
        return appendByte(rlp, data[0]);
    }

    FfxRlpStatus status = appendHeader(rlp, TAG_DATA, length);
    if (status) { return status; }

    return appendBytes(rlp, data, length);
}

FfxRlpStatus ffx_rlp_appendString(FfxRlpBuilder *rlp, const char *data) {
    return ffx_rlp_appendData(rlp, (uint8_t*)data, strlen(data));
}

FfxRlpStatus ffx_rlp_appendArray(FfxRlpBuilder *rlp, size_t count) {
    // Zero-length arrays can be stored directly in their compact
    // representation. Otherwise we reserve 4 bytes where we include
    // the length in items to fix in finalize
    return appendHeader(rlp, count ? TAG_RESERVE: TAG_ARRAY, count);
}

static size_t readValue(uint8_t *data, size_t count) {
    size_t v = 0;
    for (int i = 0; i < count; i++) {
        v <<= 8;
        v |= data[i];
    }
    return v;
}

static size_t finalize(FfxRlpBuilder *rlp) {
    uint8_t v = rlp->data[rlp->offset];

    if (v <= 127) { return 1; }

    // Data or non-4-byte Array are already compact
    if ((v & TAG_MASK) == TAG_DATA || v != (TAG_ARRAY + 55 + 4)) {
        v &= 0x3f;

        if (v <= 55) { return 1 + v; }
        v -= 55;

        // Overflow!
        if (v > 4) { return 0; }

        return 1 + v + readValue(&rlp->data[rlp->offset + 1], v);
    }

    // The base offset of this array and start of child data
    size_t baseOffset = rlp->offset;
    size_t dataOffset = baseOffset + 5;

    size_t count = readValue(&rlp->data[rlp->offset + 1], 4);
    rlp->offset = dataOffset;

    size_t length = 0;
    for (int i = 0; i < count; i++) {
        size_t l = finalize(rlp);
        if (l == 0) { return FfxRlpStatusOverflow; }
        rlp->offset += l;
        length += l;
    }

    // Overwrite the 5-byte header with its compact form
    rlp->offset = baseOffset;
    FfxRlpStatus status = appendHeader(rlp, TAG_ARRAY, length);
    if (status) { return 0; }

    // Compact the children, shifting the data left.
    if (rlp->offset != dataOffset) {
        memmove(&rlp->data[rlp->offset], &rlp->data[dataOffset],
          rlp->length - dataOffset);
    }

    // The header length and the content length
    return rlp->offset - baseOffset + length;
}

size_t ffx_rlp_finalize(FfxRlpBuilder *rlp) {
    // Store the non-compact length to minimize compaction memmoves
    rlp->length = rlp->offset;

    // Move to the start of the data
    rlp->offset = 0;

    // Finalize recursively
    return finalize(rlp);
}

