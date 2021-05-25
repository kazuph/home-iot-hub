idf_component_register(INCLUDE_DIRS "include")

include(FetchContent)

FetchContent_Declare(
    rapidjson
    GIT_REPOSITORY  https://github.com/Tencent/rapidjson.git
    GIT_TAG         master
)

FetchContent_GetProperties(rapidjson)
if(NOT rapidjson_POPULATED)
  FetchContent_Populate(rapidjson)
endif()

target_include_directories(${COMPONENT_LIB} INTERFACE ${rapidjson_SOURCE_DIR}/include)