# Set the root directory for libjoybus
set(LIBJOYBUS_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR})

# Hand off to ESP-IDF build if being built from idf.py
if(ESP_PLATFORM)
  include(${CMAKE_CURRENT_LIST_DIR}/cmake/esp-idf.cmake)
  return()
endif()

# Set minimum CMake version
cmake_minimum_required(VERSION "3.21")

# Configure project and languages
project(libjoybus LANGUAGES C)

# Define the library
file(GLOB SOURCES src/*.c src/host/*.c src/target/*.c)
add_library(joybus INTERFACE)
target_sources(joybus INTERFACE ${SOURCES})

# Specify the include paths
target_include_directories(joybus INTERFACE include)

# Build tests by default only when libjoybus is the top-level project
option(JOYBUS_BUILD_TESTS "Build libjoybus tests" ${PROJECT_IS_TOP_LEVEL})

# Include an embedded backend if JOYBUS_BACKEND is defined
if(JOYBUS_BACKEND)
  if(NOT IS_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/src/backend/${JOYBUS_BACKEND}")
    message(FATAL_ERROR "Unknown JOYBUS_BACKEND '${JOYBUS_BACKEND}' "
                        "(expected gecko_sdk, rp2xxx, or loopback)")
  endif()
  add_subdirectory(src/backend/${JOYBUS_BACKEND})
endif()

# Include tests if JOYBUS_BUILD_TESTS is enabled
if(JOYBUS_BUILD_TESTS)
  include(CTest)
  add_subdirectory(test)
endif()
