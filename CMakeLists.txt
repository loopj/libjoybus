# Set minimum CMake version
cmake_minimum_required(VERSION "3.21")

# Configure project and languages
project(libjoybus LANGUAGES C)

# Add CTest for running tests
include(CTest)

# Define the library
add_library(joybus INTERFACE)
target_sources(
  joybus
  INTERFACE
    src/crc8.c
    src/host/common.c
    src/host/gamecube.c
    src/host/n64.c
    src/target/gc_controller.c
    src/target/n64_controller.c
)

# Specify the include paths
target_include_directories(joybus INTERFACE include)

# Add the appropriate backend
if(DEFINED JOYBUS_BACKEND_GECKO)
  add_subdirectory(src/backend/gecko)
elseif(DEFINED JOYBUS_BACKEND_RP2XXX)
  add_subdirectory(src/backend/rp2xxx)
elseif(DEFINED JOYBUS_BACKEND_LOOPBACK)
  add_subdirectory(src/backend/loopback)
else()
  message(FATAL_ERROR "No valid JOYBUS_BACKEND_* defined")
endif()

# Add the test target
if(NOT CMAKE_CROSSCOMPILING)
  add_subdirectory(test)
endif()
