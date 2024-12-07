
#ifndef __KEYPAD_H__
#define __KEYPAD_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "panel.h"


#define KEYPAD_SAMPLE_COUNT    (10)

// Keypad Context Object (opaque; do not inspect or rely on internals)
typedef void* KeypadContext;

extern size_t KeypadContextSize;

KeypadContext keypad_init(void *memory);
KeypadContext keypad_alloc();
void keypad_free(KeypadContext context);

void keypad_clone(KeypadContext *dst, KeypadContext *src);

// Add the sample
void keypad_sample(KeypadContext context);

// Latch the inputs from the samples
void keypad_latch(KeypadContext context);

// Returns the bits set for the keys that changed since the last latch
Keys keypad_didChange(KeypadContext context, Keys keys);

// Return non-zero if all keys are down.
bool keypad_isDown(KeypadContext context, Keys keys);

Keys keypad_read(KeypadContext context);


//uint32_t keypad_readHoldTime(uint32_t keys);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __KEYPAD_H__ */
