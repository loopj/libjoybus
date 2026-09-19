if(CONFIG_JOYBUS)
  zephyr_library_named(joybus)

  # The protocol core and the host and target roles, gathered as the top-level build does
  file(
    GLOB JOYBUS_SOURCES
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/*.c
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/host/*.c
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/target/*.c
  )
  zephyr_library_sources(${JOYBUS_SOURCES})

  # The library's public headers, scanned so the device API class gets a section
  zephyr_include_directories(${ZEPHYR_CURRENT_MODULE_DIR}/include)
  zephyr_syscall_include_directories(${ZEPHYR_CURRENT_MODULE_DIR}/include)

  # Backend drivers, one per part
  add_subdirectory(drivers)
endif()
