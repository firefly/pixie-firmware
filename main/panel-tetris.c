#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "firefly-scene.h"
#include "firefly-color.h"
#include "panel.h"
#include "panel-tetris.h"
#include "utils.h"

#define GRID_SIZE 10
#define BOARD_WIDTH 20  // Rotated: was height, now width  
#define BOARD_HEIGHT 10 // Rotated: was width, now height
#define PIECE_SIZE 4

typedef enum PieceType {
    PIECE_I = 0,
    PIECE_O,
    PIECE_T,
    PIECE_S,
    PIECE_Z,
    PIECE_J,
    PIECE_L,
    PIECE_COUNT
} PieceType;

typedef struct TetrisState {
    FfxScene scene;
    FfxNode gameArea;
    FfxNode board[BOARD_HEIGHT][BOARD_WIDTH];
    FfxNode scoreLabel;
    FfxNode linesLabel;
    FfxNode pausedLabel;
    
    // Game board (0 = empty, 1-7 = filled)
    uint8_t grid[BOARD_HEIGHT][BOARD_WIDTH];
    
    // Current falling piece
    PieceType currentPiece;
    int pieceX, pieceY;
    int pieceRotation;
    
    // Game state
    int score;
    int lines;
    int level;
    bool gameOver;
    bool paused;
    uint32_t lastDrop;
    uint32_t dropSpeed;
    char scoreText[32];
    char linesText[32];
    
    Keys currentKeys;
    uint32_t southHoldStart;
    uint32_t gameStartTime;
} TetrisState;

// Tetris piece definitions (4x4 grids, 4 rotations each)
static const uint8_t pieces[PIECE_COUNT][4][4][4] = {
    // I piece
    {
        {{0,0,0,0}, {1,1,1,1}, {0,0,0,0}, {0,0,0,0}},
        {{0,0,1,0}, {0,0,1,0}, {0,0,1,0}, {0,0,1,0}},
        {{0,0,0,0}, {0,0,0,0}, {1,1,1,1}, {0,0,0,0}},
        {{0,1,0,0}, {0,1,0,0}, {0,1,0,0}, {0,1,0,0}}
    },
    // O piece
    {
        {{0,0,0,0}, {0,1,1,0}, {0,1,1,0}, {0,0,0,0}},
        {{0,0,0,0}, {0,1,1,0}, {0,1,1,0}, {0,0,0,0}},
        {{0,0,0,0}, {0,1,1,0}, {0,1,1,0}, {0,0,0,0}},
        {{0,0,0,0}, {0,1,1,0}, {0,1,1,0}, {0,0,0,0}}
    },
    // T piece
    {
        {{0,0,0,0}, {0,1,0,0}, {1,1,1,0}, {0,0,0,0}},
        {{0,0,0,0}, {0,1,0,0}, {0,1,1,0}, {0,1,0,0}},
        {{0,0,0,0}, {0,0,0,0}, {1,1,1,0}, {0,1,0,0}},
        {{0,0,0,0}, {0,1,0,0}, {1,1,0,0}, {0,1,0,0}}
    },
    // S piece
    {
        {{0,0,0,0}, {0,1,1,0}, {1,1,0,0}, {0,0,0,0}},
        {{0,0,0,0}, {0,1,0,0}, {0,1,1,0}, {0,0,1,0}},
        {{0,0,0,0}, {0,0,0,0}, {0,1,1,0}, {1,1,0,0}},
        {{0,0,0,0}, {1,0,0,0}, {1,1,0,0}, {0,1,0,0}}
    },
    // Z piece
    {
        {{0,0,0,0}, {1,1,0,0}, {0,1,1,0}, {0,0,0,0}},
        {{0,0,0,0}, {0,0,1,0}, {0,1,1,0}, {0,1,0,0}},
        {{0,0,0,0}, {0,0,0,0}, {1,1,0,0}, {0,1,1,0}},
        {{0,0,0,0}, {0,1,0,0}, {1,1,0,0}, {1,0,0,0}}
    },
    // J piece
    {
        {{0,0,0,0}, {1,0,0,0}, {1,1,1,0}, {0,0,0,0}},
        {{0,0,0,0}, {0,1,1,0}, {0,1,0,0}, {0,1,0,0}},
        {{0,0,0,0}, {0,0,0,0}, {1,1,1,0}, {0,0,1,0}},
        {{0,0,0,0}, {0,1,0,0}, {0,1,0,0}, {1,1,0,0}}
    },
    // L piece
    {
        {{0,0,0,0}, {0,0,1,0}, {1,1,1,0}, {0,0,0,0}},
        {{0,0,0,0}, {0,1,0,0}, {0,1,0,0}, {0,1,1,0}},
        {{0,0,0,0}, {0,0,0,0}, {1,1,1,0}, {1,0,0,0}},
        {{0,0,0,0}, {1,1,0,0}, {0,1,0,0}, {0,1,0,0}}
    }
};

static color_ffxt pieceColors[PIECE_COUNT] = {
    0x00ff0000, // I - red
    0x00ffff00, // O - yellow  
    0x00ff00ff, // T - magenta
    0x0000ff00, // S - green
    0x000000ff, // Z - blue
    0x00ffa500, // J - orange
    0x00800080  // L - purple
};

static bool checkCollision(TetrisState *state, int x, int y, int rotation) {
    for (int py = 0; py < 4; py++) {
        for (int px = 0; px < 4; px++) {
            if (pieces[state->currentPiece][rotation][py][px]) {
                int nx = x + px;
                int ny = y + py;
                
                // Check bounds (horizontal layout: pieces fall from right to left)
                if (nx < 0 || nx >= BOARD_WIDTH || ny < 0 || ny >= BOARD_HEIGHT) {
                    return true;
                }
                
                // Check collision with placed blocks
                if (nx >= 0 && nx < BOARD_WIDTH && ny >= 0 && ny < BOARD_HEIGHT && state->grid[ny][nx]) {
                    return true;
                }
            }
        }
    }
    return false;
}

static void placePiece(TetrisState *state) {
    for (int py = 0; py < 4; py++) {
        for (int px = 0; px < 4; px++) {
            if (pieces[state->currentPiece][state->pieceRotation][py][px]) {
                int nx = state->pieceX + px;
                int ny = state->pieceY + py;
                
                if (nx >= 0 && nx < BOARD_WIDTH && ny >= 0 && ny < BOARD_HEIGHT) {
                    state->grid[ny][nx] = state->currentPiece + 1;
                }
            }
        }
    }
}

static int clearLines(TetrisState *state) {
    int linesCleared = 0;
    
    for (int y = BOARD_HEIGHT - 1; y >= 0; y--) {
        bool fullLine = true;
        for (int x = 0; x < BOARD_WIDTH; x++) {
            if (!state->grid[y][x]) {
                fullLine = false;
                break;
            }
        }
        
        if (fullLine) {
            // Move everything down
            for (int moveY = y; moveY > 0; moveY--) {
                for (int x = 0; x < BOARD_WIDTH; x++) {
                    state->grid[moveY][x] = state->grid[moveY - 1][x];
                }
            }
            // Clear top line
            for (int x = 0; x < BOARD_WIDTH; x++) {
                state->grid[0][x] = 0;
            }
            
            linesCleared++;
            y++; // Check the same line again
        }
    }
    
    return linesCleared;
}

static void spawnPiece(TetrisState *state) {
    state->currentPiece = rand() % PIECE_COUNT;
    state->pieceX = 0;                    // Spawn from left side
    state->pieceY = BOARD_HEIGHT / 2 - 2; // Center vertically
    state->pieceRotation = 0;
    
    if (checkCollision(state, state->pieceX, state->pieceY, state->pieceRotation)) {
        state->gameOver = true;
    }
}

static void updateVisuals(TetrisState *state) {
    // Clear board visuals
    for (int y = 0; y < BOARD_HEIGHT; y++) {
        for (int x = 0; x < BOARD_WIDTH; x++) {
            if (state->grid[y][x] == 0) {
                ffx_sceneBox_setColor(state->board[y][x], COLOR_BLACK);
            } else {
                ffx_sceneBox_setColor(state->board[y][x], pieceColors[state->grid[y][x] - 1]);
            }
        }
    }
    
    // Draw current piece
    if (!state->gameOver) {
        for (int py = 0; py < 4; py++) {
            for (int px = 0; px < 4; px++) {
                if (pieces[state->currentPiece][state->pieceRotation][py][px]) {
                    int nx = state->pieceX + px;
                    int ny = state->pieceY + py;
                    
                    if (nx >= 0 && nx < BOARD_WIDTH && ny >= 0 && ny < BOARD_HEIGHT) {
                        ffx_sceneBox_setColor(state->board[ny][nx], pieceColors[state->currentPiece]);
                    }
                }
            }
        }
    }
}

static void keyChanged(EventPayload event, void *_state) {
    TetrisState *state = _state;
    printf("[tetris] keyChanged called! keys=0x%04x\n", event.props.keys.down);
    
    // Update current keys for continuous movement
    state->currentKeys = event.props.keys.down;
    
    // Standardized controls:
    // Button 1 (KeyCancel) = Primary action (rotate piece)
    // Button 2 (KeyOk) = Pause/Exit (hold 1s) 
    // Button 3 (KeyNorth) = Up/Right movement (90° counter-clockwise)
    // Button 4 (KeySouth) = Down/Left movement
    
    static uint32_t okHoldStart = 0;
    
    // Ignore key events for first 500ms to prevent immediate exits from residual button state
    if (state->gameStartTime == 0) {
        state->gameStartTime = ticks();
        okHoldStart = 0; // Reset static variable when game restarts
        printf("[tetris] Game start time set, ignoring keys for 500ms\n");
        return;
    }
    if (ticks() - state->gameStartTime < 500) {
        printf("[tetris] Ignoring keys due to startup delay\n");
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
            memset(state->grid, 0, sizeof(state->grid));
            state->score = 0;
            state->lines = 0;
            state->level = 1;
            state->gameOver = false;
            state->paused = false;
            state->dropSpeed = 1000;
            spawnPiece(state);
            snprintf(state->scoreText, sizeof(state->scoreText), "Score: %d", state->score);
            snprintf(state->linesText, sizeof(state->linesText), "Lines: %d", state->lines);
            ffx_sceneLabel_setText(state->scoreLabel, state->scoreText);
            ffx_sceneLabel_setText(state->linesLabel, state->linesText);
        }
        return;
    }
    
    if (state->paused) return;
    
    // Button 1 (Cancel) = Primary action (rotate piece)
    if (event.props.keys.down & KeyCancel) {
        int newRotation = (state->pieceRotation + 1) % 4;
        if (!checkCollision(state, state->pieceX, state->pieceY, newRotation)) {
            state->pieceRotation = newRotation;
        }
    }
    
    // Rotated controls: pieces fall left, up/down movement for positioning
    // Button 3 (North) = Move up
    if (event.props.keys.down & KeyNorth) {
        if (!checkCollision(state, state->pieceX, state->pieceY - 1, state->pieceRotation)) {
            state->pieceY--;
        }
    }
    
    // Button 4 (South) = Move down  
    if (event.props.keys.down & KeySouth) {
        if (!checkCollision(state, state->pieceX, state->pieceY + 1, state->pieceRotation)) {
            state->pieceY++;
        }
    }
}

static void render(EventPayload event, void *_state) {
    TetrisState *state = _state;
    
    uint32_t now = ticks();
    
    // Check for hold-to-exit during gameplay
    if (state->southHoldStart > 0 && (now - state->southHoldStart) > 1000) {
        panel_pop();
        return;
    }
    
    if (state->paused || state->gameOver) {
        updateVisuals(state);
        return;
    }
    
    if (now - state->lastDrop > state->dropSpeed) {
        if (!checkCollision(state, state->pieceX + 1, state->pieceY, state->pieceRotation)) {
            state->pieceX++;  // Move right in rotated layout (left to right fall)
        } else {
            // Piece has landed
            placePiece(state);
            
            int linesCleared = clearLines(state);
            if (linesCleared > 0) {
                state->lines += linesCleared;
                state->score += linesCleared * 100 * state->level;
                state->level = state->lines / 10 + 1;
                state->dropSpeed = 1000 - (state->level - 1) * 50;
                if (state->dropSpeed < 100) state->dropSpeed = 100;
                
                snprintf(state->scoreText, sizeof(state->scoreText), "Score: %d", state->score);
                snprintf(state->linesText, sizeof(state->linesText), "Lines: %d", state->lines);
                ffx_sceneLabel_setText(state->scoreLabel, state->scoreText);
                ffx_sceneLabel_setText(state->linesLabel, state->linesText);
            }
            
            spawnPiece(state);
        }
        state->lastDrop = now;
    }
    
    updateVisuals(state);
}

static int init(FfxScene scene, FfxNode node, void* _state, void* arg) {
    TetrisState *state = _state;
    
    // Clear entire state first for fresh start
    memset(state, 0, sizeof(*state));
    state->scene = scene;
    
    // Create game area background - rotated 90° CCW, horizontal layout
    // Pieces fall from right to left, player controls on right
    FfxNode gameArea = ffx_scene_createBox(scene, ffx_size(BOARD_WIDTH * GRID_SIZE, BOARD_HEIGHT * GRID_SIZE));
    ffx_sceneBox_setColor(gameArea, COLOR_BLACK);
    ffx_sceneGroup_appendChild(node, gameArea);
    ffx_sceneNode_setPosition(gameArea, (FfxPoint){ .x = 20, .y = 110 }); // Horizontal layout, bottom of screen
    
    // Create score labels - positioned on left side for visibility
    state->scoreLabel = ffx_scene_createLabel(scene, FfxFontSmall, "Score: 0");
    ffx_sceneGroup_appendChild(node, state->scoreLabel);
    ffx_sceneNode_setPosition(state->scoreLabel, (FfxPoint){ .x = 10, .y = 20 });
    
    state->linesLabel = ffx_scene_createLabel(scene, FfxFontSmall, "Lines: 0");
    ffx_sceneGroup_appendChild(node, state->linesLabel);
    ffx_sceneNode_setPosition(state->linesLabel, (FfxPoint){ .x = 10, .y = 40 });
    
    // Create paused label - centered on screen
    state->pausedLabel = ffx_scene_createLabel(scene, FfxFontLarge, "PAUSED");
    ffx_sceneGroup_appendChild(node, state->pausedLabel);
    ffx_sceneNode_setPosition(state->pausedLabel, (FfxPoint){ .x = -300, .y = 120 }); // Hidden initially
    
    // Create board blocks - positioned to match rotated game area
    for (int y = 0; y < BOARD_HEIGHT; y++) {
        for (int x = 0; x < BOARD_WIDTH; x++) {
            state->board[y][x] = ffx_scene_createBox(scene, ffx_size(GRID_SIZE-1, GRID_SIZE-1));
            ffx_sceneBox_setColor(state->board[y][x], COLOR_BLACK);
            ffx_sceneGroup_appendChild(node, state->board[y][x]);
            ffx_sceneNode_setPosition(state->board[y][x], (FfxPoint){ 
                .x = 20 + x * GRID_SIZE, // Match game area x position (horizontal layout)
                .y = 110 + y * GRID_SIZE 
            });
        }
    }
    
    // Initialize game state values
    memset(state->grid, 0, sizeof(state->grid));
    state->score = 0;
    state->lines = 0;
    state->level = 1;
    state->gameOver = false;
    state->paused = false;
    state->dropSpeed = 1000;
    state->lastDrop = ticks();
    state->currentKeys = 0;
    state->southHoldStart = 0;
    state->gameStartTime = 0;
    
    spawnPiece(state);
    snprintf(state->scoreText, sizeof(state->scoreText), "Score: %d", state->score);
    snprintf(state->linesText, sizeof(state->linesText), "Lines: %d", state->lines);
    ffx_sceneLabel_setText(state->scoreLabel, state->scoreText);
    ffx_sceneLabel_setText(state->linesLabel, state->linesText);
    
    // Register events (4 buttons: Cancel, Ok, North, South)
    panel_onEvent(EventNameKeysChanged | KeyCancel | KeyOk | KeyNorth | KeySouth, keyChanged, state);
    panel_onEvent(EventNameRenderScene, render, state);
    
    return 0;
}

void pushPanelTetris(void* arg) {
    panel_push(init, sizeof(TetrisState), PanelStyleSlideLeft, arg);
}