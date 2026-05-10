// Le Space - Space Invaders, vertical / portrait layout.
// Ship at the bottom, aliens descend from the top, bullets fire up.
// Five progressive levels with increasing alien grid and decreasing
// bullet capacity.

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "firefly-scene.h"
#include "firefly-hollows.h"

#include "feedback.h"
#include "image-data.h"
#include "panels.h"
#include "utils.h"


#define MAX_COLS        (6)
#define MAX_ROWS        (5)
#define MAX_ALIENS      (MAX_COLS * MAX_ROWS)
#define MAX_BULLETS     (5)
#define MAX_LEVEL       (5)

// Sprite dimensions after the 90-deg CW rotation in images.c:
// alien is 26 wide x 20 tall, ship is 38 wide x 36 tall.
#define ALIEN_W         (26)
#define ALIEN_H         (20)
#define ALIEN_STEP_X    (32)
#define ALIEN_STEP_Y    (28)

#define SHIP_W          (38)
#define SHIP_H          (36)
#define SHIP_BOTTOM_Y   (240 - SHIP_H - 2)

#define HUD_H           (16)

#define SPLASH_MS       (1500)
#define END_HOLD_MS     (1500)


typedef struct LevelConfig {
    int cols;            // alien columns (horizontal)
    int rows;            // alien rows (vertical)
    int bulletCap;
    int alienAnimMod;
    int stepDivisor;
} LevelConfig;

static const LevelConfig levels[MAX_LEVEL] = {
    { 4, 3, 5, 6, 2 },   // L1
    { 4, 4, 5, 4, 1 },   // L2 (original)
    { 5, 4, 5, 4, 1 },   // L3
    { 5, 5, 4, 3, 1 },   // L4
    { 6, 5, 4, 2, 1 },   // L5 BOSS
};


typedef enum GameState {
    StateSplash,
    StatePlaying,
    StateLevelClear,
    StateGameOver,
    StateVictory,
} GameState;


typedef struct SpaceState {
    GameState state;
    uint32_t stateEnteredAt;

    int level;
    int cols, rows;
    int bulletCap;
    int alienAnimMod;
    int stepDivisor;

    uint32_t score;
    uint32_t resetTimer;
    uint32_t tick;

    FfxScene scene;
    FfxNode panel;
    FfxNode ship;
    FfxNode aliens;
    FfxNode alien[MAX_ALIENS];
    FfxNode bullet[MAX_BULLETS];
    FfxNode boom[MAX_BULLETS];

    FfxNode hud;
    FfxNode hudLabel;
    FfxNode bigLabel;
    FfxNode subLabel;

    uint8_t boomLife[MAX_BULLETS];
    uint8_t dead[MAX_ALIENS];

    FfxKeys keys;
} SpaceState;


static int alienIdx(SpaceState *s, int c, int r) {
    return r * s->cols + c;
}

static void enterState(SpaceState *s, GameState st) {
    s->state = st;
    s->stateEnteredAt = ticks();
}

static void updateHud(SpaceState *s) {
    ffx_sceneLabel_setTextFormat(s->hudLabel, "L%d  %lu",
      s->level, (unsigned long)s->score);
}

static void hideOverlay(SpaceState *s) {
    ffx_sceneNode_setHidden(s->bigLabel, true);
    ffx_sceneNode_setHidden(s->subLabel, true);
}

static void showOverlay(SpaceState *s, const char *big, const char *sub) {
    ffx_sceneLabel_setText(s->bigLabel, big);
    ffx_sceneLabel_setText(s->subLabel, sub);
    ffx_sceneNode_setHidden(s->bigLabel, false);
    ffx_sceneNode_setHidden(s->subLabel, false);
}

static void resetShipAndBullets(SpaceState *s) {
    ffx_sceneNode_stopAnimations(s->ship, false);
    ffx_sceneNode_setPosition(s->ship,
      ffx_point(120 - SHIP_W / 2, SHIP_BOTTOM_Y));

    for (int i = 0; i < MAX_BULLETS; i++) {
        ffx_sceneNode_setPosition(s->bullet[i], ffx_point(0, 280));
        ffx_sceneNode_setPosition(s->boom[i],   ffx_point(300, 0));
        s->boomLife[i] = 0;
    }
}

static void setupLevel(SpaceState *s, int level) {
    LevelConfig cfg = levels[level - 1];

    s->level = level;
    s->cols = cfg.cols;
    s->rows = cfg.rows;
    s->bulletCap = cfg.bulletCap;
    s->alienAnimMod = cfg.alienAnimMod;
    s->stepDivisor = cfg.stepDivisor;

    memset(s->dead, 0, sizeof(s->dead));

    for (int i = 0; i < MAX_ALIENS; i++) {
        ffx_sceneNode_setHidden(s->alien[i], true);
        ffx_sceneNode_setPosition(s->alien[i], ffx_point(-300, 0));
    }

    ffx_sceneNode_stopAnimations(s->aliens, false);
    int gridW = s->cols * ALIEN_STEP_X;
    int aliensX = (240 - gridW) / 2 + (ALIEN_STEP_X - ALIEN_W) / 2;
    ffx_sceneNode_setPosition(s->aliens, ffx_point(aliensX, HUD_H + 8));

    for (int r = 0; r < s->rows; r++) {
        for (int c = 0; c < s->cols; c++) {
            FfxNode alien = s->alien[alienIdx(s, c, r)];
            ffx_sceneNode_setHidden(alien, false);
            ffx_sceneImage_setData(alien, image_alien1_cw, image_alien1_cw_len);
            ffx_sceneNode_setPosition(alien,
              ffx_point(c * ALIEN_STEP_X, r * ALIEN_STEP_Y));
        }
    }

    resetShipAndBullets(s);
    updateHud(s);

    char splash[16];
    snprintf(splash, sizeof(splash), "LEVEL %d", level);
    showOverlay(s, splash, level == MAX_LEVEL ? "FINAL" : "READY");
    enterState(s, StateSplash);
}

static void startNewGame(SpaceState *s) {
    s->score = 0;
    setupLevel(s, 1);
}

static void explodeShip(SpaceState *s) {
    FfxPoint ship = ffx_sceneNode_getPosition(s->ship);
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (s->boomLife[i]) { continue; }
        s->boomLife[i] = 12;
        ffx_sceneNode_setPosition(s->boom[i], ship);
    }
    ship.y = 300;
    ffx_sceneNode_setPosition(s->ship, ship);
}

static void explodeAlien(SpaceState *s, int index) {
    s->dead[index] = 1;
    s->score += 10 * s->level;

    FfxPoint alien = ffx_sceneNode_getPosition(s->alien[index]);
    FfxPoint aliens = ffx_sceneNode_getPosition(s->aliens);

    for (int i = 0; i < MAX_BULLETS; i++) {
        if (s->boomLife[i]) { continue; }
        s->boomLife[i] = 12;
        ffx_sceneNode_setPosition(s->boom[i], (FfxPoint){
            .x = aliens.x + alien.x,
            .y = aliens.y + alien.y
        });
    }

    alien.y = 300;
    ffx_sceneNode_setPosition(s->alien[index], alien);
    updateHud(s);
}

static void onFocus(FfxEvent event, FfxEventProps props, void *_app) { }

static void renderPlaying(SpaceState *s) {
    FfxPoint ship = ffx_sceneNode_getPosition(s->ship);
    FfxPoint aliens = ffx_sceneNode_getPosition(s->aliens);

    if (s->keys == FfxKeyOk && ticks() - s->resetTimer > 3000) {
        ffx_popPanel(GameResultQuit);
        return;
    }

    // Ship moves horizontally.
    if (s->keys & FfxKeyNorth) {
        if (ship.x > 0) { ship.x -= 3; }
    } else if (s->keys & FfxKeySouth) {
        if (ship.x < 240 - SHIP_W) { ship.x += 3; }
    }
    ffx_sceneNode_setPosition(s->ship, ship);

    s->tick++;

    // Bullets travel UP.
    for (int i = 0; i < MAX_BULLETS; i++) {
        FfxPoint b = ffx_sceneNode_getPosition(s->bullet[i]);
        if (b.y < 280) { b.y -= 4; }
        ffx_sceneNode_setPosition(s->bullet[i], b);

        if (s->boomLife[i]) {
            s->boomLife[i]--;
            if (s->boomLife[i] == 0) {
                ffx_sceneNode_setPosition(s->boom[i], ffx_point(300, 0));
            }
        }
    }

    if ((s->tick % s->alienAnimMod) == 0) {
        int total = s->cols * s->rows;
        if (total > 0) {
            int toggle = (s->tick / s->alienAnimMod) % total;
            if (!s->dead[toggle]) {
                const uint16_t *current =
                  ffx_sceneImage_getData(s->alien[toggle]);
                if (current == image_alien1_cw) {
                    ffx_sceneImage_setData(s->alien[toggle], image_alien2_cw,
                      image_alien2_cw_len);
                } else {
                    ffx_sceneImage_setData(s->alien[toggle], image_alien1_cw,
                      image_alien1_cw_len);
                }
            }
        }
    }

    bool allKill = true;
    int total = s->cols * s->rows;
    for (int i = 0; i < total; i++) {
        if (s->dead[i]) { continue; }
        allKill = false;

        FfxPoint a = ffx_sceneNode_getPosition(s->alien[i]);
        // Alien centre in screen coords.
        FfxPoint w = {
            aliens.x + a.x + ALIEN_W / 2,
            aliens.y + a.y + ALIEN_H / 2
        };

        for (int j = 0; j < MAX_BULLETS; j++) {
            FfxPoint b = ffx_sceneNode_getPosition(s->bullet[j]);
            if (abs(b.x + 4 - w.x) < ALIEN_W / 2 &&
                abs(b.y + 4 - w.y) < ALIEN_H / 2) {
                explodeAlien(s, i);
                b.y = 280;
                ffx_sceneNode_setPosition(s->bullet[j], b);
                break;
            }
        }
    }

    if (allKill) {
        s->score += 100 * s->level;
        updateHud(s);
        ffx_sceneNode_animatePosition(s->ship, ffx_point(ship.x, 280),
          0, 800, FfxCurveEaseInQuad, NULL, NULL);
        showOverlay(s, "CLEAR!", "");
        enterState(s, StateLevelClear);
        return;
    }

    bool isDead = false;
    int topMost = 240, bottomMost = 0;
    for (int i = 0; i < total; i++) {
        if (s->dead[i]) { continue; }
        FfxPoint a = ffx_sceneNode_getPosition(s->alien[i]);
        if (a.x < topMost) { topMost = a.x; }
        if (a.x + ALIEN_W > bottomMost) { bottomMost = a.x + ALIEN_W; }

        FfxPoint w = {
            aliens.x + a.x + ALIEN_W / 2,
            aliens.y + a.y + ALIEN_H / 2
        };
        if (w.y + ALIEN_H / 2 > ship.y &&
            abs(ship.x + SHIP_W / 2 - w.x) < (SHIP_W + ALIEN_W) / 2 - 4) {
            isDead = true;
            break;
        }
    }

    if (isDead) {
        explodeShip(s);
        ffx_sceneNode_animatePosition(s->aliens,
          ffx_point(aliens.x, 480), 0, 1000, FfxCurveEaseInBack, NULL, NULL);
        showOverlay(s, "GAME OVER", "OK = AGAIN");
        enterState(s, StateGameOver);
        return;
    }

    if ((s->tick % s->stepDivisor) == 0) {
        if ((aliens.y % 8) == 0) {
            if (topMost + aliens.x > 0) {
                aliens.x -= 2;
            } else {
                aliens.y += 4;
            }
        } else {
            if (bottomMost + aliens.x < 240) {
                aliens.x += 2;
            } else {
                aliens.y += 4;
            }
        }
        ffx_sceneNode_setPosition(s->aliens, aliens);
    }
}

static void onRender(FfxEvent event, FfxEventProps props, void *_app) {
    SpaceState *s = _app;
    uint32_t t = ticks();

    switch (s->state) {
        case StateSplash:
            if (t - s->stateEnteredAt > SPLASH_MS) {
                hideOverlay(s);
                enterState(s, StatePlaying);
            }
            break;

        case StatePlaying:
            renderPlaying(s);
            break;

        case StateLevelClear:
            if (t - s->stateEnteredAt > END_HOLD_MS) {
                int next = s->level + 1;
                if (next > MAX_LEVEL) {
                    showOverlay(s, "VICTORY!", "OK = AGAIN");
                    enterState(s, StateVictory);
                } else {
                    setupLevel(s, next);
                }
            }
            break;

        case StateGameOver:
        case StateVictory:
            break;
    }
}

static void onKeys(FfxEvent event, FfxEventProps props, void *_app) {
    SpaceState *s = _app;
    uint32_t keys = props.keys.down;
    s->keys = keys;
    feedback_onKey(keys);

    if (s->state == StateGameOver || s->state == StateVictory) {
        if (keys & FfxKeyOk) { startNewGame(s); return; }
        if (keys & FfxKeyCancel) {
            ffx_popPanel(s->state == StateVictory
              ? GameResultWin : GameResultLose);
            return;
        }
        return;
    }

    if (s->state != StatePlaying) { return; }

    s->resetTimer = (keys == FfxKeyOk) ? ticks() : 0;

    if (keys & FfxKeyCancel) {
        FfxPoint ship = ffx_sceneNode_getPosition(s->ship);
        for (int i = 0; i < s->bulletCap; i++) {
            FfxPoint b = ffx_sceneNode_getPosition(s->bullet[i]);
            if (b.y < 280) { continue; }
            b.x = ship.x + SHIP_W / 2 - 4;
            b.y = ship.y - 8;
            ffx_sceneNode_setPosition(s->bullet[i], b);
            break;
        }
    }
}

static int initFunc(FfxScene scene, FfxNode panel, void *panelState,
  void *arg) {
    SpaceState *s = panelState;
    s->scene = scene;
    s->panel = panel;

    FfxNode bg = ffx_scene_createImage(scene, image_space, image_space_len);
    ffx_sceneGroup_appendChild(panel, bg);

    for (int i = 0; i < MAX_BULLETS; i++) {
        FfxNode bullet = ffx_scene_createImage(scene, image_bullet,
          image_bullet_len);
        s->bullet[i] = bullet;
        ffx_sceneGroup_appendChild(panel, bullet);
        ffx_sceneNode_setPosition(bullet, ffx_point(0, 280));

        FfxNode boom = ffx_scene_createImage(scene, image_alienboom_cw,
          image_alienboom_cw_len);
        s->boom[i] = boom;
        ffx_sceneGroup_appendChild(panel, boom);
        ffx_sceneNode_setPosition(boom, ffx_point(300, 0));
    }

    FfxNode ship = ffx_scene_createImage(scene, image_ship_cw, image_ship_cw_len);
    s->ship = ship;
    ffx_sceneGroup_appendChild(panel, ship);

    FfxNode aliens = ffx_scene_createGroup(scene);
    s->aliens = aliens;
    ffx_sceneGroup_appendChild(panel, aliens);

    for (int i = 0; i < MAX_ALIENS; i++) {
        FfxNode alien = ffx_scene_createImage(scene, image_alien1_cw,
          image_alien1_cw_len);
        s->alien[i] = alien;
        ffx_sceneGroup_appendChild(aliens, alien);
        ffx_sceneNode_setHidden(alien, true);
        ffx_sceneNode_setPosition(alien, ffx_point(-300, 0));
    }

    s->hud = ffx_scene_createBox(scene, ffx_size(240, HUD_H));
    ffx_sceneBox_setColor(s->hud, RGBA_DARKER75);
    ffx_sceneGroup_appendChild(panel, s->hud);
    ffx_sceneNode_setPosition(s->hud, ffx_point(0, 0));

    s->hudLabel = ffx_scene_createLabel(scene, FfxFontMedium, "L1  0");
    ffx_sceneGroup_appendChild(panel, s->hudLabel);
    ffx_sceneNode_setPosition(s->hudLabel, ffx_point(120, 8));
    ffx_sceneLabel_setAlign(s->hudLabel,
      FfxTextAlignCenter | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(s->hudLabel, COLOR_BLACK);

    s->bigLabel = ffx_scene_createLabel(scene, FfxFontLargeBold, "");
    ffx_sceneGroup_appendChild(panel, s->bigLabel);
    ffx_sceneNode_setPosition(s->bigLabel, ffx_point(120, 110));
    ffx_sceneLabel_setAlign(s->bigLabel,
      FfxTextAlignCenter | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(s->bigLabel, COLOR_BLACK);
    ffx_sceneNode_setHidden(s->bigLabel, true);

    s->subLabel = ffx_scene_createLabel(scene, FfxFontMedium, "");
    ffx_sceneGroup_appendChild(panel, s->subLabel);
    ffx_sceneNode_setPosition(s->subLabel, ffx_point(120, 140));
    ffx_sceneLabel_setAlign(s->subLabel,
      FfxTextAlignCenter | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(s->subLabel, COLOR_BLACK);
    ffx_sceneNode_setHidden(s->subLabel, true);

    feedback_addButtonLegend(panel, "UP", "DOWN", "HOLD", "FIRE");

    startNewGame(s);

    ffx_onEvent(FfxEventKeys, onKeys, s);
    ffx_onEvent(FfxEventRenderScene, onRender, s);
    ffx_onEvent(FfxEventFocus, onFocus, s);

    return 0;
}

GameResult pushPanelSpace() {
    return ffx_pushPanel(initFunc, sizeof(SpaceState), NULL);
}
