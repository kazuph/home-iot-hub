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

#include "rapidjson/document.h"
#include "rapidjson/istreamwrapper.h"

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
            if (filesystem::init() != ESP_OK)
            {
                ESP_LOGE(TAG, "Could not initialize filesystem.");
                throw std::runtime_error("Could not initialize filesystem.");
            }
        }

        void initialize_ble() const
        {
            if (!ble::init().is_valid())
            {
                ESP_LOGE(TAG, "Could not initialize BLE module.");
                throw std::runtime_error("Could not initialize BLE module.");
            }

            if (!ble::scanner::init().is_valid())
            {
                ESP_LOGE(TAG, "Could not initialize BLE scanner.");
                ble::deinit();
                throw std::runtime_error("Could not initialize BLE scanner.");
            }
        }

        void connect_to_wifi(const configuration& config) const
        {
            if (wifi::connect(config.wifi.ssid, config.wifi.password) != ESP_OK)
            {
                ESP_LOGE(TAG, "Could not connect to WiFi.");
                throw std::runtime_error("Could not connect to WiFi.");   
            }
        }

        configuration read_config(std::string_view path) const
        {
            configuration config;
            rapidjson::Document config_json;

            {
                std::ifstream config_file(path.data());
                rapidjson::IStreamWrapper config_ifstream(config_file);

                if (!config_file)
                {
                    ESP_LOGE(TAG, "Could not open config_json.json.");
                    throw std::runtime_error("Could not open config_json.json");
                }

                config_json.ParseStream(config_ifstream);
            }

            if (
                !config_json.IsObject() || 
                !config_json["WIFI_SSID"].IsString() || 
                !config_json["WIFI_PASSWORD"].IsString() || 
                !config_json["MQTT_URI"].IsString() ||
                !config_json["DEVICE_NAME"].IsString())
            {
                ESP_LOGE(TAG, "Bad config_json file format.");
                throw std::runtime_error("Bad config_json file format.");
            }

            config.wifi.ssid = config_json["WIFI_SSID"].GetString();
            config.wifi.password = config_json["WIFI_PASSWORD"].GetString();
            esp_read_mac(config.wifi.mac.to_address(), ESP_MAC_WIFI_STA);

            config.mqtt.uri = config_json["MQTT_URI"].GetString();

            config.hub.device_name = config_json["DEVICE_NAME"].GetString();

            return config;
        }

        void publish_scan_message(const configuration& config) const
        {
            constexpr std::string_view SCAN_TOPIC{ "home/scan" };

            auto mqtt_client = mqtt::client();
            
            if (mqtt_client.connect(config.mqtt.uri) != ESP_OK)
            {
                ESP_LOGE(TAG, "Could not connect to the given URI.");
                throw std::runtime_error("Could not connect to the given URI.");
            }

            {
                rapidjson::Document json;
                json.SetObject();

                json.AddMember("device_name", rapidjson::StringRef(config.hub.device_name), json.GetAllocator());
                json.AddMember("device_address", rapidjson::StringRef(config.wifi.mac.to_string()), json.GetAllocator());

                if (mqtt_client.publish(SCAN_TOPIC, std::string_view(json.GetString(), json.GetStringLength())) != ESP_OK)
                {
                    ESP_LOGE(TAG, "Publish scan message failed..");
                    throw std::runtime_error("Publish scan message failed..");
                }
            }
        }

    private:

        static constexpr const char* TAG{ "hub:init_t" };
    };
}

#endif