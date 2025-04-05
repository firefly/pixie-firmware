#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "transaction.h"

#include "cbor.h"
#include "rlp.h"

typedef struct TxBuilder {
    uint8_t *data;
    size_t offset;
    size_t length;
} TxBuilder;


static TxStatus mungeStatus(RlpStatus status) {
    switch(status) {
        case RlpStatusOK:
            return TxStatusOK;
        case RlpStatusBufferOverrun:
            return TxStatusBufferOverrun;
        case RlpStatusOverflow:
            return TxStatusOverflow;
    }

    return TxStatusBadData;
}

typedef enum Format {
    FormatData = 0,
    FormatNumber,
    FormatAddress,
    FormatNullableAddress,
} Format;

static TxStatus append(RlpBuilder *rlp, Format format, CborCursor *tx,
  const char* key) {

    CborCursor value;
    cbor_clone(&value, tx);

    CborStatus status = cbor_followKey(&value, key);
    if (status == CborStatusNotFound) {
        return mungeStatus(rlp_appendData(rlp, NULL, 0));
    }

    if (status || cbor_getType(&value) != CborTypeData) {
        return TxStatusBadData;
    }

    size_t length = 0;
    uint8_t *data = NULL;
    status = cbor_getData(&value, &data, &length);
    if (status) { return TxStatusBadData; }

    // Consume any leading 0 bytes
    if (format == FormatNumber) {
        while (length) {
            if (data[0]) { break; }
            data++;
            length--;
        }
        if (length > 32) { return TxStatusOverflow; }

    } else if (format == FormatAddress) {
        if (length != 20) { return TxStatusBadData; }

    } else if (format == FormatNullableAddress) {
        if (length != 0 && length != 20) { return TxStatusBadData; }
    }

    return mungeStatus(rlp_appendData(rlp, data, length));
}

TxStatus tx_serializeUnsigned(CborCursor *tx, uint8_t *data, size_t *_length) {
    size_t length = *_length;

    if (length < 1) { return TxStatusBufferOverrun; }

    // Add the EIP-2718 Envelope Type
    data[0] = 2;

    // Skip the Envelope Type
    RlpBuilder rlp = { 0 };
    rlp_build(&rlp, &data[1], length - 1);

    // @TODO: for non-1559: TxStatusUnsupportedVersion,

    RlpStatus rlpStatus = rlp_appendArray(&rlp, 9);
    if (rlpStatus) { return mungeStatus(rlpStatus); }

    TxStatus status = TxStatusOK;

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

    rlpStatus = rlp_appendArray(&rlp, 0);
    if (rlpStatus) { return mungeStatus(rlpStatus); }

    *_length = rlp_finalize(&rlp) + 1;

    return TxStatusOK;
}
