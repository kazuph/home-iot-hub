idf_component_register(
    SRCS 
        "wifi.cpp"
    INCLUDE_DIRS 
        "include"
    REQUIRES
        "log"
        "nvs_flash"
        "hub-timing"
        "hub-utils")

target_include_directories(${COMPONENT_LIB} PUBLIC ${expected_SOURCE_DIR}/include)
