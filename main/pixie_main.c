
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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

#include "images/img_moonbeam.h"

#define BOARD_REV         (3)


#if BOARD_REV == 2

// The rev.2 board used the CS0 pin for the display. The rev.3 board and beyond
// tie it to ground to save the pin (as the display is always selected) which
// allow re-arranging the pins a bit to aid in cleaner trace routing.

#define DISPLAY_BUS        (DisplaySpiBus2_cs)
#define PIN_DISPLAY_DC     (0)
#define PIN_DISPLAY_RESET  (5)

#define PIN_BUTTON_1       (1 << 2)
#define PIN_BUTTON_2       (1 << 3)
#define PIN_BUTTON_3       (1 << 4)
#define PIN_BUTTON_4       (1 << 1)

#elif BOARD_REV == 3

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

void bounce(FfxScene scene, FfxNode node, FfxSceneActionStop stopAction) {
//    FfxPoint *_point = ffx_scene_nodePosition(node);

    FfxPoint point;
    point.x = rand() % 220;
    point.y = rand() % 220;

    //ffx_scene_boxSetColor(node, rand() & 0xffff);

    ffx_scene_nodeAnimatePosition(scene, node, point, 500 + rand() % 3500, FfxCurveEaseOutElastic, bounce);
}

void renderScene(uint8_t *fragment, uint32_t y0, void *context) {
    FfxScene scene = context;
    ffx_scene_render(scene, fragment, y0, FfxDisplayFragmentHeight);
}

static void setNodePos(FfxNode node, int32_t x, int32_t y) {
    FfxPoint *point = ffx_scene_nodePosition(node);
    point->x = x;
    point->y = y;
}

void taskIoFunc(void* pvParameter) {
    uint32_t *ready = (uint32_t*)pvParameter;

    // All keys
    uint32_t keys = PIN_BUTTON_1 | PIN_BUTTON_2 | PIN_BUTTON_3 | PIN_BUTTON_4;

    // The subset of keys to trigger a system reset
    uint32_t resetKeys = PIN_BUTTON_1 | PIN_BUTTON_3;

    // Scene context
    FfxScene scene;
    scene = ffx_scene_init(2048);

    // I/O contexts
    FfxDisplayContext display;
    KeypadContext keypad;
    PixelsContext pixels;

    {
        uint32_t t0 = ticks();
        display = ffx_display_init(DISPLAY_BUS,
          PIN_DISPLAY_DC, PIN_DISPLAY_RESET, FfxDisplayRotationPinsLeft,
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

    {
        uint32_t t0 = ticks();
        pixels = pixels_init(PIN_PIXELS);
        printf("[io] init pixels: dt=%ldms\n", ticks() - t0);

        //pixels_setRGB(pixels, 0, RGB32(0, 0, 255));
        //uint32_t colorRamp[] = { RGB32(255, 0, 0), RGB32(0, 255, 0), RGB32(0, 0, 255) };
        const uint32_t delta = 40;
        color_ffxt colorRamp1[] = {
            ffx_color_hsv(0 + 0 * delta, 0x3c, 0x1c, 0x0c),
            ffx_color_hsv(360 + 0 * delta, 0x3c, 0x1c, 0x0c),
            ffx_color_hsv(0 + 0 * delta, 0x3c, 0x1c, 0x0c),
        };
        color_ffxt colorRamp2[] = {
            ffx_color_hsv(0 + 1 * delta, 0x3c, 0x1c, 0x0c),
            ffx_color_hsv(360 + 1 * delta, 0x3c, 0x1c, 0x0c),
            ffx_color_hsv(0 + 1 * delta, 0x3c, 0x1c, 0x0c),
        };
        color_ffxt colorRamp3[] = {
            ffx_color_hsv(0 + 2 * delta, 0x3c, 0x1c, 0x0c),
            ffx_color_hsv(360 + 2 * delta, 0x3c, 0x1c, 0x0c),
            ffx_color_hsv(0 + 2 * delta, 0x3c, 0x1c, 0x0c),
        };
        color_ffxt colorRamp4[] = {
            ffx_color_hsv(0 + 3 * delta, 0x3c, 0x1c, 0x0c),
            ffx_color_hsv(360 + 3 * delta, 0x3c, 0x1c, 0x0c),
            ffx_color_hsv(0 + 3 * delta, 0x3c, 0x1c, 0x0c),
        };
        pixels_animateColor(pixels, 0, colorRamp1, 2, 6000, 1);
        pixels_animateColor(pixels, 1, colorRamp2, 2, 6000, 1);
        pixels_animateColor(pixels, 2, colorRamp3, 2, 6000, 1);
        pixels_animateColor(pixels, 3, colorRamp4, 2, 6000, 1);
    }

    char fpsTextBuffer[12];
    memset(fpsTextBuffer, 0, sizeof(fpsTextBuffer));

    FfxNode fpsText, text;

    FfxNode cursor;
    int32_t cursorIndex = 0;

    {
        uint32_t t0 = ticks();

        FfxNode root = ffx_scene_root(scene);
        FfxNode fill = ffx_scene_createFill(scene, 0x21ea); //RGB(0x23, 0x3b, 0x52));
        ffx_scene_appendChild(root, fill);

        FfxNode img = ffx_scene_createImage(scene, moonbeam, sizeof(moonbeam));
        setNodePos(img, 150, 0);
        ffx_scene_appendChild(root, img);

        FfxNode img2 = ffx_scene_createImage(scene, (uint16_t*)screen, sizeof(screen) / 2);
        ffx_scene_appendChild(root, img2);

        const char* const phrase = "Hello y g World!";
        text = ffx_scene_createText(scene, phrase, strlen(phrase));
        ffx_scene_appendChild(root, text);
        setNodePos(text, 100, 120);

        for (uint32_t i = 0; i < 30; i++)
        {
            FfxNode box = ffx_scene_createBox(scene, (FfxSize){ .width = 5, .height = 5 }, RGB16(0, 0, 255));
            setNodePos(box, 10, 10);
            ffx_scene_appendChild(root, box);
            bounce(scene, box, FfxSceneActionStopFinal);
        }

        cursor = ffx_scene_createBox(scene, (FfxSize){ .width = 220, .height = 32 }, RGB16(40, 40, 128));
        setNodePos(cursor, 10, 10);
        ffx_scene_appendChild(root, cursor);

        fpsText = ffx_scene_createTextFlip(scene, fpsTextBuffer, sizeof(fpsTextBuffer));
        ffx_scene_appendChild(root, fpsText);
        ffx_scene_textSetTextInt(fpsText, 0);
        setNodePos(fpsText, 200, 230);

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
    TickType_t lastFrameTime = xTaskGetTickCount();

    while (1) {
        // Sample the keypad
        keypad_sample(keypad);

        // Render a screen fragment; if the last fragment is complete, the frame is complete
        uint32_t frameDone = ffx_display_renderFragment(display);

        if (frameDone) {
            pixels_tick(pixels);

            // Latch the keypad values
            keypad_latch(keypad);

            if (keypad_didChange(keypad, keys)) {
                printf("KEYS: CHANGE=%4lx SW1=%ld SW2=%ld SW3=%ld SW4=%ld\n", keypad_didChange(keypad, keys), keypad_isDown(keypad, PIN_BUTTON_1),
                keypad_isDown(keypad, PIN_BUTTON_2), keypad_isDown(keypad, PIN_BUTTON_3), keypad_isDown(keypad, PIN_BUTTON_4));
            }

            // Check for holding the reset sequence to start a timer
            if (keypad_didChange(keypad, keys)) {
                if (keypad_read(keypad) == resetKeys) {
                    resetStart = xTaskGetTickCount();
                } else {
                    resetStart = 0;
                }
            }

            FfxPoint *point = ffx_scene_nodePosition(text);

            if (keypad_isDown(keypad, PIN_BUTTON_4)) { point->x++; }
            if (keypad_isDown(keypad, PIN_BUTTON_3)) { point->x--; }
            if (keypad_isDown(keypad, PIN_BUTTON_2)) { point->y++; }
            if (keypad_isDown(keypad, PIN_BUTTON_1)) { point->y--; }

            if (keypad_read(keypad) == (PIN_BUTTON_1 | PIN_BUTTON_2)) {
                //scene_dump(scene);
            }

            // The reset sequence was held for 2s... reset!
            if (keypad_read(keypad) == resetKeys && resetStart && (xTaskGetTickCount() - resetStart) > 2000) {
                esp_restart();
                while(1) { }
            }

            if (keypad_didChange(keypad, PIN_BUTTON_3 | PIN_BUTTON_4)) {
                if (keypad_isDown(keypad, PIN_BUTTON_4)) {
                    cursorIndex++;
                } else if (keypad_isDown(keypad, PIN_BUTTON_3)) {
                    cursorIndex--;
                    if (cursorIndex < 0) { cursorIndex = 0; }
                }

                ffx_scene_stopAnimations(cursor, FfxSceneActionStopCurrent);
//              //  scene_nodeAnimatePosition(scene, cursor, (Point){ .x = 10, .y = 10 + (cursorIndex * 32) }, 150, CurveEaseOutQuad, NULL);
                ffx_scene_nodeAnimatePosition(scene, cursor, (FfxPoint){ .x = 10, .y = 10 + (cursorIndex * 32) }, 150, FfxCurveEaseOutQuad, NULL);
            }

            static uint32_t fpsTrigger = 0;
            if (fpsTrigger++ > 100) {
                fpsTrigger = 0;
                ffx_scene_textSetTextInt(fpsText, ffx_display_fps(display));
            }

//            if (lastFrameTime == 0) { lastFrameTime = xTaskGetTickCount(); }
            //int32_t skewLastFrameTime = lastFrameTime;
            ffx_scene_sequence(scene);

            BaseType_t didDelay = xTaskDelayUntil(&lastFrameTime, FRAMEDELAY);

            // We are falling behind, catch up by dropping frames
            if (didDelay == pdFALSE) {
                //uint32_t dt = xTaskGetTickCount() - skewLastFrameTime;
                //printf("[System.Display] Framerate Skew Detected; dt=%d dropped=%d\n", dt, (dt + FRAMEDELAY - 1) / FRAMEDELAY);
                //printf("[init] skew detected");
                lastFrameTime = xTaskGetTickCount();
            }
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
        // Pointer passed to taskReplFunc to notify us when REPL is ready
    device_init();

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
