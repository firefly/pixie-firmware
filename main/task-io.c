
#include <stdlib.h>
#include <string.h>

#include <driver/gpio.h>
#include <hal/gpio_ll.h>
#include "esp_random.h"

#include "firefly-display.h"
#include "firefly-scene.h"

#include "config.h"

#include "panel.h"
#include "events.h"
#include "pixels.h"
#include "utils.h"

#include "images/image-background.h"
#include "images/image-pixie.h"
#include "task-io.h"



#define FRAMERATE          (60)
#define FRAMEDELAY         (1000 / ((FRAMERATE) + 1))

// Idea: If current rate of last 60 frames if going to be early,
// use delay_hi
//#define FRAME_DELAY_LO     (1000 / (FRAMERATE))
//#define FRAME_DELAY_HI     ((FRAME_DELAY_LO) + 1)


///////////////////////////////
// Keypad

#define KEYPAD_SAMPLE_COUNT    (10)

typedef struct KeypadContext {
    Keys keys;

    uint32_t pins;

    uint32_t count;
    uint32_t samples[KEYPAD_SAMPLE_COUNT];

    uint32_t previousLatch;
    uint32_t latch;
} KeypadContext;

static Keys remapKeypadPins(uint32_t value) {
    Keys keys = 0;
    if (value & PIN_BUTTON_1) { keys |= KeyCancel; }
    if (value & PIN_BUTTON_2) { keys |= KeyOk; }
    if (value & PIN_BUTTON_3) { keys |= KeyNorth; }
    if (value & PIN_BUTTON_4) { keys |= KeySouth; }
    return keys;
}

static void keypad_init(KeypadContext *context) {
    uint32_t pins = PIN_BUTTON_1 | PIN_BUTTON_2 | PIN_BUTTON_3 | PIN_BUTTON_4;
    context->pins = pins;

    context->keys = KeyCancel | KeyOk | KeyNorth | KeySouth;

    // Setup the GPIO input pins
    for (uint32_t i = 0; i < 32; i++) {
        if ((pins & (1 << i)) == 0) { continue; }
        gpio_reset_pin(i);
        gpio_set_direction(i, GPIO_MODE_INPUT);
        gpio_pullup_en(i);
    }
}

static void keypad_sample(KeypadContext *context) {
    context->samples[context->count % KEYPAD_SAMPLE_COUNT] = ~(REG_READ(GPIO_IN_REG));
    context->count++;
}

static void keypad_latch(KeypadContext *context) {
    uint32_t samples = context->count;
    if (samples > KEYPAD_SAMPLE_COUNT) { samples = KEYPAD_SAMPLE_COUNT; }

    uint32_t latch = 0;
    for (uint32_t i = 0; i < 32; i++) {
        uint32_t mask = (1 << i);
        if ((context->pins & mask) == 0) { continue; }

        uint32_t count = 0;
        for (uint32_t s = 0; s < samples; s++) {
            if (context->samples[s] & mask) { count++; }
        }

        if (count * 2 > samples) { latch |= mask; }
    }

    context->count = 0;

    context->previousLatch = context->latch;
    context->latch = remapKeypadPins(latch);
}

static Keys keypad_didChange(KeypadContext *context, Keys keys) {
    keys &= context->keys;
    return (context->previousLatch ^ context->latch) & keys;
}

static Keys keypad_read(KeypadContext *context) {
    return (context->latch & context->keys);
}


///////////////////////////////
// Pixels

PixelsContext pixels;

static void animateColorRamp(color_ffxt *colors, size_t count,
  fixed_ffxt t, void *arg) {
    colors[0] = ffx_color_lerpColorRamp(arg, 12, t);
}

void panel_setPixel(uint32_t pixel, color_ffxt color) {
    pixels_setPixel(pixels, pixel, color);
}


///////////////////////////////
// Pixies

// Animate the image alpha to:
//  - t=0:  alpha = 0%
//  - t=.5: alpha = 100%
//  - t=1:  alpha = 0%
static fixed_ffxt CurveGlow(fixed_ffxt t) {
    //fixed_ffxt t0 = t;
    if (t < FM_1_2) {
        t *= 2;
    } else {
        t = FM_1 - (t - FM_1_2) * 2;
    }

    return FfxCurveLinear(t);
}

// Animate the position, quadratically
static fixed_ffxt CurveWaft(fixed_ffxt t) {
    if (t < FM_1_2) {
        return mulfx(FfxCurveEaseOutQuad(t * 2), FM_1_2);
    }
    t -= FM_1_2;
    return FM_1_2 + mulfx(FfxCurveEaseInQuad(t * 2), FM_1_2);
}

static void runPixieComplete(FfxNode, FfxSceneActionStop, void*);

static void animateGlow(FfxNode pixie, FfxNodeAnimation *anim, void *arg) {
    anim->duration = *(uint32_t*)arg;
    anim->curve = CurveGlow;
    ffx_sceneImage_setTint(pixie, ffx_color_rgb(255, 255, 255));
}

static void animateWaft(FfxNode pixie, FfxNodeAnimation *anim, void *arg) {
    anim->duration = *(int*)arg;
    anim->curve = CurveWaft;
    anim->onComplete = runPixieComplete;

    ffx_sceneNode_setPosition(pixie, (FfxPoint){
        .x = (esp_random() % 300) - 30,
        .y = (esp_random() % 300) - 30
    });
}

static void runPixieComplete(FfxNode pixie, FfxSceneActionStop stopAction,
  void *arg) {

    ffx_sceneImage_setTint(pixie, ffx_color_rgba(0, 0, 0, 0));

    uint32_t duration = 4500 + esp_random() % 4500;

    ffx_sceneNode_animate(pixie, animateGlow, &duration);
    ffx_sceneNode_animate(pixie, animateWaft, &duration);

    // On first animation, fast forward to a random time in its life
    if (stopAction == FfxSceneActionStopFinal) {
        uint32_t advance = duration * (esp_random() % 100) / 100;
        ffx_sceneNode_advanceAnimations(pixie, advance);
    }
}


///////////////////////////////
// Scene

FfxScene scene;

static uint8_t* allocSpace(size_t size, void *arg) {
    void* result = malloc(size);
    if (result == NULL) {
        printf("EEK! Crash, no memory left\n");
    }
    return result;
}

static void freeSpace(uint8_t *pointer, void *arg) {
    free(pointer);
}

typedef struct Callback {
  FfxNodeAnimationCompletionFunc callFunc;
  FfxNode node;
  FfxSceneActionStop stopType;
  void *arg;
} Callback;

static void executeCallback(EventPayload event, void* arg) {
    panel_offEvent(event.eventId);

    Callback *cb = (Callback*)&event.props.custom;
    cb->callFunc(cb->node, cb->stopType, cb->arg);
}

static void* sceneSetup(FfxNode node, FfxNodeAnimation,
  void *initArg) {

    static uint32_t nextCallback = 1;

    uint32_t cbid = nextCallback++;

    int eventId = panel_onEvent(EventNameCustom | nextCallback,
      executeCallback, NULL);
    if (eventId == 0) { cbid = 0; }

    return (void*)cbid;
}

static void sceneDispatch(void *setupArg,
  FfxNodeAnimationCompletionFunc callFunc, FfxNode node,
  FfxSceneActionStop stopType, void *arg, void *initArg) {

    uint32_t cbid = *(uint32_t*)setupArg;

    // Callback is on a task with no event queue; i.e. the IO task
    if (cbid == 0) {
        callFunc(node, stopType, arg);
        return;
    }

    EventPayloadProps props;
    Callback *cb = (Callback*)&props.custom;

    cb->callFunc = callFunc;
    cb->node = node;
    cb->stopType = stopType;
    cb->arg = arg;

    panel_emitEvent(EventNameCustom | cbid, props);
}

// Custom renderer state
static CustomRenderFunc customRenderer = NULL;
static void *customRendererContext = NULL;

void taskIo_setCustomRenderer(CustomRenderFunc renderFunc, void *context) {
    customRenderer = renderFunc;
    customRendererContext = context;
    printf("[io] Custom renderer %s\n", renderFunc ? "enabled" : "disabled");
}

static void renderScene(uint8_t *fragment, uint32_t y0, void *context) {
    // Use custom renderer if set
    if (customRenderer) {
        customRenderer(fragment, y0, customRendererContext);
        return;
    }
    
    // Default scene rendering
    FfxScene scene = context;
    ffx_scene_render(scene, (uint16_t*)fragment,
      (FfxPoint){ .x = 0, .y = y0 },
      (FfxSize){
          .width = FfxDisplayFragmentWidth,
          .height = FfxDisplayFragmentHeight
      });
}


///////////////////////////////
// Task

void taskIoFunc(void* pvParameter) {
    uint32_t *ready = (uint32_t*)pvParameter;
    vTaskSetApplicationTaskTag( NULL, (void*) NULL);

    // All keys
    //uint32_t keys = PIN_BUTTON_1 | PIN_BUTTON_2 | PIN_BUTTON_3 | PIN_BUTTON_4;

    // The subset of keys to trigger a system reset
    //uint32_t resetKeys = KeyCancel | KeyNorth;

    // Scene context
    //scene = ffx_scene_init(allocSpace, freeSpace, sceneSetup, sceneDispatch,
    //  NULL);
    scene = ffx_scene_init(allocSpace, freeSpace, NULL, NULL, NULL);

    FfxDisplayContext display;
    {
        uint32_t t0 = ticks();
        display = ffx_display_init(DISPLAY_BUS,
          PIN_DISPLAY_DC, PIN_DISPLAY_RESET, FfxDisplayRotationRibbonRight,
          renderScene, scene);
        printf("[io] init display: dt=%ldms\n", ticks() - t0);
    }

    KeypadContext keypad = { 0 };
    {
        uint32_t t0 = ticks();
        keypad_init(&keypad);
        printf("[io] init keypad: dt=%ldms\n", ticks() - t0);
    }

    color_ffxt colorRamp1[] = {
        ffx_color_hsva(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsva(275, 0x3f, 0x08, 0x0c),
        ffx_color_hsva(275, 0x3f, 0x0a, 0x0c),
        ffx_color_hsva(275, 0x3f, 0x08, 0x0c),
        ffx_color_hsva(150, 0x3f, 0x00, 0x0c),
        ffx_color_hsva(275, 0x3f, 0x00, 0x0c),

        ffx_color_hsva(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsva(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsva(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsva(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsva(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsva(275, 0x3f, 0x00, 0x0c),
        ffx_color_rgba(0, 0, 0, 0),
        ffx_color_rgba(0, 0, 0, 0),
        ffx_color_rgba(0, 0, 0, 0),
    };

    color_ffxt colorRamp2[] = {
        ffx_color_hsva(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsva(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsva(275, 0x3f, 0x08, 0x0c),
        ffx_color_hsva(275, 0x3f, 0x0a, 0x0c),
        ffx_color_hsva(275, 0x3f, 0x0f, 0x0c),
        ffx_color_hsva(150, 0x3f, 0x00, 0x0c),

        ffx_color_hsva(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsva(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsva(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsva(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsva(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsva(275, 0x3f, 0x00, 0x0c),
        ffx_color_rgba(0, 0, 0, 0),
        ffx_color_rgba(0, 0, 0, 0),
        ffx_color_rgba(0, 0, 0, 0),
    };

    color_ffxt colorRamp3[] = {
        ffx_color_hsva(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsva(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsva(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsva(275, 0x3f, 0x08, 0x0c),
        ffx_color_hsva(275, 0x3f, 0x0a, 0x0c),
        ffx_color_hsva(275, 0x3f, 0x0f, 0x0c),

        ffx_color_hsva(150, 0x3f, 0x00, 0x0c),
        ffx_color_hsva(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsva(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsva(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsva(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsva(275, 0x3f, 0x00, 0x0c),

        ffx_color_rgba(0, 0, 0, 0),
        ffx_color_rgba(0, 0, 0, 0),
        ffx_color_rgba(0, 0, 0, 0),
    };

    color_ffxt colorRamp4[] = {
        ffx_color_hsva(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsva(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsva(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsva(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsva(275, 0x3f, 0x00, 0x0c),
        ffx_color_hsva(275, 0x3f, 0x08, 0x0c),

        ffx_color_hsva(275, 0x3f, 0x3a, 0x0c),
        ffx_color_hsva(275, 0x3f, 0x3f, 0x0c),
        ffx_color_hsva(275, 0x3f, 0x3f, 0x0c),
        ffx_color_hsva(275, 0x0, 0x3f, 0x0c),
        ffx_color_hsva(275, 0x3f, 0x3f, 0x0c),
        ffx_color_hsva(275, 0x0, 0x00, 0x0c),

        ffx_color_rgba(0, 0, 0, 0),
        ffx_color_rgba(0, 0, 0, 0),
        ffx_color_rgba(0, 0, 0, 0),
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
        ffx_sceneGroup_appendChild(root, fill);

        FfxNode bg = ffx_scene_createImage(scene, image_background,
          sizeof(image_background));
        ffx_sceneGroup_appendChild(root, bg);

        FfxNode pixies = ffx_scene_createGroup(scene);
        ffx_sceneGroup_appendChild(root, pixies);

        for (int i = 0; i < 10; i++) {
            FfxNode pixie = ffx_scene_createImage(scene, image_pixie,
              sizeof(image_pixie));
            ffx_sceneGroup_appendChild(pixies, pixie);
            ffx_sceneNode_setPosition(pixie, (FfxPoint){  // @TODO: move this into run?
                .x = ((esp_random() % 300) - 30),
                .y = ((esp_random() % 300) - 30)
            });

            runPixieComplete(pixie, FfxSceneActionStopFinal, NULL);
        }

        ffx_scene_sequence(scene);
        ffx_scene_dump(scene);

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
        keypad_sample(&keypad);

        // Render a screen fragment; if the last fragment is
        // complete, the frame is complete
        uint32_t frameDone = ffx_display_renderFragment(display);

        static uint32_t frameCount = 0;

        if (frameDone) {
            frameCount++;

            pixels_tick(pixels);

            // Latch the keypad values de-bouncing with the inter-frame samples
            keypad_latch(&keypad);

            // Check for holding the reset sequence to start a timer
            if (keypad_didChange(&keypad, KeyAll)) {
                if (keypad_read(&keypad) == KeyReset) {
                    resetStart = ticks();
                } else {
                    resetStart = 0;
                }
            }

            // The reset sequence was held for 2s... reset!
            if (keypad_read(&keypad) == KeyReset && resetStart && (ticks() - resetStart) > 2000) {
                esp_restart();
                while(1) { }
            }

            panel_emitEvent(EventNameKeys, (EventPayloadProps){
                .keys = {
                    .down = keypad_read(&keypad),
                    .changed = keypad_didChange(&keypad, KeyAll),
                    .flags = 0
                }
            });

            ffx_scene_sequence(scene);

            uint32_t now = ticks();

            panel_emitEvent(EventNameRenderScene, (EventPayloadProps){
                .render = { .ticks = now }
            });

            BaseType_t didDelay = xTaskDelayUntil(&lastFrameTime, FRAMEDELAY);

            // We are falling behind, catch up by dropping frames
            if (didDelay == pdFALSE) {
                //printf("Frame dropped dt=%ld\n", now - lastFrameTime);
                lastFrameTime = ticks();
            }
        }

        fflush(stdout);
    }
}
