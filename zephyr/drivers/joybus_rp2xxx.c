#define DT_DRV_COMPAT loopj_joybus_rp2xxx

#include <zephyr/device.h>
#include <zephyr/irq.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/misc/pio_rpi_pico/pio_rpi_pico.h>

#include <hardware/irq.h>
#include <hardware/timer.h>

#include <joybus/backend/rp2xxx.h>
#include <joybus/zephyr/device.h>

// What devicetree says about a bus
struct joybus_rp2xxx_drv_config {
  const struct device *pio;
  uint8_t gpio;
  timer_hw_t *timer;
  uint32_t freq;
  uint8_t alarm_num;
};

// The instance a device drives
struct joybus_rp2xxx_drv_data {
  struct joybus_rp2xxx bus;
};

// Reach a backend handler, which takes no argument of its own
static void joybus_rp2xxx_isr(const void *arg)
{
  ((irq_handler_t)(uintptr_t)arg)();
}

// Put a backend handler on its vector, which the kernel owns the table for
static void joybus_rp2xxx_set_irq_handler(uint irq_num, irq_handler_t handler)
{
  irq_connect_dynamic(irq_num, CONFIG_JOYBUS_RP2XXX_IRQ_PRIORITY, joybus_rp2xxx_isr,
                      (const void *)(uintptr_t)handler, 0);
  irq_enable(irq_num);
}

static struct joybus *joybus_rp2xxx_get_bus(const struct device *dev)
{
  struct joybus_rp2xxx_drv_data *data = dev->data;

  return JOYBUS(&data->bus);
}

static DEVICE_API(joybus, joybus_rp2xxx_api) = {
  .get_bus = joybus_rp2xxx_get_bus,
};

static int joybus_rp2xxx_drv_init(const struct device *dev)
{
  const struct joybus_rp2xxx_drv_config *config = dev->config;
  struct joybus_rp2xxx_drv_data *data           = dev->data;

  if (!device_is_ready(config->pio))
    return -ENODEV;

  // Built by hand, since the library's defaults reach for the pico-sdk interrupt installer
  struct joybus_rp2xxx_config bus_config = {
    .gpio            = config->gpio,
    .pio             = pio_rpi_pico_get_pio(config->pio),
    .timer           = config->timer,
    .alarm_num       = config->alarm_num,
    .freq            = config->freq,
    .set_irq_handler = joybus_rp2xxx_set_irq_handler,
  };

  if (joybus_rp2xxx_init(&data->bus, bus_config) != 0)
    return -EINVAL;

  return 0;
}

#define JOYBUS_RP2XXX_DEFINE(inst)                                                                                     \
  /* A PIO instance reaches pins 0 to 31, which is the first GPIO device */                                            \
  BUILD_ASSERT(DT_SAME_NODE(DT_GPIO_CTLR(DT_DRV_INST(inst), gpios), DT_NODELABEL(gpio0)),                              \
               "joybus data line must be on gpio0");                                                                   \
  static struct joybus_rp2xxx_drv_data joybus_rp2xxx_data_##inst;                                                      \
  static const struct joybus_rp2xxx_drv_config joybus_rp2xxx_config_##inst = {                                         \
    .pio       = DEVICE_DT_GET(DT_INST_PHANDLE(inst, pio)),                                                            \
    .gpio      = DT_INST_GPIO_PIN(inst, gpios),                                                                        \
    .timer     = (timer_hw_t *)DT_REG_ADDR(DT_INST_PHANDLE(inst, timer)),                                              \
    .freq      = DT_INST_PROP(inst, clock_frequency),                                                                  \
    .alarm_num = DT_INST_PROP(inst, alarm),                                                                            \
  };                                                                                                                   \
  DEVICE_DT_INST_DEFINE(inst, joybus_rp2xxx_drv_init, NULL, &joybus_rp2xxx_data_##inst,                                \
                        &joybus_rp2xxx_config_##inst, POST_KERNEL, CONFIG_JOYBUS_RP2XXX_INIT_PRIORITY,                 \
                        &joybus_rp2xxx_api);

DT_INST_FOREACH_STATUS_OKAY(JOYBUS_RP2XXX_DEFINE)
