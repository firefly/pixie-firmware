
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "firefly-demos.h"
#include "firefly-hollows.h"

#include "utils.h"

#include "image-data.h"
#include "panels.h"


// This is populated with a signature after signing
//__attribute__((used)) const char code_signature[] =
//  "<FFX-SIGNATURE>xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx</FFX-SIGNATURE>";


// This is added from the CMakeLists.txt
#ifndef GIT_COMMIT
#define GIT_COMMIT ("unknown")
#endif

#define PIXIE_FW_VERSION    FFX_VERSION(0, 2, 0)


static int initPanel(void *arg) {
    return pushPanelMenu();
}

void app_main() {
    vTaskSetApplicationTaskTag( NULL, (void*)NULL);

    FFX_LOG("GIT Commit: %s", GIT_COMMIT);

    images_initRotated();

    ffx_init(PIXIE_FW_VERSION, ffx_demo_backgroundPixies, initPanel, NULL);

    while (1) {
        ffx_dumpStats();
        delay(60000);
    }
}





