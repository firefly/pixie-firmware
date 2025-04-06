#include <stdio.h>
#include <string.h>

#include "firefly-cbor.h"

#include "device-info.h"

#include "panel.h"
#include "panel-attest.h"

#define TEXT_LENGTH      (20)

typedef struct AttestState {
    FfxScene scene;
    FfxNode connect;
} AttestState;


static void keyChanged(EventPayload event, void *_state) {
    AttestState *state = _state;

    switch(event.props.keys.down) {
        case KeyOk:
            printf("ok\n");
            break;
        case KeyCancel:
            panel_pop();
            break;
    }
}

const char* const txtListening = "listening...";

static void setMessage(AttestState *state, char *message) {
}

static int _init(FfxScene scene, FfxNode node, void *_state, void *arg) {
    AttestState *state = _state;
    state->scene = scene;

    FfxNode box = ffx_scene_createBox(scene, ffx_size(200, 180));
    ffx_sceneGroup_appendChild(node, box);
    ffx_sceneBox_setColor(box, RGBA_DARKER75);
    ffx_sceneNode_setPosition(box, ffx_point(20, 30));

    char text[32];
    FfxNode label;

    label = ffx_scene_createLabel(scene, FfxFontLarge, "Device");
    ffx_sceneGroup_appendChild(node, label);
    ffx_sceneNode_setPosition(label, ffx_point(120, 70));
    ffx_sceneLabel_setAlign(label, FfxTextAlignCenter | FfxTextAlignBaseline);

    snprintf(text, sizeof(text), "DEV: 0x%lx", device_modelNumber());

    label = ffx_scene_createLabel(scene, FfxFontLarge, text);
    ffx_sceneGroup_appendChild(node, label);
    ffx_sceneNode_setPosition(label, ffx_point(40, 120));

    snprintf(text, sizeof(text), "S/N: %ld", device_serialNumber());

    label = ffx_scene_createLabel(scene, FfxFontLarge, text);
    ffx_sceneGroup_appendChild(node, label);
    ffx_sceneNode_setPosition(label, ffx_point(40, 160));

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

    return 0;
}

void pushPanelAttest(void *arg) {
    panel_push(_init, sizeof(AttestState), PanelStyleSlideLeft, arg);
}
