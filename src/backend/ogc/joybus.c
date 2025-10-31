#include <ogc/si.h>

#include <joybus/backend/ogc.h>

static struct joybus bus_instances[4];

static int joybus_ogc_enable(struct joybus *bus)
{
  return 0;
}

static int joybus_ogc_disable(struct joybus *bus)
{
  return 0;
}

static void si_transfer_callback(int32_t chan, uint32_t type)
{
  struct joybus *bus           = &bus_instances[chan];
  struct joybus_ogc_data *data = &JOYBUS_OGC(bus)->data;

  if (data->done_callback)
    data->done_callback(bus, 0, data->done_user_data);
}

static int joybus_ogc_transfer(struct joybus *bus, const uint8_t *write_buf, uint8_t write_len, uint8_t *read_buf,
                               uint8_t read_len, joybus_transfer_cb_t callback, void *user_data)
{
  struct joybus_ogc_data *data = &JOYBUS_OGC(bus)->data;

  data->done_callback  = callback;
  data->done_user_data = user_data;

  SI_Transfer(data->channel, (uint8_t *)write_buf, write_len, read_buf, read_len, si_transfer_callback, 0);

  return 0;
}

static const struct joybus_api ogc_api = {
  .enable   = joybus_ogc_enable,
  .disable  = joybus_ogc_disable,
  .transfer = joybus_ogc_transfer,
};

int joybus_ogc_init(struct joybus_ogc *ogc_bus, int channel)
{
  struct joybus *bus = JOYBUS(ogc_bus);
  bus->api           = &ogc_api;

  struct joybus_ogc_data *data = &ogc_bus->data;
  data->channel                = channel;

  return 0;
}