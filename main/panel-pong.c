#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "firefly-scene.h"
#include "firefly-color.h"
#include "panel.h"
#include "panel-pong.h"
#include "utils.h"

#define PADDLE_WIDTH 4
#define PADDLE_HEIGHT 30
#define BALL_SIZE 6
#define GAME_WIDTH 200  // Rotated: horizontal layout, wider
#define GAME_HEIGHT 120 // Rotated: horizontal layout, shorter
#define PADDLE_SPEED 3
#define BALL_SPEED 2

typedef struct PongState {
    FfxScene scene;
    FfxNode gameArea;
    FfxNode playerPaddle;  // Bottom paddle (player)
    FfxNode aiPaddle;      // Top paddle (AI)
    FfxNode ball;
    FfxNode scoreLabel;
    FfxNode centerLine;
    FfxNode pausedLabel;
    
    // Game state (rotated: paddles move vertically)
    float playerPaddleY;   // Player paddle Y position (right side)
    float aiPaddleY;       // AI paddle Y position (left side)
    float ballX, ballY;
    float ballVelX, ballVelY;
    int playerScore, aiScore;
    bool gameOver;
    bool paused;
    Keys keys;
    uint32_t southHoldStart;
    uint32_t gameStartTime;
    char scoreText[32];
} PongState;

static void resetBall(PongState *state) {
    state->ballX = GAME_WIDTH / 2;
    state->ballY = GAME_HEIGHT / 2;
    
    // Random direction but not too steep for horizontal play
    float angle = (rand() % 60 - 30) * M_PI / 180.0; // -30 to +30 degrees
    if (rand() % 2) angle += M_PI; // Sometimes go left instead of right
    
    state->ballVelX = BALL_SPEED * cos(angle);  // Primarily horizontal
    state->ballVelY = BALL_SPEED * sin(angle);  // Some vertical component
}

static void updateGame(PongState *state) {
    if (state->paused || state->gameOver) return;
    
    // Standardized controls:
    // Button 1 (KeyCancel) = Primary action (speed boost)
    // Button 2 (KeyOk) = Pause/Exit (handled in keyChanged)
    // Button 3 (KeyNorth) = Up/Right movement (90° counter-clockwise)
    // Button 4 (KeySouth) = Down/Left movement
    
    float speed = (state->keys & KeyCancel) ? PADDLE_SPEED * 2 : PADDLE_SPEED;
    
    // Rotated controls: paddles move up/down
    // Button 3 (North) = Move up
    if (state->keys & KeyNorth) {
        state->playerPaddleY -= speed;
        if (state->playerPaddleY < 0) {
            state->playerPaddleY = 0;
        }
    }
    
    // Button 4 (South) = Move down
    if (state->keys & KeySouth) {
        state->playerPaddleY += speed;
        if (state->playerPaddleY > GAME_HEIGHT - PADDLE_HEIGHT) {
            state->playerPaddleY = GAME_HEIGHT - PADDLE_HEIGHT;
        }
    }
    
    // Simple AI for left paddle (follows ball vertically)
    float paddleCenter = state->aiPaddleY + PADDLE_HEIGHT / 2;
    float ballCenter = state->ballY + BALL_SIZE / 2;
    
    if (paddleCenter < ballCenter - 5) {
        state->aiPaddleY += PADDLE_SPEED * 0.8; // AI slightly slower
        if (state->aiPaddleY > GAME_HEIGHT - PADDLE_HEIGHT) {
            state->aiPaddleY = GAME_HEIGHT - PADDLE_HEIGHT;
        }
    } else if (paddleCenter > ballCenter + 5) {
        state->aiPaddleY -= PADDLE_SPEED * 0.8;
        if (state->aiPaddleY < 0) state->aiPaddleY = 0;
    }
    
    // Move ball
    state->ballX += state->ballVelX;
    state->ballY += state->ballVelY;
    
    // Ball collision with top/bottom walls
    if (state->ballY <= 0 || state->ballY >= GAME_HEIGHT - BALL_SIZE) {
        state->ballVelY = -state->ballVelY;
        if (state->ballY <= 0) state->ballY = 0;
        if (state->ballY >= GAME_HEIGHT - BALL_SIZE) state->ballY = GAME_HEIGHT - BALL_SIZE;
    }
    
    // Ball collision with player paddle (right side)
    if (state->ballX + BALL_SIZE >= GAME_WIDTH - PADDLE_WIDTH && 
        state->ballY + BALL_SIZE >= state->playerPaddleY && 
        state->ballY <= state->playerPaddleY + PADDLE_HEIGHT) {
        
        state->ballVelX = -state->ballVelX;
        state->ballX = GAME_WIDTH - PADDLE_WIDTH - BALL_SIZE;
        
        // Add some english based on where ball hits paddle
        float hitPos = (state->ballY + BALL_SIZE/2 - state->playerPaddleY - PADDLE_HEIGHT/2) / (PADDLE_HEIGHT/2);
        state->ballVelY += hitPos * 0.5;
        
        // Limit ball speed
        if (fabs(state->ballVelY) > BALL_SPEED * 1.5) {
            state->ballVelY = (state->ballVelY > 0) ? BALL_SPEED * 1.5 : -BALL_SPEED * 1.5;
        }
    }
    
    // Ball collision with AI paddle (left side)
    if (state->ballX <= PADDLE_WIDTH && 
        state->ballY + BALL_SIZE >= state->aiPaddleY && 
        state->ballY <= state->aiPaddleY + PADDLE_HEIGHT) {
        
        state->ballVelX = -state->ballVelX;
        state->ballX = PADDLE_WIDTH;
        
        // Add some english
        float hitPos = (state->ballY + BALL_SIZE/2 - state->aiPaddleY - PADDLE_HEIGHT/2) / (PADDLE_HEIGHT/2);
        state->ballVelY += hitPos * 0.5;
        
        // Limit ball speed
        if (fabs(state->ballVelY) > BALL_SPEED * 1.5) {
            state->ballVelY = (state->ballVelY > 0) ? BALL_SPEED * 1.5 : -BALL_SPEED * 1.5;
        }
    }
    
    // Ball goes off left/right edges (scoring)
    if (state->ballX < -BALL_SIZE) {
        state->playerScore++;  // Player scores when ball goes off left (AI side)
        resetBall(state);
        snprintf(state->scoreText, sizeof(state->scoreText), "Player %d - AI %d", state->playerScore, state->aiScore);
        ffx_sceneLabel_setText(state->scoreLabel, state->scoreText);
        
        if (state->playerScore >= 7) {
            state->gameOver = true;
        }
    } else if (state->ballX > GAME_WIDTH) {
        state->aiScore++;  // AI scores when ball goes off right (player side)
        resetBall(state);
        snprintf(state->scoreText, sizeof(state->scoreText), "Player %d - AI %d", state->playerScore, state->aiScore);
        ffx_sceneLabel_setText(state->scoreLabel, state->scoreText);
        
        if (state->aiScore >= 7) {
            state->gameOver = true;
        }
    }
}

static void updateVisuals(PongState *state) {
    // Apply game area offset for rotated layout
    int offsetX = 20;
    int offsetY = 60;
    
    // Player paddle at right side
    ffx_sceneNode_setPosition(state->playerPaddle, (FfxPoint){ 
        .x = offsetX + GAME_WIDTH - PADDLE_WIDTH,
        .y = offsetY + (int)state->playerPaddleY
    });
    
    // AI paddle at left side
    ffx_sceneNode_setPosition(state->aiPaddle, (FfxPoint){ 
        .x = offsetX,
        .y = offsetY + (int)state->aiPaddleY
    });
    
    ffx_sceneNode_setPosition(state->ball, (FfxPoint){ 
        .x = offsetX + (int)state->ballX, 
        .y = offsetY + (int)state->ballY 
    });
}

static void keyChanged(EventPayload event, void *_state) {
    PongState *state = _state;
    printf("[pong] keyChanged called! keys=0x%04x\n", event.props.keys.down);
    
    // Standardized controls:
    // Button 1 (KeyCancel) = Primary action (speed boost) 
    // Button 2 (KeyOk) = Pause/Exit (hold 1s)
    // Button 3 (KeyNorth) = Up/Right movement (90° counter-clockwise)
    // Button 4 (KeySouth) = Down/Left movement
    
    static uint32_t okHoldStart = 0;
    
    // Ignore key events for first 500ms to prevent immediate exits from residual button state
    if (state->gameStartTime == 0) {
        state->gameStartTime = ticks();
        okHoldStart = 0; // Reset static variable when game restarts
        printf("[pong] Game start time set, ignoring keys for 500ms\n");
        return;
    }
    if (ticks() - state->gameStartTime < 500) {
        printf("[pong] Ignoring keys due to startup delay\n");
        return;
    }
    
    // Handle Ok button hold-to-exit, short press for pause
    if (event.props.keys.down & KeyOk) {
        if (okHoldStart == 0) {
            okHoldStart = ticks();
        }
    } else {
        if (okHoldStart > 0) {
            uint32_t holdDuration = ticks() - okHoldStart;
            if (holdDuration > 1000) { // 1 second hold
                panel_pop();
                return;
            } else {
                // Short press - pause/unpause
                if (!state->gameOver) {
                    state->paused = !state->paused;
                    // Show/hide paused label
                    if (state->paused) {
                        ffx_sceneNode_setPosition(state->pausedLabel, (FfxPoint){ .x = 85, .y = 120 });
                    } else {
                        ffx_sceneNode_setPosition(state->pausedLabel, (FfxPoint){ .x = -300, .y = 120 });
                    }
                }
            }
            okHoldStart = 0;
        }
    }
    
    if (state->gameOver) {
        if (event.props.keys.down & KeyCancel) {
            // Reset game with Cancel button
            state->playerScore = 0;
            state->aiScore = 0;
            state->playerPaddleY = GAME_HEIGHT / 2 - PADDLE_HEIGHT / 2;
            state->aiPaddleY = GAME_HEIGHT / 2 - PADDLE_HEIGHT / 2;
            state->gameOver = false;
            state->paused = false;
            resetBall(state);
            snprintf(state->scoreText, sizeof(state->scoreText), "Player %d - AI %d", state->playerScore, state->aiScore);
            ffx_sceneLabel_setText(state->scoreLabel, state->scoreText);
        }
        return;
    }
    
    // Store key state for 2-button control
    state->keys = event.props.keys.down;
}

static void render(EventPayload event, void *_state) {
    PongState *state = _state;
    
    uint32_t now = ticks();
    
    // Check for hold-to-exit during gameplay
    if (state->southHoldStart > 0 && (now - state->southHoldStart) > 1000) {
        panel_pop();
        return;
    }
    
    updateGame(state);
    updateVisuals(state);
}

static int init(FfxScene scene, FfxNode node, void* _state, void* arg) {
    PongState *state = _state;
    
    // Clear entire state first for fresh start
    memset(state, 0, sizeof(*state));
    state->scene = scene;
    
    // Create game area background - rotated 90° CCW, horizontal layout
    state->gameArea = ffx_scene_createBox(scene, ffx_size(GAME_WIDTH, GAME_HEIGHT));
    ffx_sceneBox_setColor(state->gameArea, COLOR_BLACK);
    ffx_sceneGroup_appendChild(node, state->gameArea);
    ffx_sceneNode_setPosition(state->gameArea, (FfxPoint){ .x = 20, .y = 60 }); // Centered horizontally
    
    // Create center line (vertical for horizontal play)
    state->centerLine = ffx_scene_createBox(scene, ffx_size(2, GAME_HEIGHT));
    ffx_sceneBox_setColor(state->centerLine, ffx_color_rgb(128, 128, 128));
    ffx_sceneGroup_appendChild(node, state->centerLine);
    ffx_sceneNode_setPosition(state->centerLine, (FfxPoint){ .x = 20 + GAME_WIDTH/2 - 1, .y = 60 });
    
    // Create score label - positioned on left side for visibility
    state->scoreLabel = ffx_scene_createLabel(scene, FfxFontMedium, "Player 0 - AI 0");
    ffx_sceneGroup_appendChild(node, state->scoreLabel);
    ffx_sceneNode_setPosition(state->scoreLabel, (FfxPoint){ .x = 10, .y = 30 });
    
    // Create paused label - centered on screen
    state->pausedLabel = ffx_scene_createLabel(scene, FfxFontLarge, "PAUSED");
    ffx_sceneGroup_appendChild(node, state->pausedLabel);
    ffx_sceneNode_setPosition(state->pausedLabel, (FfxPoint){ .x = -300, .y = 120 }); // Hidden initially
    
    // Create paddles (vertical for horizontal play)
    // Player paddle (right side)
    state->playerPaddle = ffx_scene_createBox(scene, ffx_size(PADDLE_WIDTH, PADDLE_HEIGHT));
    ffx_sceneBox_setColor(state->playerPaddle, ffx_color_rgb(255, 255, 255));
    ffx_sceneGroup_appendChild(node, state->playerPaddle);
    
    // AI paddle (left side)
    state->aiPaddle = ffx_scene_createBox(scene, ffx_size(PADDLE_WIDTH, PADDLE_HEIGHT));
    ffx_sceneBox_setColor(state->aiPaddle, ffx_color_rgb(255, 255, 255));
    ffx_sceneGroup_appendChild(node, state->aiPaddle);
    
    // Create ball
    state->ball = ffx_scene_createBox(scene, ffx_size(BALL_SIZE, BALL_SIZE));
    ffx_sceneBox_setColor(state->ball, ffx_color_rgb(255, 255, 255));
    ffx_sceneGroup_appendChild(node, state->ball);
    
    // Initialize game state values
    state->playerScore = 0;
    state->aiScore = 0;
    state->playerPaddleY = GAME_HEIGHT / 2 - PADDLE_HEIGHT / 2;  // Center vertically
    state->aiPaddleY = GAME_HEIGHT / 2 - PADDLE_HEIGHT / 2;      // Center vertically  
    state->gameOver = false;
    state->paused = false;
    state->keys = 0;
    state->southHoldStart = 0;
    state->gameStartTime = 0;
    
    resetBall(state);
    snprintf(state->scoreText, sizeof(state->scoreText), "Player %d - AI %d", state->playerScore, state->aiScore);
    ffx_sceneLabel_setText(state->scoreLabel, state->scoreText);
    
    // Register events (4 buttons: Cancel, Ok, North, South)
    panel_onEvent(EventNameKeysChanged | KeyCancel | KeyOk | KeyNorth | KeySouth, keyChanged, state);
    panel_onEvent(EventNameRenderScene, render, state);
    
    return 0;
}

void pushPanelPong(void* arg) {
    panel_push(init, sizeof(PongState), PanelStyleSlideLeft, arg);
}