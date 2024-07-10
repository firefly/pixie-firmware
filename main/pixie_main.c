
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_efuse.h"
#include "esp_random.h"
#include "esp_timer.h"

#include "firefly-scene.h"
#include "firefly-display.h"

#include "crypto/keccak256.h"
#include "crypto/ecc.h"

#include "system/ble.h"
//#include "system/color.h"
//#include "system/curves.h"
#include "system/device-info.h"
#include "system/keypad.h"
#include "system/pixels.h"
#include "system/utils.h"

#include "system/output.h"

#include "images/image-arrow.h"
#include "images/image-background.h"
#include "images/image-pixie.h"
#include "images/image-title.h"
#include "images/image-alien-1.h"
#include "images/image-alien-2.h"
#include "images/image-ship.h"
#include "images/image-text-dead.h"
#include "images/image-text-hold.h"
#include "images/image-text-win.h"

#define BOARD_REV         (5)

#define VERSION           (0x00000101)

#if BOARD_REV == 2

// The rev.2 board used the CS0 pin for the display. The rev.3
// board and beyond tie it to ground to save the pin (as the
// display is always enabled) which allow re-arranging the pins
// a bit to aid in cleaner trace routing.

#define DISPLAY_BUS        (DisplaySpiBus2_cs)
#define PIN_DISPLAY_DC     (0)
#define PIN_DISPLAY_RESET  (5)

#define PIN_BUTTON_1       (1 << 2)
#define PIN_BUTTON_2       (1 << 3)
#define PIN_BUTTON_3       (1 << 4)
#define PIN_BUTTON_4       (1 << 1)

#elif BOARD_REV == 4

// The rev.4 board added an addressable LED

#define DISPLAY_BUS        (FfxDisplaySpiBus2)
#define PIN_DISPLAY_DC     (4)
#define PIN_DISPLAY_RESET  (5)

#define PIN_BUTTON_1       (1 << 10)
#define PIN_BUTTON_2       (1 << 8)
#define PIN_BUTTON_3       (1 << 3)
#define PIN_BUTTON_4       (1 << 2)

#define PIN_PIXELS         (9)
#define PIXEL_COUNT        (1)

#elif BOARD_REV == 5

// The rev.5 board added an addressable LED next to each button

#define DISPLAY_BUS        (FfxDisplaySpiBus2)
#define PIN_DISPLAY_DC     (4)
#define PIN_DISPLAY_RESET  (5)


#define PIN_BUTTON_1       (1 << 10)
#define PIN_BUTTON_2       (1 << 8)
#define PIN_BUTTON_3       (1 << 3)
#define PIN_BUTTON_4       (1 << 2)

#define PIN_PIXELS         (9)
#define PIXEL_COUNT        (4)

#endif


#define FRAMERATE          (60)
#define FRAMEDELAY         (1000 / ((FRAMERATE) + 1))

// Idea: If current rate of last 60 frames if going to be early,
// use delay_hi
#define FRAME_DELAY_LO     (1000 / (FRAMERATE))
#define FRAME_DELAY_HI     ((FRAME_DELAY_LO) + 1)

static TaskHandle_t taskIoHandle = NULL;
static TaskHandle_t taskReplHandle = NULL;
static TaskHandle_t taskAppHandle = NULL;

static const fixed_ffxt FM_1_2    =     0x8000;
#define FM_1_3     (FM_1 / 3)
#define FM_2_3     (2 * FM_1 / 3)

int abs(int a) {
    if (a < 0) { a *= -1; }
    return a;
}

fixed_ffxt FadeInFadeOut(fixed_ffxt t) {
    fixed_ffxt t0 = t;
    if (t < FM_1_2) {
        t *= 2;
    } else {
        t = FM_1 - (t - FM_1_2) * 2;
    }
    //fixed_ffxt result = FfxCurveEaseInCubic(t);
    fixed_ffxt result = FfxCurveLinear(t);

    if (result > FM_1) {
        printf("BAR: t0=%08lx t=%08lx t'=%08lx\n", t0, t, FfxCurveEaseInCubic(t));
    }

    return result;
}

fixed_ffxt AnimateWaft2(fixed_ffxt t) {
    if (t < FM_1_3) {
        return mulfx(FfxCurveEaseOutQuad(t * 3), FM_1_2);
    } else if (t >= FM_2_3) {
        t -= FM_2_3;
        return FM_1_2 + mulfx(FfxCurveEaseInQuad(t * 3), FM_1_2);
    }
    return FM_1_2;
}

fixed_ffxt AnimateWaft(fixed_ffxt t) {
    if (t < FM_1_2) {
        return mulfx(FfxCurveEaseOutQuad(t * 2), FM_1_2);
    }
    t -= FM_1_2;
    return FM_1_2 + mulfx(FfxCurveEaseInQuad(t * 2), FM_1_2);
}

static void setNodePos(FfxNode node, int32_t x, int32_t y) {
    FfxPoint *point = ffx_scene_nodePosition(node);
    point->x = x;
    point->y = y;
}

void showReset(FfxScene scene, FfxNode mover, FfxSceneActionStop stopAction) {
    FfxNode texthold = ffx_scene_createImage(scene, image_texthold,
      sizeof(image_texthold));
    FfxNode root = ffx_scene_root(scene);
    ffx_scene_appendChild(root, texthold);
    setNodePos(texthold, 240, 0);

    ffx_scene_nodeAnimatePosition(scene, texthold,
      (FfxPoint){ .x = 240 - 50, .y = 0 }, 800, FfxCurveEaseOutElastic, NULL);
}

void bounce(FfxScene scene, FfxNode mover, FfxSceneActionStop stopAction) {
    ffx_scene_stopAnimations(mover, FfxSceneActionStopCurrent);

    FfxNode pixie = ffx_scene_groupFirstChild(mover);
    ffx_scene_stopAnimations(pixie, FfxSceneActionStopCurrent);

//    FfxPoint *_point = ffx_scene_nodePosition(node);
//    _point->x = rand() % 240;
//    _point->y = rand() % 240;

    color_ffxt *color = ffx_scene_imageColor(pixie);
    *color = ffx_color_rgb(0, 0, 0, 0);

    uint32_t duration = 4500 + esp_random() % 4500;

    FfxPoint target;
    target.x = (esp_random() % 300) - 30;
    target.y = (esp_random() % 300) - 30;

    ffx_scene_nodeAnimatePosition(scene, mover, target, duration, AnimateWaft, bounce);
    ffx_scene_imageAnimateAlpha(scene, pixie, 32, duration, FadeInFadeOut, NULL);

    if (stopAction == FfxSceneActionStopFinal) {
        uint32_t advance = duration / (esp_random() % 100);
        ffx_scene_advanceAnimations(mover, advance);
        ffx_scene_advanceAnimations(pixie, advance);
    }
}

void renderScene(uint8_t *fragment, uint32_t y0, void *context) {
    FfxScene scene = context;
    ffx_scene_render(scene, fragment, y0, FfxDisplayFragmentHeight);
}

void animateColorRamp(color_ffxt *colors, size_t count, fixed_ffxt t, void *arg) {
    colors[0] = ffx_color_lerpColorRamp(arg, 12, t);
}

void rollRainbow(color_ffxt *colors, size_t count, fixed_ffxt t, void *arg) {
    for (int i = 0; i < count; i++) {
        colors[i] = ffx_color_hsv(scalarfx(360, t), 0x3c, 0x05, 0x20);
    }
}

void rollGreen(color_ffxt *colors, size_t count, fixed_ffxt t, void *arg) {
    t = (sinfx(mulfx(FM_2PI, t)) + FM_1) >> 1;
    for (int i = 0; i < count; i++) {
        colors[i] = ffx_color_hsv(120, 0x3c, scalarfx(0x5, t), 0x20);
    }
}

void rollRed(color_ffxt *colors, size_t count, fixed_ffxt t, void *arg) {
    t = (sinfx(mulfx(FM_2PI, t)) + FM_1) >> 1;
    for (int i = 0; i < count; i++) {
        colors[i] = ffx_color_hsv(0, 0x3c, scalarfx(0x5, t), 0x20);
    }
}

void offColorRamp(color_ffxt *colors, size_t count, fixed_ffxt t, void *arg) {
    for (int i = 0; i < count; i++) {
        colors[i] = ffx_color_rgb(0, 0, 0, 0);
    }
}

static bool animating = false;

void clearAnimating(FfxScene scene, FfxNode mover, FfxSceneActionStop stopAction) {
    animating = false;
}

void taskIoFunc(void* pvParameter) {
    uint32_t *ready = (uint32_t*)pvParameter;

    // All keys
    uint32_t keys = PIN_BUTTON_1 | PIN_BUTTON_2 | PIN_BUTTON_3 | PIN_BUTTON_4;

    // The subset of keys to trigger a system reset
    uint32_t resetKeys = PIN_BUTTON_1 | PIN_BUTTON_3;

    // Scene context
    FfxScene scene;
    scene = ffx_scene_init(3000);

    // I/O contexts
    FfxDisplayContext display;
    KeypadContext keypad;
    PixelsContext pixels;

    {
        uint32_t t0 = ticks();
        display = ffx_display_init(DISPLAY_BUS,
          PIN_DISPLAY_DC, PIN_DISPLAY_RESET, FfxDisplayRotationRibbonRight,
          renderScene, scene);
        printf("[io] init display: dt=%ldms\n", ticks() - t0);
    }

    {
        uint32_t t0 = ticks();
        keypad = keypad_init(keys);
        printf("[io] init keypad: dt=%ldms\n", ticks() - t0);

        // DEBUG; allow halting device on tight crash loops
        keypad_sample(keypad);
        keypad_latch(keypad);
        if (keypad_isDown(keypad, PIN_BUTTON_3 | PIN_BUTTON_4)) {
            printf("[io] keypad halted execution (hold 1 + 3 to restart)");
            while(1) {
                keypad_sample(keypad);
                keypad_latch(keypad);
                if (keypad_isDown(keypad, resetKeys)) {
                    esp_restart();
                    while(1);
                }
                delay(16);
            }
        }
    }

    color_ffxt colorRamp1[] = {
        ffx_color_hsv(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsv(275, 0x3f, 0x08, 0x0c),
        ffx_color_hsv(275, 0x3f, 0x0a, 0x0c),
        ffx_color_hsv(275, 0x3f, 0x08, 0x0c),
        ffx_color_hsv(150, 0x3f, 0x00, 0x0c),
        ffx_color_hsv(275, 0x3f, 0x00, 0x0c),

        ffx_color_hsv(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsv(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsv(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsv(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsv(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsv(275, 0x3f, 0x00, 0x0c),
        ffx_color_rgb(0, 0, 0, 0),
        ffx_color_rgb(0, 0, 0, 0),
        ffx_color_rgb(0, 0, 0, 0),
    };

    color_ffxt colorRamp2[] = {
        ffx_color_hsv(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsv(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsv(275, 0x3f, 0x08, 0x0c),
        ffx_color_hsv(275, 0x3f, 0x0a, 0x0c),
        ffx_color_hsv(275, 0x3f, 0x0f, 0x0c),
        ffx_color_hsv(150, 0x3f, 0x00, 0x0c),

        ffx_color_hsv(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsv(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsv(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsv(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsv(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsv(275, 0x3f, 0x00, 0x0c),
        ffx_color_rgb(0, 0, 0, 0),
        ffx_color_rgb(0, 0, 0, 0),
        ffx_color_rgb(0, 0, 0, 0),
    };

    color_ffxt colorRamp3[] = {
        ffx_color_hsv(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsv(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsv(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsv(275, 0x3f, 0x08, 0x0c),
        ffx_color_hsv(275, 0x3f, 0x0a, 0x0c),
        ffx_color_hsv(275, 0x3f, 0x0f, 0x0c),

        ffx_color_hsv(150, 0x3f, 0x00, 0x0c),
        ffx_color_hsv(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsv(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsv(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsv(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsv(275, 0x3f, 0x00, 0x0c),

        ffx_color_rgb(0, 0, 0, 0),
        ffx_color_rgb(0, 0, 0, 0),
        ffx_color_rgb(0, 0, 0, 0),
    };

    color_ffxt colorRamp4[] = {
        ffx_color_hsv(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsv(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsv(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsv(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsv(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsv(275, 0x3f, 0x08, 0x0c),

        ffx_color_hsv(275, 0x3f, 0x3a, 0x0c),
        ffx_color_hsv(275, 0x3f, 0x3f, 0x0c),
        ffx_color_hsv(275, 0x3f, 0x3f, 0x0c),
        ffx_color_hsv(275, 0x0, 0x3f, 0x0c),
        ffx_color_hsv(275, 0x3f, 0x3f, 0x0c),
        ffx_color_hsv(275, 0x0, 0x00, 0x0c),

        ffx_color_rgb(0, 0, 0, 0),
        ffx_color_rgb(0, 0, 0, 0),
        ffx_color_rgb(0, 0, 0, 0),
    };

    color_ffxt colorRampGreen[] = {
        ffx_color_hsv(120, 0x3f, 0x0f, 0x0c),
        ffx_color_hsv(120, 0x3f, 0x00, 0x0c),
        ffx_color_hsv(120, 0x3f, 0x0f, 0x0c),
    };

    {
        uint32_t t0 = ticks();
        pixels = pixels_init(PIXEL_COUNT, PIN_PIXELS);
        printf("[io] init pixels: dt=%ldms\n", ticks() - t0);

        pixels_animatePixel(pixels, 0, animateColorRamp, 780, 0, colorRamp1);
        pixels_animatePixel(pixels, 1, animateColorRamp, 780, 0, colorRamp2);
        pixels_animatePixel(pixels, 2, animateColorRamp, 780, 0, colorRamp3);
        pixels_animatePixel(pixels, 3, animateColorRamp, 780, 0, colorRamp4);
    }

    char fpsTextBuffer[12];
    memset(fpsTextBuffer, 0, sizeof(fpsTextBuffer));

    FfxNode fpsText;

    FfxNode cursor;
    int32_t cursorIndex = 0;

    // Screen
    //  - 0: menu off-screen below
    //  - 1: main menu
    //  - 2: device info
    //  - 3: spaced invaders
    //  - 4: game over
    uint32_t screen = 0;

    const char* const phrasePixies = "Show BG";
    const char* const phraseDevice = "Device";
    const char* const phraseSpace  = "Le Space";

    char strModel[20];
    char strSerial[20];
    char strVersion[20];

    FfxNode bg, menu, pixies, invaders, ship, aliens;
    FfxNode aliens1[4 * 4];
    FfxNode aliens2[4 * 4];
    FfxNode bullets[3];

    bool alive[4 * 4];
    for (int i = 0; i < 16; i++) { alive[i] = true; }

    {
        uint32_t t0 = ticks();

        FfxNode root = ffx_scene_root(scene);

        FfxNode fill = ffx_scene_createFill(scene, 0x0000);
        ffx_scene_appendChild(root, fill);

        bg = ffx_scene_createImage(scene, image_background, sizeof(image_background));
        ffx_scene_appendChild(root, bg);

        invaders = ffx_scene_createGroup(scene);
        ffx_scene_appendChild(root, invaders);
        setNodePos(invaders, 0, -240);

        aliens = ffx_scene_createGroup(scene);
        ffx_scene_appendChild(invaders, aliens);

        for (int i = 0; i < 16; i++) {
            aliens1[i] = ffx_scene_createImage(scene, image_alien1, sizeof(image_alien1));
            ffx_scene_appendChild(aliens, aliens1[i]);
            setNodePos(aliens1[i], (i / 4) * 30, (i % 4) * 40);

            aliens2[i] = ffx_scene_createImage(scene, image_alien2, sizeof(image_alien2));
            ffx_scene_appendChild(aliens, aliens2[i]);
            setNodePos(aliens2[i], (i / 4) * 30, (i % 4) * 40 - 512);
        }

        ship = ffx_scene_createImage(scene, image_ship, sizeof(image_ship));
        ffx_scene_appendChild(invaders, ship);
        setNodePos(ship, 240 - 32, 120 - 16);

        for (int i = 0; i < 3; i++) {
            bullets[i] = ffx_scene_createBox(scene, (FfxSize){ .width = 4, .height = 4 }, ffx_color_rgb(255, 255, 255, 32));
            ffx_scene_appendChild(invaders, bullets[i]);
            setNodePos(bullets[i], -10, 100);
        }

        pixies = ffx_scene_createGroup(scene);
        ffx_scene_appendChild(root, pixies);

        menu = ffx_scene_createGroup(scene);
        ffx_scene_appendChild(root, menu);
        setNodePos(menu, 0, 300);

        for (uint32_t i = 0; i < 8; i++)
        {
            FfxNode mover = ffx_scene_createGroup(scene);
            ffx_scene_appendChild(pixies, mover);
            setNodePos(mover, (esp_random() % 300) - 30, (esp_random() % 300) - 30);

            FfxNode pixie = ffx_scene_createImage(scene, image_pixie, sizeof(image_pixie));
            ffx_scene_appendChild(mover, pixie);

            bounce(scene, mover, FfxSceneActionStopFinal);
        }

        fpsText = ffx_scene_createTextFlip(scene, fpsTextBuffer, sizeof(fpsTextBuffer));
        //ffx_scene_appendChild(root, fpsText);
        ffx_scene_textSetTextInt(fpsText, 0);
        setNodePos(fpsText, 200, 230);

        {
            FfxNode box = ffx_scene_createBox(scene, (FfxSize){ .width = 200, .height = 180 }, RGB_DARK75);
            ffx_scene_appendChild(menu, box);
            setNodePos(box, 20, 30);

            box = ffx_scene_createBox(scene, (FfxSize){ .width = 200, .height = 180 }, RGB_DARK75);
            ffx_scene_appendChild(menu, box);
            setNodePos(box, 240 + 20, 30);

            {
                FfxNode text = ffx_scene_createText(scene, phrasePixies, strlen(phrasePixies));
                ffx_scene_appendChild(menu, text);
                setNodePos(text, 70, 85);
            }

            {
                FfxNode text = ffx_scene_createText(scene, phraseDevice, strlen(phraseDevice));
                ffx_scene_appendChild(menu, text);
                setNodePos(text, 70, 125);
            }

            {
                FfxNode text = ffx_scene_createText(scene, phraseSpace, strlen(phraseSpace));
                ffx_scene_appendChild(menu, text);
                setNodePos(text, 70, 165);
            }

            cursor = ffx_scene_createImage(scene, image_arrow, sizeof(image_arrow));
            ffx_scene_appendChild(menu, cursor);
            setNodePos(cursor, 25, 58);

            {
                uint32_t model = esp_efuse_read_reg(EFUSE_BLK3, 1);
                snprintf(strModel, sizeof(strSerial), "Dev: 0x%03lx", model);
                FfxNode text = ffx_scene_createText(scene, strModel, strlen(strModel));
                ffx_scene_appendChild(menu, text);
                setNodePos(text, 240 + 40, 85);
            }

            {
                uint32_t serial = esp_efuse_read_reg(EFUSE_BLK3, 2);
                snprintf(strSerial, sizeof(strSerial), "S/N: %ld", serial);
                FfxNode text = ffx_scene_createText(scene, strSerial, strlen(strSerial));
                ffx_scene_appendChild(menu, text);
                setNodePos(text, 240 + 40, 125);
            }

            {
                snprintf(strVersion, sizeof(strVersion), "Ver: %d.%d.%d",
                  (VERSION >> 16) & 0xff, (VERSION >> 8) & 0xff,
                  VERSION & 0xff);
                FfxNode text = ffx_scene_createText(scene, strVersion, strlen(strVersion));
                ffx_scene_appendChild(menu, text);
                setNodePos(text, 240 + 40, 165);
            }
        }

        ffx_scene_sequence(scene);

        //scene_dump(scene);

        printf("[io] init scene: dt=%ldms\n", ticks() - t0);
    }

    // The IO is up; unblock the bootstrap process and start the app
    *ready = 1;

    // How long the reset sequence has been held down for
    uint32_t resetStart = 0;

    // The time of the last frame; used to enforce a constant framerate
    // The special value 0 causes an immediate update
    TickType_t lastFrameTime = ticks();

    while (1) {
        // Sample the keypad
        keypad_sample(keypad);

        // Render a screen fragment; if the last fragment is
        // complete, the frame is complete
        uint32_t frameDone = ffx_display_renderFragment(display);

        static uint32_t frameCount = 0;

        if (frameDone) {
            frameCount++;

            pixels_tick(pixels);

            // Latch the keypad values de-bouncing with the inter-frame samples
            keypad_latch(keypad);

            if (keypad_didChange(keypad, keys)) {
                printf("KEYS: CHANGE=%4lx SW1=%ld SW2=%ld SW3=%ld SW4=%ld\n", keypad_didChange(keypad, keys), keypad_isDown(keypad, PIN_BUTTON_1),
                keypad_isDown(keypad, PIN_BUTTON_2), keypad_isDown(keypad, PIN_BUTTON_3), keypad_isDown(keypad, PIN_BUTTON_4));
            }

            // Check for holding the reset sequence to start a timer
            if (keypad_didChange(keypad, keys)) {
                if (keypad_read(keypad) == resetKeys) {
                    resetStart = ticks();
                } else {
                    resetStart = 0;
                }
            }

            // The reset sequence was held for 2s... reset!
            if (keypad_read(keypad) == resetKeys && resetStart && (ticks() - resetStart) > 2000) {
                esp_restart();
                while(1) { }
            }

            if (!animating && screen == 1) {
                if (keypad_didChange(keypad, PIN_BUTTON_3 | PIN_BUTTON_4)) {
                    if (keypad_isDown(keypad, PIN_BUTTON_4)) {
                        cursorIndex++;
                        if (cursorIndex > 2) { cursorIndex = 2; }
                    } else if (keypad_isDown(keypad, PIN_BUTTON_3)) {
                        cursorIndex--;
                        if (cursorIndex < 0) { cursorIndex = 0; }
                    }

                    ffx_scene_stopAnimations(cursor, FfxSceneActionStopCurrent);
                    ffx_scene_nodeAnimatePosition(scene, cursor, (FfxPoint){
                      .x = 25, .y = 58 + (cursorIndex * 40) }, 150,
                      FfxCurveEaseOutQuad, NULL);
                }

                if (keypad_didChange(keypad, PIN_BUTTON_2)) {
                    if (cursorIndex == 0) {
                        animating = true;
                        screen = 0;
                        pixels_animatePixel(pixels, 0, rollRainbow, 1000, 1, NULL);
                        pixels_animatePixel(pixels, 1, rollRainbow, 2000, 1, NULL);
                        pixels_animatePixel(pixels, 2, rollRainbow, 3000, 1, NULL);
                        pixels_animatePixel(pixels, 3, rollRainbow, 4000, 1, NULL);
                        ffx_scene_nodeAnimatePosition(scene, menu,
                          (FfxPoint){ .x = 0, .y = 300 }, 400, FfxCurveEaseOutQuad, clearAnimating);

                    } else if (cursorIndex == 1) {
                        animating = true;
                        screen = 2;
                        pixels_animatePixel(pixels, 0, rollRed, 1000, 1, NULL);
                        pixels_animatePixel(pixels, 1, offColorRamp, 1000, 1, NULL);
                        ffx_scene_nodeAnimatePosition(scene, menu,
                          (FfxPoint){ .x = -240, .y = 0 }, 500, FfxCurveEaseOutQuad, clearAnimating);

                    } else if (cursorIndex == 2) {
                        animating = true;
                        screen = 3;
                        pixels_animatePixel(pixels, 0, offColorRamp, 1000, 1, NULL);
                        pixels_animatePixel(pixels, 1, offColorRamp, 1000, 1, NULL);
                        pixels_animatePixel(pixels, 2, offColorRamp, 1000, 1, NULL);
                        pixels_animatePixel(pixels, 3, offColorRamp, 1000, 1, NULL);
                        ffx_scene_nodeAnimatePosition(scene, bg,
                          (FfxPoint){ .x = 0, .y = 240 }, 1000, FfxCurveEaseOutQuad, clearAnimating);
                        ffx_scene_nodeAnimatePosition(scene, pixies,
                          (FfxPoint){ .x = 0, .y = 320 }, 1000, FfxCurveEaseOutQuad, clearAnimating);
                        ffx_scene_nodeAnimatePosition(scene, menu,
                          (FfxPoint){ .x = 0, .y = 240 }, 1000, FfxCurveEaseOutQuad, clearAnimating);
                        ffx_scene_nodeAnimatePosition(scene, invaders,
                          (FfxPoint){ .x = 0, .y = 0 }, 1000, FfxCurveEaseOutQuad, clearAnimating);
                    }
                }

            } else if (!animating && screen == 2) {
                if (keypad_read(keypad) == PIN_BUTTON_1) {
                    animating = true;
                    screen = 1;
                    pixels_animatePixel(pixels, 0, offColorRamp, 1000, 1, NULL);
                    pixels_animatePixel(pixels, 1, rollGreen, 1000, 1, NULL);
                    pixels_animatePixel(pixels, 2, offColorRamp, 1000, 1, NULL);
                    pixels_animatePixel(pixels, 3, offColorRamp, 1000, 1, NULL);
                    ffx_scene_nodeAnimatePosition(scene, menu,
                      (FfxPoint){ .x = 0, .y = 0 }, 300, FfxCurveEaseOutQuad, clearAnimating);
                }

            } else if (!animating && screen == 0) {
                if (keypad_read(keypad)) {
                    animating = true;
                    screen = 1;
                    pixels_animatePixel(pixels, 0, offColorRamp, 1000, 1, NULL);
                    pixels_animatePixel(pixels, 1, rollGreen, 1000, 1, NULL);
                    pixels_animatePixel(pixels, 2, offColorRamp, 1000, 1, NULL);
                    pixels_animatePixel(pixels, 3, offColorRamp, 1000, 1, NULL);
                    ffx_scene_nodeAnimatePosition(scene, menu,
                      (FfxPoint){ .x = 0, .y = 0 }, 500, FfxCurveEaseOutQuad, clearAnimating);
                }
            }

            if (frameCount == 60) {
                animating = true;
                screen = 1;
                pixels_animatePixel(pixels, 1, rollGreen, 1000, 1, NULL);
                ffx_scene_nodeAnimatePosition(scene, menu,
                  (FfxPoint){ .x = 0, .y = 0 }, 500, FfxCurveEaseOutQuad,
                  clearAnimating);
            }

            // Le Space
            if (screen == 3) {
                FfxPoint *s = ffx_scene_nodePosition(ship);

                // Move left/right
                if (keypad_isDown(keypad, PIN_BUTTON_4)) {
                    if (s->y < 240 - 16) { s->y += 2; }
                } else if (keypad_isDown(keypad, PIN_BUTTON_3)) {
                    if (s->y > 16) { s->y -= 2; }
                }

                // Fire!
                if (keypad_didChange(keypad, PIN_BUTTON_1) && keypad_isDown(keypad, PIN_BUTTON_1)) {
                    for (int i = 0; i < 3; i++) {
                        FfxPoint *b = ffx_scene_nodePosition(bullets[i]);

                        // Already in-flight
                        if (b->x > -10) { continue; }

                        b->y = s->y + 16 - 2;
                        b->x = 240 - 32 - 2;
                        break;
                    }
                }

                FfxPoint *f = ffx_scene_nodePosition(aliens);

                // Animate bullets
                for (int bi = 0; bi < 3; bi++) {
                    FfxPoint *b = ffx_scene_nodePosition(bullets[bi]);
                    if (b->x <= -10) { continue; }
                    b->x--;
                }

                // Toggle an alien animation ever 4 frames
                if ((frameCount % 4) == 0) {
                    int toggle = (frameCount / 4) % 16;
                    if (alive[toggle]) {
                        FfxPoint *a1 = ffx_scene_nodePosition(aliens1[toggle]);
                        FfxPoint *a2 = ffx_scene_nodePosition(aliens2[toggle]);
                        int y1 = a1->y, y2 = a2->y;
                        a1->y = y2;
                        a2->y = y1;
                    }
                }

                // Check for bullet-alien collisions
                bool allKill = true;
                for (int ai = 0; ai < 16; ai++) {
                    if (!alive[ai]) { continue; }
                    allKill = false;

                    FfxPoint *a = ffx_scene_nodePosition(aliens1[ai]);
                    if (a->y < 0) { a = ffx_scene_nodePosition(aliens2[ai]); }

                    FfxPoint w;
                    w.x = f->x + a->x + 8;
                    w.y = f->y + a->y + 11;

                    for (int bi = 0; bi < 3; bi++) {
                        FfxPoint *b = ffx_scene_nodePosition(bullets[bi]);
                        if (abs(b->x - w.x) < 8 && abs(b->y - w.y) < 11) {
                            alive[ai] = false;
                            a->x = -400;
                            b->x = -10;
                            break;
                        }
                    }
                }

                // Move the aliens
                bool isDead = false;
                int leftMost = 0, rightMost = 240;
                for (int ai = 0; ai < 16; ai++) {
                    if (!alive[ai]) { continue; }

                    FfxPoint *a = ffx_scene_nodePosition(aliens1[ai]);
                    if (a->y < 0) { a = ffx_scene_nodePosition(aliens2[ai]); }

                    if (a->y < rightMost) { rightMost = a->y; }
                    if (a->y + 22 > leftMost) { leftMost = a->y + 22; }

                    // Check for alien-ship collision
                    FfxPoint w;
                    w.x = f->x + a->x + 8;
                    w.y = f->y + a->y + 11;

                    if (w.x + 7 > s->x && abs(s->y - 14 - w.y) < 20) {
                        isDead = true;
                        break;
                    }
                }

                // Move the alien field; zig-zag
                if ((f->x % 8) == 0) {
                    if (leftMost + f->y < 240) {
                        f->y += 2;
                    } else {
                        f->x += 4;
                    }
                } else {
                    if (rightMost + f->y > 0) {
                        f->y -= 2;
                    } else {
                        f->x += 4;
                    }
                }

                if (isDead) {
                    // "You dead!"
                    FfxNode text = ffx_scene_createImage(scene, image_textdead, sizeof(image_textdead));
                    ffx_scene_appendChild(invaders, text);
                    setNodePos(text, 50, (240 - 118) / 2);
                    ffx_scene_imageAnimateAlpha(scene, ship,
                      0, 1200, FfxCurveLinear, showReset);
                    screen = 4;

                } else if (allKill) {
                    // "You win!"
                    FfxNode text = ffx_scene_createImage(scene, image_textwin, sizeof(image_textwin));
                    ffx_scene_appendChild(invaders, text);
                    setNodePos(text, 50, (240 - 118) / 2);
                    ffx_scene_nodeAnimatePosition(scene, ship,
                      (FfxPoint){ .x = -100, .y = s->y }, 1200, FfxCurveEaseInCubic, showReset);
                    screen = 4;
                }
            }


            static uint32_t fpsTrigger = 0;
            if (fpsTrigger++ > 100) {
                fpsTrigger = 0;
                ffx_scene_textSetTextInt(fpsText, ffx_display_fps(display));
            }

            ffx_scene_sequence(scene);

            BaseType_t didDelay = xTaskDelayUntil(&lastFrameTime, FRAMEDELAY);

            // We are falling behind, catch up by dropping frames
            if (didDelay == pdFALSE) { lastFrameTime = ticks(); }
        }

        fflush(stdout);
    }
}

void taskReplFunc(void* pvParameter) {

    // No need to block init while bringing the REPL service online
    uint32_t *ready = (uint32_t*)pvParameter;
    *ready = 1;

    ble_init();

    uint8_t buffer[256];
    memset(buffer, 0, sizeof(buffer));

    while (1) {
    /*
        int length = usb_serial_jtag_read_bytes(buffer, (BUF_SIZE - 1), 10);

        if (length > 0) {
            buffer[length] = 0;
            //usb_serial_jtag_write_bytes((const char *) data, len, 20 / portTICK_PERIOD_MS);
            printf("ECHO: (%d) %s\n", length, buffer);
            //continue;
        }
*/
        delay(1000);
    }
}

void taskAppFunc(void* pvParameter) {
    while (1) {
        printf("Hello from App\n");
        /*

        {
            int32_t t0 = ticks();

            uint8_t checksum[KECCAK256_DIGEST_SIZE];

            Keccak256Context ctx;

            for (uint32_t i = 0; i < 1000; i++) {
                keccak256_init(&ctx);
                keccak256_update(&ctx, checksum, KECCAK256_DIGEST_SIZE);
                keccak256_final(&ctx, checksum);
            }

            int32_t dt = ticks() - t0;

            printf("Checksum: 0x");
            for (int i = 0; i < KECCAK256_DIGEST_SIZE; i++) {
                printf("%02x", checksum[i]);
            }
            printf(" (took %lds for 1000 hash ops)\n", dt);
        }

        {
            uint8_t signature[ECC_SIGNATURE_SIZE];

            int32_t t0 = ticks();

            ecc_signSecp256k1(signature, signature, signature);

            int32_t dt = ticks() - t0;

            printf("Sig: 0x");
            for (int i = 0; i < ECC_SIGNATURE_SIZE; i++) {
                printf("%02x", signature[i]);
            }
            printf(" (took %lds for 1 sign op)\n", dt);
        }
        */

        delay(60000);
    }
}

void app_main() {
    printf("Hello world!\n");

    device_init();

    // Start the IO task (handles the display, LEDs and keypad)
    {
        // Pointer passed to taskIoFunc to notify us when IO is ready
        uint32_t ready = 0;

        BaseType_t status = xTaskCreatePinnedToCore(&taskIoFunc, "io", 8192, &ready, 3, &taskIoHandle, 0);
        printf("[main] start IO task: status=%d\n", status);
        assert(taskIoHandle != NULL);

        // Wait for the IO task to complete setup
        while (!ready) { delay(1); }
        printf("[main] IO ready\n");
    }

    // Start the Command task (handles Serial REPL)
    {
        device_init();

        // Pointer passed to taskReplFunc to notify us when REPL is ready
        uint32_t ready = 1; // @TODO: set this to 0 and set in the task

        BaseType_t status = xTaskCreatePinnedToCore(&transport_task, "repl", 4096, &ready, 2, &taskReplHandle, 0);
        printf("[main] start REPL task: status=%d\n", status);
        assert(taskReplHandle != NULL);

        // Wait for the REPL task to complete setup
        while (!ready) { delay(1); }
        printf("[main] REPL ready\n");
    }

    // Start the App Process
    {
        BaseType_t status = xTaskCreatePinnedToCore(&taskAppFunc, 
        "app", 8192 * 8, NULL, 1, &taskAppHandle, 0);
        printf("[main] init app task: status=%d\n", status);
        assert(taskAppHandle != NULL);
    }

    while (1) {
        printf("[main] high-water: boot=%d io=%d, app=%d freq=%ld\n",
            uxTaskGetStackHighWaterMark(NULL),
            uxTaskGetStackHighWaterMark(taskIoHandle),
            uxTaskGetStackHighWaterMark(taskAppHandle),
            portTICK_PERIOD_MS);
        delay(60000);
    }
}
