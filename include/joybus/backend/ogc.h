#pragma once

#include <joybus/bus.h>

#define JOYBUS_OGC(bus) ((struct joybus_ogc *)(bus))

struct joybus_ogc_data {
  // SI channel
  int channel;

  // Transfer state
  joybus_transfer_cb_t done_callback;
  void *done_user_data;
};

struct joybus_ogc {
  struct joybus base;
  struct joybus_ogc_data data;
};