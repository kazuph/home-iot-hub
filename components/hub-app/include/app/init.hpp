#ifndef HUB_INIT_HPP
#define HUB_INIT_HPP

#include "configuration.hpp"

#include "filesystem/filesystem.hpp"
#include "wifi/wifi.hpp"
#include "ble/ble.hpp"
#include "ble/scanner.hpp"
#include "ble/errc.hpp"
#include "ble/mac.hpp"
#include "mqtt/client.hpp"
#include "timing/timing.hpp"
#include "utils/esp_exception.hpp"
#include "utils/json.hpp"

#include "rxcpp/rx.hpp"

#include "rapidjson/document.h"

#include "tl/expected.hpp"

#include "esp_mac.h"
#include "esp_log.h"

#include <string>
#include <string_view>
#include <fstream>
#include <stdexcept>

namespace hub
{
    class init_t
    {
    public:

        init_t()                            = default;

        init_t(const init_t&)               = delete;

        init_t(init_t&&)                    = default;

        init_t& operator=(const init_t&)    = delete;

        init_t& operator=(init_t&&)         = default;

        ~init_t()                           = default;

        tl::expected<void, esp_err_t> initialize_filesystem() const noexcept
        {
            return filesystem::init();
        }

        tl::expected<void, esp_err_t> initialize_ble() const noexcept
        {
            using result_type = tl::expected<void, esp_err_t>;

            auto result =  ble::init();
            return (result.is_valid()) ? result_type() : result_type(tl::unexpect, ESP_FAIL);
        }

        tl::expected<void, esp_err_t> connect_to_wifi(const configuration& config) const noexcept
        {
            using namespace timing::literals;
            return wifi::connect(config.wifi.ssid, config.wifi.password, 60_s);
        }

        tl::expected<configuration, esp_err_t> read_config(std::string_view path) const
        {
            using result_type = tl::expected<configuration, esp_err_t>;

            configuration config;
            rapidjson::Document config_json = utils::json::parse_file(path);

            if (
                config_json.HasParseError() ||
                !config_json.IsObject() || 
                !config_json["WIFI_SSID"].IsString() || 
                !config_json["WIFI_PASSWORD"].IsString() || 
                !config_json["MQTT_URI"].IsString() ||
                !config_json["DEVICE_NAME"].IsString())
            {
                return result_type(tl::unexpect, ESP_ERR_INVALID_ARG);
            }

            esp_read_mac(static_cast<uint8_t*>(config.wifi.mac), ESP_MAC_WIFI_STA);

            config.wifi.ssid        = config_json["WIFI_SSID"].GetString();
            config.wifi.password    = config_json["WIFI_PASSWORD"].GetString();
            config.mqtt.uri         = config_json["MQTT_URI"].GetString();
            config.hub.device_name  = config_json["DEVICE_NAME"].GetString();

            return config;
        }

    private:

        static constexpr const char* TAG{ "hub:init_t" };
    };
}

#endif