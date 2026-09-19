/**
 * @defgroup joybus_zephyr Zephyr Device API
 * @ingroup joybus_backends
 *
 * Zephyr device binding for a Joybus bus.
 *
 * @{
 */

#pragma once

#include <zephyr/device.h>

#include <joybus/bus.h>

/**
 * Joybus driver API
 */
__subsystem struct joybus_driver_api {
  /** Return the libjoybus bus the device drives */
  struct joybus *(*get_bus)(const struct device *dev);
};

/**
 * Get the libjoybus bus for this Zephyr device
 *
 * @param dev the Joybus device
 * @return the libjoybus bus instance
 */
static inline struct joybus *joybus_zephyr_get_bus(const struct device *dev)
{
  const struct joybus_driver_api *api = (const struct joybus_driver_api *)dev->api;
  return api->get_bus(dev);
}

/** @} */
