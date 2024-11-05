
#include <stdlib.h>
#include <string.h>

#include <driver/gpio.h>
#include <hal/gpio_ll.h>

#include "../config.h"

#include "./keypad.h"

typedef struct _KeypadContext {
    Keys keys;

    uint32_t pins;

    uint32_t count;
    uint32_t samples[KEYPAD_SAMPLE_COUNT];

    uint32_t previousLatch;
    uint32_t latch;
} _KeypadContext;

size_t KeypadContextSize = sizeof(_KeypadContext);

static Keys remapKeypadPins(uint32_t value) {
    Keys keys = 0;
    if (value & PIN_BUTTON_1) { keys |= KeyCancel; }
    if (value & PIN_BUTTON_2) { keys |= KeyOk; }
    if (value & PIN_BUTTON_3) { keys |= KeyNorth; }
    if (value & PIN_BUTTON_4) { keys |= KeySouth; }
    return keys;
}

KeypadContext keypad_init(void *data) {
    _KeypadContext *context = data;
    memset(context, 0, KeypadContextSize);

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

    return context;
}

KeypadContext keypad_alloc() {
    return keypad_init(malloc(KeypadContextSize));
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

Keys keypad_didChange(KeypadContext _context, Keys keys) {
    _KeypadContext *context = _context;
    keys &= context->keys;
    return (context->previousLatch ^ context->latch) & keys;
}

Keys keypad_read(KeypadContext _context) {
    _KeypadContext *context = _context;
    return (context->latch & context->keys);
}

bool keypad_isDown(KeypadContext _context, Keys keys) {
    _KeypadContext *context = _context;
    keys &= context->keys;
    return ((context->latch & keys) == keys) ? true: false;
}
