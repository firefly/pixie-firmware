
#ifndef __SPRINGBOARD_H__
#define __SPRINGBOARD_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>


// Springboard Context Object (opaque; do not inspect or rely on internals)
typedef void* SpringboardContext;

SpringboardContext springboard_init();
void springboard_free(SpringboardContext context);

void springboard_tick(SpringboardContext context);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __SPRINGBOARD_H__ */
