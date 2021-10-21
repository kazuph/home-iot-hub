// #include "esp_mac.h"

#include "rxcpp/rx.hpp"

#include "rapidjson/document.h"

#include "filesystem/filesystem.hpp"
#include "wifi/wifi.hpp"
#include "ble/ble.hpp"
#include "timing/timing.hpp"
#include "utils/json.hpp"
#include "mqtt/client.hpp"

#include "app/consts.hpp"
#include "app/init.hpp"

namespace hub
{
    tl::expected<std::reference_wrapper<init_t>, esp_err_t> init_t::initialize_filesystem() noexcept
    {
        return filesystem::init()
            .and_then([this]() mutable -> tl::expected<std::reference_wrapper<init_t>, esp_err_t> {
                return std::ref(*this);
            });
    }

    tl::expected<std::reference_wrapper<init_t>, esp_err_t> init_t::initialize_ble() noexcept
    {
        return ble::init()
            .and_then([this]() mutable -> tl::expected<std::reference_wrapper<init_t>, esp_err_t> {
                return std::ref(*this);
            });
    }

    tl::expected<std::reference_wrapper<init_t>, esp_err_t> init_t::connect_to_wifi(const configuration& config) noexcept
    {
        using namespace timing::literals;
        return wifi::connect(config.wifi.ssid, config.wifi.password, 60_s)
            .and_then([this]() mutable -> tl::expected<std::reference_wrapper<init_t>, esp_err_t> {
                return std::ref(*this);
            });
    }

    tl::expected<configuration, esp_err_t> init_t::read_config(std::string_view path) const noexcept
    {
        configuration config;

        return std::invoke([path]() -> tl::expected<rapidjson::Document, esp_err_t> {
            auto js_config = utils::json::parse_file(path);
            return (!js_config.HasParseError() && js_config.IsObject()) ?
                std::move(js_config) :
                tl::expected<rapidjson::Document, esp_err_t>(tl::unexpect, ESP_ERR_INVALID_ARG);
        })
            .and_then([&config](rjs::Document&& js_config) -> tl::expected<rapidjson::Document, esp_err_t> {
                if (!js_config["wifi"].IsObject() ||
                    !js_config["wifi"]["ssid"].IsString() ||
                    !js_config["wifi"]["password"].IsString())
                {
                    return tl::make_unexpected<esp_err_t>(ESP_ERR_INVALID_ARG);
                }

                config.wifi.ssid = js_config["wifi"]["ssid"].GetString();
                config.wifi.password = js_config["wifi"]["password"].GetString();

                return std::move(js_config);
            })
            .and_then([&config](rjs::Document&& js_config) -> tl::expected<rapidjson::Document, esp_err_t> {
                if (!js_config["mqtt"].IsObject() ||
                    !js_config["mqtt"]["uri"].IsString())
                {
                    return tl::make_unexpected<esp_err_t>(ESP_ERR_INVALID_ARG);
                }

                config.mqtt.uri = js_config["mqtt"]["uri"].GetString();

                return std::move(js_config);
            })
            .and_then([&config](rjs::Document&& js_config) -> tl::expected<configuration, esp_err_t> {
                if (!js_config["general"].IsObject() ||
                    !js_config["general"]["name"].IsString() ||
                    !js_config["general"]["object_id"].IsString() ||
                    !js_config["general"]["discovery_prefix"].IsString())
                {
                    return tl::make_unexpected<esp_err_t>(ESP_ERR_INVALID_ARG);
                }

                config.general.name = js_config["general"]["name"].GetString();
                config.general.object_id = js_config["general"]["object_id"].GetString();
                config.general.discovery_prefix = js_config["general"]["discovery_prefix"].GetString();

                return std::move(config);
            });
    }

    tl::expected<std::reference_wrapper<init_t>, esp_err_t> init_t::send_mqtt_config(const configuration& config) noexcept
    {
        using namespace std::literals;

        namespace rx = rxcpp;
        using namespace rx::operators;

        const auto make_topic = [](std::string_view topic_prefix, std::string_view topic_postfix) {
            constexpr std::string_view topic_fmt{ "{0}/{1}" };
            return fmt::format(topic_fmt, topic_prefix, topic_postfix);
        };

        const auto switch_topic_prefix = fmt::format(
            TOPIC_PREFIX_FMT,
            config.general.discovery_prefix,
            SWITCH_DEVICE_NAME,
            config.general.object_id);

        const auto sensor_topic_prefix = fmt::format(
            TOPIC_PREFIX_FMT,
            config.general.discovery_prefix,
            SENSOR_DEVICE_NAME,
            config.general.object_id);

        auto mqtt_client = mqtt::make_client(config.mqtt.uri)
            .or_else([](esp_err_t err) {
                ESP_LOGE(TAG, "MQTT client initialization failed.");
            })
            .value();  // Factory of observables and subscribers. Abort on error.

        /*
        *   Publish configuration messages on sensor and switch configuration topics.
        */

        std::string config_topic = make_topic(switch_topic_prefix, "config");
        std::string config_payload = fmt::format(
            SWITCH_CONFIG_PAYLOAD_FMT,
            switch_topic_prefix,
            config.general.name);

        rx::observable<>::from<std::string_view>(config_payload) |
            mqtt_client.publish(config_topic) |
            subscribe<int>();

        config_topic = make_topic(sensor_topic_prefix, "config");
        config_payload = fmt::format(
            SENSOR_CONFIG_PAYLOAD_FMT,
            sensor_topic_prefix,
            config.general.name);

        rx::observable<>::from<std::string_view>(config_payload) |
            mqtt_client.publish(config_topic) |
            subscribe<int>();

        return std::ref(*this);
    }
}
