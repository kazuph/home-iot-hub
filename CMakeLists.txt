cmake_minimum_required(VERSION 3.5)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED 20)

set(SUPPORTED_TARGETS esp32)
set(EXTRA_COMPONENT_DIRS ./components)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(home-iot-hub)