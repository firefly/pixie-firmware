#ifndef __FIREFLY_TRANSACTION_H__
#define __FIREFLY_TRANSACTION_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "firefly-cbor.h"


typedef enum FfxTxStatus {
    FfxTxStatusOK                    = 0,
    FfxTxStatusBufferOverrun,
    FfxTxStatusBadData,
    FfxTxStatusOverflow,
    FfxTxStatusUnsupportedVersion,
} FfxTxStatus;


FfxTxStatus ffx_tx_serializeUnsigned(FfxCborCursor *tx, uint8_t *data,
  size_t *length);

//FfxTxStatus ffx_tx_serializeSigned(FfxCborCursor *tx, uint8_t *signature,);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __FIREFLY_TRANSACTION_H__ */
