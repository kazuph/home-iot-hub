#ifndef HUB_WIFI_H
#define HUB_WIFI_H

#include "esp_err.h"
#include "esp_wifi.h"

#include <string_view>

namespace hub::wifi
{

    /*
    *   Initialize and connect to WiFi. 
    *   SSID and password need to be set in sdkconfig.
    */
    esp_err_t connect(std::string_view ssid, std::string_view password);

    /*
    *   Disconnect and cleanup all resources.
    */
    esp_err_t disconnect();

    /*
    *   Wait for WiFi connection. Number of retries is specified
    *   with CONFIG_MAXIMUM_RETRY macro that can be set in sdkconfig.
    *   If set to -1, number of retries is infinite.
    */
    esp_err_t wait_for_connection(const int timeout);
}

#endif