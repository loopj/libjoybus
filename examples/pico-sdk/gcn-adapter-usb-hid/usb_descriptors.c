#include <string.h>

#include "tusb.h"

#define VENDOR_ID   0x1209
#define PRODUCT_ID  0x5750
#define EP_IN       0x83

// Device Descriptors
static const tusb_desc_device_t device_desc = {
  .bLength            = sizeof(tusb_desc_device_t),
  .bDescriptorType    = TUSB_DESC_DEVICE,
  .bcdUSB             = 0x0200,
  .bDeviceClass       = TUSB_CLASS_MISC,
  .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
  .bDeviceProtocol    = MISC_PROTOCOL_IAD,
  .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
  .idVendor           = 0x1209,
  .idProduct          = 0x5750,
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
static const uint8_t hid_report_desc[] = {
  TUD_HID_REPORT_DESC_GAMEPAD(),
};

// Invoked when received GET HID REPORT DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const *tud_hid_descriptor_report_cb(uint8_t itf)
{
  return hid_report_desc;
}

// Configuration Descriptor
static const uint8_t configuration_desc[] = {
  TUD_CONFIG_DESCRIPTOR(1, 1, 0, (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN), 0x00, 100),
  TUD_HID_DESCRIPTOR(0, 4, HID_ITF_PROTOCOL_NONE, sizeof(hid_report_desc), EP_IN, CFG_TUD_HID_EP_BUFSIZE, 1),
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
  (const char[]){0x09, 0x04}, // 0: is supported language is English (0x0409)
  "libjoybus",                // 1: Manufacturer
  "GameCube Adapter",         // 2: Product
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