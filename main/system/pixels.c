#include "pixels.h"

#include <string.h>

#include "esp_check.h"

#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"

#define LED_COUNT     (1)
#define MAX_COLORS    (16)

typedef struct ColorRamp {
    uint32_t colors[MAX_COLORS];
    uint32_t count;
} ColorRamp;

typedef struct Animation {
    int32_t startTime;
    uint32_t duration;
    uint32_t repeat;
} Animation;

typedef struct _PixelsContext {
    ColorRamp colorRamp[LED_COUNT];
    Animation animation[LED_COUNT];
    uint32_t color[LED_COUNT];
} _PixelsContext;


typedef struct {
    uint32_t resolution;
} led_strip_encoder_config_t;

#define RMT_LED_STRIP_RESOLUTION_HZ 10000000 // 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define RMT_LED_STRIP_GPIO_NUM      9

#define EXAMPLE_LED_NUMBERS         1
#define EXAMPLE_CHASE_SPEED_MS      10

static uint8_t led_strip_pixels[EXAMPLE_LED_NUMBERS * 3];

/*

static const char *TAG = "led_encoder";

typedef struct {
    rmt_encoder_t base;
    rmt_encoder_t *bytes_encoder;
    rmt_encoder_t *copy_encoder;
    int state;
    rmt_symbol_word_t reset_code;
} rmt_led_strip_encoder_t;

static size_t rmt_encode_led_strip(rmt_encoder_t *encoder, rmt_channel_handle_t channel, const void *primary_data, size_t data_size, rmt_encode_state_t *ret_state)
{
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    rmt_encoder_handle_t bytes_encoder = led_encoder->bytes_encoder;
    rmt_encoder_handle_t copy_encoder = led_encoder->copy_encoder;
    rmt_encode_state_t session_state = RMT_ENCODING_RESET;
    rmt_encode_state_t state = RMT_ENCODING_RESET;
    size_t encoded_symbols = 0;
    switch (led_encoder->state) {
    case 0: // send RGB data
        encoded_symbols += bytes_encoder->encode(bytes_encoder, channel, primary_data, data_size, &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            led_encoder->state = 1; // switch to next state when current encoding session finished
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            state |= RMT_ENCODING_MEM_FULL;
            goto out; // yield if there's no free space for encoding artifacts
        }
    // fall-through
    case 1: // send reset code
        encoded_symbols += copy_encoder->encode(copy_encoder, channel, &led_encoder->reset_code,
                                                sizeof(led_encoder->reset_code), &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            led_encoder->state = RMT_ENCODING_RESET; // back to the initial encoding session
            state |= RMT_ENCODING_COMPLETE;
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            state |= RMT_ENCODING_MEM_FULL;
            goto out; // yield if there's no free space for encoding artifacts
        }
    }
out:
    *ret_state = state;
    return encoded_symbols;
}

static esp_err_t rmt_del_led_strip_encoder(rmt_encoder_t *encoder)
{
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    rmt_del_encoder(led_encoder->bytes_encoder);
    rmt_del_encoder(led_encoder->copy_encoder);
    free(led_encoder);
    return ESP_OK;
}

static esp_err_t rmt_led_strip_encoder_reset(rmt_encoder_t *encoder)
{
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    rmt_encoder_reset(led_encoder->bytes_encoder);
    rmt_encoder_reset(led_encoder->copy_encoder);
    led_encoder->state = RMT_ENCODING_RESET;
    return ESP_OK;
}

esp_err_t rmt_new_led_strip_encoder(const led_strip_encoder_config_t *config, rmt_encoder_handle_t *ret_encoder)
{
    esp_err_t ret = ESP_OK;
    rmt_led_strip_encoder_t *led_encoder = NULL;
    ESP_GOTO_ON_FALSE(config && ret_encoder, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");
    led_encoder = calloc(1, sizeof(rmt_led_strip_encoder_t));
    ESP_GOTO_ON_FALSE(led_encoder, ESP_ERR_NO_MEM, err, TAG, "no mem for led strip encoder");
    led_encoder->base.encode = rmt_encode_led_strip;
    led_encoder->base.del = rmt_del_led_strip_encoder;
    led_encoder->base.reset = rmt_led_strip_encoder_reset;
    // different led strip might have its own timing requirements, following parameter is for WS2812
    rmt_bytes_encoder_config_t bytes_encoder_config = {
        .bit0 = {
            .level0 = 1,
            .duration0 = 0.3 * config->resolution / 1000000, // T0H=0.3us
            .level1 = 0,
            .duration1 = 0.9 * config->resolution / 1000000, // T0L=0.9us
        },
        .bit1 = {
            .level0 = 1,
            .duration0 = 0.9 * config->resolution / 1000000, // T1H=0.9us
            .level1 = 0,
            .duration1 = 0.3 * config->resolution / 1000000, // T1L=0.3us
        },
        .flags.msb_first = 1 // WS2812 transfer bit order: G7...G0R7...R0B7...B0
    };
    ESP_GOTO_ON_ERROR(rmt_new_bytes_encoder(&bytes_encoder_config, &led_encoder->bytes_encoder), err, TAG, "create bytes encoder failed");
    rmt_copy_encoder_config_t copy_encoder_config = {};
    ESP_GOTO_ON_ERROR(rmt_new_copy_encoder(&copy_encoder_config, &led_encoder->copy_encoder), err, TAG, "create copy encoder failed");

    uint32_t reset_ticks = config->resolution / 1000000 * 50 / 2; // reset code duration defaults to 50us
    led_encoder->reset_code = (rmt_symbol_word_t) {
        .level0 = 0,
        .duration0 = reset_ticks,
        .level1 = 0,
        .duration1 = reset_ticks,
    };
    *ret_encoder = &led_encoder->base;
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
*/

#define PIXELS_COUNT      (10)

/*
typedef struct _PixelsContext {
    uint32_t pixels;

    uint32_t samples[PIXELS_COUNT];
} _PixelsContext;
*/

PixelsContext pixels_init() {

    _PixelsContext *context = malloc(sizeof(_PixelsContext));
    memset(context, 0, sizeof(_PixelsContext));

    // // Setup the GPIO input pins
    // for (uint32_t i = 0; i < 32; i++) {
    //     if ((keys & (1 << i)) == 0) { continue; } 
    //     gpio_reset_pin(i);
    //     gpio_set_direction(i, GPIO_MODE_INPUT);
    //     gpio_pullup_en(i);
    // }

    return context;
}

void pixels_free(PixelsContext context) {
    free(context);
}

void pixels_setRGB(PixelsContext _context, uint32_t index, uint32_t rgb) {
    if (index >= LED_COUNT) { return; }

    _PixelsContext *context = (_PixelsContext*)_context;
    context->animation[index].startTime = -1;
    context->color[index] = rgb; 
}

/*
void pixels_animateRGB(PixelsContext _context, uint32_t index, uint32_t* colorRamp, uint32_t colorCount, uint32_t duration, uint32_t repeat) {
    if (index >= LED_COUNT) { return; }

    _PixelContext *context = (_PixelsContext*)_context;

    // Set up the color ramp
    if (colorCount > MAX_COLORS) { colorCount = MAX_COLORS; }
    if (colorCount) {
        context->colorRamp.count = colorCount;
        for (let i = 0; i < colorCount; i++) {
            context->colorRamp[index].colors[i] = colorRamp[i];
        }

        if (colorCount > 1)
            // Set up the animation
            context->animations[index].startTime = millis();
            context->animations[index].duration = duration;
            context->animations[index].repeat = repeat;
        }  

    } else {
        context->colorRamp[index].count = 1;
        context->colorRamp[index].colors[0] = 0;
    }

    context->color = context->colorRamp[index].colors[0];
}
*/
