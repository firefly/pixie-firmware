#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "firefly-tx.h"

#include "firefly-cbor.h"
#include "firefly-rlp.h"


typedef struct TxBuilder {
    uint8_t *data;
    size_t offset;
    size_t length;
} TxBuilder;


static FfxTxStatus mungeStatus(FfxRlpStatus status) {
    switch(status) {
        case FfxRlpStatusOK:
            return FfxTxStatusOK;
        case FfxRlpStatusBufferOverrun:
            return FfxTxStatusBufferOverrun;
        case FfxRlpStatusOverflow:
            return FfxTxStatusOverflow;
    }

    return FfxTxStatusBadData;
}

typedef enum Format {
    FormatData = 0,
    FormatNumber,
    FormatAddress,
    FormatNullableAddress,
} Format;

static FfxTxStatus append(FfxRlpBuilder *rlp, Format format, FfxCborCursor *tx,
  const char* key) {

    FfxCborCursor value;
    ffx_cbor_clone(&value, tx);

    FfxCborStatus status = ffx_cbor_followKey(&value, key);
    if (status == FfxCborStatusNotFound) {
        return mungeStatus(ffx_rlp_appendData(rlp, NULL, 0));
    }

    if (status || ffx_cbor_getType(&value) != FfxCborTypeData) {
        return FfxTxStatusBadData;
    }

    size_t length = 0;
    uint8_t *data = NULL;
    status = ffx_cbor_getData(&value, &data, &length);
    if (status) { return FfxTxStatusBadData; }

    // Consume any leading 0 bytes
    if (format == FormatNumber) {
        while (length) {
            if (data[0]) { break; }
            data++;
            length--;
        }
        if (length > 32) { return FfxTxStatusOverflow; }

    } else if (format == FormatAddress) {
        if (length != 20) { return FfxTxStatusBadData; }

    } else if (format == FormatNullableAddress) {
        if (length != 0 && length != 20) { return FfxTxStatusBadData; }
    }

    return mungeStatus(ffx_rlp_appendData(rlp, data, length));
}

FfxTxStatus ffx_tx_serializeUnsigned(FfxCborCursor *tx, uint8_t *data, size_t *_length) {
    size_t length = *_length;

    if (length < 1) { return FfxTxStatusBufferOverrun; }

    // Add the EIP-2718 Envelope Type
    data[0] = 2;

    // Skip the Envelope Type
    FfxRlpBuilder rlp = { 0 };
    ffx_rlp_build(&rlp, &data[1], length - 1);

    // @TODO: for non-1559: FfxTxStatusUnsupportedVersion,

    FfxRlpStatus rlpStatus = ffx_rlp_appendArray(&rlp, 9);
    if (rlpStatus) { return mungeStatus(rlpStatus); }

    FfxTxStatus status = FfxTxStatusOK;

    status = append(&rlp, FormatNumber, tx, "chainId");
    if (status) { return status; }

    status = append(&rlp, FormatNumber, tx, "nonce");
    if (status) { return status; }

    status = append(&rlp, FormatNumber, tx, "maxPriorityFeePerGas");
    if (status) { return status; }

    status = append(&rlp, FormatNumber, tx, "maxFeePerGas");
    if (status) { return status; }

    status = append(&rlp, FormatNumber, tx, "gasLimit");
    if (status) { return status; }

    status = append(&rlp, FormatNullableAddress, tx, "to");
    if (status) { return status; }

    status = append(&rlp, FormatNumber, tx, "value");
    if (status) { return status; }

    status = append(&rlp, FormatData, tx, "data");
    if (status) { return status; }

    rlpStatus = ffx_rlp_appendArray(&rlp, 0);
    if (rlpStatus) { return mungeStatus(rlpStatus); }

    *_length = ffx_rlp_finalize(&rlp) + 1;

    return FfxTxStatusOK;
}
