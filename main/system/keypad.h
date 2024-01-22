
#ifndef __KEYPAD_H__
#define __KEYPAD_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>


#define KEYPAD_SAMPLE_COUNT    (10)


// Keypad Context Object (opaque; do not inspect or rely on internals)
typedef void* KeypadContext;

KeypadContext keypad_init(uint32_t keys);
void keypad_free(KeypadContext context);

// Add the sample
void keypad_sample(KeypadContext context);

// Latch the inputs from the samples
void keypad_latch(KeypadContext context);

// Returns the bits set for the keys that changed since the last latch
uint32_t keypad_didChange(KeypadContext context, uint32_t keys);

// Return non-zero if all keys are down. 
uint32_t keypad_isDown(KeypadContext context, uint32_t keys);

uint32_t keypad_read(KeypadContext context);


//uint32_t keypad_readHoldTime(uint32_t keys);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __KEYPAD_H__ */
