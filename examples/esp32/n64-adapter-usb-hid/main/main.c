#include <esp_timer.h>

#include <joybus/joybus.h>
#include <joybus/backend/esp32.h>

#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "class/hid/hid_device.h"

// Change this define to match your hardware setup
#define JOYBUS_GPIO      1

// How often to poll the controller and send a USB report
#define POLL_INTERVAL_MS 1

// USB identity (VID/PID from https://pid.codes)
#define USB_VENDOR_ID    0x1209
#define USB_PRODUCT_ID   0x5750
#define USB_HID_EP_IN    0x81

// Joybus instance
static struct joybus_esp32 esp32_bus;
static struct joybus *bus = JOYBUS(&esp32_bus);

// Joybus state
static struct joybus_id id;
static struct joybus_n64_controller_state input;

// Polling mode
enum { POLL_MODE_IDENTIFY, POLL_MODE_READ };
static uint8_t poll_mode = POLL_MODE_IDENTIFY;

// Device descriptor
static const tusb_desc_device_t device_descriptor = {
  .bLength            = sizeof(tusb_desc_device_t),
  .bDescriptorType    = TUSB_DESC_DEVICE,
  .bcdUSB             = 0x0200,
  .bDeviceClass       = 0x00,
  .bDeviceSubClass    = 0x00,
  .bDeviceProtocol    = 0x00,
  .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
  .idVendor           = USB_VENDOR_ID,
  .idProduct          = USB_PRODUCT_ID,
  .bcdDevice          = 0x0100,
  .iManufacturer      = 0x01,
  .iProduct           = 0x02,
  .iSerialNumber      = 0x00,
  .bNumConfigurations = 0x01,
};

// HID report descriptor: a standard gamepad
static const uint8_t hid_report_descriptor[] = {
  TUD_HID_REPORT_DESC_GAMEPAD(),
};

// Configuration descriptor: a single HID interface
static const uint8_t configuration_descriptor[] = {
  TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN, 0x00, 100),
  TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE, sizeof(hid_report_descriptor), USB_HID_EP_IN,
                     CFG_TUD_HID_EP_BUFSIZE, 1),
};

// String descriptors
static const char *string_descriptor[] = {
  (const char[]){0x09, 0x04}, // 0: English (0x0409)
  "libjoybus",                // 1: Manufacturer
  "Nintendo 64 Adapter",      // 2: Product
};

// Return the HID report descriptor to the USB stack
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
  return hid_report_descriptor;
}

// The host never requests reports from us, so this stalls the request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer,
                               uint16_t reqlen)
{
  return 0;
}

// The host never sends reports to us, so this is unused
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, const uint8_t *buffer,
                           uint16_t bufsize)
{
}

// Joybus identify callback
static void joybus_identify_cb(struct joybus *bus, int status, void *user_data)
{
  // Stay in identify mode on any Joybus error
  if (status < 0)
    return;

  // Check it's an N64 controller
  if (!(id.type & JOYBUS_DEVICE_N64_CONTROLLER))
    return;

  // Move to polling for input
  poll_mode = POLL_MODE_READ;
}

// Joybus N64 read callback
static void joybus_read_cb(struct joybus *bus, int status, void *user_data)
{
  // Return to identify mode on any error
  if (status < 0)
    poll_mode = POLL_MODE_IDENTIFY;
}

// Map N64 buttons to HID gamepad buttons
static inline uint32_t get_buttons(const struct joybus_n64_controller_state *input)
{
  uint32_t buttons = 0;

  // Use semantic aliases where possible
  buttons |= (input->buttons & JOYBUS_N64_BUTTON_A) ? GAMEPAD_BUTTON_A : 0;
  buttons |= (input->buttons & JOYBUS_N64_BUTTON_B) ? GAMEPAD_BUTTON_B : 0;
  buttons |= (input->buttons & JOYBUS_N64_BUTTON_START) ? GAMEPAD_BUTTON_START : 0;
  buttons |= (input->buttons & JOYBUS_N64_BUTTON_Z) ? GAMEPAD_BUTTON_Z : 0;
  buttons |= (input->buttons & JOYBUS_N64_BUTTON_L) ? GAMEPAD_BUTTON_TL : 0;
  buttons |= (input->buttons & JOYBUS_N64_BUTTON_R) ? GAMEPAD_BUTTON_TR : 0;

  // No good semantic mapping for the C buttons, so map them to the high buttons
  buttons |= (input->buttons & JOYBUS_N64_BUTTON_C_UP) ? GAMEPAD_BUTTON_12 : 0;
  buttons |= (input->buttons & JOYBUS_N64_BUTTON_C_DOWN) ? GAMEPAD_BUTTON_13 : 0;
  buttons |= (input->buttons & JOYBUS_N64_BUTTON_C_LEFT) ? GAMEPAD_BUTTON_14 : 0;
  buttons |= (input->buttons & JOYBUS_N64_BUTTON_C_RIGHT) ? GAMEPAD_BUTTON_15 : 0;

  return buttons;
}

// Map the N64 D-pad to a HID hat value
static inline uint8_t get_hat(const struct joybus_n64_controller_state *input)
{
  bool up    = input->buttons & JOYBUS_N64_BUTTON_UP;
  bool down  = input->buttons & JOYBUS_N64_BUTTON_DOWN;
  bool left  = input->buttons & JOYBUS_N64_BUTTON_LEFT;
  bool right = input->buttons & JOYBUS_N64_BUTTON_RIGHT;

  if (up && right)
    return GAMEPAD_HAT_UP_RIGHT;
  if (down && right)
    return GAMEPAD_HAT_DOWN_RIGHT;
  if (down && left)
    return GAMEPAD_HAT_DOWN_LEFT;
  if (up && left)
    return GAMEPAD_HAT_UP_LEFT;
  if (up)
    return GAMEPAD_HAT_UP;
  if (right)
    return GAMEPAD_HAT_RIGHT;
  if (down)
    return GAMEPAD_HAT_DOWN;
  if (left)
    return GAMEPAD_HAT_LEFT;

  return GAMEPAD_HAT_CENTERED;
}

// Send the latest input state as a USB HID gamepad report
static void send_hid_report(void)
{
  // The N64 read reports the stick as a signed displacement, so it maps straight to a HID axis
  tud_hid_gamepad_report(0, input.stick_x, input.stick_y, 0, 0, 0, 0, get_hat(&input), get_buttons(&input));
}

// Poll the controller and send a USB report at regular intervals
static void poll_task(void *arg)
{
  switch (poll_mode) {
    case POLL_MODE_IDENTIFY:
      joybus_identify_async(bus, &id, joybus_identify_cb, NULL);
      break;

    case POLL_MODE_READ:
      joybus_n64_read_async(bus, &input, joybus_read_cb, NULL);

      // Send the latest input state to the USB host
      if (tud_hid_ready())
        send_hid_report();
      break;
  }
}

void app_main(void)
{
  // Initialize Joybus as a host
  joybus_esp32_init(&esp32_bus, joybus_esp32_config_default(JOYBUS_GPIO));
  joybus_enable(bus, JOYBUS_MODE_HOST);

  // Initialize TinyUSB with our gamepad descriptors
  tinyusb_config_t tusb_cfg             = TINYUSB_DEFAULT_CONFIG();
  tusb_cfg.descriptor.device            = &device_descriptor;
  tusb_cfg.descriptor.full_speed_config = configuration_descriptor;
  tusb_cfg.descriptor.string            = string_descriptor;
  tusb_cfg.descriptor.string_count      = sizeof(string_descriptor) / sizeof(string_descriptor[0]);
  ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

  // Poll for Joybus data and send USB reports at regular intervals
  const esp_timer_create_args_t poll_args = {.callback = poll_task, .name = "poll"};
  esp_timer_handle_t poll_timer;
  esp_timer_create(&poll_args, &poll_timer);
  esp_timer_start_periodic(poll_timer, POLL_INTERVAL_MS * 1000);
}
