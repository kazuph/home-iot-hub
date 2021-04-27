idf_component_register(
    SRCS
       "client.cpp"
       "service.cpp"
       "characteristic.cpp"
       "descriptor.cpp"
       "mac.cpp"     
    INCLUDE_DIRS
        "include"
    REQUIRES 
        "bt" 
        "log"
        "event"
        "error"
        "timing")

include(FetchContent)

FetchContent_Declare(
    range-v3
    GIT_REPOSITORY  https://github.com/ericniebler/range-v3.git
    GIT_TAG         0.11.0
)

FetchContent_GetProperties(range-v3)
if(NOT range-v3_POPULATED)
    FetchContent_Populate(range-v3)
    add_subdirectory(${range-v3_SOURCE_DIR} ${range-v3_BINARY_DIR})
endif()

target_include_directories(${COMPONENT_LIB} PUBLIC ${range-v3_SOURCE_DIR}"/include")

target_link_libraries(${COMPONENT_LIB} INTERFACE range-v3)