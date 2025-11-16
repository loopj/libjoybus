cmake_minimum_required(VERSION 3.20)

set(JOYBUS_BACKEND_RP2XXX TRUE)
set(LIBJOYBUS_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../../..)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

set(PICO_BOARD pico2 CACHE STRING "Board type")

project(usb_serial C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

add_subdirectory(${LIBJOYBUS_ROOT} ${CMAKE_BINARY_DIR}/_deps/libjoybus-build)

# Add executable. Default name is the project name, version 0.1

add_executable(usb_serial main.c)

pico_enable_stdio_usb(usb_serial 1)
pico_enable_stdio_uart(usb_serial 0)

target_link_libraries(usb_serial pico_stdlib)
pico_add_extra_outputs(usb_serial)

# pull in common dependencies
target_link_libraries(usb_serial pico_stdlib joybus)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(usb_serial)

# add url via pico_set_program_url
