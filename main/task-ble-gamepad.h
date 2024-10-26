#ifndef __TASK_BLE_GAMEPAD_H__
#define __TASK_BLE_GAMEPAD_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>


// The USB decriptor is from the 8Bitdo SN30 Pro, which identifies itself
// as a Sony Playstation DualShock 4 controller (see the VenderId and
// ProductId below). This device was choosen because it works in Chrome
// the vibrationActuator API.
// VendorID+ProductId: https://www.the-sz.com/products/usbid/index.php?v=0x054C&p=0x05C4&n=
// USB Descriptor Parser: https://eleccelerator.com/usbdescreqparser/
uint8_t hidReportMap[] = {
/*
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x05,        // Usage (Game Pad)
    0xA1, 0x01,        // Collection (Application)

    0x85, 0x01,        //   Report ID (1)
                       // ID: 0x01
    0x09, 0x30,        //   Usage (X)
    0x09, 0x31,        //   Usage (Y)
    0x09, 0x32,        //   Usage (Z)
    0x09, 0x35,        //   Usage (Rz)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x04,        //   Report Count (4)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
                       // IN: 4x 8

    0x09, 0x39,        //   Usage (Hat switch)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x07,        //   Logical Maximum (7)
    0x75, 0x04,        //   Report Size (4)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x42,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
                       // IN: 1x 4

    0x05, 0x09,        //   Usage Page (Button)
    0x19, 0x01,        //   Usage Minimum (0x01)
    0x29, 0x0E,        //   Usage Maximum (0x0E)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x0E,        //   Report Count (14)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
                       // IN: 14x 1

    0x75, 0x06,        //   Report Size (6)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x01,        //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
                       // IN: 6x 1

    0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls)
    0x09, 0x33,        //   Usage (Rx)
    0x09, 0x34,        //   Usage (Ry)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x02,        //   Report Count (2)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
                       // IN: 2x 8

    0x06, 0x04, 0xFF,  //   Usage Page (Vendor Defined 0xFF04)
    0x85, 0x02,        //   Report ID (2)
                       // ID: 2
    0x09, 0x24,        //   Usage (0x24)
    0x95, 0x24,        //   Report Count (36)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
                       // F: 36x 8
    0x85, 0xA3,        //   Report ID (-93)
    0x09, 0x25,        //   Usage (0x25)
    0x95, 0x30,        //   Report Count (48)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x05,        //   Report ID (5)
    0x09, 0x26,        //   Usage (0x26)
    0x95, 0x28,        //   Report Count (40)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x06,        //   Report ID (6)
    0x09, 0x27,        //   Usage (0x27)
    0x95, 0x34,        //   Report Count (52)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x07,        //   Report ID (7)
    0x09, 0x28,        //   Usage (0x28)
    0x95, 0x30,        //   Report Count (48)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x08,        //   Report ID (8)
    0x09, 0x29,        //   Usage (0x29)
    0x95, 0x2F,        //   Report Count (47)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x09,        //   Report ID (9)
    0x09, 0x2A,        //   Usage (0x2A)
    0x95, 0x13,        //   Report Count (19)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x06, 0x03, 0xFF,  //   Usage Page (Vendor Defined 0xFF03)
    0x85, 0x03,        //   Report ID (3)
    0x09, 0x21,        //   Usage (0x21)
    0x95, 0x26,        //   Report Count (38)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x04,        //   Report ID (4)
    0x09, 0x22,        //   Usage (0x22)
    0x95, 0x2E,        //   Report Count (46)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xF0,        //   Report ID (-16)
    0x09, 0x47,        //   Usage (0x47)
    0x95, 0x3F,        //   Report Count (63)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xF1,        //   Report ID (-15)
    0x09, 0x48,        //   Usage (0x48)
    0x95, 0x3F,        //   Report Count (63)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xF2,        //   Report ID (-14)
    0x09, 0x49,        //   Usage (0x49)
    0x95, 0x0F,        //   Report Count (15)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined 0xFF00)
    0x85, 0x11,        //   Report ID (17)
                       // ID: 0x11
    0x09, 0x20,        //   Usage (0x20)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x4D,        //   Report Count (77)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
                       // IN: 77x 8

    0x09, 0x21,        //   Usage (0x21)
    0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
                       // OUT: 77x 8

    0x85, 0x12,        //   Report ID (18)
                       // ID: 0x12
    0x09, 0x22,        //   Usage (0x22)
    0x95, 0x8D,        //   Report Count (-115)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
                       // IN: 141x 8 (assuming unsigned?)

    0x09, 0x23,        //   Usage (0x23)
    0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
                       // OUT: 141x 8 (assuming unsigned?)

    0x85, 0x13,        //   Report ID (19)
                       // ID: 0x13
    0x09, 0x24,        //   Usage (0x24)
    0x95, 0xCD,        //   Report Count (-51)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
                       // IN: 205x 8 (assuming unsigned?)

    0x09, 0x25,        //   Usage (0x25)
    0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
                       // OUT: 205x 8 (assuming unsigned?)

    0x85, 0x14,        //   Report ID (20)
                       // ID: 0x14
    0x09, 0x26,        //   Usage (0x26)
    0x96, 0x0D, 0x01,  //   Report Count (269)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
                       // IN: 269x 8

    0x09, 0x27,        //   Usage (0x27)
    0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
                       // OUT: 269x 8

    0x85, 0x15,        //   Report ID (21)
                       // ID: 0x15
    0x09, 0x28,        //   Usage (0x28)
    0x96, 0x4D, 0x01,  //   Report Count (333)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
                       // IN: 333x 8

    0x09, 0x29,        //   Usage (0x29)
    0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
                       // OUT: 333x 8

    0x85, 0x16,        //   Report ID (22)
                       // ID: 0x16
    0x09, 0x2A,        //   Usage (0x2A)
    0x96, 0x8D, 0x01,  //   Report Count (397)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
                       // IN: 397x 8

    0x09, 0x2B,        //   Usage (0x2B)
    0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
                       // OUT: 397x 8

    0x85, 0x17,        //   Report ID (23)
                       // ID: 0x17
    0x09, 0x2C,        //   Usage (0x2C)
    0x96, 0xCD, 0x01,  //   Report Count (461)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
                       // IN: 461x 8

    0x09, 0x2D,        //   Usage (0x2D)
    0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
                       // OUT: 461x 8

    0x85, 0x18,        //   Report ID (24)
                       // ID: 0x18
    0x09, 0x2E,        //   Usage (0x2E)
    0x96, 0x0D, 0x02,  //   Report Count (525)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
                       // IN: 525x 8

    0x09, 0x2F,        //   Usage (0x2F)
    0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
                       // OUT: 525x 8

    0x85, 0x19,        //   Report ID (25)
                       // ID: 0x19
    0x09, 0x30,        //   Usage (0x30)
    0x96, 0x22, 0x02,  //   Report Count (546)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
                       // IN: 546x 8

    0x09, 0x31,        //   Usage (0x31)
    0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
                       // OUT: 546x 8

    0x06, 0x80, 0xFF,  //   Usage Page (Vendor Defined 0xFF80)
    0x85, 0x82,        //   Report ID (-126)
    0x09, 0x22,        //   Usage (0x22)
    0x95, 0x3F,        //   Report Count (63)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x83,        //   Report ID (-125)
    0x09, 0x23,        //   Usage (0x23)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x84,        //   Report ID (-124)
    0x09, 0x24,        //   Usage (0x24)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x90,        //   Report ID (-112)
    0x09, 0x30,        //   Usage (0x30)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x91,        //   Report ID (-111)
    0x09, 0x31,        //   Usage (0x31)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x92,        //   Report ID (-110)
    0x09, 0x32,        //   Usage (0x32)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x93,        //   Report ID (-109)
    0x09, 0x33,        //   Usage (0x33)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xA0,        //   Report ID (-96)
    0x09, 0x40,        //   Usage (0x40)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xA4,        //   Report ID (-92)
    0x09, 0x44,        //   Usage (0x44)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)


    0xC0,              // End Collection

    // 364 bytes
*/

    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x05,        // Usage (Game Pad)
    0xA1, 0x01,        // Collection (Application)

    0x85, 0x01,        //   Report ID (1)
                       // ID: 0x01
    0x09, 0x30,        //   Usage (X)
    0x09, 0x31,        //   Usage (Y)
    0x09, 0x32,        //   Usage (Z)
    0x09, 0x35,        //   Usage (Rz)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x04,        //   Report Count (4)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
                       // IN: 4x 8

    0x09, 0x39,        //   Usage (Hat switch)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x07,        //   Logical Maximum (7)
    0x75, 0x04,        //   Report Size (4)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x42,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
                       // IN: 1x 4

    0x05, 0x09,        //   Usage Page (Button)
    0x19, 0x01,        //   Usage Minimum (0x01)
    0x29, 0x0E,        //   Usage Maximum (0x0E)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x0E,        //   Report Count (14)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
                       // IN: 14x 1

    0x75, 0x06,        //   Report Size (6)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x01,        //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
                       // IN: 6x 1

    0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls)
    0x09, 0x33,        //   Usage (Rx)
    0x09, 0x34,        //   Usage (Ry)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x02,        //   Report Count (2)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
                       // IN: 2x 8

    0x06, 0x04, 0xFF,  //   Usage Page (Vendor Defined 0xFF04)
    0x85, 0x02,        //   Report ID (2)
                       // ID: 2
    0x09, 0x24,        //   Usage (0x24)
    0x95, 0x24,        //   Report Count (36)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
                       // F: 36x 8
    0x85, 0xA3,        //   Report ID (-93)
    0x09, 0x25,        //   Usage (0x25)
    0x95, 0x30,        //   Report Count (48)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x05,        //   Report ID (5)
    0x09, 0x26,        //   Usage (0x26)
    0x95, 0x28,        //   Report Count (40)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x06,        //   Report ID (6)
    0x09, 0x27,        //   Usage (0x27)
    0x95, 0x34,        //   Report Count (52)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x07,        //   Report ID (7)
    0x09, 0x28,        //   Usage (0x28)
    0x95, 0x30,        //   Report Count (48)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x08,        //   Report ID (8)
    0x09, 0x29,        //   Usage (0x29)
    0x95, 0x2F,        //   Report Count (47)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x09,        //   Report ID (9)
    0x09, 0x2A,        //   Usage (0x2A)
    0x95, 0x13,        //   Report Count (19)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x06, 0x03, 0xFF,  //   Usage Page (Vendor Defined 0xFF03)
    0x85, 0x03,        //   Report ID (3)
    0x09, 0x21,        //   Usage (0x21)
    0x95, 0x26,        //   Report Count (38)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x04,        //   Report ID (4)
    0x09, 0x22,        //   Usage (0x22)
    0x95, 0x2E,        //   Report Count (46)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xF0,        //   Report ID (-16)
    0x09, 0x47,        //   Usage (0x47)
    0x95, 0x3F,        //   Report Count (63)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xF1,        //   Report ID (-15)
    0x09, 0x48,        //   Usage (0x48)
    0x95, 0x3F,        //   Report Count (63)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xF2,        //   Report ID (-14)
    0x09, 0x49,        //   Usage (0x49)
    0x95, 0x0F,        //   Report Count (15)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined 0xFF00)
    0x85, 0x11,        //   Report ID (17)
                       // ID: 0x11
    0x09, 0x20,        //   Usage (0x20)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x4D,        //   Report Count (77)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
                       // IN: 77x 8

    0x09, 0x21,        //   Usage (0x21)
    0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
                       // OUT: 77x 8

    0x85, 0x12,        //   Report ID (18)
                       // ID: 0x12
    0x09, 0x22,        //   Usage (0x22)
    0x95, 0x8D,        //   Report Count (-115)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
                       // IN: 141x 8 (assuming unsigned?)

    0x09, 0x23,        //   Usage (0x23)
    0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
                       // OUT: 141x 8 (assuming unsigned?)

    0x85, 0x13,        //   Report ID (19)
                       // ID: 0x13
    0x09, 0x24,        //   Usage (0x24)
    0x95, 0xCD,        //   Report Count (-51)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
                       // IN: 205x 8 (assuming unsigned?)

    0x09, 0x25,        //   Usage (0x25)
    0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
                       // OUT: 205x 8 (assuming unsigned?)

    0x85, 0x14,        //   Report ID (20)
                       // ID: 0x14
    0x09, 0x26,        //   Usage (0x26)
    0x96, 0x0D, 0x01,  //   Report Count (269)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
                       // IN: 269x 8

    0x09, 0x27,        //   Usage (0x27)
    0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
                       // OUT: 269x 8

    0x85, 0x15,        //   Report ID (21)
                       // ID: 0x15
    0x09, 0x28,        //   Usage (0x28)
    0x96, 0x4D, 0x01,  //   Report Count (333)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
                       // IN: 333x 8

    0x09, 0x29,        //   Usage (0x29)
    0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
                       // OUT: 333x 8

    0x85, 0x16,        //   Report ID (22)
                       // ID: 0x16
    0x09, 0x2A,        //   Usage (0x2A)
    0x96, 0x8D, 0x01,  //   Report Count (397)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
                       // IN: 397x 8

    0x09, 0x2B,        //   Usage (0x2B)
    0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
                       // OUT: 397x 8

    0x85, 0x17,        //   Report ID (23)
                       // ID: 0x17
    0x09, 0x2C,        //   Usage (0x2C)
    0x96, 0xCD, 0x01,  //   Report Count (461)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
                       // IN: 461x 8

    0x09, 0x2D,        //   Usage (0x2D)
    0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
                       // OUT: 461x 8

    0x85, 0x18,        //   Report ID (24)
                       // ID: 0x18
    0x09, 0x2E,        //   Usage (0x2E)
    0x96, 0x0D, 0x02,  //   Report Count (525)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
                       // IN: 525x 8

    0x09, 0x2F,        //   Usage (0x2F)
    0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
                       // OUT: 525x 8

    0x85, 0x19,        //   Report ID (25)
                       // ID: 0x19
    0x09, 0x30,        //   Usage (0x30)
    0x96, 0x22, 0x02,  //   Report Count (546)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
                       // IN: 546x 8

    0x09, 0x31,        //   Usage (0x31)
    0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
                       // OUT: 546x 8

    0x06, 0x80, 0xFF,  //   Usage Page (Vendor Defined 0xFF80)
    0x85, 0x82,        //   Report ID (-126)
    0x09, 0x22,        //   Usage (0x22)
    0x95, 0x3F,        //   Report Count (63)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x83,        //   Report ID (-125)
    0x09, 0x23,        //   Usage (0x23)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x84,        //   Report ID (-124)
    0x09, 0x24,        //   Usage (0x24)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x90,        //   Report ID (-112)
    0x09, 0x30,        //   Usage (0x30)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x91,        //   Report ID (-111)
    0x09, 0x31,        //   Usage (0x31)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x92,        //   Report ID (-110)
    0x09, 0x32,        //   Usage (0x32)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x93,        //   Report ID (-109)
    0x09, 0x33,        //   Usage (0x33)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xA0,        //   Report ID (-96)
    0x09, 0x40,        //   Usage (0x40)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xA4,        //   Report ID (-92)
    0x09, 0x44,        //   Usage (0x44)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)


    0xC0,              // End Collection

    // 364 bytes

};


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __TASK_BLE_GAMEPAD_H__ */
