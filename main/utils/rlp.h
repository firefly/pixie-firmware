#ifndef __RLP_H__
#define __RLP_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stddef.h>
#include <stdint.h>



typedef enum RlpStatus {
    RlpStatusOK = 0,

    RlpStatusBufferOverrun = -31,
    RlpStatusOverflow = -55
} RlpStatus;

typedef struct RlpBuilder {
    uint8_t *data;
    size_t offset, length;
} RlpBuilder;

/**
 *  Initializes a new RLP builder.
 *
 *  During the build, intermediate data is included in the %%data%%
 *  buffer, so the [[rlp_finalize]] MUST be called to complete RLP
 *  serialization.
 */
void rlp_build(RlpBuilder *builder, uint8_t *data, size_t length);

/**
 *  Remove all intermediate data and return the length of the RLP-encoded
 *  data.
 */
size_t rlp_finalize(RlpBuilder *builder);

RlpStatus rlp_appendData(RlpBuilder *builder, uint8_t *data, size_t length);
RlpStatus rlp_appendString(RlpBuilder *builder, const char *data);

RlpStatus rlp_appendArray(RlpBuilder *builder, size_t count);



#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __RLP_H__ */
