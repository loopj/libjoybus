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

static volatile bool polling_enabled = false;

static const struct led_dt_spec status_led = LED_DT_SPEC_GET_OR(DT_ALIAS(pwm_led0), {0});

static void on_response(struct joybus *bus, int result, void *user_data)
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
  uint16_t buttons = ((response[1] << 8) | response[0]) & JOYBUS_GCN_BUTTON_MASK;

  down_buttons = (buttons ^ last_buttons) & buttons;
  up_buttons   = (buttons ^ last_buttons) & last_buttons;

  last_buttons = buttons;
}

void polling_thread(void *p1, void *p2, void *p3)
{
  while (1) {
    if (polling_enabled)
      joybus_gcn_read(bus, JOYBUS_GCN_ANALOG_MODE_3, JOYBUS_GCN_MOTOR_STOP, response, read_cb, NULL);

    k_msleep(17); // ~16.6ms
  }
}

K_THREAD_DEFINE(polling_thread_id, 512, polling_thread, NULL, NULL, NULL, 5, 0, 0);

int main(void)
{
  joybus_gecko_init(&gecko_bus, gpioPortD, 3, TIMER1, USART0);
  joybus_enable(bus);
  k_msleep(100);

  // Fetch controller ID
  joybus_identify(bus, response, on_response, NULL);
  k_msleep(100);

  // Fetch controller origin
  joybus_gcn_read_origin(bus, response, on_response, NULL);
  k_msleep(1);

  // Enable polling after read_origin
  polling_enabled = true;

  while (1) {
    if (last_buttons & JOYBUS_GCN_BUTTON_A) {
      led_on_dt(&status_led);
    } else {
      led_off_dt(&status_led);
    }

    k_usleep(100);
  }

  return 0;
}
