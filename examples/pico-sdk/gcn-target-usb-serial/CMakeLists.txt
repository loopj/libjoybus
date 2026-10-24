# Set the minimum CMake version
cmake_minimum_required(VERSION 3.20)

# Include the Pico SDK
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

# Configure project
project(gcn-target-usb-serial C CXX ASM)

# Initialize the Pico SDK
pico_sdk_init()

# Define platform for libjoybus
set(JOYBUS_BACKEND rp2xxx)

# Add libjoybus as a subdirectory
set(LIBJOYBUS_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../..)
add_subdirectory(${LIBJOYBUS_ROOT} ${CMAKE_BINARY_DIR}/_deps/libjoybus-build)

# Create the executable
add_executable(gcn-target-usb-serial main.c)

# Link with the standard library and libjoybus
target_link_libraries(gcn-target-usb-serial pico_stdlib joybus)

# Route stdio over USB serial instead of the UART
pico_enable_stdio_usb(gcn-target-usb-serial 1)
pico_enable_stdio_uart(gcn-target-usb-serial 0)

# Generate additional binary outputs
pico_add_extra_outputs(gcn-target-usb-serial)
