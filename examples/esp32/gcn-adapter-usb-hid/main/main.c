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
static struct joybus_gcn_controller_state input;
static struct joybus_gcn_controller_state origin;

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
  "GameCube Adapter",         // 2: Product
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

// Joybus read origin callback
static void joybus_read_origin_cb(struct joybus *bus, int status, void *user_data)
{
  // Return to identify mode on any error
  if (status < 0)
    poll_mode = POLL_MODE_IDENTIFY;
}

// Joybus identify callback
static void joybus_identify_cb(struct joybus *bus, int status, void *user_data)
{
  // Stay in identify mode on any Joybus error
  if (status < 0)
    return;

  // Check it's a GameCube controller
  uint16_t type = id.type;
  if (!(type & JOYBUS_TYPE_GCN_DEVICE))
    return;

  // Check we've received data if it's a wireless controller
  if ((type & JOYBUS_TYPE_GCN_WIRELESS) && !(type & JOYBUS_TYPE_GCN_WIRELESS_RECEIVED))
    return;

  // Check it's a standard controller
  if (!(type & JOYBUS_TYPE_GCN_STANDARD))
    return;

  // Read the origin, then move to polling for input
  joybus_gcn_read_origin_async(bus, &origin, joybus_read_origin_cb, NULL);
  poll_mode = POLL_MODE_READ;
}

// Joybus GCN read callback
static void joybus_read_cb(struct joybus *bus, int status, void *user_data)
{
  // Return to identify mode on any error
  if (status < 0) {
    poll_mode = POLL_MODE_IDENTIFY;
    return;
  }

  // Refetch the origin if the controller asks for it
  if (input.buttons & JOYBUS_GCN_NEED_ORIGIN)
    joybus_gcn_read_origin_async(bus, &origin, joybus_read_origin_cb, NULL);
}

// Clamp an axis value based on the origin and expected resting position
static inline uint8_t clamp_axis(uint8_t value, uint8_t origin, uint8_t resting)
{
  int adjusted = value + (resting - origin);
  if (adjusted < 0)
    adjusted = 0;
  else if (adjusted > 255)
    adjusted = 255;

  return adjusted;
}

// Map a GameCube analog stick value (0-255) to a HID axis value (-128 to 127)
static inline int8_t get_stick(uint8_t value, uint8_t origin)
{
  return clamp_axis(value, origin, 128) - 128;
}

// Map a GameCube trigger value (0-255) to a HID axis value (-128 to 127)
static inline int8_t get_trigger(uint8_t value, uint8_t origin)
{
  return clamp_axis(value, origin, 0) - 128;
}

// Map GameCube buttons to HID gamepad buttons
static inline uint32_t get_buttons(const struct joybus_gcn_controller_state *input)
{
  uint32_t buttons = 0;

  buttons |= (input->buttons & JOYBUS_GCN_BUTTON_A) ? GAMEPAD_BUTTON_A : 0;
  buttons |= (input->buttons & JOYBUS_GCN_BUTTON_B) ? GAMEPAD_BUTTON_B : 0;
  buttons |= (input->buttons & JOYBUS_GCN_BUTTON_X) ? GAMEPAD_BUTTON_X : 0;
  buttons |= (input->buttons & JOYBUS_GCN_BUTTON_Y) ? GAMEPAD_BUTTON_Y : 0;
  buttons |= (input->buttons & JOYBUS_GCN_BUTTON_START) ? GAMEPAD_BUTTON_START : 0;
  buttons |= (input->buttons & JOYBUS_GCN_BUTTON_Z) ? GAMEPAD_BUTTON_Z : 0;
  buttons |= (input->buttons & JOYBUS_GCN_BUTTON_L) ? GAMEPAD_BUTTON_TL : 0;
  buttons |= (input->buttons & JOYBUS_GCN_BUTTON_R) ? GAMEPAD_BUTTON_TR : 0;

  return buttons;
}

// Map the GameCube D-pad to a HID hat value
static inline uint8_t get_hat(const struct joybus_gcn_controller_state *input)
{
  bool up    = input->buttons & JOYBUS_GCN_BUTTON_UP;
  bool down  = input->buttons & JOYBUS_GCN_BUTTON_DOWN;
  bool left  = input->buttons & JOYBUS_GCN_BUTTON_LEFT;
  bool right = input->buttons & JOYBUS_GCN_BUTTON_RIGHT;

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
  tud_hid_gamepad_report(0, get_stick(input.stick_x, origin.stick_x), get_stick(input.stick_y, origin.stick_y),
                         get_stick(input.substick_x, origin.substick_x), get_stick(input.substick_y, origin.substick_y),
                         get_trigger(input.trigger_left, origin.trigger_left),
                         get_trigger(input.trigger_right, origin.trigger_right), get_hat(&input), get_buttons(&input));
}

// Poll the controller and send a USB report at regular intervals
static void poll_task(void *arg)
{
  switch (poll_mode) {
    case POLL_MODE_IDENTIFY:
      joybus_identify_async(bus, &id, joybus_identify_cb, NULL);
      break;

    case POLL_MODE_READ:
      joybus_gcn_read_async(bus, JOYBUS_GCN_ANALOG_MODE_3, JOYBUS_GCN_MOTOR_STOP, &input, joybus_read_cb, NULL);

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
