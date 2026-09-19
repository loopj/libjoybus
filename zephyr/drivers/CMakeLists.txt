if(CONFIG_JOYBUS_RP2XXX)
  # Build the driver and the backend it wraps
  zephyr_library_sources(joybus_rp2xxx.c ${JOYBUS_RP2XXX_SOURCES})
endif()

if(CONFIG_JOYBUS_ESP32)
  # Build the driver and the backend it wraps
  zephyr_library_sources(joybus_esp32.c ${JOYBUS_ESP32_SOURCES})

  # Add the RMT signal table, which the HAL builds only for its own RMT driver
  if(NOT CONFIG_PULSE_IO_ESP32_RMT)
    zephyr_library_sources(
      ${ZEPHYR_HAL_ESPRESSIF_MODULE_DIR}/components/esp_hal_rmt/${CONFIG_SOC_SERIES}/rmt_periph.c
    )
  endif()
endif()
