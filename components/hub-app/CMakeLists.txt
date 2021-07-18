idf_component_register(
    INCLUDE_DIRS 
        "include" 
    REQUIRES 
        "hub-filesystem"
        "hub-wifi"
        "hub-ble"
        "hub-utils"
        "hub-mqtt"
        "hub-timing")

target_include_directories(${COMPONENT_LIB} INTERFACE ${rapidjson_SOURCE_DIR}/include ${expected_SOURCE_DIR}/include ${rxcpp_SOURCE_DIR}/Rx/v2/src)

