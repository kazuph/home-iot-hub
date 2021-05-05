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

#include "esp_log.h"

#include <utility>
#include <variant>
#include <fstream>

extern "C" void app_main()
{
    using namespace hub;
    using namespace timing::literals;
    using namespace service::operators;

    utils::json config;

    filesystem::init();

    {
        std::ifstream ifs("/spiffs/config.json");

        if (!ifs)
        {
            ESP_LOGE("MAIN", "Could not open config.json.");
            abort();
        }

        ifs >> config;
    }

    wifi::connect(config["WIFI_SSID"].get<std::string>(), config["WIFI_PASSWORD"].get<std::string>());
    wifi::wait_for_connection(10_s);

    auto mqtt_service   = service::mqtt_service(config["MQTT_URI"].get<std::string>());
    auto ble_service    = service::ble_service();

    config.clear();

    auto mqtt_data_to_json = [](service::mqtt_service::out_message_t&& message) {
        return utils::json::parse(message.m_data);
    };

    auto is_scan_topic = [](auto&& message) {
        return message.m_topic == service::mqtt_service::MQTT_BLE_SCAN_ENABLE_TOPIC;
    };

    auto is_device_connect_topic = [](auto&& message) {
        return message.m_topic == service::mqtt_service::MQTT_BLE_CONNECT_TOPIC;
    };

    auto is_device_disconnect_topic = [](auto&& message) {
        return message.m_topic == service::mqtt_service::MQTT_BLE_DISCONNECT_TOPIC;
    };

    auto is_device_write_topic = [](auto&& message) {
        return message.m_topic == service::mqtt_service::MQTT_BLE_DEVICE_WRITE_TOPIC;
    };

    auto make_ble_scan_control_message = [](utils::json&& data) {
        return service::ble_service::in_message_t(
            service::ble_service::scan_control_t{ 
                data["enable"].get<bool>() 
            }
        );
    };

    auto make_ble_device_connect_message = [](utils::json&& data) {
        return service::ble_service::in_message_t(
            service::ble_service::device_connect_t{ 
                ble::mac(data["address"].get<std::string>()),
                data["id"].get<std::string>(),
                data["name"].get<std::string>()
            }
        );
    };

    auto make_ble_device_disconnect_message = [](utils::json&& data) {
        return service::ble_service::in_message_t(
            service::ble_service::device_disconnect_t{ 
                data["id"].get<std::string>() 
            }
        );
    };

    auto make_ble_device_update_message = [](utils::json&& data) {
        return service::ble_service::in_message_t(
            service::ble_service::device_update_t{ 
                data["id"].get<std::string>(),
                data["data"]
            }
        );
    };

    auto mqtt_to_ble_scan_pipeline = 
        service::service_ref(mqtt_service)              |
        filter(is_scan_topic)                           |
        transform(mqtt_data_to_json)                    |
        transform(make_ble_scan_control_message)        |
        service::service_ref(ble_service);

    auto mqtt_to_ble_device_connect_pipeline =
        service::service_ref(mqtt_service)              |
        filter(is_device_connect_topic)                 |
        transform(mqtt_data_to_json)                    |
        transform(make_ble_device_connect_message)      |
        service::service_ref(ble_service);

    auto mqtt_to_ble_device_disconnect_pipeline =
        service::service_ref(mqtt_service)              |
        filter(is_device_disconnect_topic)              |
        transform(mqtt_data_to_json)                    |
        transform(make_ble_device_disconnect_message)   |
        service::service_ref(ble_service);

    auto mqtt_to_ble_device_update_pipeline =
        service::service_ref(mqtt_service)              |
        filter(is_device_write_topic)                   |
        transform(mqtt_data_to_json)                    |
        transform(make_ble_device_update_message)       |
        service::service_ref(ble_service);
    
    timing::sleep_for(timing::MAX_DELAY);

    wifi::disconnect();
    ble::deinit();
    filesystem::deinit();
}