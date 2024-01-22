#ifndef __RLP_H__
#define __RLP_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stddef.h>
#include <stdint.h>


#define RLP_SUCCESS     (1)
#define RLP_ERROR       (0)


typedef void* RLP;


RLP rlp_init(uint8_t *buffer, size_t bufferLength);

// int32_t data_appendByte(Data data, uint8_t byte);
// int32_t data_appendBytes(Data data, uint8_t *bytes, size_t bytesLength);
// int32_t data_appendData(Data data, Data otherData);
// int32_t data_appendString(Data data, const char *str);

// int32_t data_remove(Data data, size_t offset, size_t length);

size_t rlp_length(RLP data);
// size_t data_capacity(Data data);

uint32_t data_getBytes(RLP data, uint8_t *output, size_t outputLength);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __RLP_H__ */
