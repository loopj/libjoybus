if(CONFIG_JOYBUS)
  zephyr_library_named(joybus)

  # Build the protocol core and the host and target roles
  include(${ZEPHYR_CURRENT_MODULE_DIR}/cmake/sources.cmake)
  zephyr_library_sources(${JOYBUS_SOURCES})

  # Put the public headers on the include path
  zephyr_include_directories(${ZEPHYR_CURRENT_MODULE_DIR}/include)

  # Hand the ESP32 backend its interrupt placement choice
  zephyr_compile_definitions_ifdef(CONFIG_JOYBUS_ESP32_ISR_IRAM_SAFE JOYBUS_ESP32_ISR_IRAM_SAFE=1)

  # Scan those headers for the device API the drivers declare
  zephyr_syscall_include_directories(${ZEPHYR_CURRENT_MODULE_DIR}/include)

  add_subdirectory(drivers)
endif()
