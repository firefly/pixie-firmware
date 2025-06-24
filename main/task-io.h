#ifndef __TASK_IO_H__
#define __TASK_IO_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>
#include "firefly-display.h"

// Custom renderer function type
typedef void (*CustomRenderFunc)(uint8_t *buffer, uint32_t y0, void *context);

void taskIoFunc(void* pvParameter);

/**
 * Set a custom renderer to override the default scene rendering
 * @param renderFunc Custom render function, or NULL to use default scene rendering
 * @param context Context to pass to the render function
 */
void taskIo_setCustomRenderer(CustomRenderFunc renderFunc, void *context);



#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __TASK_IO_H__ */
