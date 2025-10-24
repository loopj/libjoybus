#pragma once

/**
 * Joybus error codes.
 */
enum joybus_error {
  /** Bus not enabled */
  JOYBUS_ERR_DISABLED = 1,

  /** Bus is busy with another operation */
  JOYBUS_ERR_BUSY,

  /** Transfer timed out */
  JOYBUS_ERR_TIMEOUT,

  /** Command not supported by Joybus target */
  JOYBUS_ERR_NOT_SUPPORTED,
};