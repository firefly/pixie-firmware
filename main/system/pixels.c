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
} AnimationType;

typedef struct ColorRamp {
    color_ffxt colors[MAX_COLORS];
    uint32_t count;
} ColorRamp;

typedef struct _PixelsContext {
    rmt_channel_handle_t channel;
    rmt_encoder_handle_t encoder;

    uint8_t pixels[LED_COUNT * 3];

    ColorRamp colorRamps[LED_COUNT];

    uint32_t startTime[LED_COUNT];
    uint32_t duration[LED_COUNT];
    AnimationType type[LED_COUNT];
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


PixelsContext pixels_init(uint32_t pin) {

    _PixelsContext *context = malloc(sizeof(_PixelsContext));
    memset(context, 0, sizeof(_PixelsContext));

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

// linear-interpolation used for color bytes in tick
//uint8_t lerp(int32_t a, int32_t b, int32_t top, int32_t bottom) {
//    return ((top * a) / bottom) + (((bottom - top) * b) / bottom);
//}

void pixels_tick(PixelsContext _context) {
    _PixelsContext *context = (_PixelsContext*)_context;

    uint32_t offset = 0;
    for (int32_t pixel = LED_COUNT - 1; pixel >= 0; pixel--) {

        rgb24_ffxt rgb = 0;
        uint32_t repeat = 0;
        switch(context->type[pixel]) {
            case AnimationTypeNone:
                break;
            case AnimationTypeStatic:
                rgb = ffx_color_rgb24(context->colorRamps[pixel].colors[0]);
                break;
            case AnimationTypeRepeat:
                repeat = 1;
                // ...falls through
            case AnimationTypeNormal: {
                uint32_t dt = ticks() - context->startTime[pixel];
                uint32_t duration = context->duration[pixel];
                if (dt > duration) {
                    // Normal animations stop after duration
                    if (!repeat) {
                        context->type[pixel] = AnimationTypeNone;
                        break;
                    }

                    // Repeat animations restart offset from the overlap
                    dt = dt % duration;
                }

                uint32_t count = context->colorRamps[pixel].count;
                uint32_t index = dt * (count - 1) / duration;
                color_ffxt c0 = context->colorRamps[pixel].colors[index];
                color_ffxt c1 = context->colorRamps[pixel].colors[(index + 1) % count];

                // @TODO: Lots of optimizations here. :)
                int32_t chunk = duration / (count - 1);

                color_ffxt color = ffx_color_lerpRatio(c0, c1, dt - index * chunk, chunk);

                rgb = ffx_color_rgb24(color);

                break;
            }
        }

        uint32_t r, g, b;
        if (rgb == 0) {
            r = g = b = 0;

        } else {
            g = (rgb >> 8) & 0xff;
            r = (rgb >> 16) & 0xff;
            b = rgb & 0xff;

            uint32_t t = g + r + b;

            r = r * r / t;
            g = g * g / t;
            b = b * b / t;
        }

        // WS2812 output pixels in GRB format
        context->pixels[offset++] = g;
        context->pixels[offset++] = r;
        context->pixels[offset++] = b;
    }

    // Broadcast the pixel data
    rmt_transmit_config_t tx_config = {
        .loop_count = 0, // no transfer loop
    };
    ESP_ERROR_CHECK(rmt_transmit(context->channel, context->encoder, context->pixels, LED_COUNT * 3, &tx_config));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(context->channel, portMAX_DELAY));
}

void pixels_postTick() {
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

    free(_context);
}

void pixels_setColor(PixelsContext _context, uint32_t index, color_ffxt color) {
    if (index >= LED_COUNT) { return; }

    _PixelsContext *context = (_PixelsContext*)_context;
    context->colorRamps[index].colors[0] = color;
    context->colorRamps[index].count = 1;
    context->type[index] = AnimationTypeStatic;
}

void pixels_animateColor(PixelsContext _context, uint32_t index, color_ffxt* colorRamp, uint32_t colorCount, uint32_t duration, uint32_t repeat) {
    if (index >= LED_COUNT) { return; }

    _PixelsContext *context = (_PixelsContext*)_context;

    // Set up the color ramp
    if (colorCount > MAX_COLORS) { colorCount = MAX_COLORS; }

    context->colorRamps[index].count = colorCount;
    for (uint32_t i = 0; i < colorCount; i++) {
        context->colorRamps[index].colors[i] = colorRamp[i];
    }

    context->startTime[index] = ticks();
    context->duration[index] = duration;
    context->type[index] = repeat ? AnimationTypeRepeat: AnimationTypeNormal;
}
