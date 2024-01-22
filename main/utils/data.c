
#include <string.h>

#include "./data.h"


typedef struct _Data {
    uint32_t capacity;
    uint32_t length;
    uint8_t *data;
} _Data;


Data data_init(uint8_t *buffer, size_t bufferLength) {
    if (bufferLength < 8) { return NULL; }

    _Data *data = (_Data*)&buffer[0];
    data->capacity = bufferLength;
    data->length = 0;

    return buffer;
}

int32_t data_appendByte(Data _data, uint8_t byte) {
    _Data *data = (_Data*)_data;
    if (data->length + 1 > data->capacity) { return DATA_ERROR; }

    data->data[data->length++] = byte;

    return DATA_SUCCESS;
}

int32_t data_appendBytes(Data _data, uint8_t *bytes, size_t bytesLength) {
    _Data *data = (_Data*)_data;
    if (data->length + bytesLength > data->capacity) { return DATA_ERROR; }

    memcpy(&data->data[data->length], bytes, bytesLength);
    data->length += bytesLength;

    return DATA_SUCCESS;
}

int32_t data_appendData(Data data, Data _otherData) {
    _Data *otherData = (_Data*)_otherData;
    return data_appendBytes(data, otherData->data, otherData->length);
}

int32_t data_appendString(Data data, const char *str) {
    const uint32_t length = strlen(str);
    return data_appendBytes(data, str, length);
}

int32_t data_remove(Data _data, size_t offset, size_t length) {
    _Data *data = (_Data*)_data;
    if (offset + length > data->length) { return DATA_ERROR; }

    for (uint32_t i = 0; i < length; i++) {
        data->data[offset + i] = data->data[offset + i + length];
    }

    data->length -= length;

    return 1; // @TODO
}

size_t data_length(Data _data) {
    _Data *data = (_Data*)_data;
    return data->length;
}

size_t data_capacity(Data _data) {
    _Data *data = (_Data*)_data;
    return data->capacity - 2 * sizeof(uint32_t);
}

uint8_t* data_getBytes(Data _data) {
    _Data *data = (_Data*)_data;
    return data->data;
}
