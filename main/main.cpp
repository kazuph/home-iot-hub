#include "filesystem/filesystem.hpp"
#include "wifi/wifi.hpp"
#include "timing/timing.hpp"
#include "ble/mac.hpp"
#include "utils/const_map.hpp"
#include "utils/json.hpp"
#include "utils/result.hpp"
#include "service/operators.hpp"
#include "service/mqtt.hpp"
#include "service/ble.hpp"
#include "service/ref.hpp"

#include "esp_log.h"

#include <utility>
#include <variant>
#include <fstream>

namespace hub
{
    template<typename T>
    inline bool is_result_valid(const utils::result_throwing<T>& result) noexcept
    {
        bool is_valid = result.is_valid();
        
        if (!is_valid)
        {
            try
            {
                std::rethrow_exception(result.error());
            }
            catch (const std::exception& err)
            {
                ESP_LOGE("hub::is_result_valid", "%s", err.what());
            }
        }

        return is_valid;
    }

    template<typename T>
    inline T unwrap_result(const utils::result_throwing<T>& result) noexcept
    {
        return result.get();
    }

    inline bool is_scan_topic(const service::mqtt::out_message_t& message) noexcept
    {
        return message.m_topic == service::mqtt::MQTT_BLE_SCAN_ENABLE_TOPIC;
    }

    inline bool is_device_connect_topic(const service::mqtt::out_message_t& message) noexcept
    {
        return message.m_topic == service::mqtt::MQTT_BLE_CONNECT_TOPIC;
    }

    inline bool is_device_disconnect_topic(const service::mqtt::out_message_t& message) noexcept
    {
        return message.m_topic == service::mqtt::MQTT_BLE_DISCONNECT_TOPIC;
    }

    inline bool is_device_write_topic(const service::mqtt::out_message_t& message) noexcept
    {
        return message.m_topic == service::mqtt::MQTT_BLE_DEVICE_WRITE_TOPIC;
    }

    inline utils::result_throwing<utils::json> mqtt_message_to_json(const service::mqtt::out_message_t& message) noexcept
    {
        return utils::catch_as_result([&]() -> utils::json { return utils::json::parse(message.m_data); });
    }

    inline utils::result_throwing<service::ble::in_message_t> make_ble_scan_control_message(const utils::result_throwing<utils::json>& message) noexcept
    {
        return utils::bind(message, [](const utils::json& message) -> utils::result_throwing<service::ble::in_message_t> {
            return utils::catch_as_result([&]() -> service::ble::in_message_t {
                return service::ble::scan_control_t{ message["enable"].get<bool>() };
            });
        });
    }

    inline utils::result_throwing<service::ble::in_message_t> make_ble_device_connect_message(const utils::result_throwing<utils::json>& message) noexcept
    {
        return utils::bind(message, [](const utils::json& message) -> utils::result_throwing<service::ble::in_message_t> {
            return utils::catch_as_result([&]() -> service::ble::in_message_t {
                return service::ble::device_connect_t{ 
                    ble::mac(message["address"].get<std::string>()),
                    message["id"].get<std::string>(),
                    message["name"].get<std::string>()
                };
            });
        });
    }

    inline utils::result_throwing<service::ble::in_message_t> make_ble_device_disconnect_message(const utils::result_throwing<utils::json>& message) noexcept
    {
        return utils::bind(message, [](const utils::json& message) -> utils::result_throwing<service::ble::in_message_t> {
            return utils::catch_as_result([&]() -> service::ble::in_message_t {
                return service::ble::device_disconnect_t{ 
                    message["id"].get<std::string>() 
                };
            });
        });
    }

    inline utils::result_throwing<service::ble::in_message_t> make_ble_device_update_message(const utils::result_throwing<utils::json>& message) noexcept
    {
        return utils::bind(message, [](const utils::json& message) -> utils::result_throwing<service::ble::in_message_t> {
            return utils::catch_as_result([&]() -> service::ble::in_message_t {
                return service::ble::device_update_t{ 
                    message["id"].get<std::string>(),
                    message["data"]
                };
            });
        });
    }

    extern "C" void app_main()
    {
        using namespace timing::literals;
        using namespace service::operators;

        static constexpr const char* TAG{ "hub::app_main" };

        filesystem::init();

        utils::json config;

        {
            std::ifstream ifs("/spiffs/config.json");

            if (!ifs)
            {
                ESP_LOGE(TAG, "Could not open config.json, aborting...");
                abort();
            }

            ifs >> config;
        }

        wifi::connect(config["WIFI_SSID"].get<std::string>(), config["WIFI_PASSWORD"].get<std::string>());
        wifi::wait_for_connection(10_s);

        auto mqtt_service   = service::mqtt(config["MQTT_URI"].get<std::string>());
        auto ble_service    = service::ble();

        config.clear();

        const auto mqtt_to_ble_scan_pipeline = 
            service::ref(mqtt_service)                                               |
            filter(is_scan_topic)                                                          |
            transform(mqtt_message_to_json)                                                |
            transform(make_ble_scan_control_message)                                       |
            filter(is_result_valid<service::ble::in_message_t>)                    |
            transform(unwrap_result<service::ble::in_message_t>)                   |
            service::ref(ble_service);

        const auto mqtt_to_ble_device_connect_pipeline =
            service::ref(mqtt_service)                                               |
            filter(is_device_connect_topic)                                                |
            transform(mqtt_message_to_json)                                                |
            transform(make_ble_device_connect_message)                                     |
            filter(is_result_valid<service::ble::in_message_t>)                    |
            transform(unwrap_result<service::ble::in_message_t>)                   |
            service::ref(ble_service);

        const auto mqtt_to_ble_device_disconnect_pipeline =
            service::ref(mqtt_service)                                               |
            filter(is_device_disconnect_topic)                                             |
            transform(mqtt_message_to_json)                                                |
            transform(make_ble_device_disconnect_message)                                  |
            filter(is_result_valid<service::ble::in_message_t>)                    |
            transform(unwrap_result<service::ble::in_message_t>)                   |
            service::ref(ble_service);

        const auto mqtt_to_ble_device_update_pipeline =
            service::ref(mqtt_service)                                               |
            filter(is_device_write_topic)                                                  |
            transform(mqtt_message_to_json)                                                |
            transform(make_ble_device_update_message)                                      |
            filter(is_result_valid<service::ble::in_message_t>)                    |
            transform(unwrap_result<service::ble::in_message_t>)                   |
            service::ref(ble_service);
        
        timing::sleep_for(timing::MAX_DELAY);

        wifi::disconnect();
        filesystem::deinit();
    }
}