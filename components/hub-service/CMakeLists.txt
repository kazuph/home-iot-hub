idf_component_register(
    SRCS 
        "mqtt_client.cpp"
    INCLUDE_DIRS 
        "include"
    REQUIRES 
        "mqtt"
        "hub-utils"
        "hub-mqtt"
        "hub-ble")

target_include_directories(${COMPONENT_LIB} PUBLIC ${rxcpp_SOURCE_DIR}/Rx/v2/src)
