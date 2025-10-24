#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/led.h>
#include <zephyr/logging/log.h>

#include <joybus/joybus.h>
#include <joybus/backend/gecko.h>

LOG_MODULE_REGISTER(app);

struct joybus_gecko gecko_bus;
struct joybus *bus = JOYBUS(&gecko_bus);

static uint8_t response[JOYBUS_BLOCK_SIZE] = {0};

static uint16_t last_buttons = 0;
static uint16_t down_buttons = 0;
static uint16_t up_buttons   = 0;
static bool motor_running    = false;

static const struct led_dt_spec status_led = LED_DT_SPEC_GET_OR(DT_ALIAS(pwm_led0), {0});

static void response_cb(struct joybus *bus, int result, void *user_data)
{
  printk("Response (%d): ", result);
  for (int i = 0; i < result; i++) {
    printk("%02x ", response[i]);
  }
  printk("\n");
  return;
}

static void read_cb(struct joybus *bus, int result, void *user_data)
{
  uint16_t buttons = ((response[1] << 8) | response[0]) & JOYBUS_N64_BUTTON_MASK;

  down_buttons = (buttons ^ last_buttons) & buttons;
  up_buttons   = (buttons ^ last_buttons) & last_buttons;

  last_buttons = buttons;
}

volatile bool polling_enabled = false;

static void detect_accessory_cb(int accessory, void *user_data)
{
  switch (accessory) {
    case JOYBUS_N64_ACCESSORY_NONE:
      printk("No accessory detected\n");
      break;
    case JOYBUS_N64_ACCESSORY_CONTROLLER_PAK:
      printk("Controller Pak detected\n");
      break;
    case JOYBUS_N64_ACCESSORY_RUMBLE_PAK:
      printk("Rumble Pak detected\n");
      break;
    case JOYBUS_N64_ACCESSORY_TRANSFER_PAK:
      printk("Transfer Pak detected\n");
      break;
    case JOYBUS_N64_ACCESSORY_BIO_SENSOR:
      printk("Bio Sensor detected\n");
      break;
    case JOYBUS_N64_ACCESSORY_SNAP_STATION:
      printk("Snap Station detected\n");
      break;
    default:
      printk("Unknown accessory detected\n");
      break;
  }

  // Enable polling
  polling_enabled = true;

  return;
}

void polling_thread(void *p1, void *p2, void *p3)
{
  while (1) {
    if (polling_enabled) {
      joybus_n64_read(bus, response, read_cb, NULL);
    }
    k_msleep(17); // ~16.6ms
  }
}

K_THREAD_DEFINE(polling_thread_id, 512, polling_thread, NULL, NULL, NULL, 5, 0, 0);

int main(void)
{
  joybus_gecko_init(&gecko_bus, gpioPortD, 3, TIMER1, USART0);
  joybus_enable(bus);

  // TODO: Remove once we have proper timeout/initialization handling
  k_msleep(100);

  // Fetch controller ID
  joybus_identify(bus, response, response_cb, NULL);
  k_msleep(10);

  joybus_n64_accessory_detect(bus, detect_accessory_cb, NULL);
  k_msleep(10);

  while (1) {
    if (last_buttons & JOYBUS_N64_BUTTON_A) {
      led_on_dt(&status_led);

      if (!motor_running) {
        motor_running = true;
        joybus_n64_motor_start(bus);
      }
    } else {
      if (motor_running) {
        motor_running = false;
        joybus_n64_motor_stop(bus);
      }

      led_off_dt(&status_led);
    }

    k_usleep(100);
  }

  return 0;
}
