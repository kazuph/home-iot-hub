idf_component_register(
    SRCS 
        "client.cpp"
    INCLUDE_DIRS 
        "include"
    REQUIRES 
        "mqtt"
        "hub-utils")

target_include_directories(${COMPONENT_LIB} PUBLIC ${rxcpp_SOURCE_DIR}/Rx/v2/src ${expected_SOURCE_DIR}/include)
