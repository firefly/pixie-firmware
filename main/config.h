#ifndef __CONFIG_H__
#define __CONFIG_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define BOARD_REV         (5)


#if BOARD_REV == 2
// The rev.2 board used the CS0 pin for the display. The rev.3
// board and beyond tie it to ground to save the pin (as the
// display is always enabled) which allow re-arranging the pins
// a bit to aid in cleaner trace routing.

#define DISPLAY_BUS        (DisplaySpiBus2_cs)
#define PIN_DISPLAY_DC     (0)
#define PIN_DISPLAY_RESET  (5)

#define PIN_BUTTON_1       (1 << 2)
#define PIN_BUTTON_2       (1 << 3)
#define PIN_BUTTON_3       (1 << 4)
#define PIN_BUTTON_4       (1 << 1)


#elif BOARD_REV == 4
// The rev.4 board added an addressable LED

#define DISPLAY_BUS        (FfxDisplaySpiBus2)
#define PIN_DISPLAY_DC     (4)
#define PIN_DISPLAY_RESET  (5)

#define PIN_BUTTON_1       (1 << 10)
#define PIN_BUTTON_2       (1 << 8)
#define PIN_BUTTON_3       (1 << 3)
#define PIN_BUTTON_4       (1 << 2)

#define PIN_PIXELS         (9)
#define PIXEL_COUNT        (1)

#elif BOARD_REV == 5
// The rev.5 board added an addressable LED next to each button

#define DISPLAY_BUS        (FfxDisplaySpiBus2)
#define PIN_DISPLAY_DC     (4)
#define PIN_DISPLAY_RESET  (5)


#define PIN_BUTTON_1       (1 << 10)
#define PIN_BUTTON_2       (1 << 8)
#define PIN_BUTTON_3       (1 << 3)
#define PIN_BUTTON_4       (1 << 2)

#define PIN_PIXELS         (9)
#define PIXEL_COUNT        (4)

#else

#error "Unknown board revision"

#endif

// Pixie mapping

#define KeyButton1        (KeyCancel)
#define KeyButton2        (KeyOk)
#define KeyButton3        (KeyNorth)
#define KeyButton4        (KeySouth)


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CONFIG_H__ */
