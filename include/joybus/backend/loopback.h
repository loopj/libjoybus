/**
 * Loopback Joybus backend for testing
 *
 * @defgroup joybus_backend_loopback Loopback Backend
 * @ingroup joybus_backends
 * @{
 */

#pragma once

#include <joybus/bus.h>

/**
 * Initialize a loopback Joybus instance.
 *
 * @param bus the Joybus instance to initialize
 * @return 0 on success, negative error code on failure
 */
int joybus_loopback_init(struct joybus *bus);

/** @} */