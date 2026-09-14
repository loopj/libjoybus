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
 * Attributes for a latency-critical function, such as a target command
 * handler. Places it in RAM instead of executing it in place from flash, so
 * a flash fetch or cache miss cannot add latency to the response. A no-op
 * when JOYBUS_USE_RAM_FUNCS is 0, or on platforms that are not yet wired up.
 */
#if JOYBUS_USE_RAM_FUNCS
#if defined(ESP_PLATFORM)
#include <esp_attr.h>
#define JOYBUS_RAM_FUNC IRAM_ATTR
#define JOYBUS_RAM_DATA DRAM_ATTR
#elif defined(PICO_ON_DEVICE) && PICO_ON_DEVICE
#include <pico.h>
#define JOYBUS_RAM_FUNC __not_in_flash("joybus")
#define JOYBUS_RAM_DATA __not_in_flash("joybus_data")
#elif defined(SL_COMPONENT_CATALOG_PRESENT)
#include <em_ramfunc.h>
#define JOYBUS_RAM_FUNC SL_RAMFUNC_DECLARATOR
#endif
#endif

#ifndef JOYBUS_RAM_FUNC
#define JOYBUS_RAM_FUNC
#endif

#ifndef JOYBUS_RAM_DATA
#define JOYBUS_RAM_DATA
#endif

/** @} */
