#ifndef __DATA_H__
#define __DATA_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stddef.h>
#include <stdint.h>


#define DATA_SUCCESS     (1)
#define DATA_ERROR       (0)


typedef void* Data;


Data data_init(uint8_t *buffer, size_t bufferLength);

int32_t data_appendByte(Data data, uint8_t byte);
int32_t data_appendBytes(Data data, uint8_t *bytes, size_t bytesLength);
int32_t data_appendData(Data data, Data otherData);
int32_t data_appendString(Data data, const char *str);

int32_t data_remove(Data data, size_t offset, size_t length);

size_t data_length(Data data);
size_t data_capacity(Data data);

uint8_t* data_getBytes(Data data);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __DATA_H__ */
