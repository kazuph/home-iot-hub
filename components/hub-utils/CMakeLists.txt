idf_component_register(SRCS "esp_exception.cpp" "mac.cpp" INCLUDE_DIRS "include")

include(FetchContent)

FetchContent_Declare(
    fmt
    GIT_REPOSITORY  https://github.com/fmtlib/fmt.git
    GIT_TAG         master
)

FetchContent_MakeAvailable(fmt)

target_include_directories(${COMPONENT_LIB} INTERFACE ${rapidjson_SOURCE_DIR}/include)
target_link_libraries(${COMPONENT_LIB} INTERFACE fmt::fmt-header-only)
