
#include <stdlib.h>
#include <string.h>

#include <driver/gpio.h>
#include <hal/gpio_ll.h>

#include "./keypad.h"


typedef struct _KeypadContext {
    uint32_t keys;

    uint32_t count;
    uint32_t samples[KEYPAD_SAMPLE_COUNT];

    uint32_t previousLatch;
    uint32_t latch;
} _KeypadContext;


KeypadContext keypad_init(uint32_t keys) {

    _KeypadContext *context = malloc(sizeof(_KeypadContext));
    memset(context, 0, sizeof(_KeypadContext));

    context->keys = keys;

    // Setup the GPIO input pins
    for (uint32_t i = 0; i < 32; i++) {
        if ((keys & (1 << i)) == 0) { continue; }
        gpio_reset_pin(i);
        gpio_set_direction(i, GPIO_MODE_INPUT);
        gpio_pullup_en(i);
    }

    return context;
}

void keypad_free(KeypadContext context) {
    free(context);
}

void keypad_sample(KeypadContext _context) {
    _KeypadContext *context = _context;

    context->samples[context->count % KEYPAD_SAMPLE_COUNT] = ~(REG_READ(GPIO_IN_REG));
    context->count++;
}

void keypad_latch(KeypadContext _context) {
    _KeypadContext *context = _context;

    uint32_t samples = context->count;
    if (samples > KEYPAD_SAMPLE_COUNT) { samples = KEYPAD_SAMPLE_COUNT; }

    uint32_t latch = 0;
    for (uint32_t i = 0; i < 32; i++) {
        uint32_t mask = (1 << i);
        if ((context->keys & mask) == 0) { continue; }

        uint32_t count = 0;
        for (uint32_t s = 0; s < samples; s++) {
            if (context->samples[s] & mask) { count++; }
        }

        if (count * 2 > samples) { latch |= mask; }
    }

    context->count = 0;

    context->previousLatch = context->latch;
    context->latch = latch;
}

uint32_t keypad_didChange(KeypadContext _context, uint32_t keys) {
    _KeypadContext *context = _context;
    keys &= context->keys;
    return (context->previousLatch ^ context->latch) & keys;
}

uint32_t keypad_read(KeypadContext _context) {
    _KeypadContext *context = _context;
    return (context->latch & context->keys);
}

uint32_t keypad_isDown(KeypadContext _context, uint32_t keys) {
    _KeypadContext *context = _context;
    keys &= context->keys;
    return (context->latch & keys) == keys;
}
