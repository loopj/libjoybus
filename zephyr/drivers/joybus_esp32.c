#define DT_DRV_COMPAT loopj_joybus_esp32

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#include <joybus/backend/esp32.h>
#include <joybus/zephyr/device.h>

// What devicetree says about a bus
struct joybus_esp32_drv_config {
  gpio_num_t gpio;
  uint32_t freq;
  uint8_t rmt_tx_ch;
  uint8_t rmt_rx_ch;
};

// The instance a device drives
struct joybus_esp32_drv_data {
  struct joybus_esp32 bus;
};

static struct joybus *joybus_esp32_get_bus(const struct device *dev)
{
  struct joybus_esp32_drv_data *data = dev->data;

  return JOYBUS(&data->bus);
}

static DEVICE_API(joybus, joybus_esp32_api) = {
  .get_bus = joybus_esp32_get_bus,
};

static int joybus_esp32_drv_init(const struct device *dev)
{
  const struct joybus_esp32_drv_config *config = dev->config;
  struct joybus_esp32_drv_data *data           = dev->data;

  struct joybus_esp32_config bus_config = {
    .gpio      = config->gpio,
    .freq      = config->freq,
    .rmt_tx_ch = config->rmt_tx_ch,
    .rmt_rx_ch = config->rmt_rx_ch,
  };

  if (joybus_esp32_init(&data->bus, bus_config) != 0)
    return -EINVAL;

  return 0;
}

// Pads past the first port hang off a second GPIO device, numbered from zero again
#if DT_NODE_EXISTS(DT_NODELABEL(gpio1))
#define JOYBUS_ESP32_PAD(inst)                                                                                         \
  (DT_INST_GPIO_PIN(inst, gpios) +                                                                                     \
   (DT_SAME_NODE(DT_GPIO_CTLR(DT_DRV_INST(inst), gpios), DT_NODELABEL(gpio1)) ? GPIO_MAX_PINS_PER_PORT : 0))
#else
#define JOYBUS_ESP32_PAD(inst) DT_INST_GPIO_PIN(inst, gpios)
#endif

#define JOYBUS_ESP32_DEFINE(inst)                                                                                      \
  static struct joybus_esp32_drv_data joybus_esp32_data_##inst;                                                        \
  static const struct joybus_esp32_drv_config joybus_esp32_config_##inst = {                                           \
    .gpio      = JOYBUS_ESP32_PAD(inst),                                                                               \
    .freq      = DT_INST_PROP(inst, clock_frequency),                                                                  \
    .rmt_tx_ch = DT_INST_PROP(inst, rmt_tx_channel),                                                                   \
    .rmt_rx_ch = DT_INST_PROP(inst, rmt_rx_channel),                                                                   \
  };                                                                                                                   \
  DEVICE_DT_INST_DEFINE(inst, joybus_esp32_drv_init, NULL, &joybus_esp32_data_##inst,                                  \
                        &joybus_esp32_config_##inst, POST_KERNEL, CONFIG_JOYBUS_ESP32_INIT_PRIORITY,                   \
                        &joybus_esp32_api);

DT_INST_FOREACH_STATUS_OKAY(JOYBUS_ESP32_DEFINE)
