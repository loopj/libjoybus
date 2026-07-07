file(
  GLOB SOURCES
  ${LIBJOYBUS_ROOT_DIR}/src/*.c
  ${LIBJOYBUS_ROOT_DIR}/src/host/*.c
  ${LIBJOYBUS_ROOT_DIR}/src/target/*.c
)

idf_component_register(
  SRCS ${SOURCES} ${LIBJOYBUS_ROOT_DIR}/src/backend/esp32/joybus.c
  INCLUDE_DIRS ${LIBJOYBUS_ROOT_DIR}/include
  REQUIRES esp_driver_gpio esp_timer esp_hw_support hal
  PRIV_REQUIRES soc esp_rom
)
