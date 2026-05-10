// Vertical Breakout. Bricks on the left, paddle on the right, ball
// bounces between. N/S move the paddle along the right edge.

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "firefly-hollows.h"
#include "firefly-scene.h"

#include "feedback.h"
#include "panels.h"
#include "utils.h"


#define BRICK_COLS      (5)
#define BRICK_ROWS      (10)
#define BRICK_W         (16)
#define BRICK_H         (16)
#define BRICK_GAP       (2)
#define BRICK_X0        (16)
#define BRICK_Y0        (24)

#define PADDLE_X        (220)
#define PADDLE_W        (5)
#define PADDLE_H        (44)

#define BALL_SIZE       (5)


typedef struct BrickState {
    FfxScene scene;
    FfxNode bg;
    FfxNode bricks[BRICK_COLS * BRICK_ROWS];
    bool brickAlive[BRICK_COLS * BRICK_ROWS];
    FfxNode paddle;
    FfxNode ball;
    FfxNode scoreLabel;
    FfxNode hint;
    FfxNode statusLabel;

    int16_t paddleY;
    int16_t ballX, ballY;
    int8_t ballVx, ballVy;
    bool launched;
    int score;
    int lives;

    FfxKeys keys;
} BrickState;


static void resetField(BrickState *state) {
    for (int i = 0; i < BRICK_COLS * BRICK_ROWS; i++) {
        state->brickAlive[i] = true;
        ffx_sceneNode_setHidden(state->bricks[i], false);
    }
    state->score = 0;
    state->lives = 3;
}

static void resetBall(BrickState *state) {
    state->ballX = PADDLE_X - BALL_SIZE - 2;
    state->ballY = state->paddleY + PADDLE_H / 2 - BALL_SIZE / 2;
    state->ballVx = -2;
    state->ballVy = -2;
    state->launched = false;
    ffx_sceneNode_setPosition(state->ball, ffx_point(state->ballX, state->ballY));
}

static void onKeys(FfxEvent event, FfxEventProps props, void *_state) {
    BrickState *state = _state;
    state->keys = props.keys.down;
    feedback_onKey(props.keys.down);
    if (props.keys.down & FfxKeyCancel) {
        ffx_popPanel(0);
        return;
    }
    if (props.keys.down & FfxKeyOk) {
        if (state->lives <= 0) {
            resetField(state);
            resetBall(state);
            ffx_sceneNode_setHidden(state->statusLabel, true);
        } else if (!state->launched) {
            state->launched = true;
        }
    }
}

static void onRender(FfxEvent event, FfxEventProps props, void *_state) {
    BrickState *state = _state;
    if (state->lives <= 0) { return; }

    if (state->keys & FfxKeyNorth) { state->paddleY -= 4; }
    if (state->keys & FfxKeySouth) { state->paddleY += 4; }
    if (state->paddleY < 0)                 { state->paddleY = 0; }
    if (state->paddleY > 240 - PADDLE_H)    { state->paddleY = 240 - PADDLE_H; }
    ffx_sceneNode_setPosition(state->paddle, ffx_point(PADDLE_X, state->paddleY));

    if (!state->launched) {
        state->ballX = PADDLE_X - BALL_SIZE - 2;
        state->ballY = state->paddleY + PADDLE_H / 2 - BALL_SIZE / 2;
        ffx_sceneNode_setPosition(state->ball, ffx_point(state->ballX, state->ballY));
        return;
    }

    state->ballX += state->ballVx;
    state->ballY += state->ballVy;

    if (state->ballY <= 0)              { state->ballY = 0; state->ballVy = -state->ballVy; }
    if (state->ballY >= 240 - BALL_SIZE){ state->ballY = 240 - BALL_SIZE; state->ballVy = -state->ballVy; }
    if (state->ballX <= 0)              { state->ballX = 0; state->ballVx = -state->ballVx; }

    if (state->ballX + BALL_SIZE >= PADDLE_X &&
        state->ballX + BALL_SIZE <= PADDLE_X + PADDLE_W &&
        state->ballY + BALL_SIZE >= state->paddleY &&
        state->ballY <= state->paddleY + PADDLE_H && state->ballVx > 0) {
        state->ballVx = -state->ballVx;
        int rel = (state->ballY + BALL_SIZE / 2) - (state->paddleY + PADDLE_H / 2);
        state->ballVy = rel / 8;
        if (state->ballVy < -3) { state->ballVy = -3; }
        if (state->ballVy >  3) { state->ballVy =  3; }
        if (state->ballVy == 0) { state->ballVy = (state->ballX & 1) ? -1 : 1; }
    }

    if (state->ballX > 240) {
        state->lives--;
        if (state->lives <= 0) {
            ffx_sceneLabel_setText(state->statusLabel, "GAME OVER  OK=RESTART");
            ffx_sceneNode_setHidden(state->statusLabel, false);
        } else {
            resetBall(state);
        }
    }

    int br = -1;
    for (int i = 0; i < BRICK_COLS * BRICK_ROWS; i++) {
        if (!state->brickAlive[i]) { continue; }
        int col = i % BRICK_COLS;
        int row = i / BRICK_COLS;
        int bx = BRICK_X0 + col * (BRICK_W + BRICK_GAP);
        int by = BRICK_Y0 + row * (BRICK_H + BRICK_GAP);
        if (state->ballX + BALL_SIZE >= bx && state->ballX <= bx + BRICK_W &&
            state->ballY + BALL_SIZE >= by && state->ballY <= by + BRICK_H) {
            br = i;
            break;
        }
    }
    if (br >= 0) {
        state->brickAlive[br] = false;
        ffx_sceneNode_setHidden(state->bricks[br], true);
        state->ballVx = -state->ballVx;
        state->score += 10;
    }

    ffx_sceneNode_setPosition(state->ball, ffx_point(state->ballX, state->ballY));
    ffx_sceneLabel_setTextFormat(state->scoreLabel, "%d  L:%d",
      state->score, state->lives);

    bool anyAlive = false;
    for (int i = 0; i < BRICK_COLS * BRICK_ROWS; i++) {
        if (state->brickAlive[i]) { anyAlive = true; break; }
    }
    if (!anyAlive) {
        ffx_sceneLabel_setText(state->statusLabel, "CLEAR  OK=AGAIN");
        ffx_sceneNode_setHidden(state->statusLabel, false);
        state->lives = 0;
    }
}

static int initFunc(FfxScene scene, FfxNode panel, void *_state, void *arg) {
    BrickState *state = _state;
    state->scene = scene;
    state->paddleY = 120 - PADDLE_H / 2;

    state->bg = ffx_scene_createBox(scene, ffx_size(240, 240));
    ffx_sceneBox_setColor(state->bg, ffx_color_rgb(4, 6, 16));
    ffx_sceneGroup_appendChild(panel, state->bg);
    ffx_sceneNode_setPosition(state->bg, ffx_point(0, 0));

    static const uint8_t rowR[BRICK_ROWS] = {
        255, 255, 255, 220, 180, 140, 80,  40,  0,   0
    };
    static const uint8_t rowG[BRICK_ROWS] = {
        40,  120, 200, 255, 255, 255, 220, 200, 180, 160
    };
    static const uint8_t rowB[BRICK_ROWS] = {
        80,  60,  40,  60,  120, 200, 240, 220, 180, 140
    };

    for (int row = 0; row < BRICK_ROWS; row++) {
        for (int col = 0; col < BRICK_COLS; col++) {
            int i = row * BRICK_COLS + col;
            FfxNode brick = ffx_scene_createBox(scene, ffx_size(BRICK_W, BRICK_H));
            ffx_sceneBox_setColor(brick,
              ffx_color_rgb(rowR[row], rowG[row], rowB[row]));
            ffx_sceneGroup_appendChild(panel, brick);
            ffx_sceneNode_setPosition(brick, ffx_point(
              BRICK_X0 + col * (BRICK_W + BRICK_GAP),
              BRICK_Y0 + row * (BRICK_H + BRICK_GAP)));
            state->bricks[i] = brick;
        }
    }

    state->paddle = ffx_scene_createBox(scene, ffx_size(PADDLE_W, PADDLE_H));
    ffx_sceneBox_setColor(state->paddle, ffx_color_rgb(0, 255, 220));
    ffx_sceneGroup_appendChild(panel, state->paddle);

    state->ball = ffx_scene_createBox(scene, ffx_size(BALL_SIZE, BALL_SIZE));
    ffx_sceneBox_setColor(state->ball, ffx_color_rgb(255, 255, 255));
    ffx_sceneGroup_appendChild(panel, state->ball);

    state->scoreLabel = ffx_scene_createLabel(scene, FfxFontMedium, "0  L:3");
    ffx_sceneGroup_appendChild(panel, state->scoreLabel);
    ffx_sceneNode_setPosition(state->scoreLabel, ffx_point(120, 12));
    ffx_sceneLabel_setAlign(state->scoreLabel,
      FfxTextAlignCenter | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(state->scoreLabel, COLOR_BLACK);

    state->hint = NULL;
    feedback_addButtonLegend(panel, "UP", "DOWN", "LAUNCH", "EXIT");

    state->statusLabel = ffx_scene_createLabel(scene, FfxFontLargeBold,
      "GAME OVER");
    ffx_sceneGroup_appendChild(panel, state->statusLabel);
    ffx_sceneNode_setPosition(state->statusLabel, ffx_point(120, 120));
    ffx_sceneLabel_setAlign(state->statusLabel,
      FfxTextAlignCenter | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(state->statusLabel, COLOR_BLACK);
    ffx_sceneNode_setHidden(state->statusLabel, true);

    resetField(state);
    resetBall(state);
    ffx_sceneNode_setPosition(state->paddle, ffx_point(PADDLE_X, state->paddleY));

    ffx_onEvent(FfxEventKeys, onKeys, state);
    ffx_onEvent(FfxEventRenderScene, onRender, state);

    return 0;
}

int pushPanelBrick() {
    return ffx_pushPanel(initFunc, sizeof(BrickState), NULL);
}
