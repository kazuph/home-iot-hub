#ifndef HUB_WIFI_HPP
#define HUB_WIFI_HPP

#include "timing/timing.hpp"
#include "tl/expected.hpp"

#include "esp_err.h"
#include "esp_wifi.h"

#include <string_view>

namespace hub::wifi
{
    /**
     * @brief Connect to WiFi.
     * 
     * @param ssid 
     * @param password 
     * @param timeout 
     * @return esp_err_t 
     */
    tl::expected<void, esp_err_t> connect(std::string_view ssid, std::string_view password, timing::duration_t timeout = timing::seconds(10)) noexcept;

    /**
    * @brief Disconnect and cleanup all resources.
    * 
    * @return esp_err_t 
    */
    tl::expected<void, esp_err_t> disconnect() noexcept;
}

#endif