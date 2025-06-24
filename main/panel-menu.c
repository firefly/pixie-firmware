#include <stdio.h>

#include "firefly-scene.h"

#include "panel.h"
#include "./panel-attest.h"
#include "./panel-gifs.h"
#include "./panel-menu.h"
#include "./panel-space.h"
#include "./panel-wallet.h"
#include "./panel-snake.h"
#include "./panel-tetris.h"
#include "./panel-pong.h"
#include "./panel-buttontest.h"

#include "images/image-arrow.h"

// Menu items array for easier management
static const char* menuItems[] = {
    "Device",
    "GIFs", 
    "Le Space",
    "Wallet",
    "Snake",
    "Tetris",
    "Pong",
    "Button Test",
    "---"
};

static const size_t MENU_ITEM_COUNT = 9;

typedef struct MenuState {
    size_t cursor;
    size_t numItems;
    FfxScene scene;
    FfxNode nodeCursor;
    FfxNode menuLabels[9];  // Support up to 9 menu items
} MenuState;

static void updateMenuDisplay(MenuState *app) {
    // Show 5 items total: 2 above cursor, cursor in center, 2 below cursor
    int centerY = 120; // Center of 240px screen
    int itemSpacing = 35;
    
    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        int relativePos = i - (int)app->cursor; // Position relative to cursor
        
        if (relativePos >= -2 && relativePos <= 2) {
            // Item is visible (within 2 positions of cursor)
            int yPos = centerY + (relativePos * itemSpacing);
            ffx_sceneNode_setPosition(app->menuLabels[i], (FfxPoint){ .x = 70, .y = yPos });
            
            // Highlight the selected item
            if (i == app->cursor) {
                ffx_sceneNode_setPosition(app->nodeCursor, (FfxPoint){ .x = 25, .y = yPos });
            }
        } else {
            // Hide items that are too far from cursor
            ffx_sceneNode_setPosition(app->menuLabels[i], (FfxPoint){ .x = -300, .y = 0 });
        }
    }
}

static void keyChanged(EventPayload event, void *_app) {
    MenuState *app = _app;

    if (event.props.keys.down & KeyOk) {
        // Don't allow selecting the separator
        if (app->cursor == 8) return;
        
        switch(app->cursor) {
            case 0:
                pushPanelAttest(NULL);
                break;
            case 1:
                pushPanelGifs(NULL);
                break;
            case 2:
                pushPanelSpace(NULL);
                break;
            case 3:
                pushPanelWallet(NULL);
                break;
            case 4:
                pushPanelSnake(NULL);
                break;
            case 5:
                pushPanelTetris(NULL);
                break;
            case 6:
                pushPanelPong(NULL);
                break;
            case 7:
                pushPanelButtonTest(NULL);
                break;
        }
        return;
    }
    else if (event.props.keys.down & KeyNorth) {
        // Circular navigation up
        if (app->cursor == 0) {
            app->cursor = MENU_ITEM_COUNT - 1; // Go to last item (---)
        } else {
            app->cursor--;
        }
        updateMenuDisplay(app);
    }
    else if (event.props.keys.down & KeySouth) {
        // Circular navigation down  
        if (app->cursor >= MENU_ITEM_COUNT - 1) {
            app->cursor = 0; // Go to first item
        } else {
            app->cursor++;
        }
        updateMenuDisplay(app);
    }
}

static int _init(FfxScene scene, FfxNode node, void *_app, void *arg) {
    MenuState *app = _app;
    app->scene = scene;
    app->cursor = 0;
    app->numItems = MENU_ITEM_COUNT;

    // Create background box
    FfxNode box = ffx_scene_createBox(scene, ffx_size(200, 220));
    ffx_sceneBox_setColor(box, RGBA_DARKER75);
    ffx_sceneGroup_appendChild(node, box);
    ffx_sceneNode_setPosition(box, (FfxPoint){ .x = 20, .y = 10 });

    // Create all menu labels dynamically
    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        app->menuLabels[i] = ffx_scene_createLabel(scene, FfxFontLarge, menuItems[i]);
        ffx_sceneGroup_appendChild(node, app->menuLabels[i]);
        // Initial position will be set by updateMenuDisplay
        ffx_sceneNode_setPosition(app->menuLabels[i], (FfxPoint){ .x = -300, .y = 0 });
    }

    // Create cursor arrow
    FfxNode cursor = ffx_scene_createImage(scene, image_arrow, sizeof(image_arrow));
    ffx_sceneGroup_appendChild(node, cursor);
    app->nodeCursor = cursor;

    // Set initial menu display
    updateMenuDisplay(app);

    panel_onEvent(EventNameKeysChanged | KeyNorth | KeySouth | KeyOk,
      keyChanged, app);

    return 0;
}

void pushPanelMenu(void *arg) {
    panel_push(_init, sizeof(MenuState), PanelStyleCoverUp, arg);
}
