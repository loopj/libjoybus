cmake_minimum_required(VERSION 3.20)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(n64-host C CXX ASM)

pico_sdk_init()

# Define platform for libjoybus
set(JOYBUS_BACKEND rp2xxx)

# Add libjoybus as a subdirectory
set(LIBJOYBUS_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../..)
add_subdirectory(${LIBJOYBUS_ROOT} ${CMAKE_BINARY_DIR}/_deps/libjoybus-build)

# Create the executable
add_executable(n64-host main.c)

# Link with the standard library and libjoybus
target_link_libraries(n64-host pico_stdlib joybus)

pico_enable_stdio_usb(n64-host 1)
pico_enable_stdio_uart(n64-host 0)

# Generate additional binary outputs
pico_add_extra_outputs(n64-host)
