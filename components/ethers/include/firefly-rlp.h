#ifndef __FIREFLY_RLP_H__
#define __FIREFLY_RLP_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stddef.h>
#include <stdint.h>


/**
 *  Recursive-Length Prefix (RLP) Encoder
 *
 *  RLP-encoding is used for various purposes in Ethereum, such as
 *  serializing transactions, which is in turn used to create the
 *  necessary hashes for signing.
 *
 *  See: https://ethereum.org/en/developers/docs/data-structures-and-encoding/rlp/
 */


typedef enum FfxRlpStatus {
    FfxRlpStatusOK = 0,

    FfxRlpStatusBufferOverrun = -31,
    FfxRlpStatusOverflow = -55
} FfxRlpStatus;


typedef struct FfxRlpBuilder {
    uint8_t *data;
    size_t offset, length;
} FfxRlpBuilder;


/**
 *  Initializes a new RLP builder.
 *
 *  During the build, intermediate data is included in the %%data%%
 *  buffer, so the [[rlp_finalize]] MUST be called to complete RLP
 *  serialization.
 */
void ffx_rlp_build(FfxRlpBuilder *builder, uint8_t *data, size_t length);

/**
 *  Remove all intermediate data and return the length of the RLP-encoded
 *  data.
 */
size_t ffx_rlp_finalize(FfxRlpBuilder *builder);


/**
 *  Append a Data.
 */
FfxRlpStatus ffx_rlp_appendData(FfxRlpBuilder *builder, uint8_t *data,
  size_t length);

/**
 *  Append a Data, with the contents of the a NULL-terminated string.
 */
FfxRlpStatus ffx_rlp_appendString(FfxRlpBuilder *builder, const char *data);

/**
 *  Append an Array, where the next %%count%% Items added will be
 *  added to this array.
 */
FfxRlpStatus ffx_rlp_appendArray(FfxRlpBuilder *builder, size_t count);


// @TODO: Future API?
//typedef uint16_t RlpBuilderTag;
//RlpStatus rlp_appendArrayMutable(RlpBuilder *builder, RlpBuilderTag *tag);
//void rlp_adjustCount(RlpBuilder *builder, RlpBuilderTag tag, uint16_t count);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __FIREFLY_RLP_H__ */
