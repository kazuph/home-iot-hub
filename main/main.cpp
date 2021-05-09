#include "filesystem.hpp"
#include "wifi.hpp"
#include "service_operators.hpp"
#include "mqtt_service.hpp"
#include "ble_service.hpp"
#include "service_ref.hpp"
#include "timing.hpp"
#include "const_map.hpp"
#include "mac.hpp"
#include "json.hpp"
#include "result.hpp"

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
                ESP_LOGE("RESULT CHECK", "%s", err.what());
            }
        }

        return is_valid;
    }

    template<typename T>
    inline T unwrap_result(const utils::result_throwing<T>& result) noexcept
    {
        return result.get();
    }

    inline bool is_scan_topic(const service::mqtt_service::out_message_t& message) noexcept
    {
        return message.m_topic == service::mqtt_service::MQTT_BLE_SCAN_ENABLE_TOPIC;
    }

    inline bool is_device_connect_topic(const service::mqtt_service::out_message_t& message) noexcept
    {
        return message.m_topic == service::mqtt_service::MQTT_BLE_CONNECT_TOPIC;
    }

    inline bool is_device_disconnect_topic(const service::mqtt_service::out_message_t& message) noexcept
    {
        return message.m_topic == service::mqtt_service::MQTT_BLE_DISCONNECT_TOPIC;
    }

    inline bool is_device_write_topic(const service::mqtt_service::out_message_t& message) noexcept
    {
        return message.m_topic == service::mqtt_service::MQTT_BLE_DEVICE_WRITE_TOPIC;
    }

    inline utils::result_throwing<utils::json> mqtt_message_to_json(const service::mqtt_service::out_message_t& message) noexcept
    {
        return utils::catch_as_result([&]() -> utils::json { return utils::json::parse(message.m_data); });
    }

    inline utils::result_throwing<service::ble_service::in_message_t> make_ble_scan_control_message(const utils::result_throwing<utils::json>& message) noexcept
    {
        return utils::bind(message, [](const utils::json& message) -> utils::result_throwing<service::ble_service::in_message_t> {
            return utils::catch_as_result([&]() -> service::ble_service::in_message_t {
                return service::ble_service::scan_control_t{ message["enable"].get<bool>() };
            });
        });
    }

    inline utils::result_throwing<service::ble_service::in_message_t> make_ble_device_connect_message(const utils::result_throwing<utils::json>& message) noexcept
    {
        return utils::bind(message, [](const utils::json& message) -> utils::result_throwing<service::ble_service::in_message_t> {
            return utils::catch_as_result([&]() -> service::ble_service::in_message_t {
                return service::ble_service::device_connect_t{ 
                    ble::mac(message["address"].get<std::string>()),
                    message["id"].get<std::string>(),
                    message["name"].get<std::string>()
                };
            });
        });
    }

    inline utils::result_throwing<service::ble_service::in_message_t> make_ble_device_disconnect_message(const utils::result_throwing<utils::json>& message) noexcept
    {
        return utils::bind(message, [](const utils::json& message) -> utils::result_throwing<service::ble_service::in_message_t> {
            return utils::catch_as_result([&]() -> service::ble_service::in_message_t {
                return service::ble_service::device_disconnect_t{ 
                    message["id"].get<std::string>() 
                };
            });
        });
    }

    inline utils::result_throwing<service::ble_service::in_message_t> make_ble_device_update_message(const utils::result_throwing<utils::json>& message) noexcept
    {
        return utils::bind(message, [](const utils::json& message) -> utils::result_throwing<service::ble_service::in_message_t> {
            return utils::catch_as_result([&]() -> service::ble_service::in_message_t {
                return service::ble_service::device_update_t{ 
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

        static constexpr const char* TAG{ "HUB MAIN" };

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

        auto mqtt_actor   = service::mqtt_service(config["MQTT_URI"].get<std::string>());
        auto ble_actor    = service::ble_service();

        config.clear();

        const auto mqtt_to_ble_scan_pipeline = 
            service::service_ref(mqtt_actor)                                               |
            filter(is_scan_topic)                                                          |
            transform(mqtt_message_to_json)                                                |
            transform(make_ble_scan_control_message)                                       |
            filter(is_result_valid<service::ble_service::in_message_t>)                    |
            transform(unwrap_result<service::ble_service::in_message_t>)                   |
            service::service_ref(ble_actor);

        const auto mqtt_to_ble_device_connect_pipeline =
            service::service_ref(mqtt_actor)                                               |
            filter(is_device_connect_topic)                                                |
            transform(mqtt_message_to_json)                                                |
            transform(make_ble_device_connect_message)                                     |
            filter(is_result_valid<service::ble_service::in_message_t>)                    |
            transform(unwrap_result<service::ble_service::in_message_t>)                   |
            service::service_ref(ble_actor);

        const auto mqtt_to_ble_device_disconnect_pipeline =
            service::service_ref(mqtt_actor)                                               |
            filter(is_device_disconnect_topic)                                             |
            transform(mqtt_message_to_json)                                                |
            transform(make_ble_device_disconnect_message)                                  |
            filter(is_result_valid<service::ble_service::in_message_t>)                    |
            transform(unwrap_result<service::ble_service::in_message_t>)                   |
            service::service_ref(ble_actor);

        const auto mqtt_to_ble_device_update_pipeline =
            service::service_ref(mqtt_actor)                                               |
            filter(is_device_write_topic)                                                  |
            transform(mqtt_message_to_json)                                                |
            transform(make_ble_device_update_message)                                      |
            filter(is_result_valid<service::ble_service::in_message_t>)                    |
            transform(unwrap_result<service::ble_service::in_message_t>)                   |
            service::service_ref(ble_actor);
        
        timing::sleep_for(timing::MAX_DELAY);

        wifi::disconnect();
        filesystem::deinit();
    }
}