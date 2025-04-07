#include <stdio.h>
#include <string.h>

#include "firefly-cbor.h"


///////////////////////////////
// Crawler - utils

static FfxCborType _getType(uint8_t header) {
    switch(header >> 5) {
        case 0:
            return FfxCborTypeNumber;
        case 2:
            return FfxCborTypeData;
        case 3:
            return FfxCborTypeString;
        case 4:
            return FfxCborTypeArray;
        case 5:
            return FfxCborTypeMap;
        case 7:
            switch(header & 0x1f) {
                case 20: case 21:
                    return FfxCborTypeBoolean;
                case 22:
                    return FfxCborTypeNull;
            }
            break;
    }
    return FfxCborTypeError;
}

static uint8_t* _getBytes(FfxCborCursor *cursor, FfxCborType *type,
  uint64_t *value, size_t *safe, size_t *headerSize, FfxCborStatus *status) {

    *headerSize = 0;

    size_t length = cursor->length;
    size_t offset = cursor->offset;
    if (offset >= length) {
        *status = FfxCborStatusBufferOverrun;
        return NULL;
    }

    *value = 0;
    *safe = length - offset - 1;
    *status = FfxCborStatusOK;

    uint8_t *data = &cursor->data[cursor->offset];

    uint8_t header = *data++;
    *headerSize = 1;

    *type = _getType(header);

    switch(*type) {
        case FfxCborTypeError:
            *status = FfxCborStatusUnsupportedType;
            return NULL;
        case FfxCborTypeNull:
            *value = 0;
            return data;
        case FfxCborTypeBoolean:
            *value = ((header & 0x1f) == 21) ? 1: 0;
            return data;
        default:
            break;
    }

    // Short value
    uint32_t count = header & 0x1f;
    if (count <= 23) {
        *value = count;
        return data;
    }

    // Indefinite lengths are not currently unsupported
    if (count > 27) {
        *status = FfxCborStatusUnsupportedType;
        return NULL;
    }

    // Count bytes
    // 24 => 0, 25 => 1, 26 => 2, 27 => 3
    count = 1 << (count - 24);
    if (count > *safe) {
        *status = FfxCborStatusBufferOverrun;
        return NULL;
    }

    *headerSize = 1 + count;

    // Read the count bytes as the value
    uint64_t v = 0;
    for (int i = 0; i < count; i++) {
        v = (v << 8) | *data++;
    }
    *value = v;

    // Remove the read bytes from the safe count
    *safe -= count;

    return data;
}


///////////////////////////////
// Crawler

void ffx_cbor_init(FfxCborCursor *cursor, uint8_t *data, size_t length) {
    cursor->data = data;
    cursor->length = length;
    cursor->offset = 0;
    cursor->containerCount = 0;
    cursor->containerIndex = 0;
}

void ffx_cbor_clone(FfxCborCursor *dst, FfxCborCursor *src) {
    memmove(dst, src, sizeof(FfxCborCursor));
}

bool ffx_cbor_isDone(FfxCborCursor *cursor) {
    return (cursor->offset == cursor->length);
}

FfxCborType ffx_cbor_getType(FfxCborCursor *cursor) {
    if (cursor->offset >= cursor->length) { return FfxCborTypeError; }
    return _getType(cursor->data[cursor->offset]);
}

FfxCborStatus ffx_cbor_getValue(FfxCborCursor *cursor, uint64_t *value) {
    FfxCborType type = 0;
    size_t safe = 0, headLen = 0;
    FfxCborStatus status = FfxCborStatusOK;
    uint8_t *data = _getBytes(cursor, &type, value, &safe, &headLen, &status);
    if (data == NULL) { return status; }

    switch (type) {
        // Should never happen
        case FfxCborTypeError:
            return FfxCborStatusUnsupportedType;
        case FfxCborTypeNull: case FfxCborTypeBoolean: case FfxCborTypeNumber:
            return FfxCborStatusOK;
        default:
            break;
    }

    return FfxCborStatusInvalidOperation;
}

// @TODO: refactor these 3 functions

FfxCborStatus ffx_cbor_getData(FfxCborCursor *cursor, uint8_t **outData, size_t *length) {
    *outData = NULL;
    *length = 0;

    FfxCborType type = 0;
    uint64_t value;
    size_t safe = 0, headLen = 0;
    FfxCborStatus status = FfxCborStatusOK;
    uint8_t *data = _getBytes(cursor, &type, &value, &safe, &headLen, &status);
    if (data == NULL) { return status; }

    if (type != FfxCborTypeData && type != FfxCborTypeString) {
        return FfxCborStatusInvalidOperation;
    }

    // Would read beyond our data
    if (value > safe) { return FfxCborStatusBufferOverrun; }

    // Only support lengths up to 16 bits
    if (value > 0xffffffff) { return FfxCborStatusOverflow; }

    *outData = data;
    *length = value;

    return FfxCborStatusOK;
}

FfxCborStatus ffx_cbor_copyData(FfxCborCursor *cursor, uint8_t *output, size_t length) {
    FfxCborType type = 0;
    uint64_t value;
    size_t safe = 0, headLen = 0;
    FfxCborStatus status = FfxCborStatusOK;
    uint8_t *data = _getBytes(cursor, &type, &value, &safe, &headLen, &status);
    if (data == NULL) { return status; }

    if (type != FfxCborTypeData && type != FfxCborTypeString) {
        return FfxCborStatusInvalidOperation;
    }

    // Would read beyond our data
    if (value > safe) { return FfxCborStatusBufferOverrun; }

    // Only support lengths up to 16 bits
    if (value > 0xffffffff) { return FfxCborStatusOverflow; }

    status = FfxCborStatusOK;

    // Not enough space; copy what we can, but mark as truncated
    if (length < value) {
        status = FfxCborStatusTruncated;
        value = length;
    }

    //for (int i = 0; i < value; i++) { output[i] = data[i]; }
    memmove(output, data, value);

    return status;
}

FfxCborStatus ffx_cbor_getLength(FfxCborCursor *cursor, size_t *count) {
    FfxCborType type = 0;
    uint64_t value;
    size_t safe = 0, headLen = 0;;
    FfxCborStatus status = FfxCborStatusOK;
    uint8_t *data = _getBytes(cursor, &type, &value, &safe, &headLen, &status);
    if (data == NULL) {
        *count = 0;
        return status;
    }

    // Only support lengths up to 16 bits
    if (value > 0xffffffff) { return FfxCborStatusOverflow; }

    *count = value;

    switch (type) {
        // Should never happen
        case FfxCborTypeError:
            return FfxCborStatusUnsupportedType;
        case FfxCborTypeData: case FfxCborTypeString:
        case FfxCborTypeArray: case FfxCborTypeMap:
            return FfxCborStatusOK;
        default:
            break;
    }

    return FfxCborStatusInvalidOperation;
}

FfxCborStatus _ffx_cbor_next(FfxCborCursor *cursor) {
    if (cursor->offset == cursor->length) { return FfxCborStatusNotFound; }

    FfxCborType type = 0;
    uint64_t value;
    size_t safe = 0, headLen = 0;
    FfxCborStatus status = FfxCborStatusOK;
    uint8_t *data = _getBytes(cursor, &type, &value, &safe, &headLen, &status);
    if (data == NULL) { return status; }

    switch (type) {
        // Should never happen
        case FfxCborTypeError:
            return FfxCborStatusUnsupportedType;

        // Enters into the first element
        case FfxCborTypeArray: case FfxCborTypeMap:
            // ... falls-through ...

        case FfxCborTypeNull: case FfxCborTypeBoolean: case FfxCborTypeNumber:
            cursor->offset += headLen;
            break;

        case FfxCborTypeData: case FfxCborTypeString:
            cursor->offset += headLen + value;
            break;
    }

    return FfxCborStatusOK;
}

FfxCborStatus ffx_cbor_firstValue(FfxCborCursor *cursor, FfxCborCursor *key) {
    FfxCborType type = 0;
    uint64_t value;
    size_t safe = 0, headLen = 0;
    FfxCborStatus status = FfxCborStatusOK;
    uint8_t *data = _getBytes(cursor, &type, &value, &safe, &headLen, &status);
    if (data == NULL) { return status; }

    if (value == 0) { return FfxCborStatusNotFound; }
    if (value > 0xffffff) { return FfxCborStatusOverflow; }

    FfxCborCursor follow;
    ffx_cbor_clone(&follow, cursor);

    if (type == FfxCborTypeArray) {
        _ffx_cbor_next(&follow);
        follow.containerCount = value;
        follow.containerIndex = 0;
        ffx_cbor_clone(cursor, &follow);
        return FfxCborStatusOK;
    }

    if (type == FfxCborTypeMap) {
        _ffx_cbor_next(&follow);
        if (key) {
            ffx_cbor_clone(key, &follow);
            follow.containerCount = 0;
            follow.containerIndex = 0;
        }
        _ffx_cbor_next(&follow);
        follow.containerCount = -value;
        follow.containerIndex = 0;
        ffx_cbor_clone(cursor, &follow);
        return FfxCborStatusOK;
    }

    return FfxCborStatusInvalidOperation;
}

FfxCborStatus ffx_cbor_nextValue(FfxCborCursor *cursor, FfxCborCursor *key) {
    bool hasKey = false;
    int32_t count = cursor->containerCount;
    if (count == 0) { return FfxCborStatusInvalidOperation; }
    if (count < 0) {
        hasKey = true;
        count *= -1;
    }

    if (cursor->containerIndex + 1 == count) { return FfxCborStatusNotFound; }
    cursor->containerIndex++;

    FfxCborCursor follow;
    ffx_cbor_clone(&follow, cursor);

    FfxCborStatus status = FfxCborStatusOK;
    int32_t skip = 1;
    size_t length = 0;
    while (skip != 0) {
        FfxCborType type = ffx_cbor_getType(&follow);
        if (type == FfxCborTypeArray) {
            status = ffx_cbor_getLength(&follow, &length);
            if (status) { return status; }
            skip += length;
        } else if (type == FfxCborTypeMap) {
            status = ffx_cbor_getLength(&follow, &length);
            if (status) { return status; }
            skip += 2 * length;
        }

        status = _ffx_cbor_next(&follow);
        if (status) { return status; }

        skip--;
    }

    if (hasKey) {
        if (key) {
            ffx_cbor_clone(key, &follow);
            cursor->containerCount = 0;
            cursor->containerIndex = 0;
        }
        _ffx_cbor_next(&follow);
    }

    ffx_cbor_clone(cursor, &follow);

    return FfxCborStatusOK;
}

static bool _keyCompare(const char *key, FfxCborCursor *cursor) {
    size_t sLen = strlen(key);

    size_t cLen = 0;
    ffx_cbor_getLength(cursor, &cLen);

    if (sLen != cLen) { return false; }

    char cStr[cLen];
    ffx_cbor_copyData(cursor, (uint8_t*)cStr, cLen);

    for (int i = 0; i < sLen; i++) {
        if (key[i] != cStr[i]) { return false; }
    }

    return true;
}

FfxCborStatus ffx_cbor_followKey(FfxCborCursor *cursor, const char *key) {
    FfxCborType type = ffx_cbor_getType(cursor);
    if (type != FfxCborTypeMap) { return FfxCborStatusInvalidOperation; }

    FfxCborCursor follow, followKey;
    ffx_cbor_clone(&follow, cursor);

    FfxCborStatus status = ffx_cbor_firstValue(&follow, &followKey);
    if (_keyCompare(key, &followKey)) {
        ffx_cbor_clone(cursor, &follow);
        return FfxCborStatusOK;
    }

    while(status == 0) {
        status = ffx_cbor_nextValue(&follow, &followKey);
        if (_keyCompare(key, &followKey)) {
            ffx_cbor_clone(cursor, &follow);
            return FfxCborStatusOK;
        }
    }

    return FfxCborStatusNotFound;
}

FfxCborStatus ffx_cbor_followIndex(FfxCborCursor *cursor, size_t index) {
    FfxCborType type = ffx_cbor_getType(cursor);

    if (type != FfxCborTypeArray && type != FfxCborTypeMap) {
        return FfxCborStatusInvalidOperation;
    }

    FfxCborCursor follow;
    ffx_cbor_clone(&follow, cursor);

    size_t length;
    FfxCborStatus status = ffx_cbor_getLength(&follow, &length);
    if (status) { return status; }

    if (index >= length) { return FfxCborStatusNotFound; }

    status = ffx_cbor_firstValue(&follow, NULL);
    if (status) { return status; }

    for (int i = 0; i < index; i++) {
        status = ffx_cbor_nextValue(&follow, NULL);
        if (status) { return status; }
    }

    ffx_cbor_clone(cursor, &follow);

    return FfxCborStatusOK;
}

static void _dump(FfxCborCursor *cursor) {
    FfxCborType type = _getType(cursor->data[cursor->offset]);

    switch(type) {
        case FfxCborTypeNumber: {
            uint64_t value;
            ffx_cbor_getValue(cursor, &value);
            printf("%lld", value);
            break;
        }

        case FfxCborTypeString: {
            size_t count;
            ffx_cbor_getLength(cursor, &count);
            char data[count];
            ffx_cbor_copyData(cursor, (uint8_t*)data, count);

            printf("\"");
            for (int i = 0; i < count; i++) {
                char c = data[i];
                if (c == '\n') {
                    printf("\\n");
                } else if (c < 32 || c >= 127) {
                    printf("\\%0x2", c);
                } else if (c == '"') {
                    printf("\\\"");
                } else {
                    printf("%c", c);
                }
            }
            printf("\"");
            break;
        }

        case FfxCborTypeData: {
            size_t count;
            ffx_cbor_getLength(cursor, &count);
            uint8_t data[count];
            ffx_cbor_copyData(cursor, data, count);

            printf("0x");
            for (int i = 0; i < count; i++) { printf("%02x", data[i]); }
            break;
        }

        case FfxCborTypeArray:
            printf("<ARRAY: not impl>");
            break;

        case FfxCborTypeMap: {
            printf("{ ");
            bool first = true;
            FfxCborCursor follow, followKey;
            ffx_cbor_clone(&follow, cursor);
            FfxCborStatus status = ffx_cbor_firstValue(&follow, &followKey);
            while (status == FfxCborStatusOK) {
                if (!first) { printf(", "); }
                first = false;
                _dump(&followKey);
                printf(": ");
                _dump(&follow);
                status = ffx_cbor_nextValue(&follow, &followKey);
            }

            printf(" }");
            break;
        }

        case FfxCborTypeBoolean: {
            uint64_t value;
            ffx_cbor_getValue(cursor, &value);
            printf("%s", value ? "true": "false");
            break;
        }

        case FfxCborTypeNull:
            printf("null");
            break;

        default:
            printf("ERROR");
    }
}

void ffx_cbor_dump(FfxCborCursor *cursor) {
    _dump(cursor);
    printf("\n");
}

///////////////////////////////
// Builder - utils

static FfxCborStatus _appendHeader(FfxCborBuilder *builder, FfxCborType type,
  uint64_t value) {

    size_t remaining = builder->length - builder->offset;

    if (value < 23) {
        if (remaining < 1) { return FfxCborStatusBufferOverrun; }
        builder->data[builder->offset++] = (type << 5) | value;
        return FfxCborStatusOK;
    }

    // Convert to 8 bytes
    size_t inset = 7;
    uint8_t bytes[8] = { 0 };
    for (int i = 7; i >= 0; i--) {
        uint64_t v = (value >> (uint64_t)(56 - (i * 8))) & 0xff;
        bytes[i] = v;
        if (v) { inset = i; }
    }

    // Align the offset to the closest power-of-two within bytes
    uint8_t counts[] = { 27, 27, 27, 27, 26, 26, 25, 24 };
    uint8_t count = counts[inset];
    inset = 8 - (1 << (count - 24));

    if (remaining < 1 + (8 - inset)) { return FfxCborStatusBufferOverrun; }

    size_t offset = builder->offset;
    builder->data[offset++] = (type << 5) | count;
    for (int i = inset; i < 8; i++) {
        builder->data[offset++] = bytes[i];
    }
    builder->offset = offset;

    return FfxCborStatusOK;
}


///////////////////////////////
// Builder

void ffx_cbor_build(FfxCborBuilder *builder, uint8_t *data, size_t length) {
    builder->data = data;
    builder->length = length;
    builder->offset = 0;
}

size_t ffx_cbor_getBuildLength(FfxCborBuilder *builder) {
    return builder->offset;
}

FfxCborStatus ffx_cbor_appendBoolean(FfxCborBuilder *builder, bool value) {
    size_t remaining = builder->length - builder->offset;
    if (remaining < 1) { return FfxCborStatusBufferOverrun; }
    size_t offset = builder->offset;
    builder->data[offset++] = (7 << 5) | (value ? 21: 20);
    builder->offset = offset;
    return FfxCborStatusOK;
}

FfxCborStatus ffx_cbor_appendNull(FfxCborBuilder *builder) {
    size_t remaining = builder->length - builder->offset;
    if (remaining < 1) { return FfxCborStatusBufferOverrun; }
    size_t offset = builder->offset;
    builder->data[offset++] = (7 << 5) | 22;
    builder->offset = offset;
    return FfxCborStatusOK;
}

FfxCborStatus ffx_cbor_appendNumber(FfxCborBuilder *builder, uint64_t value) {
    return _appendHeader(builder, 0, value);
}

FfxCborStatus ffx_cbor_appendData(FfxCborBuilder *builder, uint8_t *data, size_t length) {
    FfxCborStatus status = _appendHeader(builder, 2, length);
    if (status) { return status; }

    size_t remaining = builder->length - builder->offset;
    if (remaining < length) { return FfxCborStatusBufferOverrun; }

    memmove(&builder->data[builder->offset], data, length);
    builder->offset += length;

    return FfxCborStatusOK;
}

FfxCborStatus ffx_cbor_appendString(FfxCborBuilder *builder, char* str) {
    size_t length = strlen(str);

    FfxCborStatus status = _appendHeader(builder, 3, length);
    if (status) { return status; }

    size_t remaining = builder->length - builder->offset;
    if (remaining < length) { return FfxCborStatusBufferOverrun; }

    memmove(&builder->data[builder->offset], str, length);
    builder->offset += length;

    return FfxCborStatusOK;
}

FfxCborStatus ffx_cbor_appendArray(FfxCborBuilder *builder, size_t count) {
    return _appendHeader(builder, 4, count);
}

FfxCborStatus ffx_cbor_appendMap(FfxCborBuilder *builder, size_t count) {
    return _appendHeader(builder, 5, count);
}

FfxCborStatus ffx_cbor_appendArrayMutable(FfxCborBuilder *builder, FfxCborBuilderTag *tag) {
    size_t remaining = builder->length - builder->offset;
    if (remaining < 3) { return FfxCborStatusBufferOverrun; }

    builder->data[builder->offset++] = (4 << 5) | 25;

    *tag = builder->offset;

    builder->data[builder->offset++] = 0;
    builder->data[builder->offset++] = 0;

    return FfxCborStatusOK;
}

FfxCborStatus ffx_cbor_appendMapMutable(FfxCborBuilder *builder, FfxCborBuilderTag *tag) {
    size_t remaining = builder->length - builder->offset;
    if (remaining < 3) { return FfxCborStatusBufferOverrun; }

    builder->data[builder->offset++] = (5 << 5) | 25;

    *tag = builder->offset;

    builder->data[builder->offset++] = 0;
    builder->data[builder->offset++] = 0;

    return FfxCborStatusOK;
}

void ffx_cbor_adjustCount(FfxCborBuilder *builder, FfxCborBuilderTag tag,
  uint16_t count) {
    builder->data[tag++] = (count >> 16) & 0xff;
    builder->data[tag++] = count & 0xff;
}

FfxCborStatus ffx_cbor_appendCborRaw(FfxCborBuilder *builder, uint8_t *data,
  size_t length) {

    size_t remaining = builder->length - builder->offset;
    if (remaining < length) { return FfxCborStatusBufferOverrun; }

    size_t offset = builder->offset;
    memmove(&builder->data[offset], data, length);
    builder->offset = offset + length;

    return FfxCborStatusOK;
}

FfxCborStatus ffx_cbor_appendCborBuilder(FfxCborBuilder *dst, FfxCborBuilder *src) {
    return ffx_cbor_appendCborRaw(dst, src->data, src->offset);
}

