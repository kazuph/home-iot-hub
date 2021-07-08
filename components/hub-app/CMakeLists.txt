idf_component_register(
    INCLUDE_DIRS 
        "include" 
    REQUIRES 
        "hub-filesystem"
        "hub-wifi"
        "hub-ble"
        "hub-utils"
        "hub-timing")

target_include_directories(${COMPONENT_LIB} INTERFACE ${rapidjson_SOURCE_DIR}/include)
target_include_directories(${COMPONENT_LIB} INTERFACE ${expected_SOURCE_DIR}/include)
target_include_directories(${COMPONENT_LIB} INTERFACE ${rxcpp_SOURCE_DIR}/Rx/v2/src)
