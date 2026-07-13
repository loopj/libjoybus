#include <string.h>

#include "tusb.h"

// Present as a Sony DualShock 4 (v1). macOS recognizes this VID/PID and drives
// the controller natively through its GameController framework, including rumble
// via output report 0x05. See main.c for how the DS4 reports are built/decoded.
#define VENDOR_ID   0x054C
#define PRODUCT_ID  0x05C4

// Interrupt endpoints. The DS4 uses both an IN endpoint (input + GET_REPORT
// responses) and an OUT endpoint (rumble/LED output reports).
#define EP_OUT      0x03
#define EP_IN       0x84

// Device Descriptor
static const tusb_desc_device_t device_desc = {
  .bLength            = sizeof(tusb_desc_device_t),
  .bDescriptorType    = TUSB_DESC_DEVICE,
  .bcdUSB             = 0x0200,
  .bDeviceClass       = 0x00,
  .bDeviceSubClass    = 0x00,
  .bDeviceProtocol    = 0x00,
  .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
  .idVendor           = VENDOR_ID,
  .idProduct          = PRODUCT_ID,
  .bcdDevice          = 0x0100,
  .iManufacturer      = 0x01,
  .iProduct           = 0x02,
  .iSerialNumber      = 0,
  .bNumConfigurations = 0x01,
};

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const *tud_descriptor_device_cb(void)
{
  return (uint8_t const *)&device_desc;
}

// HID Report Descriptor
//
// This is the DualShock 4 (USB) report descriptor, reproduced verbatim so that
// macOS binds its native DS4 support: input report 0x01 (controller state),
// output report 0x05 (rumble/LED), and the DS4 feature reports. The trailing
// 0xF0-0xF3 vendor collection is the PS4 console authentication interface; it is
// harmless on macOS (which never exercises it) and is kept for fidelity.
static const uint8_t hid_report_desc[] = {
  0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
  0x09, 0x05,        // Usage (Game Pad)
  0xA1, 0x01,        // Collection (Application)
  0x85, 0x01,        //   Report ID (1)
  0x09, 0x30,        //   Usage (X)
  0x09, 0x31,        //   Usage (Y)
  0x09, 0x32,        //   Usage (Z)
  0x09, 0x35,        //   Usage (Rz)
  0x15, 0x00,        //   Logical Minimum (0)
  0x26, 0xFF, 0x00,  //   Logical Maximum (255)
  0x75, 0x08,        //   Report Size (8)
  0x95, 0x04,        //   Report Count (4)
  0x81, 0x02,        //   Input (Data,Var,Abs)
  0x09, 0x39,        //   Usage (Hat switch)
  0x15, 0x00,        //   Logical Minimum (0)
  0x25, 0x07,        //   Logical Maximum (7)
  0x35, 0x00,        //   Physical Minimum (0)
  0x46, 0x3B, 0x01,  //   Physical Maximum (315)
  0x65, 0x14,        //   Unit (Eng Rot: Degrees)
  0x75, 0x04,        //   Report Size (4)
  0x95, 0x01,        //   Report Count (1)
  0x81, 0x42,        //   Input (Data,Var,Abs,Null State)
  0x65, 0x00,        //   Unit (None)
  0x05, 0x09,        //   Usage Page (Button)
  0x19, 0x01,        //   Usage Minimum (0x01)
  0x29, 0x0E,        //   Usage Maximum (0x0E)
  0x15, 0x00,        //   Logical Minimum (0)
  0x25, 0x01,        //   Logical Maximum (1)
  0x75, 0x01,        //   Report Size (1)
  0x95, 0x0E,        //   Report Count (14)
  0x81, 0x02,        //   Input (Data,Var,Abs)
  0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined 0xFF00)
  0x09, 0x20,        //   Usage (0x20)
  0x75, 0x06,        //   Report Size (6)
  0x95, 0x01,        //   Report Count (1)
  0x81, 0x02,        //   Input (Data,Var,Abs)
  0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls)
  0x09, 0x33,        //   Usage (Rx)
  0x09, 0x34,        //   Usage (Ry)
  0x15, 0x00,        //   Logical Minimum (0)
  0x26, 0xFF, 0x00,  //   Logical Maximum (255)
  0x75, 0x08,        //   Report Size (8)
  0x95, 0x02,        //   Report Count (2)
  0x81, 0x02,        //   Input (Data,Var,Abs)
  0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined 0xFF00)
  0x09, 0x21,        //   Usage (0x21)
  0x95, 0x36,        //   Report Count (54)
  0x81, 0x02,        //   Input (Data,Var,Abs)
  0x85, 0x05,        //   Report ID (5)
  0x09, 0x22,        //   Usage (0x22)
  0x95, 0x1F,        //   Report Count (31)
  0x91, 0x02,        //   Output (Data,Var,Abs)
  0x85, 0x03,        //   Report ID (3)
  0x0A, 0x21, 0x27,  //   Usage (0x2721)
  0x95, 0x2F,        //   Report Count (47)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0x02,        //   Report ID (2)
  0x09, 0x24,        //   Usage (0x24)
  0x95, 0x24,        //   Report Count (36)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0x08,        //   Report ID (8)
  0x09, 0x25,        //   Usage (0x25)
  0x95, 0x03,        //   Report Count (3)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0x10,        //   Report ID (16)
  0x09, 0x26,        //   Usage (0x26)
  0x95, 0x04,        //   Report Count (4)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0x11,        //   Report ID (17)
  0x09, 0x27,        //   Usage (0x27)
  0x95, 0x02,        //   Report Count (2)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0x12,        //   Report ID (18)
  0x06, 0x02, 0xFF,  //   Usage Page (Vendor Defined 0xFF02)
  0x09, 0x21,        //   Usage (0x21)
  0x95, 0x0F,        //   Report Count (15)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0x13,        //   Report ID (19)
  0x09, 0x22,        //   Usage (0x22)
  0x95, 0x16,        //   Report Count (22)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0x14,        //   Report ID (20)
  0x06, 0x05, 0xFF,  //   Usage Page (Vendor Defined 0xFF05)
  0x09, 0x20,        //   Usage (0x20)
  0x95, 0x10,        //   Report Count (16)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0x15,        //   Report ID (21)
  0x09, 0x21,        //   Usage (0x21)
  0x95, 0x2C,        //   Report Count (44)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x06, 0x80, 0xFF,  //   Usage Page (Vendor Defined 0xFF80)
  0x85, 0x80,        //   Report ID (128)
  0x09, 0x20,        //   Usage (0x20)
  0x95, 0x06,        //   Report Count (6)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0x81,        //   Report ID (129)
  0x09, 0x21,        //   Usage (0x21)
  0x95, 0x06,        //   Report Count (6)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0x82,        //   Report ID (130)
  0x09, 0x22,        //   Usage (0x22)
  0x95, 0x05,        //   Report Count (5)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0x83,        //   Report ID (131)
  0x09, 0x23,        //   Usage (0x23)
  0x95, 0x01,        //   Report Count (1)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0x84,        //   Report ID (132)
  0x09, 0x24,        //   Usage (0x24)
  0x95, 0x04,        //   Report Count (4)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0x85,        //   Report ID (133)
  0x09, 0x25,        //   Usage (0x25)
  0x95, 0x06,        //   Report Count (6)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0x86,        //   Report ID (134)
  0x09, 0x26,        //   Usage (0x26)
  0x95, 0x06,        //   Report Count (6)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0x87,        //   Report ID (135)
  0x09, 0x27,        //   Usage (0x27)
  0x95, 0x23,        //   Report Count (35)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0x88,        //   Report ID (136)
  0x09, 0x28,        //   Usage (0x28)
  0x95, 0x22,        //   Report Count (34)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0x89,        //   Report ID (137)
  0x09, 0x29,        //   Usage (0x29)
  0x95, 0x02,        //   Report Count (2)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0x90,        //   Report ID (144)
  0x09, 0x30,        //   Usage (0x30)
  0x95, 0x05,        //   Report Count (5)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0x91,        //   Report ID (145)
  0x09, 0x31,        //   Usage (0x31)
  0x95, 0x03,        //   Report Count (3)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0x92,        //   Report ID (146)
  0x09, 0x32,        //   Usage (0x32)
  0x95, 0x03,        //   Report Count (3)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0x93,        //   Report ID (147)
  0x09, 0x33,        //   Usage (0x33)
  0x95, 0x0C,        //   Report Count (12)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0xA0,        //   Report ID (160)
  0x09, 0x40,        //   Usage (0x40)
  0x95, 0x06,        //   Report Count (6)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0xA1,        //   Report ID (161)
  0x09, 0x41,        //   Usage (0x41)
  0x95, 0x01,        //   Report Count (1)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0xA2,        //   Report ID (162)
  0x09, 0x42,        //   Usage (0x42)
  0x95, 0x01,        //   Report Count (1)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0xA3,        //   Report ID (163)
  0x09, 0x43,        //   Usage (0x43)
  0x95, 0x30,        //   Report Count (48)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0xA4,        //   Report ID (164)
  0x09, 0x44,        //   Usage (0x44)
  0x95, 0x0D,        //   Report Count (13)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0xA5,        //   Report ID (165)
  0x09, 0x45,        //   Usage (0x45)
  0x95, 0x15,        //   Report Count (21)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0xA6,        //   Report ID (166)
  0x09, 0x46,        //   Usage (0x46)
  0x95, 0x15,        //   Report Count (21)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0xA7,        //   Report ID (247)
  0x09, 0x4A,        //   Usage (0x4A)
  0x95, 0x01,        //   Report Count (1)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0xA8,        //   Report ID (250)
  0x09, 0x4B,        //   Usage (0x4B)
  0x95, 0x01,        //   Report Count (1)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0xA9,        //   Report ID (251)
  0x09, 0x4C,        //   Usage (0x4C)
  0x95, 0x08,        //   Report Count (8)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0xAA,        //   Report ID (252)
  0x09, 0x4E,        //   Usage (0x4E)
  0x95, 0x01,        //   Report Count (1)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0xAB,        //   Report ID (253)
  0x09, 0x4F,        //   Usage (0x4F)
  0x95, 0x39,        //   Report Count (57)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0xAC,        //   Report ID (254)
  0x09, 0x50,        //   Usage (0x50)
  0x95, 0x39,        //   Report Count (57)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0xAD,        //   Report ID (255)
  0x09, 0x51,        //   Usage (0x51)
  0x95, 0x0B,        //   Report Count (11)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0xAE,        //   Report ID (174)
  0x09, 0x52,        //   Usage (0x52)
  0x95, 0x01,        //   Report Count (1)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0xAF,        //   Report ID (175)
  0x09, 0x53,        //   Usage (0x53)
  0x95, 0x02,        //   Report Count (2)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0xB0,        //   Report ID (176)
  0x09, 0x54,        //   Usage (0x54)
  0x95, 0x3F,        //   Report Count (63)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0xC0,              // End Collection
  0x06, 0xF0, 0xFF,  // Usage Page (Vendor Defined 0xFFF0)
  0x09, 0x40,        // Usage (0x40)
  0xA1, 0x01,        // Collection (Application)
  0x85, 0xF0,        //   Report ID (240) - PS4 auth challenge
  0x09, 0x47,        //   Usage (0x47)
  0x95, 0x3F,        //   Report Count (63)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0xF1,        //   Report ID (241) - PS4 auth response
  0x09, 0x48,        //   Usage (0x48)
  0x95, 0x3F,        //   Report Count (63)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0xF2,        //   Report ID (242) - PS4 auth status
  0x09, 0x49,        //   Usage (0x49)
  0x95, 0x0F,        //   Report Count (15)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0x85, 0xF3,        //   Report ID (243) - PS4 auth reset
  0x0A, 0x01, 0x47,  //   Usage (0x4701)
  0x95, 0x07,        //   Report Count (7)
  0xB1, 0x02,        //   Feature (Data,Var,Abs)
  0xC0,              // End Collection
};

// Invoked when received GET HID REPORT DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const *tud_hid_descriptor_report_cb(uint8_t itf)
{
  return hid_report_desc;
}

// Configuration Descriptor
// The DS4 interface has both an interrupt IN and an interrupt OUT endpoint, so
// use the HID in/out descriptor rather than the input-only variant.
static const uint8_t configuration_desc[] = {
  TUD_CONFIG_DESCRIPTOR(1, 1, 0, (TUD_CONFIG_DESC_LEN + TUD_HID_INOUT_DESC_LEN), 0x00, 500),
  TUD_HID_INOUT_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE, sizeof(hid_report_desc), EP_OUT, EP_IN,
                           CFG_TUD_HID_EP_BUFSIZE, 1),
};

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
  return configuration_desc;
}

// String Descriptors
static const char *string_desc[] = {
  (const char[]){0x09, 0x04},      // 0: is supported language is English (0x0409)
  "Sony Computer Entertainment",   // 1: Manufacturer
  "Wireless Controller",           // 2: Product
};

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
const uint16_t *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
  static uint16_t desc_str[32 + 1];
  size_t chr_count;

  switch (index) {
    case 0x00:
      memcpy(&desc_str[1], string_desc[0], 2);
      chr_count = 1;
      break;

    default:
      if (!(index < sizeof(string_desc) / sizeof(string_desc[0])))
        return NULL;

      const char *str = string_desc[index];

      // Cap at max char
      chr_count              = strlen(str);
      size_t const max_count = sizeof(desc_str) / sizeof(desc_str[0]) - 1; // -1 for string type
      if (chr_count > max_count)
        chr_count = max_count;

      // Convert ASCII string into UTF-16
      for (size_t i = 0; i < chr_count; i++)
        desc_str[1 + i] = str[i];
      break;
  }

  // first byte is length (including header), second byte is string type
  desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));

  return desc_str;
}
