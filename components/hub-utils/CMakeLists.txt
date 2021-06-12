idf_component_register(INCLUDE_DIRS "include")

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

FetchContent_GetProperties(rapidjson)
if(NOT rapidjson_POPULATED)
  FetchContent_Populate(rapidjson)
endif()

FetchContent_GetProperties(rxcpp)
if(NOT rxcpp_POPULATED)
  FetchContent_Populate(rxcpp)
endif()

target_include_directories(${COMPONENT_LIB} INTERFACE ${rapidjson_SOURCE_DIR}/include)
target_include_directories(${COMPONENT_LIB} INTERFACE ${rxcpp_SOURCE_DIR}/Rx/v2/src)

target_compile_definitions(${COMPONENT_LIB} INTERFACE RAPIDJSON_HAS_STDSTRING=1 RAPIDJSON_ALLOCATOR_DEFAULT_CHUNK_CAPACITY=1024)