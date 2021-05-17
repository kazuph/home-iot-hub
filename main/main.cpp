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
#include "service/scanner.hpp"

#include "esp_log.h"
#include "esp_heap_caps.h"

#include <utility>
#include <variant>
#include <fstream>
#include <cstdint>

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

    inline utils::result_throwing<service::scanner::in_message_t> make_scanner_message(utils::result_throwing<utils::json>&& message) noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        return utils::bind(std::move(message), [](utils::json&& message) -> utils::result_throwing<service::scanner::in_message_t> {
            return utils::catch_as_result([message{ std::move(message) }]() -> service::scanner::in_message_t {
                return { message[0]["enable"].get<bool>() };
            });
        });
    }

    inline utils::result_throwing<utils::json> scan_result_to_json(service::scanner::out_message_t&& message) noexcept
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

    inline utils::result_throwing<std::shared_ptr<ble::client>> connect_ble_client(utils::result_throwing<utils::json>&& message) noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        return utils::bind(std::move(message), [](utils::json&& message) -> utils::result_throwing<std::shared_ptr<ble::client>> {
            return utils::catch_as_result([message{ std::move(message) }]() -> std::shared_ptr<ble::client> {
                auto client_ptr = ble::client::make_client(message[0]["id"].get<std::string>());
                client_ptr->connect(ble::mac(message[0]["address"].get<std::string>()));
                return client_ptr;
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

        auto mqtt_service       = service::mqtt(config["MQTT_URI"].get<std::string>());
        auto scanner_service    = service::scanner();

        config.clear();

        const auto mqtt_to_ble_scan_pipeline = 
            service::ref(mqtt_service)                                                          |
            filter(is_scan_topic)                                                               |
            transform(mqtt_message_to_json)                                                     |
            transform(make_scanner_message)                                                     |
            filter(is_result_valid<service::scanner::in_message_t>)                             |
            transform(unwrap_result<service::scanner::in_message_t>)                            |
            sink([&](auto&& message) { scanner_service.process_message(std::move(message)); });

        const auto ble_to_mqtt_scan_pipeline =
            service::ref(scanner_service)                                                       |           
            transform(scan_result_to_json)                                                      |
            filter(is_result_valid<utils::json>)                                                |
            transform(make_mqtt_scan_result_message)                                            |
            sink([&](auto&& message) { mqtt_service.process_message(std::move(message)); });

        const auto ble_device_connect_pipeline =
            service::ref(mqtt_service)                                                          |
            filter(is_device_connect_topic)                                                     |
            transform(mqtt_message_to_json)                                                     |
            transform(connect_ble_client)                                                       |
            filter(is_result_valid<std::shared_ptr<ble::client>>)                               |
            sink([](auto client) { ESP_LOGI(TAG, "%s", client.get()->get_id().data()); });
        
        while (true)
        {
            timing::sleep_for(10_s);
            ESP_LOGI(TAG, "Heap free size: %i.", heap_caps_get_free_size(MALLOC_CAP_8BIT));
        }

        ble::deinit();
        wifi::disconnect();
        filesystem::deinit();
    }
}