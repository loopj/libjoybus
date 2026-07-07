cmake_minimum_required(VERSION 3.16)

# libjoybus is pulled in as a local component-manager dependency (see main/idf_component.yml).
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(gcn-target)
