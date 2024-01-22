
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

#include "crypto/keccak256.h"
#include "crypto/secp256k1.h"

#include "system/curves.h"
#include "system/display.h"
#include "system/keypad.h"
#include "system/pixels.h"
#include "system/scene.h"

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

#endif


#define FRAMERATE          (48)
#define FRAMEDELAY         (1000 / (FRAMERATE + 2))

static TaskHandle_t taskSystemHandle = NULL;
static TaskHandle_t taskAppHandle = NULL;
static uint32_t systemReady = 0;

uint32_t millis() {
    return esp_timer_get_time() / 1000;
}

void bounce(SceneContext scene, Node node, SceneActionStop stopAction) {
    Point point = scene_nodePosition(node);
    point.x = point.y = (point.x > 120) ? 10: 210;

    // point.x = rand() % 220;
    // point.y = rand() % 220;
    AnimationId animationId = scene_nodeAnimatePosition(scene, node, point, 2000, CurveEaseOutElastic);

    scene_onAnimationCompletion(scene, animationId, bounce, NULL);
}

void taskSystemFunc(void* pvParameter) {
    // All keys
    uint32_t keys = PIN_BUTTON_1 | PIN_BUTTON_2 | PIN_BUTTON_3 | PIN_BUTTON_4;

    // The subset of keys to trigger a system reset
    uint32_t resetKeys = PIN_BUTTON_3 | PIN_BUTTON_1;

    // {
    //     int32_t i = -1;
    //     if ((i >> 1) > 0) {
    //         printf("Not arithmetic shift\n");
    //         while(1);
    //     }
    // }

    /*
    {
        const uint32_t count = 0x10000;

        uint32_t t0 = millis();
        float total = 0;
        for (uint32_t i = 0; i < count; i++) {
            total += CurveEaseOutBounce((float)i / count);
        }
        
        uint32_t dt = millis() - t0;
        printf("Time Float: %f %d\n", total, dt);

        t0 = millis(); total = 0;
        for (uint32_t i = 0; i < count; i++) {
            total += FixedCurveEaseOutBounce(count * i / count);
        }
        dt = millis() - t0;
        printf("Time Fixed: %f %d\n", (float)total / count, dt);
    }

    while(1);
    */

    // I/O contexts
    DisplayContext display = display_init(DISPLAY_BUS, PIN_DISPLAY_DC, PIN_DISPLAY_RESET, DisplayRotationPinsLeft);
    KeypadContext keypad = keypad_init(keys);
    //PixelsContext pixels = pixels_init();
    //pixels_setRGB(pixels, 0, 0xffffffff);

    // DEBUG; allow halting device on tight crash loops
    keypad_sample(keypad);
    keypad_latch(keypad);
    if (keypad_isDown(keypad, PIN_BUTTON_3 | PIN_BUTTON_4)) {
        while(1) {
            keypad_sample(keypad);
            keypad_latch(keypad);
            if (keypad_isDown(keypad, resetKeys)) {
                esp_restart();
                while(1);
            }
            vTaskDelay(16);
        }
    }

    // Scene context

    SceneContext scene = scene_init(2048);
    Node root = scene_root(scene);
    Node fill = scene_createFill(scene, 0x21ea); //RGB(0x23, 0x3b, 0x52));
    scene_appendChild(root, fill);

    Node img = scene_createImage(scene, moonbeam, sizeof(moonbeam));
    scene_nodeSetPosition(img, (Point){ .x = 150, .y = 0 });
    scene_appendChild(root, img);

    Node img2 = scene_createImage(scene, (uint16_t*)screen, sizeof(screen) / 2);
    scene_appendChild(root, img2);

    char fpsTextBuffer[10];
    Node fpsText = scene_createTextFlip(scene, fpsTextBuffer, sizeof(fpsTextBuffer));
    scene_appendChild(root, fpsText);
    scene_nodeSetPosition(fpsText, (Point){ .x = 200, .y = 230 });

    char fpsContent[4];
    snprintf(fpsContent, 4, "%-2d", 0);
    scene_textSetText(fpsText, fpsContent, 4);

    const char* const phrase = "Hello y g World!";
    Node text = scene_createText(scene, phrase, strlen(phrase));
    scene_appendChild(root, text);
    scene_nodeSetPosition(text, (Point){ .x = 100, .y = 120 });

    AnimationId animation = AnimationIdNull;

    //for (uint32_t i = 0; i < 300; i++) 
    {
        Node box = scene_createBox(scene, (Size){ .width = 20, .height = 20 }, RGB(0, 0, 255));
        scene_nodeSetPosition(box, (Point){ .x = 10, .y = 10 });
        scene_appendChild(root, box);
        bounce(scene, box, SceneActionStopFinal);
    }

    scene_sequence(scene);


    scene_dump(scene);

    // The system is up; unblock the bootstrap process and start the app
    systemReady = 1;

    // How long the reset sequence has been held down for
    uint32_t resetStart = 0;

    // The time of the last frame; used to enforce a constant framerate
    // The special value 0 causes an immediate update
    TickType_t lastFrameTime = 0;

    while (1) {
        // Sample the keypad
        keypad_sample(keypad);

        // Render a screen fragment; if the last fragment is complete, the frame is complete
        uint32_t frameDone = display_renderScene(display, scene);

        if (frameDone) {

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

            static uint32_t fpsTrigger = 0;
            if (fpsTrigger++ > 100) {
                fpsTrigger = 0;
                uint32_t fps = display_fps(display);
                snprintf(fpsContent, 4, "%-2d", (uint8_t)fps);
                scene_textSetText(fpsText, fpsContent, 4);
            }

            //if (lastFrameTime == 0) { lastFrameTime = xTaskGetTickCount() - FRAMEDELAY; }
            // int32_t skewLastFrameTime = lastFrameTime;

            //BaseType_t didDelay = xTaskDelayUntil(&lastFrameTime, FRAMEDELAY);

            // We are falling behind, catch up by dropping frames
            //if (didDelay == pdFALSE) {
                //uint32_t dt = xTaskGetTickCount() - skewLastFrameTime;
                //printf("[System.Display] Framerate Skew Detected; dt=%d dropped=%d\n", dt, (dt + FRAMEDELAY - 1) / FRAMEDELAY);
            //    lastFrameTime = 0;
            //}
        }

        fflush(stdout);
    }
}

void taskAppFunc(void* pvParameter) {
  while (1) {
    printf("Hello from App\n");
    vTaskDelay(10000);
  }
}


void setup() {
  printf("Hello world!\n");

  // Start the System Process (handles the display and keypad)
  BaseType_t foo = xTaskCreatePinnedToCore(&taskSystemFunc, "sys", 8192 * 20, NULL, 1, &taskSystemHandle, 0);
  printf("Initializing system task: %d %d\n", taskSystemHandle != NULL, foo);
  assert(taskSystemHandle != NULL);

  // Wait for the System Process to complete setup
  while (!systemReady) { vTaskDelay(1); }
  printf("Started system task\n");

  // Start the App Process
  xTaskCreatePinnedToCore(&taskAppFunc, "app", 8192 * 12, NULL, 2, &taskAppHandle, 0);
  assert(taskSystemHandle != NULL);
  printf("Initializing app task: %d\n", taskSystemHandle != NULL);
}

void loop() {
    printf("[System] High-Water Marks: sys=%d, app=%d freq=%ld\n",
         uxTaskGetStackHighWaterMark(taskSystemHandle),
         uxTaskGetStackHighWaterMark(taskAppHandle),
         portTICK_PERIOD_MS);

    {
        int32_t t0 = millis();

        uint8_t checksum[KECCAK256_DIGEST_SIZE];

        Keccak256Context ctx;

        for (uint32_t i = 0; i < 1000; i++) {
            keccak256_init(&ctx);
            keccak256_update(&ctx, checksum, KECCAK256_DIGEST_SIZE);
            keccak256_final(&ctx, checksum);
        }

        int32_t dt = millis() - t0;

        printf("Checksum: 0x");
        for (int i = 0; i < KECCAK256_DIGEST_SIZE; i++) {
            printf("%02x", checksum[i]);
        }
        printf(" (took %lds)\n", dt);
    }

    {
        uint8_t signature[SECP256K1_SIGNATURE_SIZE];

        int32_t t0 = millis();

        secp256k1_sign(signature, signature, signature);

        int32_t dt = millis() - t0;

        printf("Sig: 0x");
        for (int i = 0; i < SECP256K1_SIGNATURE_SIZE; i++) {
            printf("%02x", signature[i]);
        }
        printf(" (took %lds)\n", dt);
    }

    vTaskDelay(5000 / portTICK_PERIOD_MS);
}

void app_main() {
    setup();

    while (1) {
        loop();
    }
}
