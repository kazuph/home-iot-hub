idf_component_register(INCLUDE_DIRS "include")

target_include_directories(${COMPONENT_LIB} INTERFACE ${rapidjson_SOURCE_DIR}/include)

target_link_libraries(${COMPONENT_LIB} INTERFACE fmt)
