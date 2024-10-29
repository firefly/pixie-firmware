
#include <string.h>

#include "./cbor.h"



// DEBUG
#include <stdio.h>

// @TODO: Check obsolete for previous code

void cbor_init(CborCursor *cursor, uint8_t *data, size_t length) {
    cursor->data = data;
    cursor->length = length;
    cursor->offset = 0;
    cursor->containerCount = 0;
    cursor->containerIndex = 0;
}

void cbor_clone(CborCursor *dst, CborCursor *src) {
    memcpy(dst, src, sizeof(CborCursor));
}

static CborType _getType(uint8_t header) {
    switch(header >> 5) {
        case 0:
            return CborTypeNumber;
        case 2: case 3:
            return CborTypeData;
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

bool cbor_isDone(CborCursor *cursor) {
    return (cursor->offset == cursor->length);
}

CborType cbor_getType(CborCursor *cursor) {
    if (cursor->offset >= cursor->length) { return CborTypeError; }
    return _getType(cursor->data[cursor->offset]);
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
        *status = CborStatusUnsupportedValue;
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

CborStatus cbor_getData(CborCursor *cursor, uint8_t *output, size_t length) {
    CborType type = 0;
    uint64_t value;
    size_t safe = 0, headLen = 0;
    CborStatus status = CborStatusOK;
    uint8_t *data = _getBytes(cursor, &type, &value, &safe, &headLen, &status);
    if (data == NULL) { return status; }

    if (type != CborTypeData) { return CborStatusInvalidOperation; }

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

    for (int i = 0; i < value; i++) { output[i] = data[i]; }

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
        case CborTypeData: case CborTypeArray: case CborTypeMap:
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

        case CborTypeData:
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

bool _keyCompare(const char *key, CborCursor *cursor) {
    size_t sLen = strlen(key);

    size_t cLen = 0;
    cbor_getLength(cursor, &cLen);

    if (sLen != cLen) { return false; }

    char cStr[cLen];
    cbor_getData(cursor, (uint8_t*)cStr, cLen);

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
