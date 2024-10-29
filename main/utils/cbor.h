#ifndef __CBOR_H__
#define __CBOR_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


/**
 *  A cursor used to traverse and read CBOR-encoded data.
 *
 *  This should not be modified directly! Only use the provided API.
 */
typedef struct CborCusror {
    uint8_t *data;
    size_t length;
    size_t offset;

    // The containerCount is used while traversing a map or array.
    // - a negative size indicates a map contains,
    // - a positive size indicates an array container
    // - 0 indicates this is not a container
    int32_t containerCount;

    // The index within the container of the cursor.
    size_t containerIndex;
} CborCursor;

typedef enum CborType {
    CborTypeError    = 0,
    CborTypeNull     = 1,
    CborTypeBoolean  = 2,
    CborTypeNumber   = 3,
    CborTypeData     = 4,
    CborTypeArray    = 5,
    CborTypeMap      = 6
} CborType;

typedef enum CborStatus {
   // Returned at the end of a map or array during iteration or
   // when attempting to follow a map with a non-existant key or
   // array past its index.
   CborStatusNotFound          = 5,

   // No error
   CborStatusOK                = 0,

   // Attempted to perform an operation on an item that is not
   // supported, such as reading data from a number or followIndex
   // on a string.
   CborStatusInvalidOperation  = -30,

   // Attempted to read past the end of the data
   CborStatusBufferOverrun     = -31,

   // Some data was discarded during a getData
   CborStatusTruncated         = -32,

   // The CBOR data contains an unsupported value type(e.g. tags)
   CborStatusUnsupportedType   = -52,

   // The CBOR data contains an unsupported value (e.g. indefinite data)
   CborStatusUnsupportedValue  = -53,

   // Value represented does not fit within a uint64
   CborStatusOverflow          = -55,
} CborStatus;


void cbor_init(CborCursor *cursor, uint8_t *data, size_t length);
void cbor_clone(CborCursor *dst, CborCursor *src);

CborType cbor_getType(CborCursor *cursor);

CborStatus cbor_getValue(CborCursor *cursor, uint64_t *value);
CborStatus cbor_getData(CborCursor *cursor, uint8_t *data, size_t length);

// Returns the number of array elements or map key-pairs or data length
CborStatus cbor_getLength(CborCursor *cursor, size_t *count);

CborStatus cbor_followKey(CborCursor *cursor, const char *key);
CborStatus cbor_followIndex(CborCursor *cursor, size_t index);

bool cbor_isDone(CborCursor *cursor);

// When called on a map, moves to the first value and updates key.
// When called on an array, moves to the first value
CborStatus cbor_firstValue(CborCursor *cursor, CborCursor *key);

// When called on a map entry, moves to the value item and copies the key.
// When called on an array element, moves to the next value
CborStatus cbor_nextValue(CborCursor *cursor, CborCursor *key);

// Low-level; not for normal use
CborStatus _cbor_next(CborCursor *cursor);
//uint8_t* cbor_raw(CborCursor *cursor, uint8_t *type, uint64_t *count);



#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CBOR_H__ */
