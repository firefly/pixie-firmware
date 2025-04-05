#ifndef __TRANSACTION_H__
#define __TRANSACTION_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cbor.h"


typedef enum TxStatus {
    TxStatusOK                    = 0,
    TxStatusBufferOverrun,
    TxStatusBadData,
    TxStatusOverflow,
    TxStatusUnsupportedVersion,
} TxStatus;


TxStatus tx_serializeUnsigned(CborCursor *tx, uint8_t *data, size_t *length);

//TxStatus tx_serializeSigned(CborCursor *tx, uint8_t *signature);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __TRANSACTION_H__ */
