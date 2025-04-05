#ifndef __CBOR_H__
#define __CBOR_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


/**
 *  A tag can be used to modify the length of an array or map when
 *  its length is unknown (or may change) after it has been appended.
 */
typedef uint16_t CborBuilderTag;

/**
 *  The supported types for CBOR data.
 */
typedef enum CborType {
    CborTypeError    = 0,
    CborTypeNull     = 1,
    CborTypeBoolean  = 2,
    CborTypeNumber   = 3,
    CborTypeString   = 4,
    CborTypeData     = 5,
    CborTypeArray    = 6,
    CborTypeMap      = 7
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

   // Attempted to read past the end of the cursor data or write past
   // the end of the builder data
   CborStatusBufferOverrun     = -31,

   // Some data was discarded during a getData
   CborStatusTruncated         = -32,

   // The CBOR data contains an unsupported value type(e.g. tags)
   CborStatusUnsupportedType   = -52,

   // Value represented does not fit within a uint64
   CborStatusOverflow          = -55,
} CborStatus;


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
    // - a negative size indicates a map container
    // - a positive size indicates an array container
    // - 0 indicates this is not a container
    int32_t containerCount;

    // The index within the container of the cursor.
    size_t containerIndex;

    CborStatus status;
} CborCursor;


/**
 *  A builder used to create and write CBOR-encoded data.
 *
 *  This should not be modified directly! Only use the provided API.
 */
typedef struct CborBuilder {
    uint8_t *data;
    size_t length;
    size_t offset;

    // Will always allot the maximum bytes for a type; useful
    // for in-place re-encoding where the extra space can
    // be used for intermediate data during transform.
    bool sparse;

    CborStatus status;
} CborBuilder;


// Detects if an error occurred during crawl or build... @TODO
CborStatus cbor_getStatus(CborCursor *cursor);
CborStatus cbor_getBuildStatus(CborCursor *cursor);

void cbor_init(CborCursor *cursor, uint8_t *data, size_t length);
void cbor_clone(CborCursor *dst, CborCursor *src);

/**
 *  Returns the type.
 */
CborType cbor_getType(CborCursor *cursor);

/**
 *  Returns the value for scalar types (Null, Boolean, Number).
 *
 *  Null is always 0. Boolean is either 0 for false, or 1 for true.
 */
CborStatus cbor_getValue(CborCursor *cursor, uint64_t *value);

/**
 *  Copies the data for data types (Data and String), up to length bytes.
 */
CborStatus cbor_copyData(CborCursor *cursor, uint8_t *data, size_t length);

/**
 *  Exposes the underlying shared data buffer and length for data
 *  types (Data and String) without copying.
 *
 *  Do NOT modify these values.
 */
CborStatus cbor_getData(CborCursor *cursor, uint8_t **data, size_t *length);

/**
 *  For Array and Map types, returns the number of values, and for
 *  Data and String types returns the length in bytes.
 */
CborStatus cbor_getLength(CborCursor *cursor, size_t *count);

/**
 *  Moves the %%cursor%% to the value for %%key%% within a Map.
 *
 *  If the Map does not have %%key%%, returns CborStatusNotFound.
 */
CborStatus cbor_followKey(CborCursor *cursor, const char *key);

/**
 *  Moves the %%cursor%% to the %%index%% value within an Array.
 *
 *  If outside the bounds of the Array, returns CborStatusNotFound.
 */
CborStatus cbor_followIndex(CborCursor *cursor, size_t index);

bool cbor_isDone(CborCursor *cursor);

/**
 *  Moves %%cursor%% to the first value within a Map or Array.
 *
 *  If the container is a Map and %%key%% is not NULL, then it
 *  is updated to point to the key's value.
 *
 *  If the continaer is empty, returns CborStatusNotFound.
 */
CborStatus cbor_firstValue(CborCursor *cursor, CborCursor *key);

/**
 *  Moves %%cursor%% to the next value within a Map or Array.
 *
 *  If the container is a Map and %%key%% is not NULL, then it
 *  is updated to point to the key's value.
 *
 *  If there are no more entries, returns CborStatusNotFound.
 */
CborStatus cbor_nextValue(CborCursor *cursor, CborCursor *key);

// Low-level; not for normal use
CborStatus _cbor_next(CborCursor *cursor);
//uint8_t* cbor_raw(CborCursor *cursor, uint8_t *type, uint64_t *count);

/**
 *  Dumps the structured CBOR data to the console.
 */
void cbor_dump(CborCursor *cursor);


// Initialize a CBOR builder.
void cbor_build(CborBuilder *builder, uint8_t *data, size_t length);

// Initialize a CBOR builder.
void cbor_buildSparse(CborBuilder *builder, uint8_t *data, size_t length);

size_t cbor_getBuildLength(CborBuilder *builder);

// Append a boolean.
CborStatus cbor_appendBoolean(CborBuilder *builder, bool value);

// Append a null value.
CborStatus cbor_appendNull(CborBuilder *builder);

// Append a numeric value.
CborStatus cbor_appendNumber(CborBuilder *builder, uint64_t value);

// Append data.
CborStatus cbor_appendData(CborBuilder *builder, uint8_t *data, size_t length);

// Append a string.
CborStatus cbor_appendString(CborBuilder *builder, char* str);

// Begin an Array of count items long. After calling this, call
// other cbor_append* methods count times.
CborStatus cbor_appendArray(CborBuilder *builder, size_t count);

// Begin a Map of count items long. After calling this, call
// other cbor_append* methods 2 * count times, once for each key
// and value of the children.
CborStatus cbor_appendMap(CborBuilder *builder, size_t count);

// Begin a dynamic length Array. The tag should be updated with
// cbor_adjustCount once the number of items in the array is known.
CborStatus cbor_appendArrayMutable(CborBuilder *builder, CborBuilderTag *tag);

// Begin a dynamic length Map. The tag should be updated with
// cbor_adjustCount once the number of items in the array is known.
CborStatus cbor_appendMapMutable(CborBuilder *builder, CborBuilderTag *tag);

// Adjust the number of elements in a dynamic array or map appended
// using cbor_appendArrayMutable or cbor_appendMapMutable.
void cbor_adjustCount(CborBuilder *builder, CborBuilderTag tag,
  uint16_t count);

/**
 *  Append raw CBRO-encoded %%data%% to an entry in %%builder%%.
 */
CborStatus cbor_appendCborRaw(CborBuilder *builder, uint8_t *data,
  size_t length);

/**
 *  Append an entire CborBuilder %%src%% to an entry in %%dst%%.
 */
CborStatus cbor_appendCborBuilder(CborBuilder *dst, CborBuilder *src);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CBOR_H__ */
