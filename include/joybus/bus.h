/**
 * @addtogroup joybus
 * @{
 */

#pragma once

#include <stdint.h>

#include <joybus/target.h>

struct joybus;

/** Bus frequency of an OEM GameCube controller */
#define JOYBUS_FREQ_GCC                 250000

/** Bus frequency of WaveBird receiver */
#define JOYBUS_FREQ_WAVEBIRD            225000

/** Bus frequency of a console */
#define JOYBUS_FREQ_CONSOLE             200000

/** Maximum size of a Joybus transfer, in bytes */
#define JOYBUS_BLOCK_SIZE               64

/** Minimum delay between Joybus transfers, in microseconds */
#define JOYBUS_INTER_TRANSFER_DELAY_US  20

/** Timeout for waiting for a reply from a target, in microseconds */
#define JOYBUS_REPLY_TIMEOUT_US         100

/**
 * Macro to cast a backend-specific Joybus instance to a generic Joybus instance.
 */
#define JOYBUS(bus) ((struct joybus *)(bus))

/**
 * Function type for transfer completion callbacks.
 *
 * @param bus the Joybus associated with the transfer
 * @param result positive number of bytes read on success, negative error code on failure
 * @param user_data user data passed to the callback
 */
typedef void (*joybus_transfer_cb_t)(struct joybus *bus, int result, void *user_data);

// API for a Joybus backend - internal use only
struct joybus_api {
  int (*enable)(struct joybus *bus);
  int (*disable)(struct joybus *bus);
  int (*transfer)(struct joybus *bus, const uint8_t *write_buf, uint8_t write_len, uint8_t *read_buf, uint8_t read_len,
                  joybus_transfer_cb_t callback, void *user_data);
  int (*target_register)(struct joybus *bus, struct joybus_target *target);
  int (*target_unregister)(struct joybus *bus, struct joybus_target *target);
};

/**
 * A Joybus instance.
 */
struct joybus {
  const struct joybus_api *api;

  struct joybus_target *target;
  uint8_t command_buffer[JOYBUS_BLOCK_SIZE];
};

/**
 * Enable the Joybus instance.
 *
 * @param bus the Joybus instance to enable
 */
static inline int joybus_enable(struct joybus *bus)
{
  return bus->api->enable(bus);
}

/**
 * Disable the Joybus instance.
 *
 * @param bus the Joybus instance to disable
 */
static inline int joybus_disable(struct joybus *bus)
{
  return bus->api->disable(bus);
}

/**
 * Perform a Joybus "write then read" transfer.
 *
 * Sends a command to a device, and reads the response.
 * The provided buffers must be valid until the transfer is complete.
 *
 * @param bus the Joybus instance to use
 * @param write_buf the buffer containing the command to send
 * @param write_len the number of bytes to write
 * @param read_buf the buffer to store the response in
 * @param read_len the number of bytes to read
 * @param callback a callback function to call when the transfer is complete
 * @param user_data user data to pass to the callback
 *
 * @return 0 on success, negative error code on failure
 */
static inline int joybus_transfer(struct joybus *bus, const uint8_t *write_buf, uint8_t write_len, uint8_t *read_buf,
                                  uint8_t read_len, joybus_transfer_cb_t callback, void *user_data)
{
  return bus->api->transfer(bus, write_buf, write_len, read_buf, read_len, callback, user_data);
}

/**
 * Enable Joybus "target" mode, and register a target to handle commands.
 *
 * @param bus the Joybus instance to use
 * @param target the target to register
 * @return 0 on success, negative error code on failure
 */
static inline int joybus_target_register(struct joybus *bus, struct joybus_target *target)
{
  return bus->api->target_register(bus, target);
}

/**
 * Unregister a Joybus target.
 *
 * @param bus the Joybus instance to use
 * @param target the target to unregister
 * @return 0 on success, negative error code on failure
 */
static inline int joybus_target_unregister(struct joybus *bus, struct joybus_target *target)
{
  return bus->api->target_unregister(bus, target);
}

/** @} */