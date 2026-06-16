/**
 * @addtogroup joybus
 *
 * @{
 */
#pragma once

/**
 * Joybus error codes.
 *
 * Errors are reported as negatives of these codes. Functions return 0 on
 * success or a negative joybus_error on failure, and the status passed to a
 * transfer callback uses the same convention. If an async function returns an
 * error, the transfer did not start, so its callback is not invoked.
 */
enum joybus_error {
  /// Bus not enabled
  JOYBUS_ERR_DISABLED = 1,

  /// Bus is busy with another operation
  JOYBUS_ERR_BUSY,

  /// Transfer timed out
  JOYBUS_ERR_TIMEOUT,

  /// Command not supported by Joybus target
  JOYBUS_ERR_NOT_SUPPORTED,

  /// Checksum error
  JOYBUS_ERR_CHECKSUM,

  /// Expected device not detected
  JOYBUS_ERR_NO_DEVICE,
};

/** @} */
