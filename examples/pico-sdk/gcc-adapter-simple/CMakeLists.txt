# Set the minimum CMake version
cmake_minimum_required(VERSION 3.20)

# Include the Pico SDK
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

# Configure project
project(gcc-adapter-simple C CXX ASM)

# CMake target
set(TARGET gcc-adapter-simple)

# Initialize the Pico SDK
pico_sdk_init()

# Define platform for libjoybus
set(JOYBUS_BACKEND_RP2XXX TRUE)

# Make libjoybus available
set(LIBJOYBUS_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../..)
add_subdirectory(${LIBJOYBUS_ROOT} ${CMAKE_BINARY_DIR}/_deps/libjoybus-build)

# Create the executable
add_executable(${TARGET} main.c usb_descriptors.c)

# Specify the include paths
target_include_directories(${TARGET} PUBLIC .)

# Depend on Pico SDK, TinyUSB, and libjoybus
target_link_libraries(${TARGET} pico_stdlib tinyusb_device tinyusb_board joybus)

# Generate UF2 file
pico_add_extra_outputs(${TARGET})
