file(
  GLOB SOURCES
  ${LIBJOYBUS_ROOT_DIR}/src/*.c
  ${LIBJOYBUS_ROOT_DIR}/src/host/*.c
  ${LIBJOYBUS_ROOT_DIR}/src/target/*.c
)

# ESP-IDF v5 vs v6 compatibility
if(IDF_VERSION_MAJOR GREATER_EQUAL 6)
  set(JOYBUS_RMT_HAL_COMPONENT esp_hal_rmt)
else()
  set(JOYBUS_RMT_HAL_COMPONENT hal)
endif()

idf_component_register(
  SRCS ${SOURCES} ${LIBJOYBUS_ROOT_DIR}/src/backend/esp32/joybus.c
  INCLUDE_DIRS ${LIBJOYBUS_ROOT_DIR}/include
  REQUIRES esp_driver_gpio esp_timer esp_hw_support
  PRIV_REQUIRES soc esp_rom ${JOYBUS_RMT_HAL_COMPONENT}
)
