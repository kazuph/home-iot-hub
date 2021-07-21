#include "app/init.hpp"

#include "filesystem/filesystem.hpp"
#include "wifi/wifi.hpp"
#include "ble/ble.hpp"
#include "timing/timing.hpp"
#include "utils/json.hpp"

#include "rapidjson/document.h"

#include "esp_mac.h"

namespace hub
{
    tl::expected<void, esp_err_t> init_t::initialize_filesystem() const noexcept
    {
        return filesystem::init();
    }

    tl::expected<void, esp_err_t> init_t::initialize_ble() const noexcept
    {
        return ble::init();
    }

    tl::expected<void, esp_err_t> init_t::connect_to_wifi(const configuration& config) const noexcept
    {
        using namespace timing::literals;
        return wifi::connect(config.wifi.ssid, config.wifi.password, 60_s);
    }

    tl::expected<configuration, esp_err_t> init_t::read_config(std::string_view path) const
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
}
