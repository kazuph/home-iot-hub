idf_component_register(
    SRCS 
        "filesystem.cpp"
    INCLUDE_DIRS 
        "include"
    REQUIRES
        "spiffs"
        "hub-utils")

target_include_directories(${COMPONENT_LIB} PUBLIC ${expected_SOURCE_DIR}/include)
