#include "filesystem/filesystem.hpp"
#include "wifi/wifi.hpp"
#include "timing/timing.hpp"
#include "ble/ble.hpp"
#include "ble/mac.hpp"
#include "ble/client.hpp"
#include "utils/const_map.hpp"
#include "utils/json.hpp"
#include "utils/result.hpp"
#include "service/operators.hpp"
#include "service/mqtt.hpp"
#include "service/ble.hpp"
#include "service/scanner.hpp"
#include "mappers/mappers.hpp"

#include "esp_log.h"

#include <utility>
#include <variant>
#include <fstream>
#include <cstdint>
#include <list>

namespace hub
{
    static constexpr const char* TAG{ "hub::app_main" };

    template<typename T>
    inline bool is_result_valid(const utils::result_throwing<T>& result) noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        bool valid = result.is_valid();

        if (!valid)
        {
            try
            {
                std::rethrow_exception(result.error());
            }
            catch (const std::exception& err)
            {
                ESP_LOGE(TAG, "%s", err.what());
            }
        }

        return valid;
    }

    template<typename T>
    inline T unwrap_result(utils::result_throwing<T>&& result) noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        return std::move(result.get());
    }

    inline bool is_scan_topic(const service::mqtt::out_message_t& message) noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        return message.m_topic == service::mqtt::MQTT_BLE_SCAN_ENABLE_TOPIC;
    }

    inline bool is_device_connect_topic(const service::mqtt::out_message_t& message) noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        return message.m_topic == service::mqtt::MQTT_BLE_CONNECT_TOPIC;
    }

    inline bool is_device_disconnect_topic(const service::mqtt::out_message_t& message) noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        return message.m_topic == service::mqtt::MQTT_BLE_DISCONNECT_TOPIC;
    }

    inline bool is_device_write_topic(const service::mqtt::out_message_t& message) noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        return message.m_topic == service::mqtt::MQTT_BLE_DEVICE_WRITE_TOPIC;
    }

    inline utils::result_throwing<utils::json> mqtt_message_to_json(service::mqtt::out_message_t&& message) noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        return utils::catch_as_result([message{ std::move(message) }]() -> utils::json { return utils::json::parse(std::move(message.m_data)); });
    }

    inline utils::result_throwing<service::ble_scanner::in_message_t> make_scanner_message(utils::result_throwing<utils::json>&& message) noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        return utils::bind(std::move(message), [](utils::json&& message) -> utils::result_throwing<service::ble_scanner::in_message_t> {
            return utils::catch_as_result([message{ std::move(message) }]() -> service::ble_scanner::in_message_t {
                return { message[0]["enable"].get<bool>() };
            });
        });
    }

    inline utils::result_throwing<utils::json> scan_result_to_json(service::ble_scanner::out_message_t&& message) noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        return utils::catch_as_result([message{ std::move(message) }]() -> utils::json {
            return utils::json::object({ { "name", std::move(message.m_name) }, { "address", std::move(message.m_address) } });
        });
    }

    inline service::mqtt::in_message_t make_mqtt_scan_result_message(utils::result_throwing<utils::json>&& message) noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        return { std::string(service::mqtt::MQTT_BLE_SCAN_RESULTS_TOPIC), message.get().dump() };
    }

    utils::result_throwing<std::weak_ptr<ble::client>> connect_ble_client(utils::result_throwing<utils::json>&& message) noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        return utils::bind(std::move(message), [](utils::json&& message) -> utils::result_throwing<std::weak_ptr<ble::client>> {
            return utils::catch_as_result([message{ std::move(message) }]() -> std::weak_ptr<ble::client> {
                using namespace device::mappers;

                auto client_ptr = ble::client::make_client(message[0].at("id").get<std::string>());
                client_ptr->connect(ble::mac(message[0].at("address").get<std::string>()));

                auto on_after_connect = get_operation_for<operation_type::on_after_connect>(message[0].at("name").get<std::string>());
                on_after_connect(client_ptr);
                
                return client_ptr;
            });
        });
    }

    inline utils::result_throwing<service::ble_message_source> make_ble_message_source(utils::result_throwing<std::weak_ptr<ble::client>>&& client) noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        return utils::bind(std::move(client), [](std::weak_ptr<ble::client>&& client) {
            return utils::catch_as_result([client{ std::move(client) }]() -> service::ble_message_source { 
                return service::ble_message_source(client); 
            });
        });
    }

    extern "C" void app_main()
    {
        using namespace timing::literals;
        using namespace service::operators;

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

        wifi::connect(
            config["WIFI_SSID"].get<std::string>(), 
            config["WIFI_PASSWORD"].get<std::string>());

        wifi::wait_for_connection(10_s);

        ble::init();
        ble::scanner::init();

        auto mqtt_service = service::mqtt(config["MQTT_URI"].get<std::string>());

        config.clear();

        const auto ble_scan = 
            service::ref(mqtt_service)                                      |
            filter(is_scan_topic)                                           |
            transform(mqtt_message_to_json)                                 |
            transform(make_scanner_message)                                 |
            filter(is_result_valid<service::ble_scanner::in_message_t>)     |
            transform(unwrap_result<service::ble_scanner::in_message_t>)    |
            ble_scanner()                                                   |           
            transform(scan_result_to_json)                                  |
            filter(is_result_valid<utils::json>)                            |
            transform(make_mqtt_scan_result_message)                        |
            sink([mqtt_service{ service::ref(mqtt_service) }](auto&& message) { 
                mqtt_service.process_message(std::move(message)); 
            });

        const auto ble_clients =
            service::ref(mqtt_service)                              |
            filter(is_device_connect_topic)                         |
            transform(mqtt_message_to_json)                         |
            transform(connect_ble_client)                           |
            transform(make_ble_message_source)                      |
            filter(is_result_valid<service::ble_message_source>)    |
            transform(unwrap_result<service::ble_message_source>)   |
            join();

        //const auto ble_client_to_mqtt_pipeline = ble_clients | sink([](auto&&) { return; });
        
        timing::sleep_for(timing::MAX_DELAY);

        ble::deinit();
        ble::scanner::deinit();
        wifi::disconnect();
        filesystem::deinit();
    }
}