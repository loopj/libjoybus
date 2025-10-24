# Define the library
zephyr_library_named(joybus)

# Set the library path
set(LIBJOYBUS_DIR ${CMAKE_CURRENT_LIST_DIR}/..)

# Add the common source files
zephyr_library_sources(
  ${LIBJOYBUS_DIR}/src/crc8.c
  ${LIBJOYBUS_DIR}/src/host/common.c
  ${LIBJOYBUS_DIR}/src/host/gamecube.c
  ${LIBJOYBUS_DIR}/src/host/n64.c
  ${LIBJOYBUS_DIR}/src/target/gc_controller.c
  ${LIBJOYBUS_DIR}/src/target/n64_controller.c
)

# Specify the include paths
zephyr_include_directories(${LIBJOYBUS_DIR}/include)

# Gecko backend and dependencies
zephyr_library_sources(${LIBJOYBUS_DIR}/src/backend/gecko/joybus.c)

zephyr_library_sources(
  ${ZEPHYR_HAL_SILABS_MODULE_DIR}/simplicity_sdk/platform/emlib/src/em_timer.c
  ${ZEPHYR_HAL_SILABS_MODULE_DIR}/simplicity_sdk/platform/emlib/src/em_ldma.c
  ${ZEPHYR_HAL_SILABS_MODULE_DIR}/simplicity_sdk/platform/emdrv/dmadrv/src/dmadrv.c
)
