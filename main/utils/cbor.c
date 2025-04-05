
#include <string.h>

#include "./cbor.h"


// DEBUG
#include <stdio.h>


///////////////////////////////
// Crawler - utils

static CborType _getType(uint8_t header) {
    switch(header >> 5) {
        case 0:
            return CborTypeNumber;
        case 2:
            return CborTypeData;
        case 3:
            return CborTypeString;
        case 4:
            return CborTypeArray;
        case 5:
            return CborTypeMap;
        case 7:
            switch(header & 0x1f) {
                case 20: case 21:
                    return CborTypeBoolean;
                case 22:
                    return CborTypeNull;
            }
            break;
    }
    return CborTypeError;
}

static uint8_t* _getBytes(CborCursor *cursor, CborType *type,
  uint64_t *value, size_t *safe, size_t *headerSize, CborStatus *status) {

    *headerSize = 0;

    size_t length = cursor->length;
    size_t offset = cursor->offset;
    if (offset >= length) {
        *status = CborStatusBufferOverrun;
        return NULL;
    }

    *value = 0;
    *safe = length - offset - 1;
    *status = CborStatusOK;

    uint8_t *data = &cursor->data[cursor->offset];

    uint8_t header = *data++;
    *headerSize = 1;

    *type = _getType(header);

    switch(*type) {
        case CborTypeError:
            *status = CborStatusUnsupportedType;
            return NULL;
        case CborTypeNull:
            *value = 0;
            return data;
        case CborTypeBoolean:
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
        *status = CborStatusUnsupportedType;
        return NULL;
    }

    // Count bytes
    // 24 => 0, 25 => 1, 26 => 2, 27 => 3
    count = 1 << (count - 24);
    if (count > *safe) {
        *status = CborStatusBufferOverrun;
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

void cbor_init(CborCursor *cursor, uint8_t *data, size_t length) {
    cursor->data = data;
    cursor->length = length;
    cursor->offset = 0;
    cursor->containerCount = 0;
    cursor->containerIndex = 0;
}

void cbor_clone(CborCursor *dst, CborCursor *src) {
    memmove(dst, src, sizeof(CborCursor));
}

bool cbor_isDone(CborCursor *cursor) {
    return (cursor->offset == cursor->length);
}

CborType cbor_getType(CborCursor *cursor) {
    if (cursor->offset >= cursor->length) { return CborTypeError; }
    return _getType(cursor->data[cursor->offset]);
}

CborStatus cbor_getValue(CborCursor *cursor, uint64_t *value) {
    CborType type = 0;
    size_t safe = 0, headLen = 0;
    CborStatus status = CborStatusOK;
    uint8_t *data = _getBytes(cursor, &type, value, &safe, &headLen, &status);
    if (data == NULL) { return status; }

    switch (type) {
        // Should never happen
        case CborTypeError:
            return CborStatusUnsupportedType;
        case CborTypeNull: case CborTypeBoolean: case CborTypeNumber:
            return CborStatusOK;
        default:
            break;
    }

    return CborStatusInvalidOperation;
}

// @TODO: refactor these 3 functions

CborStatus cbor_getData(CborCursor *cursor, uint8_t **outData, size_t *length) {
    *outData = NULL;
    *length = 0;

    CborType type = 0;
    uint64_t value;
    size_t safe = 0, headLen = 0;
    CborStatus status = CborStatusOK;
    uint8_t *data = _getBytes(cursor, &type, &value, &safe, &headLen, &status);
    if (data == NULL) { return status; }

    if (type != CborTypeData && type != CborTypeString) {
        return CborStatusInvalidOperation;
    }

    // Would read beyond our data
    if (value > safe) { return CborStatusBufferOverrun; }

    // Only support lengths up to 16 bits
    if (value > 0xffffffff) { return CborStatusOverflow; }

    *outData = data;
    *length = value;

    return CborStatusOK;
}

CborStatus cbor_copyData(CborCursor *cursor, uint8_t *output, size_t length) {
    CborType type = 0;
    uint64_t value;
    size_t safe = 0, headLen = 0;
    CborStatus status = CborStatusOK;
    uint8_t *data = _getBytes(cursor, &type, &value, &safe, &headLen, &status);
    if (data == NULL) { return status; }

    if (type != CborTypeData && type != CborTypeString) {
        return CborStatusInvalidOperation;
    }

    // Would read beyond our data
    if (value > safe) { return CborStatusBufferOverrun; }

    // Only support lengths up to 16 bits
    if (value > 0xffffffff) { return CborStatusOverflow; }

    status = CborStatusOK;

    // Not enough space; copy what we can, but mark as truncated
    if (length < value) {
        status = CborStatusTruncated;
        value = length;
    }

    //for (int i = 0; i < value; i++) { output[i] = data[i]; }
    memmove(output, data, value);

    return status;
}

CborStatus cbor_getLength(CborCursor *cursor, size_t *count) {
    CborType type = 0;
    uint64_t value;
    size_t safe = 0, headLen = 0;;
    CborStatus status = CborStatusOK;
    uint8_t *data = _getBytes(cursor, &type, &value, &safe, &headLen, &status);
    if (data == NULL) {
        *count = 0;
        return status;
    }

    // Only support lengths up to 16 bits
    if (value > 0xffffffff) { return CborStatusOverflow; }

    *count = value;

    switch (type) {
        // Should never happen
        case CborTypeError:
            return CborStatusUnsupportedType;
        case CborTypeData: case CborTypeString:
        case CborTypeArray: case CborTypeMap:
            return CborStatusOK;
        default:
            break;
    }

    return CborStatusInvalidOperation;
}

CborStatus _cbor_next(CborCursor *cursor) {
    if (cursor->offset == cursor->length) { return CborStatusNotFound; }

    CborType type = 0;
    uint64_t value;
    size_t safe = 0, headLen = 0;
    CborStatus status = CborStatusOK;
    uint8_t *data = _getBytes(cursor, &type, &value, &safe, &headLen, &status);
    if (data == NULL) { return status; }

    switch (type) {
        // Should never happen
        case CborTypeError:
            return CborStatusUnsupportedType;

        // Enters into the first element
        case CborTypeArray: case CborTypeMap:
            // ... falls-through ...

        case CborTypeNull: case CborTypeBoolean: case CborTypeNumber:
            cursor->offset += headLen;
            break;

        case CborTypeData: case CborTypeString:
            cursor->offset += headLen + value;
            break;
    }

    return CborStatusOK;
}

CborStatus cbor_firstValue(CborCursor *cursor, CborCursor *key) {
    CborType type = 0;
    uint64_t value;
    size_t safe = 0, headLen = 0;
    CborStatus status = CborStatusOK;
    uint8_t *data = _getBytes(cursor, &type, &value, &safe, &headLen, &status);
    if (data == NULL) { return status; }

    if (value == 0) { return CborStatusNotFound; }
    if (value > 0xffffff) { return CborStatusOverflow; }

    CborCursor follow;
    cbor_clone(&follow, cursor);

    if (type == CborTypeArray) {
        _cbor_next(&follow);
        follow.containerCount = value;
        follow.containerIndex = 0;
        cbor_clone(cursor, &follow);
        return CborStatusOK;
    }

    if (type == CborTypeMap) {
        _cbor_next(&follow);
        if (key) {
            cbor_clone(key, &follow);
            follow.containerCount = 0;
            follow.containerIndex = 0;
        }
        _cbor_next(&follow);
        follow.containerCount = -value;
        follow.containerIndex = 0;
        cbor_clone(cursor, &follow);
        return CborStatusOK;
    }

    return CborStatusInvalidOperation;
}

CborStatus cbor_nextValue(CborCursor *cursor, CborCursor *key) {
    bool hasKey = false;
    int32_t count = cursor->containerCount;
    if (count == 0) { return CborStatusInvalidOperation; }
    if (count < 0) {
        hasKey = true;
        count *= -1;
    }

    if (cursor->containerIndex + 1 == count) { return CborStatusNotFound; }
    cursor->containerIndex++;

    CborCursor follow;
    cbor_clone(&follow, cursor);

    CborStatus status = CborStatusOK;
    int32_t skip = 1;
    size_t length = 0;
    while (skip != 0) {
        CborType type = cbor_getType(&follow);
        if (type == CborTypeArray) {
            status = cbor_getLength(&follow, &length);
            if (status) { return status; }
            skip += length;
        } else if (type == CborTypeMap) {
            status = cbor_getLength(&follow, &length);
            if (status) { return status; }
            skip += 2 * length;
        }

        status = _cbor_next(&follow);
        if (status) { return status; }

        skip--;
    }

    if (hasKey) {
        if (key) {
            cbor_clone(key, &follow);
            cursor->containerCount = 0;
            cursor->containerIndex = 0;
        }
        _cbor_next(&follow);
    }

    cbor_clone(cursor, &follow);

    return CborStatusOK;
}

static bool _keyCompare(const char *key, CborCursor *cursor) {
    size_t sLen = strlen(key);

    size_t cLen = 0;
    cbor_getLength(cursor, &cLen);

    if (sLen != cLen) { return false; }

    char cStr[cLen];
    cbor_copyData(cursor, (uint8_t*)cStr, cLen);

    for (int i = 0; i < sLen; i++) {
        if (key[i] != cStr[i]) { return false; }
    }

    return true;
}

CborStatus cbor_followKey(CborCursor *cursor, const char *key) {
    CborType type = cbor_getType(cursor);
    if (type != CborTypeMap) { return CborStatusInvalidOperation; }

    CborCursor follow, followKey;
    cbor_clone(&follow, cursor);

    CborStatus status = cbor_firstValue(&follow, &followKey);
    if (_keyCompare(key, &followKey)) {
        cbor_clone(cursor, &follow);
        return CborStatusOK;
    }

    while(status == 0) {
        status = cbor_nextValue(&follow, &followKey);
        if (_keyCompare(key, &followKey)) {
            cbor_clone(cursor, &follow);
            return CborStatusOK;
        }
    }

    return CborStatusNotFound;
}

CborStatus cbor_followIndex(CborCursor *cursor, size_t index) {
    CborType type = cbor_getType(cursor);

    if (type != CborTypeArray && type != CborTypeMap) {
        return CborStatusInvalidOperation;
    }

    CborCursor follow;
    cbor_clone(&follow, cursor);

    size_t length;
    CborStatus status = cbor_getLength(&follow, &length);
    if (status) { return status; }

    if (index >= length) { return CborStatusNotFound; }

    status = cbor_firstValue(&follow, NULL);
    if (status) { return status; }

    for (int i = 0; i < index; i++) {
        status = cbor_nextValue(&follow, NULL);
        if (status) { return status; }
    }

    cbor_clone(cursor, &follow);

    return CborStatusOK;
}

static void _dump(CborCursor *cursor) {
    CborType type = _getType(cursor->data[cursor->offset]);

    switch(type) {
        case CborTypeNumber: {
            uint64_t value;
            cbor_getValue(cursor, &value);
            printf("%lld", value);
            break;
        }

        case CborTypeString: {
            size_t count;
            cbor_getLength(cursor, &count);
            char data[count];
            cbor_copyData(cursor, (uint8_t*)data, count);

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

        case CborTypeData: {
            size_t count;
            cbor_getLength(cursor, &count);
            uint8_t data[count];
            cbor_copyData(cursor, data, count);

            printf("0x");
            for (int i = 0; i < count; i++) { printf("%02x", data[i]); }
            break;
        }

        case CborTypeArray:
            printf("<ARRAY: not impl>");
            break;

        case CborTypeMap: {
            printf("{ ");
            bool first = true;
            CborCursor follow, followKey;
            cbor_clone(&follow, cursor);
            CborStatus status = cbor_firstValue(&follow, &followKey);
            while (status == CborStatusOK) {
                if (!first) { printf(", "); }
                first = false;
                _dump(&followKey);
                printf(": ");
                _dump(&follow);
                status = cbor_nextValue(&follow, &followKey);
            }

            printf(" }");
            break;
        }

        case CborTypeBoolean: {
            uint64_t value;
            cbor_getValue(cursor, &value);
            printf("%s", value ? "true": "false");
            break;
        }

        case CborTypeNull:
            printf("null");
            break;

        default:
            printf("ERROR");
    }
}

void cbor_dump(CborCursor *cursor) {
    _dump(cursor);
    printf("\n");
}

///////////////////////////////
// Builder - utils

static CborStatus _appendHeader(CborBuilder *builder, CborType type,
  uint64_t value) {

    size_t remaining = builder->length - builder->offset;

    if (value < 23) {
        if (remaining < 1) { return CborStatusBufferOverrun; }
        builder->data[builder->offset++] = (type << 5) | value;
        return CborStatusOK;
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

    if (remaining < 1 + (8 - inset)) { return CborStatusBufferOverrun; }

    size_t offset = builder->offset;
    builder->data[offset++] = (type << 5) | count;
    for (int i = inset; i < 8; i++) {
        builder->data[offset++] = bytes[i];
    }
    builder->offset = offset;

    return CborStatusOK;
}


///////////////////////////////
// Builder

void cbor_build(CborBuilder *builder, uint8_t *data, size_t length) {
    builder->data = data;
    builder->length = length;
    builder->offset = 0;
}

size_t cbor_getBuildLength(CborBuilder *builder) {
    return builder->offset;
}

CborStatus cbor_appendBoolean(CborBuilder *builder, bool value) {
    size_t remaining = builder->length - builder->offset;
    if (remaining < 1) { return CborStatusBufferOverrun; }
    size_t offset = builder->offset;
    builder->data[offset++] = (7 << 5) | (value ? 21: 20);
    builder->offset = offset;
    return CborStatusOK;
}

CborStatus cbor_appendNull(CborBuilder *builder) {
    size_t remaining = builder->length - builder->offset;
    if (remaining < 1) { return CborStatusBufferOverrun; }
    size_t offset = builder->offset;
    builder->data[offset++] = (7 << 5) | 22;
    builder->offset = offset;
    return CborStatusOK;
}

CborStatus cbor_appendNumber(CborBuilder *builder, uint64_t value) {
    return _appendHeader(builder, 0, value);
}

CborStatus cbor_appendData(CborBuilder *builder, uint8_t *data, size_t length) {
    CborStatus status = _appendHeader(builder, 2, length);
    if (status) { return status; }

    size_t remaining = builder->length - builder->offset;
    if (remaining < length) { return CborStatusBufferOverrun; }

    //for (int i = 0; i < length; i++) {
    //    builder->data[builder->offset++] = data[i];
    //}
    size_t offset = builder->offset;
    memmove(&builder->data[offset], data, length);
    builder->offset = offset + length;

    return CborStatusOK;
}

CborStatus cbor_appendString(CborBuilder *builder, char* str) {
    size_t length = strlen(str);

    CborStatus status = _appendHeader(builder, 3, length);
    if (status) { return status; }

    size_t remaining = builder->length - builder->offset;
    if (remaining < length) { return CborStatusBufferOverrun; }

    //for (int i = 0; i < length; i++) {
    //    builder->data[builder->offset++] = str[i];
    //}
    size_t offset = builder->offset;
    memmove(&builder->data[offset], str, length);
    builder->offset = offset + length;

    return CborStatusOK;
}

CborStatus cbor_appendArray(CborBuilder *builder, size_t count) {
    return _appendHeader(builder, 4, count);
}

CborStatus cbor_appendMap(CborBuilder *builder, size_t count) {
    return _appendHeader(builder, 5, count);
}

CborStatus cbor_appendArrayMutable(CborBuilder *builder, CborBuilderTag *tag) {
    size_t remaining = builder->length - builder->offset;
    if (remaining < 3) { return CborStatusBufferOverrun; }

    builder->data[builder->offset++] = (4 << 5) | 25;

    *tag = builder->offset;

    builder->data[builder->offset++] = 0;
    builder->data[builder->offset++] = 0;

    return CborStatusOK;
}

CborStatus cbor_appendMapMutable(CborBuilder *builder, CborBuilderTag *tag) {
    size_t remaining = builder->length - builder->offset;
    if (remaining < 3) { return CborStatusBufferOverrun; }

    builder->data[builder->offset++] = (5 << 5) | 25;

    *tag = builder->offset;

    builder->data[builder->offset++] = 0;
    builder->data[builder->offset++] = 0;

    return CborStatusOK;
}

void cbor_adjustCount(CborBuilder *builder, CborBuilderTag tag,
  uint16_t count) {
    builder->data[tag++] = (count >> 16) & 0xff;
    builder->data[tag++] = count & 0xff;
}

CborStatus cbor_appendCborRaw(CborBuilder *builder, uint8_t *data,
  size_t length) {

    size_t remaining = builder->length - builder->offset;
    if (remaining < length) { return CborStatusBufferOverrun; }

    size_t offset = builder->offset;
    memmove(&builder->data[offset], data, length);
    builder->offset = offset + length;

    return CborStatusOK;
}

CborStatus cbor_appendCborBuilder(CborBuilder *dst, CborBuilder *src) {
    return cbor_appendCborRaw(dst, src->data, src->offset);
}


///////////////////////////////
// RLP
/*
static size_t getByteCount(size_t value) {
    if (value < 0x100) { return 1; }
    if (value < 0x10000) { return 2; }
    if (value < 0x1000000) { return 3; }
    return 4;
}

static CborStatus appendByte(CborBuilder *rlp, uint8_t byte) {
    size_t remaining = rlp->length - rlp->offset;
    if (remaining < 1) { return CborStatusBufferOverrun; }
    rlp->data[rlp->offset++] = byte;
    return CborStatusOK;
}

static CborStatus appendBytes(CborBuilder *rlp, uint8_t *data, size_t length) {
    size_t remaining = rlp->length - rlp->offset;
    if (remaining < length) { return CborStatusBufferOverrun; }
    memmove(&rlp->data[rlp->offset], data, length);
    rlp->offset += length;
    return CborStatusOK;
}

static CborStatus appendHeader(CborBuilder *rlp, uint8_t prefix, size_t size) {
    if (size <= 55) { return appendByte(rlp, prefix + size); }

    size_t byteCount = getByteCount(size);

    CborStatus status = appendByte(rlp, prefix + 55 + byteCount);
    if (status) { return status; }

    for (int i = byteCount - 1; i >= 0; i--) {
        status = appendByte(rlp, size >> (8 * byteCount));
        if (status) { return status; }
    }

    return CborStatusOK;
}

static size_t rlpSize(CborCursor *_cursor);

// Returns the RLP size of the sum of all array children
static size_t rlpArraySize(CborCursor *_cursor) {
    CborCursor cursor;
    cbor_clone(&cursor, _cursor);

    size_t size = 0;

    CborStatus status = cbor_firstValue(&cursor, NULL);
    if (status && status != CborStatusNotFound) { return 0; }

    while (status == CborStatusOK) {
        size_t l = rlpSize(&cursor);
        if (l == 0) { return 0; }
        size += l;

        status = cbor_nextValue(&cursor, NULL);
        if (status && status != CborStatusNotFound) { return 0; }
    }

    return size;
}

// Returns the RLP size
static size_t rlpSize(CborCursor *_cursor) {
    CborCursor cursor;
    cbor_clone(&cursor, _cursor);

    switch (cbor_getType(&cursor)) {

        case CborTypeString: case CborTypeData: {
            uint8_t *data = NULL;
            size_t length = 0;
            CborStatus status = cbor_getData(&cursor, &data, &length);
            if (length == 1 && data[0] <= 127) { return 1; }
            if (length < 55) { return 1 + length; }
            return 1 + getByteCount(length) + length;
        }

        case CborTypeArray: {
            size_t size = rlpArraySize(&cursor);
            if (size <= 55) { return 1 + size; }
            return 1 + getByteCount(size) + size;
        }

        default:
            break;
    }
    return 0;
}


static CborStatus encode(CborCursor *_cursor, CborBuilder *rlp) {
    CborCursor cursor;
    cbor_clone(&cursor, _cursor);

    switch (cbor_getType(&cursor)) {

        case CborTypeString: case CborTypeData: {
            uint8_t *data = NULL;
            size_t length = 0;
            CborStatus status = cbor_getData(&cursor, &data, &length);

            if (length == 1 && data[0] <= 127) {
                return appendByte(rlp, data[0]);
            }

            status = appendHeader(rlp, 0x80, length);
            if (status) { return status; }

            return appendBytes(rlp, data, length);
        }

        case CborTypeArray: {
            size_t size = rlpArraySize(&cursor);

            CborStatus status = appendHeader(rlp, 0xc0, size);
            if (status) { return status; }

            status = cbor_firstValue(&cursor, NULL);
            if (status && status != CborStatusNotFound) { return 0; }

            while (status == CborStatusOK) {
                status = encode(&cursor, rlp);
                if (status) { return status; }

                status = cbor_nextValue(&cursor, NULL);
                if (status && status != CborStatusNotFound) { return 0; }
            }

            return CborStatusOK;
        }

        default:
            break;
    }

    return CborStatusUnsupportedType;
}

CborStatus cbor_encodeRlp(CborCursor *cursor, uint8_t *data, size_t *length) {
    size_t offset = 0;

    CborBuilder rlp = { 0 };
    rlp.data = data;
    rlp.length = *length;

    CborStatus status = encode(cursor, &rlp);

    // Failed to create RLP data
    if(status) {
        *length = 0;
        return status;
    }

    *length = rlp.offset;
    return CborStatusOK;
}
*/
