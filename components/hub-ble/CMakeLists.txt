idf_component_register(
    SRCS
        "ble.cpp"
        "scanner.cpp"
        "client.cpp"
        "service.cpp"
        "characteristic.cpp"
        "descriptor.cpp"
    INCLUDE_DIRS
        "include"
    REQUIRES 
        "bt" 
        "log"
        "hub-utils"
        "hub-timing")

target_include_directories(${COMPONENT_LIB} PUBLIC ${rxcpp_SOURCE_DIR}/Rx/v2/src ${expected_SOURCE_DIR}/include)
