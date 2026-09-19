# Resolve the library root, so any build system can include this file directly
get_filename_component(LIBJOYBUS_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR}/.. ABSOLUTE)

# List the protocol core and the host and target roles
set(
  JOYBUS_SOURCES
  ${LIBJOYBUS_ROOT_DIR}/src/bus.c
  ${LIBJOYBUS_ROOT_DIR}/src/checksum.c
  ${LIBJOYBUS_ROOT_DIR}/src/n64_pak_fs.c
  ${LIBJOYBUS_ROOT_DIR}/src/host/common.c
  ${LIBJOYBUS_ROOT_DIR}/src/host/gcn.c
  ${LIBJOYBUS_ROOT_DIR}/src/host/n64.c
  ${LIBJOYBUS_ROOT_DIR}/src/host/n64_pak_rumble.c
  ${LIBJOYBUS_ROOT_DIR}/src/target/gcn_controller.c
  ${LIBJOYBUS_ROOT_DIR}/src/target/n64_controller.c
  ${LIBJOYBUS_ROOT_DIR}/src/target/n64_pak_controller.c
  ${LIBJOYBUS_ROOT_DIR}/src/target/n64_pak_rumble.c
  ${LIBJOYBUS_ROOT_DIR}/src/target/pixelfx_gameid.c
)

# List the backend for each MCU family
set(JOYBUS_ESP32_SOURCES ${LIBJOYBUS_ROOT_DIR}/src/backend/esp32/joybus.c)
set(JOYBUS_GECKO_SDK_SOURCES ${LIBJOYBUS_ROOT_DIR}/src/backend/gecko_sdk/joybus.c)
set(JOYBUS_RP2XXX_SOURCES ${LIBJOYBUS_ROOT_DIR}/src/backend/rp2xxx/joybus.c)
