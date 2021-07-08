cmake_minimum_required(VERSION 3.5)

include(FetchContent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SUPPORTED_TARGETS esp32)
set(EXTRA_COMPONENT_DIRS ./components)

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
    fmt
    GIT_REPOSITORY  https://github.com/fmtlib/fmt.git
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

FetchContent_GetProperties(fmt)
if(NOT fmt_POPULATED)
  FetchContent_Populate(fmt)
  add_subdirectory(${fmt_SOURCE_DIR} ${fmt_BINARY_DIR})
endif()

FetchContent_GetProperties(expected)
if(NOT expected_POPULATED)
  FetchContent_Populate(expected)
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(home-iot-hub)
