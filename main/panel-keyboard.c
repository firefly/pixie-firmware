#include <stdio.h>

#include "firefly-scene.h"
#include "firefly-color.h"

#include "utils.h"

#include "panel.h"
#include "panel-keyboard.h"

typedef struct KeyboardKey {
    int8_t row, col;
    FfxNode label;
} KeyboardKey;

typedef struct State {
    FfxNode text, caret, node;
    KeyboardKey keys[26];
    FfxNode cursor;
    int row;

    uint32_t ticks;
    uint32_t caretFrames;

    //uint32_t lastTyped;
    //uint32_t lastStrobe;

    uint32_t keysDown;

    uint32_t steps;
    int stepCount;
} State;

//static char* keyboard = "QWERTYUIOPASDFGHJKLZXCVBNM";
//#define KB_ROW_ONE      (10)
//#define KB_ROW_TWO      (19)

static char* keyboard = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
#define KB_ROW_ONE      (9)
#define KB_ROW_TWO      (18)

static int indexToKeyboard(int index) { return keyboard[index] - 'A'; }

static void resetKeys(State *state) {
    for (int i = 0; i < 26; i++) {

        int row = 0;
        int col = i;
        if (i >= KB_ROW_TWO) {
             row = 2;
             col -= KB_ROW_TWO;
        } else if (i >= KB_ROW_ONE) {
             row = 1;
             col -= KB_ROW_ONE;
        }

        int kb = indexToKeyboard(i);
        state->keys[kb].row = row;
        state->keys[kb].col = col;
    }

    state->steps = 0;
    state->stepCount = 0;
}

// Sets the (x, y) of the %%index%% key (0 => A, 1 => B, ...)
static void updateKeyPosition(State *state, int index, bool animated) {
    int row = state->keys[index].row;

    FfxNode label = state->keys[index].label;

    ffx_sceneLabel_setFont(label, FfxFontLarge);

    if (animated) {
        ffx_sceneNode_stopAnimations(label, FfxSceneActionStopCurrent);
    }

    if (row == -1) {
        if (animated) {
            ffx_sceneLabel_animateOpacity(label, 0, 0, 500, FfxCurveLinear,
              NULL, NULL);
        } else {
            ffx_sceneLabel_setOpacity(label, 0);
        }
        return;
    }

    if (animated) {
        ffx_sceneLabel_animateOpacity(label, OPACITY_90, 0, 500,
          FfxCurveLinear, NULL, NULL);
    } else {
        ffx_sceneLabel_setOpacity(label, OPACITY_90);
    }

    // Count how many keys are in the top and this row
    int topRowCount = 0;
    int count = 0;
    for (int i = 0; i < 26; i++) {
        int r = state->keys[i].row;
        if (r == 0) { topRowCount++; }
        if (r == row) { count++; }
    }

    int padding = 30;
    int width = (240 - (2 * padding)) / (topRowCount - 1);

    int col = state->keys[index].col;

    int x = ((240 - ((count - 1) * width)) / 2) + col * width;
    int y = 100 + row * 50;
    if (animated) {
        ffx_sceneNode_animatePosition(label, ffx_point(x, y), 0, 500,
          FfxCurveEaseOutQuad, NULL, NULL);
    } else {
        ffx_sceneNode_setPosition(state->keys[index].label, ffx_point(x, y));
    }
}

static void updateCaret(State *state) {
    FfxFontMetrics metrics = ffx_scene_getFontMetrics(FfxFontLarge);
    size_t length = ffx_sceneLabel_getTextLength(state->text);
    int width = length * (metrics.size.width + 2);
    if (width) { width -= 2; }
    ffx_sceneNode_setPosition(state->caret, ffx_point(3 + 30 + width, 50));
}


static void resetKeyboard(State *state, bool animated) {
    resetKeys(state);

    for (int i = 0; i < 26; i++) {
        updateKeyPosition(state, i, animated);
    }
}

static void removeNode(FfxNode node, FfxSceneActionStop stopType, void *_state) {
    State *state = _state;

    char output[8];
    ffx_sceneLabel_copyText(node, output, sizeof(output));

    state->caretFrames = 0;
    ffx_sceneLabel_appendCharacter(state->text, output[0]);
    updateCaret(state);

    ffx_sceneNode_remove(node, true);
}

static bool selectRow(State *state, int row, bool animated) {
    // Count and the number of items in the row and hide other row items
    int count = 0;
    int c = 0;
    for (int i = 0; i < 26; i++) {
        if (state->keys[i].row == row) {
            c = i;
            count++;
        } else {
            state->keys[i].row = -1;
        }
    }

    if (count == 0) { return false; }

    // Selected a single-character
    if (count == 1) {
        char chr = ((char)c) + 'A';

        if (animated) {
            FfxNode key = state->keys[c].label;

            FfxScene scene = ffx_sceneNode_getScene(key);

            char text[] = { chr, '\0' };
            FfxNode *ghost = ffx_scene_createLabel(scene, FfxFontLarge, text);
            ffx_sceneNode_setPosition(ghost, ffx_sceneNode_getPosition(key));
            ffx_sceneGroup_appendChild(state->node, ghost);

            ffx_sceneNode_animatePosition(ghost,
              ffx_sceneNode_getPosition(state->caret), 0, 200,
              FfxCurveEaseOutQuad, removeNode, state);
        } else {
            ffx_sceneLabel_appendCharacter(state->text, chr);
            updateCaret(state);
            state->caretFrames = 0;
        }


        return true;
    }

    // Track the selection history
    state->steps |= row << (2 * state->stepCount);
    state->stepCount++;

    int topRowCount = (count + 2) / 3;

    int found = 0;
    for (int i = 0; i < 26; i++) {
        int kb = indexToKeyboard(i);

        if (state->keys[kb].row == -1) { continue; }

        state->keys[kb].row = found / topRowCount;
        state->keys[kb].col = found % topRowCount;

        found++;
    }

    return false;
}

static void drillDown(State *state, int row, bool animated) {
    bool selected = selectRow(state, row, animated);

    if (selected) {
        resetKeyboard(state, animated);
        return;
    }

    for (int i = 0; i < 26; i++) {
        updateKeyPosition(state, i, animated);
    }
}

static void highlightRow(State *state, int row) {
    for (int i = 0; i < 26; i++) {
        uint8_t opacity = OPACITY_40;
        FfxFont font = FfxFontLarge;
        int r = state->keys[i].row;
        if (r == -1) {
            opacity = 0;
        } else if (r == row) {
            opacity = MAX_OPACITY;
            font = FfxFontLargeBold;
        }
        ffx_sceneLabel_setOpacity(state->keys[i].label, opacity);
        ffx_sceneLabel_setFont(state->keys[i].label, font);
    }
}

static void keysDown(EventPayload event, void *_state) {
    State *state = _state;

    state->keysDown = event.props.keys.down;

    switch(state->keysDown) {
        case KeyOk:
            highlightRow(state, 0);
            break;
        case KeyNorth:
            highlightRow(state, 1);
            break;
        case KeySouth:
            highlightRow(state, 2);
            break;
    }
}

static void keysUp(EventPayload event, void *_state) {
    State *state = _state;

    switch(state->keysDown) {
        case KeyCancel:
            if (state->stepCount == 0) {
                size_t length = ffx_sceneLabel_getTextLength(state->text);
                if (length) {
                    ffx_sceneLabel_snipText(state->text, length - 1, 1);
                    updateCaret(state);
                    state->caretFrames = 0;
                }

            } else {
                // Replay the last n-1 steps
                uint32_t steps = state->steps;
                int stepCount = state->stepCount;

                resetKeys(state);
                printf("STEP: steps=%lx count=%d\n", steps, stepCount);
                for (int i = 0; i < stepCount - 1; i++) {
                    int row = (steps >> (2 * i)) & 0x3;
                    selectRow(state, row, false);
                }

                // Animate the keys back to where they were
                for (int i = 0; i < 26; i++) {
                    updateKeyPosition(state, i, true);
                }
            }
            break;
        case KeyOk:
            drillDown(state, 0, true);
            break;
        case KeyNorth:
            drillDown(state, 1, true);
            break;
        case KeySouth:
            drillDown(state, 2, true);
            break;
    }
}

static void render(EventPayload event, void *_state) {
    State *state = _state;

    state->ticks = event.props.render.ticks;
    state->caretFrames++;

    // Blink the caret
    ffx_sceneLabel_setTextColor(state->caret,
      ffx_color_gray(((state->caretFrames >> 5) & 0x1) ? 128: 255));

    /*
    uint32_t deltaStrobe = state->ticks - state->lastStrobe;
    int strobe = -1;
    if (deltaStrobe > 3000) {
        int s = (deltaStrobe - 3000) / 256;
        if (s < 3) {
            strobe = s + 1;
        } else {
            state->lastStrobe = state->ticks;
        }
    }

    for (int i = 1; i < 4; i++) {
        if (i == strobe) {
            panel_setPixel(i, ffx_color_rgb(40, 40, 40, MAX_OPACITY));
        } else {
            panel_setPixel(i, 0);
        }

        for (int i = 0; i < 26; i++) {
            int row = state->keys[i].row;
            //ffx_sceneLabel_setOpacity(state->keys[i].label,
            //  (strobe == -1 || row + 1 == strobe) ? MAX_OPACITY: OPACITY_80);
            ffx_sceneLabel_setFont(state->keys[i].label,
              (row + 1 == strobe) ? FfxFontLargeBold: FfxFontLarge);
        }
    }
    */
}

static int _init(FfxScene scene, FfxNode node, void *_state, void *arg) {
    State *state = _state;

    state->node = node;

    FfxNode box = ffx_scene_createBox(scene, ffx_size(200, 200));
    ffx_sceneNode_setPosition(box, ffx_point(20, 20));
    ffx_sceneBox_setColor(box, RGBA_DARKER75);
    ffx_sceneGroup_appendChild(node, box);

    FfxNode text = ffx_scene_createLabel(scene, FfxFontLargeBold, "");
    state->text = text;
    ffx_sceneLabel_setAlign(text, FfxTextAlignLeft | FfxTextAlignMiddle);
    ffx_sceneNode_setPosition(text, ffx_point(30, 50));
    ffx_sceneGroup_appendChild(node, text);

    FfxNode caret = ffx_scene_createLabel(scene, FfxFontLargeBold, "_");
    state->caret = caret;
    ffx_sceneLabel_setAlign(caret, FfxTextAlignLeft | FfxTextAlignMiddle);
    ffx_sceneNode_setPosition(caret, ffx_point(30, 50));
    ffx_sceneGroup_appendChild(node, caret);
    updateCaret(state);

    // Create the key labels
    for (int i = 0; i < 26; i++) {
        char chr = keyboard[i];
        char str[] = { chr, '\0' };

        FfxNode label = ffx_scene_createLabel(scene, FfxFontLarge, str);
        state->keys[chr - 'A'].label = label;
        ffx_sceneLabel_setAlign(label, FfxTextAlignCenter | FfxTextAlignMiddle);
        ffx_sceneLabel_setTextColor(label,
          ffx_color_hsv((i * 94) % 360, 0x0f, 0x3f));
        ffx_sceneGroup_appendChild(node, label);

    }
    resetKeyboard(state, false);

    panel_onEvent(EventNameKeysUp | KeyNorth | KeySouth | KeyOk | KeyCancel,
      keysUp, state);

    panel_onEvent(EventNameKeysDown | KeyNorth | KeySouth | KeyOk | KeyCancel,
      keysDown, state);

    panel_onEvent(EventNameRenderScene, render, state);

    return 0;
}

void pushPanelKeyboard(void *arg) {
    panel_push(_init, sizeof(State), PanelStyleSlideLeft, arg);
}
