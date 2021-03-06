idf_component_register(
    SRCS 
    "hub_mqtt.cpp" 
    "hub_wifi.cpp" 
    "hub_ble.cpp"
    "hub_ble_mac.cpp"
    "device_mikettle.cpp" 
    "hub_device.cpp" 
    "hub_device_factory.cpp"
    "hub_mutex.cpp"
    "hub_json.cpp"
    INCLUDE_DIRS "include" REQUIRES "mqtt" "log" "nvs_flash" "bt" "json", "vfs")