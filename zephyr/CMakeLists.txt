if(CONFIG_JOYBUS)
  zephyr_library_named(joybus)

  # Gather the protocol core and the host and target roles
  file(
    GLOB JOYBUS_SOURCES
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/*.c
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/host/*.c
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/target/*.c
  )
  zephyr_library_sources(${JOYBUS_SOURCES})

  # Expose the public headers, and scan them for the device API class
  zephyr_include_directories(${ZEPHYR_CURRENT_MODULE_DIR}/include)
  zephyr_syscall_include_directories(${ZEPHYR_CURRENT_MODULE_DIR}/include)

  # Add the backend drivers, one per part
  add_subdirectory(drivers)
endif()
