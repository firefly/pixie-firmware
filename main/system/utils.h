
#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/////////////////////////////
// Timer functions

// How many ticks since the system stsarted
uint32_t ticks();

// Delay %duration% ms
void delay(uint32_t duration);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __UTILS_H__ */
