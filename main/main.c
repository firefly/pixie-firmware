
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

#include "task-ble.h"
#include "system/device-info.h"
#include "system/keypad.h"
#include "system/pixels.h"
#include "system/utils.h"
#include "system/output.h"

#include "images/image-background.h"
#include "images/image-pixie.h"
#include "images/image-title.h"

#include "./app.h"
#include "./panel-menu.h"
#include "./panel-space.h"
#include "./config.h"


#define FRAMERATE          (60)
#define FRAMEDELAY         (1000 / ((FRAMERATE) + 1))

// Idea: If current rate of last 60 frames if going to be early,
// use delay_hi
#define FRAME_DELAY_LO     (1000 / (FRAMERATE))
#define FRAME_DELAY_HI     ((FRAME_DELAY_LO) + 1)


typedef struct _AppInit {
    AppInit init;
    int id;
    size_t stateSize;
    void *arg;
    uint32_t ready;
} _AppInit;

typedef struct AppContext {
    QueueHandle_t events;
    int id;
    uint8_t *state;
    FfxNode node;
} AppContext;

typedef struct EventFilter {
    int id;

    AppContext *app;

    EventName event;
    EventCallback callback;
    void *arg;
} EventFilter;

typedef struct EventDispatch {
    EventCallback callback;
    void* arg;
    EventPayload payload;
} EventDispatch;


#define MAX_EVENT_FILTERS  (32)
#define MAX_EVENT_BACKLOG  (8)

static EventFilter eventFilters[MAX_EVENT_FILTERS] = { 0 };
static SemaphoreHandle_t lockEvents;

static bool animating = false;
static FfxScene scene;

static void emitKeyEvents(KeypadContext keypad) {
    xSemaphoreTake(lockEvents, portMAX_DELAY);

    for (int i = 0; i < MAX_EVENT_FILTERS; i++) {
        EventFilter *filter = &eventFilters[i];
        if ((filter->event & EventNameCategoryMask) != EventNameCategoryKeys) {
            continue;
        }

        Keys keys = filter->event & KeyAll;

        Keys changed = keypad_didChange(keypad, keys);
        if (!changed) { continue; }

        Keys down = keypad_read(keypad);

        switch (filter->event & EventNameMask) {
            case EventNameKeysDown:
                if ((keys & down) != keys) { continue; }
                break;
            case EventNameKeysUp:
                if ((keys & down) != 0) { continue; }
                break;
            case EventNameKeysPress:
                // @TODO:
                printf("not impl\n");
                continue;
            case EventNameKeysChanged:
                break;
        }

        EventDispatch event = { 0 };

        event.callback = filter->callback;
        event.arg = filter->arg;

        event.payload.event = filter->event;
        event.payload.eventId = filter->id;
        event.payload.props.keyEvent.down = down;
        event.payload.props.keyEvent.changed = changed;

        QueueHandle_t events = filter->app->events;
        xQueueSendToBack(events, &event, 2);
    }

    xSemaphoreGive(lockEvents);
}

static void emitDisplayEvents(FfxScene scene) {

    xSemaphoreTake(lockEvents, portMAX_DELAY);

    static uint32_t countOk = 0, countFail = 0;
    for (int i = 0; i < MAX_EVENT_FILTERS; i++) {
        EventFilter *filter = &eventFilters[i];
        if (filter->event != EventNameRenderScene) { continue; }

        // Already has a display event and is falling behind;
        // don't overwhel it
        //if (filter->app->hasRenderEvent) { continue; }

        EventDispatch event = { 0 };

        event.callback = filter->callback;
        event.arg = filter->arg;

        event.payload.event = filter->event;
        event.payload.eventId = filter->id;
        event.payload.props.renderEvent.ticks = ticks();

        QueueHandle_t events = filter->app->events;
        BaseType_t status = xQueueSendToBack(events, &event, 0);
        if (status != pdTRUE) {
            countFail++;
            if (countFail == 1 || countFail == 100) {
                printf("[%s] emit:RenderScene failed: to=app-%d id=%d ok=%ld fail=%ld status=%d\n",
                  taskName(NULL), filter->app->id, filter->id, countOk,
                  countFail, status);
                if (countFail == 100) { countOk = countFail = 0; }
            }
        } else {
            countOk++;
        }
    }

    xSemaphoreGive(lockEvents);
}


// Caller must own the lockEvents mutex
static EventFilter* _getEmptyFilter() {
    for (int i = 0; i < MAX_EVENT_FILTERS; i++) {
        EventFilter *filter = &eventFilters[i];

        // Find an empty DispatchWait entry
        if (filter->event != 0) { continue; }

        return filter;
    }

    return NULL;
}


int app_onEvent(EventName event, EventCallback callback, void *arg) {
    AppContext *ctx = (void*)xTaskGetApplicationTaskTag(NULL);

    static int nextEventId = 1;

    int eventId = nextEventId++;

    xSemaphoreTake(lockEvents, portMAX_DELAY);

    EventFilter *filter = _getEmptyFilter(event);
    if (filter == NULL) {
        // @TODO: panic?
        printf("No filter\n");
        xSemaphoreGive(lockEvents);
        return -1;
    }

    filter->id = eventId;
    filter->app = ctx;

    filter->event = event;
    filter->callback = callback;
    filter->arg = arg;

    xSemaphoreGive(lockEvents);

    return eventId;
}

void app_offEvent(uint32_t eventId) {
    xSemaphoreTake(lockEvents, portMAX_DELAY);

    for (int i = 0; i < MAX_EVENT_FILTERS; i++) {
        EventFilter *filter = &eventFilters[i];
        if (filter->id == eventId) {
            filter->event = 0;
            break;
        }
    }

    xSemaphoreGive(lockEvents);
}


// Animate the image alpha to:
//  - t=0:  alpha = 0%
//  - t=.5: alpha = 100%
//  - t=1:  alpha = 0%
static fixed_ffxt FadeInFadeOut(fixed_ffxt t) {
    //fixed_ffxt t0 = t;
    if (t < FM_1_2) {
        t *= 2;
    } else {
        t = FM_1 - (t - FM_1_2) * 2;
    }

    return FfxCurveLinear(t);
}

// Animate the position, quadratically 
static fixed_ffxt AnimateWaft(fixed_ffxt t) {
    if (t < FM_1_2) {
        return mulfx(FfxCurveEaseOutQuad(t * 2), FM_1_2);
    }
    t -= FM_1_2;
    return FM_1_2 + mulfx(FfxCurveEaseInQuad(t * 2), FM_1_2);
}

static void animatePixie(FfxScene scene, FfxNode mover, FfxSceneActionStop stopAction) {
    ffx_scene_stopAnimations(mover, FfxSceneActionStopCurrent);

    FfxNode pixie = ffx_scene_groupFirstChild(mover);
    ffx_scene_stopAnimations(pixie, FfxSceneActionStopCurrent);

    color_ffxt *color = ffx_scene_imageColor(pixie);
    *color = ffx_color_rgb(0, 0, 0, 0);

    uint32_t duration = 4500 + esp_random() % 4500;

    FfxPoint target;
    target.x = (esp_random() % 300) - 30;
    target.y = (esp_random() % 300) - 30;

    ffx_scene_nodeAnimatePosition(scene, mover, target, duration,
      AnimateWaft, animatePixie);
    ffx_scene_imageAnimateAlpha(scene, pixie, 32, duration, FadeInFadeOut,
      NULL);

    // On first animation, fast forward to a random time in its life
    if (stopAction == FfxSceneActionStopFinal) {
        uint32_t advance = duration / (esp_random() % 100);
        ffx_scene_advanceAnimations(mover, advance);
        ffx_scene_advanceAnimations(pixie, advance);
    }
}

static void renderScene(uint8_t *fragment, uint32_t y0, void *context) {
    FfxScene scene = context;
    ffx_scene_render(scene, fragment, y0, FfxDisplayFragmentHeight);
}

static void animateColorRamp(color_ffxt *colors, size_t count, fixed_ffxt t, void *arg) {
    colors[0] = ffx_color_lerpColorRamp(arg, 12, t);
}

//static void rollRainbow(color_ffxt *colors, size_t count, fixed_ffxt t, void *arg) {
//    for (int i = 0; i < count; i++) {
//        colors[i] = ffx_color_hsv(scalarfx(360, t), 0x3c, 0x05, 0x20);
//    }
//}

//static void rollGreen(color_ffxt *colors, size_t count, fixed_ffxt t, void *arg) {
//    t = (sinfx(mulfx(FM_2PI, t)) + FM_1) >> 1;
//    for (int i = 0; i < count; i++) {
//        colors[i] = ffx_color_hsv(120, 0x3c, scalarfx(0x5, t), 0x20);
//    }
//}

//static void rollRed(color_ffxt *colors, size_t count, fixed_ffxt t, void *arg) {
//    t = (sinfx(mulfx(FM_2PI, t)) + FM_1) >> 1;
//    for (int i = 0; i < count; i++) {
//        colors[i] = ffx_color_hsv(0, 0x3c, scalarfx(0x5, t), 0x20);
//    }
//}

//static void offColorRamp(color_ffxt *colors, size_t count, fixed_ffxt t, void *arg) {
//    for (int i = 0; i < count; i++) {
//        colors[i] = ffx_color_rgb(0, 0, 0, 0);
//    }
//}

//static void clearAnimating(FfxScene scene, FfxNode mover, FfxSceneActionStop stopAction) {
//    animating = false;
//}


static void taskIoFunc(void* pvParameter) {
    uint32_t *ready = (uint32_t*)pvParameter;
    vTaskSetApplicationTaskTag( NULL, (void*) NULL);

    // All keys
    //uint32_t keys = PIN_BUTTON_1 | PIN_BUTTON_2 | PIN_BUTTON_3 | PIN_BUTTON_4;

    // The subset of keys to trigger a system reset
    //uint32_t resetKeys = KeyCancel | KeyNorth;

    // Scene context
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
        keypad = keypad_alloc();
        printf("[io] init keypad: dt=%ldms\n", ticks() - t0);

        // DEBUG; allow halting device on tight crash loops
        /*
        keypad_sample(keypad);
        keypad_latch(keypad);
        if (keypad_isDown(keypad, KeyNorth | KeySouth)) {
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
        */
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

    //color_ffxt colorRampGreen[] = {
    //    ffx_color_hsv(120, 0x3f, 0x0f, 0x0c),
    //    ffx_color_hsv(120, 0x3f, 0x00, 0x0c),
    //    ffx_color_hsv(120, 0x3f, 0x0f, 0x0c),
    //};

    {
        uint32_t t0 = ticks();
        pixels = pixels_init(PIXEL_COUNT, PIN_PIXELS);
        printf("[io] init pixels: dt=%ldms\n", ticks() - t0);

        pixels_animatePixel(pixels, 0, animateColorRamp, 780, 0, colorRamp1);
        pixels_animatePixel(pixels, 1, animateColorRamp, 780, 0, colorRamp2);
        pixels_animatePixel(pixels, 2, animateColorRamp, 780, 0, colorRamp3);
        pixels_animatePixel(pixels, 3, animateColorRamp, 780, 0, colorRamp4);
    }

    {
        uint32_t t0 = ticks();

        FfxNode root = ffx_scene_root(scene);

        FfxNode fill = ffx_scene_createFill(scene, 0x0000);
        ffx_scene_appendChild(root, fill);

        FfxNode bg = ffx_scene_createImage(scene, image_background, sizeof(image_background));
        ffx_scene_appendChild(root, bg);

        FfxNode pixies = ffx_scene_createGroup(scene);
        ffx_scene_appendChild(root, pixies);

        for (int i = 0; i < 8; i++) {
            FfxNode mover = ffx_scene_createGroup(scene);
            ffx_scene_appendChild(pixies, mover);
            ffx_scene_nodeSetPosition(mover, (FfxPoint){
                .x = ((esp_random() % 300) - 30),
                .y = ((esp_random() % 300) - 30)
            });

            FfxNode pixie = ffx_scene_createImage(scene, image_pixie, sizeof(image_pixie));
            ffx_scene_appendChild(mover, pixie);

            animatePixie(scene, mover, FfxSceneActionStopFinal);
        }

        ffx_scene_sequence(scene);

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

            if (keypad_didChange(keypad, KeyAll)) {
                printf("KEYS: CHANGE=%4x SW1=%d SW2=%d SW3=%d SW4=%d\n",
                  keypad_didChange(keypad, KeyAll),
                  keypad_isDown(keypad, KeyCancel),
                  keypad_isDown(keypad, KeyOk),
                  keypad_isDown(keypad, KeyNorth),
                  keypad_isDown(keypad, KeySouth));
            }

            // Check for holding the reset sequence to start a timer
            if (keypad_didChange(keypad, KeyAll)) {
                if (keypad_read(keypad) == KeyReset) {
                    resetStart = ticks();
                } else {
                    resetStart = 0;
                }
            }

            // The reset sequence was held for 2s... reset!
            if (keypad_read(keypad) == KeyReset && resetStart && (ticks() - resetStart) > 2000) {
                esp_restart();
                while(1) { }
            }

            emitKeyEvents(keypad);

/*
            if (frameCount == 60) {
                animating = true;
                screen = 1;
                pixels_animatePixel(pixels, 1, rollGreen, 1000, 1, NULL);
                ffx_scene_nodeAnimatePosition(scene, menu,
                  (FfxPoint){ .x = 0, .y = 0 }, 500, FfxCurveEaseOutQuad,
                  clearAnimating);
            }
*/
            ffx_scene_sequence(scene);

            emitDisplayEvents(scene);

            BaseType_t didDelay = xTaskDelayUntil(&lastFrameTime, FRAMEDELAY);

            // We are falling behind, catch up by dropping frames
            if (didDelay == pdFALSE) { lastFrameTime = ticks(); }
        }

        fflush(stdout);
    }
}



static void _appInit(void *_arg) {
    // Copy the AppInit so we can unblock the caller and it can
    // free this from its stack (by returning)
    AppInit init = NULL;
    size_t stateSize = 0;
    int appId = 0;
    void* arg;
    {
        _AppInit *appInit = _arg;
        init = appInit->init;
        appId = appInit->id;
        stateSize = appInit->stateSize ? appInit->stateSize: 1;
        arg = appInit->arg;
        appInit->ready = 1;
    }

    // Create the app state
    uint8_t state[stateSize];
    memset(state, 0, stateSize);

    // Create an incoming event queue
    StaticQueue_t eventQueue;
    uint8_t eventStore[MAX_EVENT_BACKLOG * sizeof(EventDispatch)];
    QueueHandle_t events = xQueueCreateStatic(MAX_EVENT_BACKLOG,
      sizeof(EventDispatch), eventStore, &eventQueue);
    assert(events != NULL);

    FfxNode node = ffx_scene_createGroup(scene);
    ffx_scene_appendChild(ffx_scene_root(scene), node);

    // Create the App context (attached to the task tag)
    AppContext context = { 0 };
    context.id = appId;
    context.state = state;
    context.events = events;
    context.node = node;
    vTaskSetApplicationTaskTag(NULL, (void*)&context);

    // Initialize the app
    init(scene, context.node, context.state, arg);

    // Begin the event loop
    EventDispatch dispatch = { 0 };
    while (1) {
        BaseType_t result = xQueueReceive(events, &dispatch, 1000);
        if (result != pdPASS) {
            printf("No task events\n");
            continue;
        }

        dispatch.callback(dispatch.payload, dispatch.arg);
    }
}

void app_push(AppInit init, size_t stateSize, void *arg) {
    static int nextAppId = 1;

    int appId = nextAppId++;

    char name[configMAX_TASK_NAME_LEN];
    snprintf(name, sizeof(name), "app-%d", appId);

    TaskHandle_t handle = NULL;
    _AppInit appInit = { 0 };
    appInit.id = appId;
    appInit.init = init;
    appInit.stateSize = stateSize;
    appInit.arg = arg;

    BaseType_t status = xTaskCreatePinnedToCore(&_appInit, name, 8192 * 4,
      &appInit, 1, &handle, 0);
        printf("[main] init app task: status=%d\n", status);
        assert(handle != NULL);

    while (!appInit.ready) { delay(2); } // ?? Maybe use event?
}

void app_main() {
    vTaskSetApplicationTaskTag( NULL, (void*)NULL);

    TaskHandle_t taskIoHandle = NULL;
    TaskHandle_t taskBleHandle = NULL;

    printf("Hello world!\n");

    StaticSemaphore_t lockEventsBuffer;
    lockEvents = xSemaphoreCreateBinaryStatic(&lockEventsBuffer);
    xSemaphoreGive(lockEvents);

    // Load NVS and eFuse provision data
    {
        DeviceStatus status = device_init();
        printf("[main] device initialized: status=%d serial=%ld model=%ld\n",
          status, device_serialNumber(), device_modelNumber());
    }

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
        uint32_t ready = 0; // @TODO: set this to 0 and set in the task

        BaseType_t status = xTaskCreatePinnedToCore(&taskBleFunc, "ble", 8196, &ready, 2, &taskBleHandle, 0);
        printf("[main] start BLE task: status=%d\n", status);
        assert(taskBleHandle != NULL);

        // Wait for the REPL task to complete setup
        while (!ready) { delay(1); }
        printf("[main] BLE ready\n");
    }

    // Start the App Process; this is started in the main task, so
    // has high-priority. Don't doddle.
    // @TODO: should we start a short-lived low-priority task to start this?
    //app_push(panelMenuInit, sizeof(PanelMenuState), NULL);
    pushPanelSpace(NULL);

    while (1) {
        printf("[main] high-water: boot=%d io=%d, ble=%d freq=%ld\n",
            uxTaskGetStackHighWaterMark(NULL),
            uxTaskGetStackHighWaterMark(taskIoHandle),
            uxTaskGetStackHighWaterMark(taskBleHandle),
//            uxTaskGetStackHighWaterMark(taskAppHandle),
            portTICK_PERIOD_MS);
        delay(60000);
    }
}





