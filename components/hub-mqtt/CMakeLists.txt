idf_component_register(
    SRCS 
        "client.cpp"
    INCLUDE_DIRS 
        "include"
    REQUIRES
        "mqtt"
        "hub-event")
