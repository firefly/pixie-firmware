#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "firefly-scene.h"
#include "firefly-color.h"
#include "panel.h"
#include "panel-buttontest.h"
#include "utils.h"

typedef struct ButtonTestState {
    FfxScene scene;
    FfxNode titleLabel;
    FfxNode button1Label;
    FfxNode button2Label;
    FfxNode button3Label;
    FfxNode button4Label;
    FfxNode instructionsLabel;
    FfxNode hexLabel;
    FfxNode exitLabel;
    
    char button1Text[64];
    char button2Text[64];
    char button3Text[64];
    char button4Text[64];
    char hexText[32];
    
    uint32_t southHoldStart;
} ButtonTestState;

static void updateButtonDisplay(ButtonTestState *state, Keys keys) {
    // Update button status display with correct key mapping
    snprintf(state->button1Text, sizeof(state->button1Text), "Button 1: %s (Cancel=0x%04x)", 
             (keys & KeyCancel) ? "PRESSED" : "released", KeyCancel);
    snprintf(state->button2Text, sizeof(state->button2Text), "Button 2: %s (Ok=0x%04x)", 
             (keys & KeyOk) ? "PRESSED" : "released", KeyOk);
    snprintf(state->button3Text, sizeof(state->button3Text), "Button 3: %s (North=0x%04x)", 
             (keys & KeyNorth) ? "PRESSED" : "released", KeyNorth);
    snprintf(state->button4Text, sizeof(state->button4Text), "Button 4: %s (South=0x%04x)", 
             (keys & KeySouth) ? "PRESSED" : "released", KeySouth);
    snprintf(state->hexText, sizeof(state->hexText), "Raw Keys: 0x%04x", keys);
    
    ffx_sceneLabel_setText(state->button1Label, state->button1Text);
    ffx_sceneLabel_setText(state->button2Label, state->button2Text);
    ffx_sceneLabel_setText(state->button3Label, state->button3Text);
    ffx_sceneLabel_setText(state->button4Label, state->button4Text);
    ffx_sceneLabel_setText(state->hexLabel, state->hexText);
    
    // Change color for pressed buttons
    ffx_sceneLabel_setTextColor(state->button1Label, 
        (keys & KeyCancel) ? ffx_color_rgb(0, 255, 0) : ffx_color_rgb(255, 255, 255));
    ffx_sceneLabel_setTextColor(state->button2Label, 
        (keys & KeyOk) ? ffx_color_rgb(0, 255, 0) : ffx_color_rgb(255, 255, 255));
    ffx_sceneLabel_setTextColor(state->button3Label, 
        (keys & KeyNorth) ? ffx_color_rgb(0, 255, 0) : ffx_color_rgb(255, 255, 255));
    ffx_sceneLabel_setTextColor(state->button4Label, 
        (keys & KeySouth) ? ffx_color_rgb(0, 255, 0) : ffx_color_rgb(255, 255, 255));
}

static void keyChanged(EventPayload event, void *_state) {
    ButtonTestState *state = _state;
    
    Keys keys = event.props.keys.down;
    
    // Detailed logging for each button
    printf("[buttontest] ======== BUTTON PRESS EVENT ========\n");
    printf("[buttontest] Raw keys value: 0x%04x\n", keys);
    printf("[buttontest] KeyCancel (0x%04x): %s\n", KeyCancel, (keys & KeyCancel) ? "PRESSED" : "released");
    printf("[buttontest] KeyOk     (0x%04x): %s\n", KeyOk, (keys & KeyOk) ? "PRESSED" : "released");
    printf("[buttontest] KeyNorth  (0x%04x): %s\n", KeyNorth, (keys & KeyNorth) ? "PRESSED" : "released");
    printf("[buttontest] KeySouth  (0x%04x): %s\n", KeySouth, (keys & KeySouth) ? "PRESSED" : "released");
    printf("[buttontest] =====================================\n");
    
    // Update visual display
    updateButtonDisplay(state, keys);
    
    // Handle exit with any button hold for 2 seconds
    bool anyButtonPressed = (keys & (KeyCancel | KeyOk | KeyNorth | KeySouth)) != 0;
    
    if (anyButtonPressed) {
        if (state->southHoldStart == 0) {
            state->southHoldStart = ticks();
        }
    } else {
        state->southHoldStart = 0;
    }
}

static void render(EventPayload event, void *_state) {
    ButtonTestState *state = _state;
    
    uint32_t now = ticks();
    
    // Check for hold-to-exit
    if (state->southHoldStart > 0 && (now - state->southHoldStart) > 2000) {
        printf("[buttontest] Exiting after 2-second button hold\n");
        panel_pop();
        return;
    }
}

static int init(FfxScene scene, FfxNode node, void* _state, void* arg) {
    ButtonTestState *state = _state;
    state->scene = scene;
    
    printf("[buttontest] Button Test App Started\n");
    printf("[buttontest] Press each physical button (1,2,3,4) to see mapping\n");
    printf("[buttontest] Hold any button for 2 seconds to exit\n");
    
    // Create title
    state->titleLabel = ffx_scene_createLabel(scene, FfxFontLarge, "Button Test");
    ffx_sceneGroup_appendChild(node, state->titleLabel);
    ffx_sceneNode_setPosition(state->titleLabel, (FfxPoint){ .x = 80, .y = 10 });
    
    // Create button status labels
    state->button1Label = ffx_scene_createLabel(scene, FfxFontSmall, "Button 1: released");
    ffx_sceneGroup_appendChild(node, state->button1Label);
    ffx_sceneNode_setPosition(state->button1Label, (FfxPoint){ .x = 10, .y = 40 });
    
    state->button2Label = ffx_scene_createLabel(scene, FfxFontSmall, "Button 2: released");
    ffx_sceneGroup_appendChild(node, state->button2Label);
    ffx_sceneNode_setPosition(state->button2Label, (FfxPoint){ .x = 10, .y = 60 });
    
    state->button3Label = ffx_scene_createLabel(scene, FfxFontSmall, "Button 3: released");
    ffx_sceneGroup_appendChild(node, state->button3Label);
    ffx_sceneNode_setPosition(state->button3Label, (FfxPoint){ .x = 10, .y = 80 });
    
    state->button4Label = ffx_scene_createLabel(scene, FfxFontSmall, "Button 4: released");
    ffx_sceneGroup_appendChild(node, state->button4Label);
    ffx_sceneNode_setPosition(state->button4Label, (FfxPoint){ .x = 10, .y = 100 });
    
    // Raw hex display
    state->hexLabel = ffx_scene_createLabel(scene, FfxFontMedium, "Raw Keys: 0x0000");
    ffx_sceneGroup_appendChild(node, state->hexLabel);
    ffx_sceneNode_setPosition(state->hexLabel, (FfxPoint){ .x = 10, .y = 130 });
    
    // Instructions
    state->instructionsLabel = ffx_scene_createLabel(scene, FfxFontSmall, "Press each button 1-4");
    ffx_sceneGroup_appendChild(node, state->instructionsLabel);
    ffx_sceneNode_setPosition(state->instructionsLabel, (FfxPoint){ .x = 10, .y = 160 });
    
    state->exitLabel = ffx_scene_createLabel(scene, FfxFontSmall, "Hold any button 2s to exit");
    ffx_sceneGroup_appendChild(node, state->exitLabel);
    ffx_sceneNode_setPosition(state->exitLabel, (FfxPoint){ .x = 10, .y = 180 });
    
    // Initialize state
    state->southHoldStart = 0;
    
    // Initialize display
    updateButtonDisplay(state, 0);
    
    // Register for all key events
    panel_onEvent(EventNameKeysChanged | KeyCancel | KeyOk | KeyNorth | KeySouth, keyChanged, state);
    panel_onEvent(EventNameRenderScene, render, state);
    
    return 0;
}

void pushPanelButtonTest(void* arg) {
    panel_push(init, sizeof(ButtonTestState), PanelStyleSlideLeft, arg);
}