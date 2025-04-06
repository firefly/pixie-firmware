#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdbool.h>
#include <stdio.h>

#include "firefly-cbor.h"
#include "firefly-crypto.h"
#include "firefly-hash.h"
#include "firefly-tx.h"

#include "panel.h"
#include "utils.h"

#include "panel-connect.h"

typedef struct State {
    FfxScene scene;
    FfxNode panel;

    uint32_t ticks;
} State;

static void onMessage(EventPayload event, void* arg) {
    // getBytes(id("test-foobar-running-moose-34"))
    uint8_t privateKey[] = {
        15, 254, 74, 18, 107, 9, 94, 32, 109, 87, 148, 60, 35, 251, 109, 95,
        51, 98, 149, 196, 4, 13, 42, 18, 147, 178, 165, 40, 128, 78, 67, 99
    };

    uint32_t messageId = event.props.message.id;
    const char* method = event.props.message.method;

    FfxCborCursor params;
    ffx_cbor_clone(&params, &event.props.message.params);

    printf("GOT MESSAGE: id=%ld, method=%s", messageId, method);
    ffx_cbor_dump(&params);

    if (!panel_acceptMessage(messageId, NULL)) {
        printf("EEK!");
        return;
    }

    uint8_t digest[FFX_KECCAK256_DIGEST_LENGTH] = { 0 };
    {
        size_t rlpLength = 256;
        uint8_t *rlp = malloc(rlpLength);
        memset(rlp, 0, rlpLength);

        FfxTxStatus txStatus = ffx_tx_serializeUnsigned(&params, rlp,
          &rlpLength);
        printf("tx: status=%d length=%d\n", txStatus, rlpLength);

        printf("RLP: 0x");
        for (int i = 0; i < rlpLength; i++) {
            printf("%02x", rlp[i]);
        }
        printf("\n");

        ffx_hash_keccak256(digest, rlp, rlpLength);
        free(rlp);
    }

    uint8_t sig[FFX_SECP256K1_SIGNATURE_LENGTH] = { 0 };
    int32_t status = ffx_pk_signSecp256k1(privateKey, digest, sig);
    printf("sig: status=%ld\n", status);

    uint8_t *_reply = malloc(256);
    FfxCborBuilder reply;
    ffx_cbor_build(&reply, _reply, 256);

    ffx_cbor_appendMap(&reply, 3);
    ffx_cbor_appendString(&reply, "r");
    ffx_cbor_appendData(&reply, &sig[0], 32);
    ffx_cbor_appendString(&reply, "s");
    ffx_cbor_appendData(&reply, &sig[32], 32);
    ffx_cbor_appendString(&reply, "v");
    ffx_cbor_appendNumber(&reply, sig[64]);

    //panel_sendErrorReply(4242, "This is an error message...");
    panel_sendReply(messageId, &reply);
    free(_reply);
}

static int _init(FfxScene scene, FfxNode panel, void* _state, void* arg) {
    State *state = _state;
    state->scene = scene;
    state->panel = panel;

    FfxNode *label = ffx_scene_createLabel(scene, FfxFontMediumBold,
      "Listening...");
    ffx_sceneLabel_setAlign(label,
      FfxTextAlignMiddleBaseline | FfxTextAlignCenter);
    ffx_sceneLabel_setOutlineColor(label, COLOR_BLACK);
    ffx_sceneNode_setPosition(label, ffx_point(120, 120));
    ffx_sceneGroup_appendChild(panel, label);


    panel_onEvent(EventNameMessage, onMessage, state);

    return 0;
}

void pushPanelConnect(void* arg) {
    panel_push(_init, sizeof(State), PanelStyleSlideLeft, arg);
}
