#!/bin/zsh

node ../firefly-scene/tools/lib/test-cli --rgb assets/background.png --tag background > main/images/image-background.h
node ../firefly-scene/tools/lib/test-cli --rgba assets/title.png --tag title > main/images/image-title.h
node ../firefly-scene/tools/lib/test-cli --rgba assets/pixie.png --tag pixie > main/images/image-pixie.h
node ../firefly-scene/tools/lib/test-cli --rgba assets/arrow.png --tag arrow > main/images/image-arrow.h
node ../firefly-scene/tools/lib/test-cli --rgba assets/alien-1.png --tag alien1 > main/images/image-alien-1.h
node ../firefly-scene/tools/lib/test-cli --rgba assets/alien-2.png --tag alien2 > main/images/image-alien-2.h
node ../firefly-scene/tools/lib/test-cli --rgba assets/ship.png --tag ship > main/images/image-ship.h
node ../firefly-scene/tools/lib/test-cli --rgb assets/text-dead.png --tag textdead > main/images/image-text-dead.h
node ../firefly-scene/tools/lib/test-cli --rgb assets/text-win.png --tag textwin > main/images/image-text-win.h
node ../firefly-scene/tools/lib/test-cli --rgb assets/text-hold.png --tag texthold > main/images/image-text-hold.h
