#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "firefly-db.h"
#include "firefly-decimal.h"
#include "firefly-eth.h"
#include "firefly-hollows.h"
#include "firefly-scene.h"
#include "firefly-tx.h"

#include "utils.h"


#define DATA(t,v)                 \
  ((FfxInfoClickArg){             \
    .a = { .i32 = (v)->length },  \
    .b = { .data = (v)->bytes },  \
    .c = { .str = (t) },          \
  })

#define DATA2(t,v,n)              \
  ((FfxInfoClickArg){             \
    .a = { .i32 = (v)->length },  \
    .b = { .data = (v)->bytes },  \
    .c = { .str = (t) },          \
    .d = { .str = (n) },          \
  })

#define GET_DATA(v)    \
  ((FfxDataResult){ .length = (v).a.i32, .bytes = (v).b.data })

#define GET_DATA_P(v)    \
  ((FfxDataResult){ .length = (v)->a.i32, .bytes = (v)->b.data })

#define GET_TITLE(v)      ((v).c.str)
#define GET_TITLE_P(v)    ((v)->c.str)

#define GET_NAME(v)       ((v).d.str)
#define GET_NAME_P(v)     ((v)->d.str)



#define NULLARG           ((FfxInfoClickArg){ })


typedef struct State {
    FfxDataResult tx;
} State;

///////////////////////////////
// Utilities

static void clickApprove(void *_state, FfxInfoClickArg clickArg) {
    ffx_popPanel(true);
}

static void clickReject(void *_state, FfxInfoClickArg clickArg) {
    ffx_popPanel(false);
}

const char* HEX = "0123456789abcdef";
static size_t getHex(char *hexOut, const uint8_t* data, size_t length) {
    size_t offset = 0;
    hexOut[offset++] = '0';
    hexOut[offset++] = 'x';
    size_t line = 2;
    for (int i = 0; i < length; i++) {
        hexOut[offset++] = HEX[data[i] >> 4];
        hexOut[offset++] = HEX[data[i] & 0x0f];
        line += 2;
        if (line >= 12) {
            hexOut[offset++] = '\n';
            line = 0;
        }
    }
    if (line == 0) { offset--; }
    hexOut[offset++] = 0;
    return offset;
}


///////////////////////////////
// Address

static int initAddressFunc(void *info, void *_state, void *_arg) {
    FfxInfoClickArg *arg = _arg;

    FfxDataResult data = GET_DATA_P(arg);
    const char* name = GET_NAME_P(arg);

    // If we have a contract name, include it
    if (name) {
        ffx_appendInfoEntry(info, "NAME", name, NULL, NULLARG);
    }

    if (data.length == 0) {
        ffx_appendInfoEntry(info, "TYPE", "deployment", NULL, NULLARG);

    } else if (data.length == 20) {
        FFX_INIT_ADDRESS(addr, data.bytes);
        FfxChecksumAddress address = ffx_eth_checksumAddress(&addr);

        char data[5 * 11];
        int offset = 0;
        for (int i = 2; i < 42; i += 8) {
            data[offset++] = ' ';
            data[offset++] = ' ';
            memcpy(&data[offset], &address.text[i], 8);
            offset += 8;
            data[offset++] = '\n';
        }
        data[offset - 1] = '\0';
        data[0] = '0';
        data[1] = 'x';

        ffx_appendInfoEntry(info, "ADDRESS", data, NULL, NULLARG);

    } else {
        printf("ERROR!! Bad Address\n");
        return false;
    }

    // Buttons
    ffx_appendInfoButton(info, "BACK", COLOR_NAVONLY, clickApprove, NULLARG);

    return 0;
}

static void clickAddress(void *_state, FfxInfoClickArg clickArg) {
    const char* title = GET_TITLE(clickArg);
    ffx_pushInfo(initAddressFunc, title, 0, &clickArg);
}

bool appendAddress(void *info, const char* title, FfxDataResult *value,
  FfxDataResult *chainId) {

    if (value->error) {
        ffx_appendInfoEntry(info, title, "ERROR!", NULL, NULLARG);
        return false;
    }

    // Deployment transaction (i.e. null address)
    if (value->length == 0) {
        ffx_appendInfoEntry(info, title, "deployment", NULL, NULLARG);
        return true;
    }

    if (value->length != 20) {
        ffx_appendInfoEntry(info, title, "ERROR!", NULL, NULLARG);
        return false;
    }

    // If we have a chain ID, look for a contract name in the database
    if (chainId) {
        FfxBigInt cid = ffx_bigint_initBytes(chainId->bytes, chainId->length);
        const char* name = ffx_db_getContractName(&cid, value);
        if (name) {
            ffx_appendInfoEntry(info, title, name, clickAddress, DATA2(title,
              value, name));
            return true;
        }
    }

    // Use a condenced address representation; i.e. "0x1234...5678"
    FFX_INIT_ADDRESS(addr, value->bytes);
    FfxChecksumAddress address = ffx_eth_checksumAddress(&addr);

    // "0x01234...5678\0"
    char str[14];
    memcpy(&str[0], &address.text[0], 6);
    str[6] = '.';
    str[7] = '.';
    str[8] = '.';
    memcpy(&str[9], &address.text[38], 5);
    ffx_appendInfoEntry(info, title, str, clickAddress, DATA(title, value));

    return true;
}

///////////////////////////////
// Data

static int initDataFunc(void *info, void *_state, void *arg) {
    FfxDataResult *data = arg;

    char title[16];
    char value[128];

    const uint8_t *bytes = data->bytes;
    size_t length = data->length;

    {
        if (length < 1000) {
            snprintf(value, sizeof(value), "%d bytes", length);
        } else {
            snprintf(value, sizeof(value), "%d,%d bytes", length / 1000,
              length % 1000);
        }
        ffx_appendInfoEntry(info, "SIZE", value, NULL, NULLARG);
    }

    if ((length % 32) == 4) {
        getHex(value, bytes, 4);
        ffx_appendInfoEntry(info, "SELECTOR", value, NULL, NULLARG);
        length -= 4;
        bytes += 4;
    }

    int i = 1;
    while (length) {
        if (length < 32) {
            getHex(value, bytes, length);
            ffx_appendInfoEntry(info, "JUNK", value, NULL, NULLARG);
            break;
        }

        snprintf(title, sizeof(title), "WORD #%d", i);
        int offset = getHex(value, bytes, 32) - 1;
        for (int j = 0; j < 6; j++) { value[offset++] = ' '; }
        value[offset] = 0;
        ffx_appendInfoEntry(info, title, value, NULL, NULLARG);

        i++;
        bytes += 32;
        length -= 32;

        // @TODO: Fix this with the callback-based info
        if (length > 32 && i > 12) {
            snprintf(value, sizeof(value), "%d words", i / 32);
            ffx_appendInfoEntry(info, "TOO LONG", value, NULL, NULLARG);
            break;
        }
    }

    // Buttons
    ffx_appendInfoButton(info, "BACK", COLOR_NAVONLY, clickApprove, NULLARG);

    return true;
}

static void clickData(void *_state, FfxInfoClickArg clickArg) {
    FfxDataResult data = GET_DATA(clickArg);
    const char* title = GET_TITLE(clickArg);
    ffx_pushInfo(initDataFunc, title, 0, &data);
}

bool appendData(void *info, const char* title, FfxDataResult *value) {
    if (value->error) {
        ffx_appendInfoEntry(info, title, "ERROR!", NULL, NULLARG);
        return false;
    }

    if (value->length == 0) {
        // No data
        ffx_appendInfoEntry(info, title, "none", NULL, NULLARG);

    } else if (value->length <= 5) {
        // Short data; will fit in a single entry
        char hex[20] = { 0 };
        getHex(hex, value->bytes, value->length);
        ffx_appendInfoEntry(info, title, hex, NULL, NULLARG);

    } else {
        // Long data; show first 4 bytes
        char hex[20] = { 0 };
        size_t offset = getHex(hex, value->bytes, 4) - 1;
        hex[offset++] = '.';
        hex[offset++] = '.';
        hex[offset++] = '.';
        hex[offset++] = '\0';
        ffx_appendInfoEntry(info, title, hex, clickData, DATA(title, value));
    }

    return true;
}

///////////////////////////////
// Decimal

//static int initDecemialFunc(void *info, void *_state, void *arg) {
//}

//static void clickDecimal(void *_state, FfxInfoClickArg clickArg) {
//    ffx_pushInfo(initDataFunc, 0, &clickArg.data);
//}

bool appendDecimal(void *info, const char* title, uint8_t decimals,
  FfxDataResult *value) {

    if (value->error) {
        ffx_appendInfoEntry(info, title, "ERROR!", NULL, NULLARG);
        return false;
    }

    FfxBigInt num = ffx_bigint_initBytes(value->bytes, value->length);

    // Reserve a space to indicate rounding occurred
    char str[1 + FFX_ETHER_STRING_LENGTH] = { 0 };
    FfxDecimalResult result = ffx_decimal_formatValue(&str[1], &num, (FfxDecimalFormat){
        .round = FfxDecimalRoundCeiling,
        .decimals = decimals,
        .groups = 3,
        .maxDecimals = 5,
        .minDecimals = 1,
    });

    if (result.flags & FfxDecimalFlagRounded) {
        str[0] = '<';
        result.str = str;
    }

    // @TODO: Drill down into value
    //appendEntry(state, "VALUE (sETH)", result.str, PanelTxViewValue);
    ffx_appendInfoEntry(info, title, result.str, NULL, NULLARG);

    return true;
}

///////////////////////////////
// Network

static int initNetworkFunc(void *info, void *_state, void *arg) {
    FfxDataResult *data = arg;

    FfxBigInt value = ffx_bigint_initBytes(data->bytes, data->length);

    const char *name = ffx_db_getNetworkName(&value);
    if (name) {
        ffx_appendInfoEntry(info, "NAME", name, NULL, NULLARG);
    }

    char str[FFX_BIGINT_STRING_LENGTH];
    size_t length = ffx_bigint_getString(&value, str);
    ffx_appendInfoEntry(info, "CHAIN ID", str, NULL, NULLARG);

    // Buttons
    ffx_appendInfoButton(info, "BACK", COLOR_NAVONLY, clickApprove, NULLARG);

    return 0;
}

static void clickNetwork(void *_state, FfxInfoClickArg clickArg) {
    FfxDataResult data = GET_DATA(clickArg);
    const char* title = GET_TITLE(clickArg);
    ffx_pushInfo(initNetworkFunc, title, 0, &clickArg);
}

bool appendNetwork(void *info, FfxDataResult *value) {
    if (value->error) {
        ffx_appendInfoEntry(info, "NETWORK", "ERROR!", NULL, NULLARG);
        return false;
    }

    FfxBigInt chainId = ffx_bigint_initBytes(value->bytes, value->length);
    const char *name = ffx_db_getNetworkName(&chainId);
    if (name) {
        ffx_appendInfoEntry(info, "NETWORK", name, clickNetwork, DATA("NETOWKR", value));

    } else {
        char str[FFX_BIGINT_STRING_LENGTH];
        size_t length = ffx_bigint_getString(&chainId, str);
        ffx_appendInfoEntry(info, "CHAIN ID", str, NULL, NULLARG);
    }

    return true;
}


///////////////////////////////
// Transaction Summary

static int initFunc(void *info, void *_state, void *arg) {

    FfxDataResult *tx = arg;
    ffx_tx_dump(tx);

    // Network
    FfxDataResult chainId = ffx_tx_getChainId(tx);
    appendNetwork(info, &chainId);

    // To Address
    {
        FfxDataResult toAddr = ffx_tx_getAddress(tx);
        appendAddress(info, "TO", &toAddr, &chainId);
    }

    // Value
    {
        FfxDataResult value = ffx_tx_getValue(tx);
        appendDecimal(info, "VALUE (sETH)", 18, &value);
    }

    // Data
    {
        FfxDataResult data = ffx_tx_getData(tx);
        appendData(info, "DATA", &data);
    }

    // Buttons
    ffx_appendInfoButton(info, "REJECT", COLOR_CANCEL, clickReject, NULLARG);
    ffx_appendInfoButton(info, "SIGN & SEND", COLOR_APPROVE, clickApprove,
      NULLARG);

/*
    appendEntry(info, "MAX FEES", "", PanelTxActionFees);
    appendEntry(info, "USD PRICE", "", 0);
    appendEntry(info, "VALUE (USD)", "", 0);
*/
    return 0;
}


bool pushPanelTx(FfxDataResult *tx) {
    return ffx_pushInfo(initFunc, "TRANSACTION", sizeof(State), tx);
}

