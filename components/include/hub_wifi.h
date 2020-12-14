#ifndef HUB_WIFI_H
#define HUB_WIFI_H

#include "esp_err.h"
#include "esp_wifi.h"

/*
*   Initialize and connect to WiFi. 
*   SSID and password need to be set in sdkconfig.
*/
esp_err_t hub_wifi_connect(wifi_config_t* config);

/*
*   Disconnect and cleanup all resources.
*/
esp_err_t hub_wifi_disconnect();

/*
*   Cleanup WiFi resources.
*/
esp_err_t hub_wifi_cleanup();

#endif