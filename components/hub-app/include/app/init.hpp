#ifndef HUB_INIT_HPP
#define HUB_INIT_HPP

#include "configuration.hpp"

#include "filesystem/filesystem.hpp"
#include "wifi/wifi.hpp"
#include "ble/ble.hpp"
#include "ble/scanner.hpp"
#include "ble/errc.hpp"
#include "ble/mac.hpp"

#include "rapidjson/document.h"
#include "rapidjson/istreamwrapper.h"

#include "esp_mac.h"
#include "esp_log.h"

#include <string>
#include <string_view>
#include <fstream>

namespace hub
{
    class init_t
    {
    public:

        init_t()
        {

        }

        init_t(const init_t&)               = delete;

        init_t(init_t&&)                    = default;

        init_t& operator=(const init_t&)    = delete;

        init_t& operator=(init_t&&)         = default;

        ~init_t()                           = default;

        void initialize_filesystem() const
        {
            filesystem::init();
        }

        void initialize_ble() const
        {
            ble::init();
            ble::scanner::init();
        }

        void connect_to_wifi(std::string_view ssid, std::string_view password) const
        {
            wifi::connect(ssid, password);
        }

        configuration read_config(std::string_view path) const noexcept
        {
            configuration config;
            rapidjson::Document config_json;

            {
                std::ifstream config_file(path.data());
                rapidjson::IStreamWrapper config_ifstream(config_file);

                if (!config_file)
                {
                    ESP_LOGE(TAG, "Could not open config_json.json, aborting...");
                    abort();
                }

                config_json.ParseStream(config_ifstream);
            }

            if (!config_json.IsObject() || !config_json["WIFI_SSID"].IsString() || !config_json["WIFI_PASSWORD"].IsString() || !config_json["MQTT_URI"].IsString())
            {
                ESP_LOGE(TAG, "Bad config_json file format, aborting...");
                abort();
            }

            config.wifi.ssid = config_json["WIFI_SSID"].GetString();
            config.wifi.password = config_json["WIFI_PASSWORD"].GetString();
            esp_read_mac(config.wifi.mac.to_address(), ESP_MAC_WIFI_STA);

            config.mqtt.uri = config_json["MQTT_URI"].GetString();

            return config;
        }

    private:

        static constexpr const char* TAG{ "hub:init_t" };
    };
}

#endif