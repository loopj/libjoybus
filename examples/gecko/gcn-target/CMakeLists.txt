cmake_minimum_required(VERSION "3.25")

project(app VERSION 1.0 LANGUAGES C CXX ASM)

# Default to release builds
set(default_build_type "Release")

# Include the SLC-generated CMake content
add_subdirectory(slc_project/libjoybus_example_cmake)

# Define platform for libjoybus
set(JOYBUS_BACKEND_GECKO TRUE)

# Add libjoybus as a subdirectory
set(LIBJOYBUS_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../..)
add_subdirectory(${LIBJOYBUS_ROOT} ${CMAKE_BINARY_DIR}/_deps/libjoybus-build)

# Create the executable
add_executable(app main.c)

# Link with the gecko sdk dependencies and libjoybus
target_link_libraries(app slc_libjoybus_example joybus)

# Create .bin, .hex and .s37 artifacts after building the project
add_custom_command(
  TARGET app
  POST_BUILD
  COMMAND
    ${CMAKE_OBJCOPY} ${OBJCOPY_SREC_CMD} "$<TARGET_FILE:app>"
    "$<TARGET_FILE_DIR:app>/$<TARGET_FILE_BASE_NAME:app>.s37"
  COMMAND
    ${CMAKE_OBJCOPY} ${OBJCOPY_IHEX_CMD} "$<TARGET_FILE:app>"
    "$<TARGET_FILE_DIR:app>/$<TARGET_FILE_BASE_NAME:app>.hex"
  COMMAND
    ${CMAKE_OBJCOPY} ${OBJCOPY_BIN_CMD} "$<TARGET_FILE:app>"
    "$<TARGET_FILE_DIR:app>/$<TARGET_FILE_BASE_NAME:app>.bin"
)
