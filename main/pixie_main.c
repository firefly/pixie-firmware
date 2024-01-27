
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

#include "keccak256.h"
#include "secp256k1.h"

#include "system/color.h"
#include "system/curves.h"
#include "system/display.h"
#include "system/keypad.h"
#include "system/pixels.h"
#include "system/scene.h"
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

#define DISPLAY_BUS        (DisplaySpiBus2)
#define PIN_DISPLAY_DC     (4)
#define PIN_DISPLAY_RESET  (5)


#define PIN_BUTTON_1       (1 << 10)
#define PIN_BUTTON_2       (1 << 8)
#define PIN_BUTTON_3       (1 << 3)
#define PIN_BUTTON_4       (1 << 2)

#define PIN_PIXELS         (9)

#endif


#define FRAMERATE          (60)
#define FRAMEDELAY         (1000 / (FRAMERATE + 1))

// Idea: If current rate of last 60 frames if going to be early,
// use delay_hi
#define FRAME_DELAY_LO     (1000 / (FRAMERATE))
#define FRAME_DELAY_HI     ((FRAME_DELAY_LO) + 1)

static TaskHandle_t taskIoHandle = NULL;
static TaskHandle_t taskAppHandle = NULL;

void bounce(SceneContext scene, Node node, SceneActionStop stopAction) {
    Point point = scene_nodePosition(node);

    point.x = rand() % 220;
    point.y = rand() % 220;
    scene_boxSetColor(node, rand() & 0xffff);

    scene_nodeAnimatePosition(scene, node, point, 500 + rand() % 3500, CurveEaseOutElastic, bounce);
}

void taskIoFunc(void* pvParameter) {
    uint32_t *ready = (uint32_t*)pvParameter;

    // All keys
    uint32_t keys = PIN_BUTTON_1 | PIN_BUTTON_2 | PIN_BUTTON_3 | PIN_BUTTON_4;

    // The subset of keys to trigger a system reset
    uint32_t resetKeys = PIN_BUTTON_1 | PIN_BUTTON_3;

    // I/O contexts
    DisplayContext display;
    KeypadContext keypad;
    PixelsContext pixels;

    {
        uint32_t t0 = ticks();
        display = display_init(DISPLAY_BUS, PIN_DISPLAY_DC, PIN_DISPLAY_RESET, DisplayRotationPinsLeft);
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
        uint32_t colorRamp[] = { RGB24(85, 0, 0), RGB24(0, 85, 0), RGB24(0, 0, 85) };
        pixels_animateRGB(pixels, 0, colorRamp, 3, 1000, 1);
    }

    char fpsTextBuffer[12];
    memset(fpsTextBuffer, 0, sizeof(fpsTextBuffer));

    // Scene context
    SceneContext scene;
    Node fpsText, text;

    Node cursor;
    int32_t cursorIndex = 0;

    {
        uint32_t t0 = ticks();

        scene = scene_init(2048);
        Node root = scene_root(scene);
        Node fill = scene_createFill(scene, 0x21ea); //RGB(0x23, 0x3b, 0x52));
        scene_appendChild(root, fill);

        Node img = scene_createImage(scene, moonbeam, sizeof(moonbeam));
        scene_nodeSetPosition(img, (Point){ .x = 150, .y = 0 });
        scene_appendChild(root, img);

        Node img2 = scene_createImage(scene, (uint16_t*)screen, sizeof(screen) / 2);
        scene_appendChild(root, img2);

        const char* const phrase = "Hello y g World!";
        text = scene_createText(scene, phrase, strlen(phrase));
        scene_appendChild(root, text);
        scene_nodeSetPosition(text, (Point){ .x = 100, .y = 120 });

        for (uint32_t i = 0; i < 30; i++)
        {
            Node box = scene_createBox(scene, (Size){ .width = 5, .height = 5 }, RGB16(0, 0, 255));
            scene_nodeSetPosition(box, (Point){ .x = 10, .y = 10 });
            scene_appendChild(root, box);
            bounce(scene, box, SceneActionStopFinal);
        }

        cursor = scene_createBox(scene, (Size){ .width = 220, .height = 32 }, RGB16(40, 40, 128));
        scene_nodeSetPosition(cursor, (Point){ .x = 10, .y = 10 });
        scene_appendChild(root, cursor);

        fpsText = scene_createTextFlip(scene, fpsTextBuffer, sizeof(fpsTextBuffer));
        scene_appendChild(root, fpsText);
        scene_textSetTextInt(fpsText, 0);
        scene_nodeSetPosition(fpsText, (Point){ .x = 200, .y = 230 });

        scene_sequence(scene);

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
        uint32_t frameDone = display_renderScene(display, scene);

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

            Point point = scene_nodePosition(text);

            if (keypad_isDown(keypad, PIN_BUTTON_4)) { point.x++; }
            if (keypad_isDown(keypad, PIN_BUTTON_3)) { point.x--; }
            if (keypad_isDown(keypad, PIN_BUTTON_2)) { point.y++; }
            if (keypad_isDown(keypad, PIN_BUTTON_1)) { point.y--; }

            scene_nodeSetPosition(text, point);

            if (keypad_read(keypad) == (PIN_BUTTON_1 | PIN_BUTTON_2)) {
                scene_dump(scene);
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

                scene_stopAnimations(cursor, SceneActionStopCurrent);
                scene_nodeAnimatePosition(scene, cursor, (Point){ .x = 10, .y = 10 + (cursorIndex * 32) }, 150, CurveEaseOutQuad, NULL);
            }

            static uint32_t fpsTrigger = 0;
            if (fpsTrigger++ > 100) {
                fpsTrigger = 0;
                scene_textSetTextInt(fpsText, display_fps(display));
            }

//            if (lastFrameTime == 0) { lastFrameTime = xTaskGetTickCount(); }
            //int32_t skewLastFrameTime = lastFrameTime;

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

void taskAppFunc(void* pvParameter) {
    while (1) {
        printf("Hello from App\n");

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
            uint8_t signature[SECP256K1_SIGNATURE_SIZE];

            int32_t t0 = ticks();

            secp256k1_sign(signature, signature, signature);

            int32_t dt = ticks() - t0;

            printf("Sig: 0x");
            for (int i = 0; i < SECP256K1_SIGNATURE_SIZE; i++) {
                printf("%02x", signature[i]);
            }
            printf(" (took %lds for 1 sign op)\n", dt);
        }

        delay(10000);
    }
}

void app_main() {
    printf("Hello world!\n");

    // Start the IO task (handles the display, LEDs and keypad)
    {
        // Pointer passed to taskIoFunc to notify us when IO is ready
        uint32_t ready = 0;

        BaseType_t status = xTaskCreatePinnedToCore(&taskIoFunc, "io", 8192, &ready, 2, &taskIoHandle, 0);
        printf("[main] start IO task: status=%d\n", status);
        assert(taskIoHandle != NULL);

        // Wait for the IO task to complete setup
        while (!ready) { delay(1); }
        printf("[main] IO ready\n");
    }

    // Start the App Process
    {
        BaseType_t status = xTaskCreatePinnedToCore(&taskAppFunc, "app", 8192 * 8, NULL, 1, &taskAppHandle, 0);
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
