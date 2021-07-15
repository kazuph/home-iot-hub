cmake_minimum_required(VERSION 3.5)

set(SUPPORTED_TARGETS esp32)
set(EXTRA_COMPONENT_DIRS ./components)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

include(FetchContent)

FetchContent_Declare(
    rapidjson
    GIT_REPOSITORY  https://github.com/Tencent/rapidjson.git
    GIT_TAG         master
)

FetchContent_Declare(
    rxcpp
    GIT_REPOSITORY  https://github.com/ReactiveX/RxCpp.git
    GIT_TAG         master
)

FetchContent_Declare(
    expected
    GIT_REPOSITORY  https://github.com/TartanLlama/expected.git
    GIT_TAG         master
)

FetchContent_GetProperties(rapidjson)
if(NOT rapidjson_POPULATED)
  FetchContent_Populate(rapidjson)
  add_compile_definitions(RAPIDJSON_HAS_STDSTRING=1 RAPIDJSON_ALLOCATOR_DEFAULT_CHUNK_CAPACITY=128)
endif()

FetchContent_GetProperties(rxcpp)
if(NOT rxcpp_POPULATED)
  FetchContent_Populate(rxcpp)
endif()

FetchContent_GetProperties(expected)
if(NOT expected_POPULATED)
  FetchContent_Populate(expected)
endif()

project(home-iot-hub)
