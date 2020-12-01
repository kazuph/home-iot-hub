#pragma once
#include "esp_err.h"

/*
*   Initialize and connect to WiFi. 
*   SSID and password need to be set in sdkconfig.
*/
esp_err_t hub_wifi_connect();

/*
*   Disconnect and cleanup all resources.
*/
void hub_wifi_disconnect();