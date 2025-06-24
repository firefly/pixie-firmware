#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdbool.h>
#include <stdio.h>

#include "firefly-scene.h"

#include "utils.h"

#include "images/image-alien-1.h"
#include "images/image-alien-2.h"
#include "images/image-alien-boom.h"
#include "images/image-bullet.h"
#include "images/image-ship.h"
#include "images/image-space.h"

#include "panel.h"

#include "panel-space.h"

#define ROWS        (4)
#define COLS        (4)
#define BULLETS     (5)

// @TODO: Refactor this a LOT; was put together in a couple hours. :p

typedef struct SpaceState {
    bool running;
    bool paused;

    uint32_t resetTimer;

    FfxScene scene;
    FfxNode panel;
    FfxNode ship;
    FfxNode aliens;
    FfxNode alien[ROWS * COLS];
    FfxNode bullet[BULLETS];
    FfxNode boom[BULLETS];
    FfxNode pausedLabel;
    uint8_t boomLife[BULLETS];
    uint8_t dead[ROWS * COLS];
    uint32_t tick;
    Keys keys;
} SpaceState;

static void explodeShip(SpaceState *space) {
    FfxPoint ship = ffx_sceneNode_getPosition(space->ship);

    for (int i = 0; i < BULLETS; i++) {
        if (space->boomLife[i]) { continue; }
        space->boomLife[i] = 12;
        ffx_sceneNode_setPosition(space->boom[i], ship);
    }

    ship.x = 300;
    ffx_sceneNode_setPosition(space->ship, ship);
}

static void explode(SpaceState *space, int index) {
    space->dead[index] = 1;

    FfxPoint alien = ffx_sceneNode_getPosition(space->alien[index]);

    for (int i = 0; i < BULLETS; i++) {
        if (space->boomLife[i]) { continue; }
        space->boomLife[i] = 12;

        FfxPoint aliens = ffx_sceneNode_getPosition(space->aliens);

        ffx_sceneNode_setPosition(space->boom[i], (FfxPoint){
            .x = aliens.x + alien.x,
            .y = aliens.y + alien.y
        });
    }

    alien.x = 300;
    ffx_sceneNode_setPosition(space->alien[index], alien);
}

static void focus(EventPayload event, void *_app) {
    SpaceState *space = _app;
    space->running = true;
}

static void render(EventPayload event, void *_app) {
    SpaceState *space = _app;

    FfxPoint ship = ffx_sceneNode_getPosition(space->ship);
    FfxPoint aliens = ffx_sceneNode_getPosition(space->aliens);

    // Finished animating the win
    if (ship.x < -50) { panel_pop(); }

    // Finished animating the loss
    if (aliens.x > 400) { panel_pop(); }

    // Either hasn't started yet or game over
    if (!space->running) { return; }

    // Reset button held down for more than 3s (legacy behavior)
    if (space->keys == KeyOk && ticks() - space->resetTimer > 3000) {
        panel_pop();
    }

    // Don't update game logic when paused
    if (space->paused) { return; }

    // Move left/right if keys are being held down
    if (space->keys & KeyNorth) {
        if (ship.y > 0) { ship.y -= 2; }
    } else if (space->keys & KeySouth) {
        if (ship.y < 240 - 38) { ship.y += 2; }
    }
    ffx_sceneNode_setPosition(space->ship, ship);

    space->tick++;

    for (int i = 0; i < BULLETS; i++) {
        // Animate bullets
        FfxPoint b = ffx_sceneNode_getPosition(space->bullet[i]);
        if (b.x > -10) { b.x -= 2; }
        ffx_sceneNode_setPosition(space->bullet[i], b);

        // Animate the exposions
        if (space->boomLife[i]) {
            space->boomLife[i]--;
            if (space->boomLife[i] == 0) {
                ffx_sceneNode_setPosition(space->boom[i], (FfxPoint){
                    .x = 300, .y = 0
                });
            }
        }
    }

    // Make the aliens dance; animate between alien1 and alien2 images
    if ((space->tick % 4) == 0) {
        int toggle = (space->tick / 4) % (ROWS * COLS);
        if (!space->dead[toggle]) {
            const uint16_t *current = ffx_sceneImage_getData(space->alien[toggle]);
            if (current == image_alien1) {
                ffx_sceneImage_setData(space->alien[toggle], image_alien2, sizeof(image_alien2));
            } else {
                ffx_sceneImage_setData(space->alien[toggle], image_alien1, sizeof(image_alien1));
            }
        }
    }


    // Check for bullets hitting any alien
    bool allKill = true;
    for (int i = 0; i < (ROWS * COLS); i++) {
        if (space->dead[i]) { continue; }
        allKill = false;

        FfxPoint alien = ffx_sceneNode_getPosition(space->alien[i]);

        FfxPoint w;
        w.x = aliens.x + alien.x + 10;
        w.y = aliens.y + alien.y + 13;

        for (int j = 0; j < BULLETS; j++) {
            FfxPoint b = ffx_sceneNode_getPosition(space->bullet[j]);
            if (abs(b.x - w.x) < 10 && abs(b.y - w.y) < 13) {
                explode(space, i);
                b.x = -10;
                ffx_sceneNode_setPosition(space->bullet[j], b);
                break;
            }
        }
    }

    if (allKill) {
        ffx_sceneNode_animatePosition(space->ship, ffx_point(-200, ship.y),
          0, 1000, FfxCurveEaseInQuad, NULL, NULL);
        space->running = false;
        return;
    }

    // Check for any alien hitting the player; compute aliens bounds
    bool isDead = false;
    int leftMost = 0, rightMost = 240;
    for (int i = 0; i < ROWS * COLS; i++) {
        if (space->dead[i]) { continue; }

        FfxPoint alien = ffx_sceneNode_getPosition(space->alien[i]);

        if (alien.y < rightMost) { rightMost = alien.y; }
        if (alien.y + 26 > leftMost) { leftMost = alien.y + 26; }

        // Check for alien-ship collision
        FfxPoint w;
        w.x = aliens.x + alien.x + 10;
        w.y = aliens.y + alien.y + 13;

        if (w.x + 7 > ship.x && abs(ship.y + 19 - w.y) < 20) {
            isDead = true;
            break;
        }
    }

    // Dead; stop the game and animate the aliens
    if (isDead) {
        explodeShip(space);

        ffx_sceneNode_animatePosition(space->aliens, ffx_point(480, aliens.y),
          0, 1000, FfxCurveEaseInBack, NULL, NULL);

        space->running = false;
        return;
    }

    // Move the alien field; zig-zag
    if ((aliens.x % 8) == 0) {
        if (leftMost + aliens.y < 240) {
            aliens.y += 2;
        } else {
            aliens.x += 4;
        }
    } else {
        if (rightMost + aliens.y > 0) {
            aliens.y -= 2;
        } else {
            aliens.x += 4;
        }
    }
    ffx_sceneNode_setPosition(space->aliens, aliens);
}

static void keyChanged(EventPayload event, void *_app) {
    SpaceState *space = _app;

    if (!space->running) { return; }

    uint32_t keys = event.props.keys.down;
    space->keys = keys;

    FfxPoint ship = ffx_sceneNode_getPosition(space->ship);

    //printf("[space] high-water: %d\n", uxTaskGetStackHighWaterMark(NULL));

    // Handle Ok button hold-to-exit, short press for pause
    static uint32_t okHoldStart = 0;
    
    if (keys & KeyOk) {
        if (okHoldStart == 0) {
            okHoldStart = ticks();
        }
        space->resetTimer = ticks(); // Keep existing reset timer logic
    } else {
        if (okHoldStart > 0) {
            uint32_t holdDuration = ticks() - okHoldStart;
            if (holdDuration > 1000) { // 1 second hold to exit
                panel_pop();
                return;
            } else {
                // Short press - pause/unpause
                space->paused = !space->paused;
                // Show/hide paused label
                if (space->paused) {
                    ffx_sceneNode_setPosition(space->pausedLabel, (FfxPoint){ .x = 85, .y = 120 });
                } else {
                    ffx_sceneNode_setPosition(space->pausedLabel, (FfxPoint){ .x = -300, .y = 120 });
                }
            }
            okHoldStart = 0;
        }
        space->resetTimer = 0;
    }

    // Don't process other controls when paused
    if (space->paused) return;

    if (keys & KeyCancel) {
        for (int i = 0; i < BULLETS; i++) {
            FfxPoint b = ffx_sceneNode_getPosition(space->bullet[i]);

            // Already in-flight
            if (b.x > -10) { continue; }

            b.y = ship.y + 16;
            b.x = 240 - 32 - 2;
            ffx_sceneNode_setPosition(space->bullet[i], b);
            break;
        }
    }
}

static int _init(FfxScene scene, FfxNode panel, void* panelState, void* arg) {
    SpaceState *space = panelState;
    space->scene = scene;
    space->panel = panel;
    space->paused = false;

    FfxNode bg = ffx_scene_createImage(scene, image_space, sizeof(image_space));
    ffx_sceneGroup_appendChild(panel, bg);

    for (int i = 0; i < BULLETS; i++) {
        FfxNode bullet = ffx_scene_createImage(scene, image_bullet, sizeof(image_bullet));
        space->bullet[i] = bullet;
        ffx_sceneGroup_appendChild(panel, bullet);
        ffx_sceneNode_setPosition(bullet, (FfxPoint){
            .x = -10, .y = 0
        });

        FfxNode boom = ffx_scene_createImage(scene, image_alienboom, sizeof(image_alienboom));
        space->boom[i] = boom;
        ffx_sceneGroup_appendChild(panel, boom);
        ffx_sceneNode_setPosition(boom, (FfxPoint){
            .x = 300, .y = 0
        });
    }

    FfxNode ship = ffx_scene_createImage(scene, image_ship, sizeof(image_ship));
    space->ship = ship;
    ffx_sceneGroup_appendChild(panel, ship);
    ffx_sceneNode_setPosition(ship, (FfxPoint){
        .x = (240 - 36), .y = (120 - 19)
    });

    FfxNode aliens = ffx_scene_createGroup(scene);
    space->aliens = aliens;
    ffx_sceneGroup_appendChild(panel, aliens);

    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            FfxNode alien = ffx_scene_createImage(scene, image_alien1, sizeof(image_alien1));
            space->alien[(r * COLS) + c] = alien;
            ffx_sceneGroup_appendChild(aliens, alien);
            ffx_sceneNode_setPosition(alien, (FfxPoint){
                .x = 30 * r, .y = c * 40
            });
        }
    }

    // Create paused label - centered on screen
    space->pausedLabel = ffx_scene_createLabel(scene, FfxFontLarge, "PAUSED");
    ffx_sceneGroup_appendChild(panel, space->pausedLabel);
    ffx_sceneNode_setPosition(space->pausedLabel, (FfxPoint){ .x = -300, .y = 120 }); // Hidden initially

    panel_onEvent(EventNameKeysChanged | KeyNorth | KeySouth | KeyOk | KeyCancel,
      keyChanged, space);

    panel_onEvent(EventNameRenderScene, render, space);

    panel_onEvent(EventNamePanelFocus, focus, space);

    return 0;
}

void pushPanelSpace(void* arg) {
    panel_push(_init, sizeof(SpaceState), PanelStyleSlideLeft, arg);
}
