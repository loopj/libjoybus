/**
 * @addtogroup joybus
 *
 * @{
 */
#pragma once

/**
 * Whether to place latency-critical functions in RAM. Enabled by default,
 * for a deterministic command-to-response turnaround. Define as 0 to keep
 * them in flash, saving RAM at the cost of a variable turnaround (a flash
 * fetch or cache miss can slow a reply).
 */
#ifndef JOYBUS_USE_RAM_FUNCS
#define JOYBUS_USE_RAM_FUNCS 1
#endif

/**
 * Attribute for a latency-critical function, such as a target command
 * handler. Places it in RAM instead of executing it in place from flash, so
 * a flash fetch or cache miss cannot add latency to the response. A no-op
 * when JOYBUS_USE_RAM_FUNCS is 0, or on platforms that are not yet wired up.
 */
#if defined(ESP_PLATFORM) && JOYBUS_USE_RAM_FUNCS
#include <esp_attr.h>
#define JOYBUS_RAM_FUNC IRAM_ATTR
#else
#define JOYBUS_RAM_FUNC
#endif

/** @} */
