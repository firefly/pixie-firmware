#include "pixels.h"

#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "esp_check.h"

#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"

#include "./utils.h"

#define LED_COUNT     (4)
#define MAX_COLORS    (16)


typedef enum AnimationType {
    AnimationTypeNone    = 0,
    AnimationTypeNormal  = 1,
    AnimationTypeRepeat  = 2,
    AnimationTypeStatic  = 3,
    //AnimationTypeFill  = , // @TODO
} AnimationType;

/*
typedef struct ColorRamp {
    color_ffxt colors[MAX_COLORS];
    uint32_t count;
} ColorRamp;
*/

typedef struct Action {
    PixelAnimationFunc func;
    void *arg;

    uint32_t startTime;
    uint32_t duration;

    AnimationType type;
} Action;

typedef struct _PixelsContext {
    rmt_channel_handle_t channel;
    rmt_encoder_handle_t encoder;

    Action *actions;
    color_ffxt *colors;
    uint8_t *pixels;

    uint32_t tick;

    size_t pixelCount;

    // If non-zero then pixels[0] is animating all pixels
    uint32_t animatePixels;
} _PixelsContext;


// 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define RMT_RESOLUTION_HZ 10000000


static const char *TAG = "led_encoder";

typedef struct {
    rmt_encoder_t base;
    rmt_encoder_t *bytes_encoder;
    rmt_encoder_t *copy_encoder;
    int state;
    rmt_symbol_word_t reset_code;
} rmt_led_strip_encoder_t;

static size_t rmt_encode_led_strip(rmt_encoder_t *encoder, rmt_channel_handle_t channel, const void *primary_data, size_t data_size, rmt_encode_state_t *ret_state) {
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);

    rmt_encode_state_t session_state = RMT_ENCODING_RESET;
    rmt_encode_state_t state = RMT_ENCODING_RESET;

    size_t encoded_symbols = 0;
    switch (led_encoder->state) {
        case 0: {
            // transmit RGB data
            rmt_encoder_handle_t bytes_encoder = led_encoder->bytes_encoder;

            encoded_symbols += bytes_encoder->encode(bytes_encoder, channel, primary_data, data_size, &session_state);
            if (session_state & RMT_ENCODING_COMPLETE) {
                // switch to next state when current encoding session finished
                led_encoder->state = 1;
            }

            // Out of memory
            if (session_state & RMT_ENCODING_MEM_FULL) {
                state |= RMT_ENCODING_MEM_FULL;
                *ret_state = state;
                return encoded_symbols;
            }
        }
        // ...fall-through
        case 1: {
            // Transmit reset code
            rmt_encoder_handle_t copy_encoder = led_encoder->copy_encoder;

            encoded_symbols += copy_encoder->encode(copy_encoder, channel, &led_encoder->reset_code, sizeof(led_encoder->reset_code), &session_state);
            if (session_state & RMT_ENCODING_COMPLETE) {
                // back to the initial encoding session
                led_encoder->state = RMT_ENCODING_RESET;
                state |= RMT_ENCODING_COMPLETE;
            }

            // Out of memory
            if (session_state & RMT_ENCODING_MEM_FULL) {
                state |= RMT_ENCODING_MEM_FULL;
                *ret_state = state;
                return encoded_symbols;
            }
        }
    }

    *ret_state = state;
    return encoded_symbols;
}

static esp_err_t rmt_del_led_strip_encoder(rmt_encoder_t *encoder) {
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    rmt_del_encoder(led_encoder->bytes_encoder);
    rmt_del_encoder(led_encoder->copy_encoder);
    free(led_encoder);
    return ESP_OK;
}

static esp_err_t rmt_led_strip_encoder_reset(rmt_encoder_t *encoder) {
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    rmt_encoder_reset(led_encoder->bytes_encoder);
    rmt_encoder_reset(led_encoder->copy_encoder);
    led_encoder->state = RMT_ENCODING_RESET;
    return ESP_OK;
}

static esp_err_t encoder_init(_PixelsContext *context) {
    esp_err_t ret = ESP_OK;
    rmt_led_strip_encoder_t *led_encoder = NULL;
    //ESP_GOTO_ON_FALSE(ret_encoder, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");
    led_encoder = calloc(1, sizeof(rmt_led_strip_encoder_t));
    ESP_GOTO_ON_FALSE(led_encoder, ESP_ERR_NO_MEM, err, TAG, "no mem for led strip encoder");
    led_encoder->base.encode = rmt_encode_led_strip;
    led_encoder->base.del = rmt_del_led_strip_encoder;
    led_encoder->base.reset = rmt_led_strip_encoder_reset;
    // different led strip might have its own timing requirements, following parameter is for WS2812
    rmt_bytes_encoder_config_t bytes_encoder_config = {
        .bit0 = {
            .level0 = 1,
            .duration0 = 0.3 * RMT_RESOLUTION_HZ / 1000000,  // T0H=220-380ns
            .level1 = 0,
            .duration1 = 0.75 * RMT_RESOLUTION_HZ / 1000000, // T0L=580-1000ns
        },
        .bit1 = {
            .level0 = 1,
            .duration0 = 0.75 * RMT_RESOLUTION_HZ / 1000000, // T1H=580-1000ns
            .level1 = 0,
            .duration1 = 0.3 * RMT_RESOLUTION_HZ / 1000000,  // T1L=220-380ns
        },
        .flags.msb_first = 1 // WS2812 transfer bit order: G7...G0R7...R0B7...B0
    };
    ESP_GOTO_ON_ERROR(rmt_new_bytes_encoder(&bytes_encoder_config, &led_encoder->bytes_encoder), err, TAG, "create bytes encoder failed");
    rmt_copy_encoder_config_t copy_encoder_config = {};
    ESP_GOTO_ON_ERROR(rmt_new_copy_encoder(&copy_encoder_config, &led_encoder->copy_encoder), err, TAG, "create copy encoder failed");

    // I think this needs to be >280us
    uint32_t reset_ticks = RMT_RESOLUTION_HZ / 1000000 * 50 / 2; // reset code duration defaults to 50us
    led_encoder->reset_code = (rmt_symbol_word_t) {
        .level0 = 0,
        .duration0 = reset_ticks,
        .level1 = 0,
        .duration1 = reset_ticks,
    };
    context->encoder = &led_encoder->base;
    return ESP_OK;
err:
    if (led_encoder) {
        if (led_encoder->bytes_encoder) {
            rmt_del_encoder(led_encoder->bytes_encoder);
        }
        if (led_encoder->copy_encoder) {
            rmt_del_encoder(led_encoder->copy_encoder);
        }
        free(led_encoder);
    }
    return ret;
}


PixelsContext pixels_init(size_t pixelCount, uint32_t pin) {

    _PixelsContext *context = malloc(sizeof(_PixelsContext));
    memset(context, 0, sizeof(_PixelsContext));

    context->pixelCount = pixelCount;

    context->pixels = malloc(3 * pixelCount);

    // @TODO: if malloc failes return NULL

    context->actions = malloc(sizeof(Action) * pixelCount);
    memset(context->actions, 0, sizeof(Action) * pixelCount);

    context->colors = malloc(sizeof(color_ffxt) * pixelCount);
    memset(context->colors, 0, sizeof(color_ffxt) * pixelCount);

    // Configure the channel; ESP32-C3 only has a software channel
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = pin,
        .mem_block_symbols = 64,
        .resolution_hz = RMT_RESOLUTION_HZ,
        .trans_queue_depth = 4,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &(context->channel)));

    // Create the RMT encoder
    ESP_ERROR_CHECK(encoder_init(context));

    ESP_ERROR_CHECK(rmt_enable(context->channel));

    // Clear the pixels (on reboot they will retain last state)
    pixels_tick(context);

    return context;
}

static void computeColor(color_ffxt *colors, uint32_t count,
  Action *action, uint32_t now) {

    uint32_t repeat = 0;
    switch(action->type) {
        case AnimationTypeNone:
            colors[0] = 0;
            break;
        case AnimationTypeStatic:
            colors[0] = (color_ffxt)(action->arg);
            break;
        case AnimationTypeRepeat:
            repeat = 1;
            // ...falls through
        case AnimationTypeNormal: {
            uint32_t dt = now - action->startTime;
            uint32_t duration = action->duration;
            if (dt > duration) {
            // @TODO: this can be made much more logical
                // Normal animations stop after duration
                if (!repeat) {
                    action->type = AnimationTypeNone;
                    action->func(colors, count, FM_1, action->arg);
                    break;
                }

                // Repeat animations restart offset from the overlap
                dt = dt % duration;
            }

            action->func(colors, count, ratiofx(dt, duration), action->arg);

            break;
        }
    }
}

void pixels_tick(PixelsContext _context) {
    uint32_t now = ticks();

    _PixelsContext *context = (_PixelsContext*)_context;
    context->tick = now;

    color_ffxt colors[LED_COUNT];

    if (context->animatePixels) {
        computeColor(colors, 4, &context->actions[0], now);
    } else {
        for (int32_t pixel = 0; pixel < LED_COUNT; pixel++) {
            computeColor(&colors[pixel], 1, &context->actions[pixel], now);
        }
    }

    uint8_t *pixels = context->pixels;

    uint32_t offset = 0;
    for (int32_t pixel = 0; pixel < LED_COUNT; pixel++) {

        rgb24_ffxt rgb = ffx_color_rgb24(colors[pixel]);

        uint32_t r, g, b;
        if (rgb == 0) {
            r = g = b = 0;

        } else {
            g = (rgb >> 8) & 0xff;
            r = (rgb >> 16) & 0xff;
            b = rgb & 0xff;

            uint32_t t = g + r + b;

            // Normalize the brightness to the euqivalent of a
            // single LED at maximum brightness

            r = r * r / t;
            g = g * g / t;
            b = b * b / t;
        }

        // WS2812 output pixels in GRB format
        pixels[offset++] = g;
        pixels[offset++] = r;
        pixels[offset++] = b;
    }

    // Broadcast the pixel data (no transfer loop)
    rmt_transmit_config_t tx_config = { .loop_count = 0 };
    ESP_ERROR_CHECK(rmt_transmit(context->channel, context->encoder, pixels, LED_COUNT * 3, &tx_config));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(context->channel, portMAX_DELAY));

    // @TODO: If necessary, move the pixels into the Context (so they are
    //        are safe from stack reclaimation), and do the wait_all at
    //        the top of this function to ensure the previous tx is complete.
    //        This allows the tx to be completed async while other operations
    //        within the IO task execute.
    //        Quick tests showed that it is 0 anyways though
}

void pixels_free(PixelsContext _context) {
    _PixelsContext *context = (_PixelsContext*)_context;

    if (context->encoder) {
        rmt_led_strip_encoder_t *led_encoder = __containerof(context->encoder, rmt_led_strip_encoder_t, base);
        if (led_encoder->bytes_encoder) {
            rmt_del_encoder(led_encoder->bytes_encoder);
        }
        if (led_encoder->copy_encoder) {
            rmt_del_encoder(led_encoder->copy_encoder);
        }
        free(led_encoder);
    }

    free(context->pixels);
    free(context->actions);
    free(context->colors);

    free(_context);
}

void pixels_setPixel(PixelsContext _context, uint32_t pixel, color_ffxt color) {
    _PixelsContext *context = (_PixelsContext*)_context;

    if (pixel >= context->pixelCount) { return; }

    context->animatePixels = 0;

    context->colors[pixel] = color;

    context->actions[pixel].func = NULL;
    context->actions[pixel].type = AnimationTypeStatic;
    context->actions[pixel].arg = (void*)color;
}

void pixels_animatePixel(PixelsContext _context, uint32_t pixel,
  PixelAnimationFunc pixelFunc, uint32_t duration, uint32_t repeat,
  void *arg) {

    _PixelsContext *context = (_PixelsContext*)_context;

    if (pixel >= context->pixelCount) { return; }

    context->animatePixels = 0;

    context->actions[pixel].func = pixelFunc;
    context->actions[pixel].arg = arg;

    context->actions[pixel].duration = duration;
    context->actions[pixel].startTime = context->tick;

    context->actions[pixel].type = repeat ? AnimationTypeRepeat: AnimationTypeNormal;
}
