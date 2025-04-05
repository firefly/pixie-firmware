#ifndef __UTILS_H__
#define __UTILS_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>

/////////////////////////////
// Timer functions

// How many ticks since the system stsarted
uint32_t ticks();

// Delay %duration% ms
void delay(uint32_t duration);

char* taskName();

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __UTILS_H__ */
