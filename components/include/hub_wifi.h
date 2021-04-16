#ifndef HUB_WIFI_H
#define HUB_WIFI_H

#include "hub_timing.h"

#include "esp_err.h"
#include "esp_wifi.h"

#include <string_view>

namespace hub::wifi
{
    /**
     * @brief Initialize and connect to WiFi.
     * 
     * @param ssid 
     * @param password 
     * @return esp_err_t 
     */
    esp_err_t connect(std::string_view ssid, std::string_view password) noexcept;

    /**
    * @brief Disconnect and cleanup all resources.
    * 
    * @return esp_err_t 
    */
    esp_err_t disconnect() noexcept;

    /**
    * @brief Wait for WiFi connection. Number of retries is specified with CONFIG_MAXIMUM_RETRY macro that can be set in sdkconfig.
    * If set to -1, number of retries is infinite. On timeout ESP_ERR_TIMEOUT is returned.
    * 
    * @param timeout 
    * @return esp_err_t 
    */
    esp_err_t wait_for_connection(timing::duration_t timeout) noexcept;
}

#endif