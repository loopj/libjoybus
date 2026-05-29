cmake_minimum_required(VERSION 3.20)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(gcn-host C CXX ASM)

pico_sdk_init()

# Define platform for libjoybus
set(JOYBUS_BACKEND_RP2XXX TRUE)

# Add libjoybus as a subdirectory
set(LIBJOYBUS_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../..)
add_subdirectory(${LIBJOYBUS_ROOT} ${CMAKE_BINARY_DIR}/_deps/libjoybus-build)

# Create the executable
add_executable(gcn-host main.c)

# Link with the standard library and libjoybus
target_link_libraries(gcn-host pico_stdlib joybus)

pico_enable_stdio_usb(gcn-host 1)
pico_enable_stdio_uart(gcn-host 0)

# Generate additional binary outputs
pico_add_extra_outputs(gcn-host)
