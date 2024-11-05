#include <stdbool.h>
#include <stdio.h>

#include "firefly-scene.h"

#include "images/image-alien-1.h"
#include "images/image-alien-2.h"
#include "images/image-alien-boom.h"
#include "images/image-ship.h"
#include "images/image-space.h"

#include "panel.h"

#include "panel-space.h"

#define ROWS        (4)
#define COLS        (4)
#define BULLETS     (5)


typedef struct SpaceState {
    FfxScene scene;
    FfxNode panel;
    FfxNode ship;
    FfxNode aliens;
    FfxNode alien[ROWS * COLS];
    FfxNode bullet[BULLETS];
    FfxNode booms[BULLETS];
    uint32_t tick;
    uint8_t dead[ROWS * COLS];
    uint16_t keys;
} SpaceState;

static void explode(SpaceState *space, size_t index) {
}

static void render(EventPayload event, void *_app) {
    SpaceState *space = _app;

    if (space->keys & KeyNorth) {
        FfxPoint *point = ffx_scene_nodePosition(space->ship);
        if (point->y > 16) { point->y -= 2; }
    } else if (space->keys & KeySouth) {
        FfxPoint *point = ffx_scene_nodePosition(space->ship);
        if (point->y < 240) { point->y += 2; }
    }

    space->tick++;

    // Animate bullets
    for (int i = 0; i < BULLETS; i++) {
        FfxPoint *b = ffx_scene_nodePosition(space->bullet[i]);
        if (b->x <= -10) { continue; }
        b->x--;
    }

    // Make the aliens dance
    if ((space->tick % 4) == 0) {
        int toggle = (space->tick / 4) % (ROWS * COLS);
        if (!space->dead[toggle]) {
            uint16_t *current = ffx_scene_imageData(space->alien[toggle]);
            if (current == image_alien1) {
                ffx_scene_imageSetData(space->alien[toggle], image_alien2, sizeof(image_alien2));
            } else {
                ffx_scene_imageSetData(space->alien[toggle], image_alien1, sizeof(image_alien1));
            }
        }
    }

    FfxPoint *ship = ffx_scene_nodePosition(space->ship);
    FfxPoint *aliens = ffx_scene_nodePosition(space->aliens);

    bool allKill = true;
    for (int i = 0; i < (ROWS * COLS); i++) {
        if (space->dead[i]) { continue; }
        allKill = false;

        FfxPoint *alien = ffx_scene_nodePosition(space->alien[i]);

        FfxPoint w;
        w.x = aliens->x + alien->x + 8;
        w.y = aliens->y + alien->y + 11;

        for (int j = 0; j < BULLETS; j++) {
            FfxPoint *b = ffx_scene_nodePosition(space->bullet[j]);
            if (abs(b->x - w.x) < 8 && abs(b->y - w.y) < 11) {
                space->dead[i] = 1;
                ffx_scene_imageSetData(space->alien[i], image_alienboom, sizeof(image_alienboom));
                b->x = -10;
                break;
            }
        }
    }

    // Check for player death and compute bounds
    bool isDead = false;
    int leftMost = 0, rightMost = 240;
    for (int i = 0; i < ROWS * COLS; i++) {
        if (space->dead[i]) { continue; }

        FfxPoint *alien = ffx_scene_nodePosition(space->alien[i]);

        if (alien->y < rightMost) { rightMost = alien->y; }
        if (alien->y + 22 > leftMost) { leftMost = alien->y + 22; }

        // Check for alien-ship collision
        FfxPoint w;
        w.x = aliens->x + alien->x + 8;
        w.y = aliens->y + alien->y + 11;

        if (w.x + 7 > ship->x && abs(ship->y - 14 - w.y) < 20) {
            isDead = true;
            break;
        }
    }

    // Move the alien field; zig-zag
    if ((aliens->x % 8) == 0) {
        if (leftMost + aliens->y < 240) {
            aliens->y += 2;
        } else {
            aliens->x += 4;
        }
    } else {
        if (rightMost + aliens->y > 0) {
            aliens->y -= 2;
        } else {
            aliens->x += 4;
        }
    }
}

static void keyChanged(EventPayload event, void *_app) {
    SpaceState *space = _app;

    uint32_t keys = event.props.keyEvent.down;
    space->keys = keys;

    FfxPoint *ship = ffx_scene_nodePosition(space->ship);

    if (keys & KeyCancel) {
        printf("Fire\n");
        for (int i = 0; i < BULLETS; i++) {
            FfxPoint *b = ffx_scene_nodePosition(space->bullet[i]);

            // Already in-flight
            if (b->x > -10) { continue; }

            b->y = ship->y + 16 - 2;
            b->x = 240 - 32 - 2;
            break;
        }
    }
}

static int _init(FfxScene scene, FfxNode panel, void* panelState, void* arg) {
    SpaceState *space = panelState;
    space->scene = scene;
    space->panel = panel;

    FfxNode bg = ffx_scene_createImage(scene, image_space, sizeof(image_space));
    ffx_scene_appendChild(panel, bg);

    FfxNode ship = ffx_scene_createImage(scene, image_ship, sizeof(image_ship));
    space->ship = ship;
    ffx_scene_appendChild(panel, ship);
    ffx_scene_nodeSetPosition(ship, (FfxPoint){
        .x = (240 - 32), .y = (120 - 16)
    });

    FfxNode aliens = ffx_scene_createGroup(scene);
    space->aliens = aliens;
    ffx_scene_appendChild(panel, aliens);

    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            FfxNode alien = ffx_scene_createImage(scene, image_alien1, sizeof(image_alien1));
            space->alien[(r * COLS) + c] = alien;
            ffx_scene_appendChild(aliens, alien);
            ffx_scene_nodeSetPosition(alien, (FfxPoint){
                .x = 30 * r, .y = c * 40
            });
        }
    }

    for (int i = 0; i < BULLETS; i++) {
        FfxNode bullet = ffx_scene_createBox(scene, (FfxSize){
            .width = 4, .height = 4
        }, ffx_color_rgb(255, 255, 255, 32));
        space->bullet[i] = bullet;
        ffx_scene_appendChild(panel, bullet);
            ffx_scene_nodeSetPosition(bullet, (FfxPoint){
                .x = -10, .y = 100
            });
    }

    panel_onEvent(EventNameKeysChanged | KeyNorth | KeySouth | KeyCancel,
      keyChanged, space);
    panel_onEvent(EventNameRenderScene, render, space);

    return 0;
}

void pushPanelSpace(void* arg) {
    panel_push(_init, sizeof(SpaceState), arg);
}
