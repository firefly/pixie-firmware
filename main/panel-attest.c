#include <stdio.h>
#include <string.h>

//#include "images/image-attest.h"

#include "system/device-info.h"
#include "utils/cbor.h"

#include "panel.h"
#include "panel-attest.h"

#define TEXT_LENGTH      (20)

typedef struct AttestState {
    FfxScene scene;
    FfxNode connect;

    char txtSerial[TEXT_LENGTH];
    char txtModel[TEXT_LENGTH];
} AttestState;

static void onMessage(EventPayload event, void *_state) {
    AttestState *state = _state;

    printf("MESSAGE: %d\n", event.props.messageEvent.length);

    CborStatus status = 0;

    CborCursor root;
    cbor_init(&root, event.props.messageEvent.data, event.props.messageEvent.length);

    CborCursor cursor;
    {
        cbor_clone(&cursor, &root);
        status = cbor_followKey(&cursor, "v");
        if (status) {
            printf("attest: fail1 status=%d", status);
            return;
        }

        uint64_t value = 0;
        status = cbor_getValue(&cursor, &value);
        if (status || value != 1) {
            printf("attest: fail2 v=%lld status=%d", value, status);
            return;
        }
    }

    {
        cbor_clone(&cursor, &root);
        status = cbor_followKey(&cursor, "attest");
        if (status) {
            printf("attest: fail3 status=%d", status);
            return;
        }

        uint8_t data[32] = { 0 };
        status = cbor_getData(&cursor, data, 8);
        if (status) {
            printf("attest: fail4 status=%d", status);
            return;
        }
        printf("GOT: %s\n", data);

        DeviceAttestation attest = { 0 };
        DeviceStatus status2 = device_attest(data, &attest);
        if (status) {
            printf("attest: fail5 status=%d", status2);
            return;
        }

        int r = panel_sendReply((uint8_t*)&attest, 512);
        printf("Reply: %d %d\n", sizeof(attest), r);
    }
}

static void keyChanged(EventPayload event, void *_state) {
    AttestState *state = _state;

    switch(event.props.keyEvent.down) {
        case KeyOk:
            printf("ok\n");
            break;
        case KeyCancel:
            panel_pop();
            break;
    }
}

const char* const txtTitle = "Device";
const char* const txtListening = "listening...";

static void setMessage(AttestState *state, char *message) {
}

static int _init(FfxScene scene, FfxNode node, void *_state, void *arg) {
    AttestState *state = _state;
    state->scene = scene;

    snprintf(state->txtSerial, TEXT_LENGTH, "DEV: 0x%lx", device_modelNumber());
    snprintf(state->txtModel, TEXT_LENGTH, "S/N: %ld", device_serialNumber());

    FfxNode box = ffx_scene_createBox(scene, (FfxSize){
        .width = 200, .height = 180
    }, RGB_DARK75);
    ffx_scene_appendChild(node, box);
    ffx_scene_nodeSetPosition(box, (FfxPoint){ .x = 20, .y = 30 });

    FfxNode text;

    text = ffx_scene_createTextStr(scene, txtTitle);
    ffx_scene_appendChild(node, text);
    ffx_scene_nodeSetPosition(text, (FfxPoint){
        .x = 120 - ((strlen(txtTitle) * 17 - 1) / 2), .y = 70
    });

    text = ffx_scene_createTextStr(scene, state->txtSerial);
    ffx_scene_appendChild(node, text);
    ffx_scene_nodeSetPosition(text, (FfxPoint){
        .x = 40, .y = 120
    });

    text = ffx_scene_createTextStr(scene, state->txtModel);
    ffx_scene_appendChild(node, text);
    ffx_scene_nodeSetPosition(text, (FfxPoint){
        .x = 40, .y = 160
    });

/*
    text = ffx_scene_createTextFlip(scene, state->txtMessage, TEXT_LENGTH * 2);
    ffx_scene_appendChild(node, text);
    ffx_scene_nodeSetPosition(text, (FfxPoint){ .x = 30, .y = 165 });

    ffx_scene_textSetText(text, txtListening, strlen(txtListening));
*/
/*
    FfxNode connect = ffx_scene_createImage(scene, image_attest, sizeof(image_attest));
    ffx_scene_appendChild(node, connect);
    ffx_scene_nodeSetPosition(connect, (FfxPoint){
        .x = (240 - 151) / 2, .y = 140
    });
    state->connect = connect;

    panel_enableMessage(true);
*/
    panel_onEvent(EventNameKeysChanged | KeyOk | KeyCancel, keyChanged, state);
//    panel_onEvent(EventNameMessage, onMessage, state);

    return 0;
}

void pushPanelAttest(void *arg) {
    panel_push(_init, sizeof(AttestState), PanelStyleSlideLeft, arg);
}
