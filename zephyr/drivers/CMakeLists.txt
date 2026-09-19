if(CONFIG_JOYBUS_RP2XXX)
  # The driver and the backend it wraps
  zephyr_library_sources(joybus_rp2xxx.c ${ZEPHYR_CURRENT_MODULE_DIR}/src/backend/rp2xxx/joybus.c)
endif()

if(CONFIG_JOYBUS_ESP32)
  # The driver and the backend it wraps
  zephyr_library_sources(joybus_esp32.c ${ZEPHYR_CURRENT_MODULE_DIR}/src/backend/esp32/joybus.c)

  # The RMT signal table, which the HAL builds only for its own RMT driver
  if(NOT CONFIG_PULSE_IO_ESP32_RMT)
    zephyr_library_sources(
      ${ZEPHYR_HAL_ESPRESSIF_MODULE_DIR}/components/esp_hal_rmt/${CONFIG_SOC_SERIES}/rmt_periph.c
    )
  endif()
endif()
