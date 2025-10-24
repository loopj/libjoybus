cmake_minimum_required(VERSION 3.20.0)

list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/../../..)

find_package(Zephyr)

project(app)
target_sources(app PRIVATE main.c)
target_link_libraries(app PRIVATE joybus)
