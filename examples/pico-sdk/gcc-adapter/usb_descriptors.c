#include <string.h>

#include "tusb.h"

#include "config.h"
#include "report_ids.h"

#define VENDOR_ID   0x057e
#define PRODUCT_ID  0x0337
#define EP_IN       0x81
#define EP_OUT      0x02

// GameCube Controller Adapter HID Report Template
#define HID_REPORT_GCC_ADAPTER(report_id, report_size, dir) \
  HID_COLLECTION(HID_COLLECTION_APPLICATION),\
    HID_REPORT_ID     ( report_id                           ) \
    HID_USAGE_MIN     ( 0                                   ) ,\
    HID_USAGE_MAX_N   ( 255, 2                              ) ,\
    HID_LOGICAL_MIN   ( 0                                   ) ,\
    HID_LOGICAL_MAX_N ( 255, 2                              ) ,\
    HID_REPORT_SIZE   ( 8                                   ) ,\
    HID_REPORT_COUNT  ( report_size                         ) ,\
    dir               ( HID_DATA | HID_ARRAY | HID_ABSOLUTE ) ,\
  HID_COLLECTION_END

// Device descriptor (identical to OEM adapter)
static const tusb_desc_device_t device_desc = {
  .bLength            = sizeof(tusb_desc_device_t),
  .bDescriptorType    = TUSB_DESC_DEVICE,
  .bcdUSB             = 0x0200,
  .bDeviceClass       = 0,
  .bDeviceSubClass    = 0,
  .bDeviceProtocol    = 0,
  .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
  .idVendor           = VENDOR_ID,
  .idProduct          = PRODUCT_ID,
  .bcdDevice          = 0x0100,
  .iManufacturer      = 0x01,
  .iProduct           = 0x02,
  .iSerialNumber      = 0x03,
  .bNumConfigurations = 0x01,
};

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const *tud_descriptor_device_cb(void)
{
  return (uint8_t const *)&device_desc;
}

// HID report descriptor (identical to OEM adapter)
static const uint8_t hid_report_desc[] = {
  HID_USAGE_PAGE(HID_USAGE_PAGE_GAME),
  HID_USAGE(0x00),

  // Define all the reports used by the adapter, matching the original order
  HID_REPORT_GCC_ADAPTER(RPT_SET_MOTOR, RPT_SET_MOTOR_LEN, HID_OUTPUT),
  HID_REPORT_GCC_ADAPTER(RPT_STATE, RPT_STATE_LEN, HID_INPUT),
  HID_REPORT_GCC_ADAPTER(RPT_GET_ORIGIN, RPT_GET_ORIGIN_LEN, HID_OUTPUT),
  HID_REPORT_GCC_ADAPTER(RPT_ORIGIN, RPT_ORIGIN_LEN, HID_INPUT),
  HID_REPORT_GCC_ADAPTER(RPT_ENABLE_POLLING, RPT_ENABLE_POLLING_LEN, HID_OUTPUT),
  HID_REPORT_GCC_ADAPTER(RPT_POLLING_ENABLED, RPT_POLLING_ENABLED_LEN, HID_INPUT),
  HID_REPORT_GCC_ADAPTER(RPT_DISABLE_POLLING, RPT_DISABLE_POLLING_LEN, HID_OUTPUT),
  HID_REPORT_GCC_ADAPTER(RPT_POLLING_DISABLED, RPT_POLLING_DISABLED_LEN, HID_INPUT),
  HID_REPORT_GCC_ADAPTER(RPT_RESET, RPT_RESET_LEN, HID_OUTPUT),
  HID_REPORT_GCC_ADAPTER(RPT_RESET_ACK, RPT_RESET_ACK_LEN, HID_INPUT),
};

// Invoked when received GET HID REPORT DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const *tud_hid_descriptor_report_cb(uint8_t itf)
{
  return hid_report_desc;
}

// Configuration descriptor
// Differs /very/ slightly from OEM adapter (HID version, buffer sizes)
static const uint8_t configuration_desc[] = {
  // Config number, interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_DESCRIPTOR(1, 1, 0, (TUD_CONFIG_DESC_LEN + TUD_HID_INOUT_DESC_LEN),
                        TUSB_DESC_CONFIG_ATT_SELF_POWERED | TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 500),

  // Interface number, string index, protocol, report descriptor len, EP OUT & IN address, size & polling interval
  TUD_HID_INOUT_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE, sizeof(hid_report_desc), EP_IN, EP_OUT, CFG_TUD_HID_EP_BUFSIZE,
                           GCCA_USB_ENDPOINT_INTERVAL),
};

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
  return configuration_desc;
}

// String descriptors (identical to OEM adapter)
static const char *string_desc[] = {
  (const char[]){0x09, 0x04}, // 0: is supported language is English (0x0409)
  "Nintendo",                 // 1: Manufacturer
  "WUP-028",                  // 2: Product
  "15/07/2014",               // 3: Serial
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