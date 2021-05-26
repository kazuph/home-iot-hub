#include "filesystem/filesystem.hpp"
#include "wifi/wifi.hpp"
#include "timing/timing.hpp"
#include "ble/ble.hpp"
#include "ble/mac.hpp"
#include "ble/client.hpp"
#include "utils/const_map.hpp"
#include "utils/json.hpp"
#include "utils/result.hpp"
#include "async/rx/operators.hpp"
#include "service/mqtt.hpp"
#include "service/ble.hpp"
#include "service/scanner.hpp"
#include "service/ref.hpp"
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
        return message.topic == service::mqtt::MQTT_BLE_SCAN_ENABLE_TOPIC;
    }

    inline bool is_device_connect_topic(const service::mqtt::out_message_t& message) noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        return message.topic == service::mqtt::MQTT_BLE_CONNECT_TOPIC;
    }

    inline bool is_device_disconnect_topic(const service::mqtt::out_message_t& message) noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        return message.topic == service::mqtt::MQTT_BLE_DISCONNECT_TOPIC;
    }

    inline bool is_device_write_topic(const service::mqtt::out_message_t& message) noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        return message.topic == service::mqtt::MQTT_BLE_DEVICE_WRITE_TOPIC;
    }

    inline utils::result_throwing<rapidjson::Document> mqtt_message_to_json(service::mqtt::out_message_t&& message) noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        return utils::invoke([message{ std::move(message.data) }]() -> rapidjson::Document {
            rapidjson::Document result;

            result.Parse(std::move(message));

            if (result.HasParseError())
            {
                throw std::runtime_error("Parsing failed.");
            }

            return result;
        });
    }

    inline utils::result_throwing<service::ble_scanner::in_message_t> make_scanner_message(utils::result_throwing<rapidjson::Document>&& message) noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        return utils::bind(std::move(message), [](rapidjson::Document&& message) -> utils::result_throwing<service::ble_scanner::in_message_t> {
            return utils::invoke([message{ std::move(message) }]() -> service::ble_scanner::in_message_t {
                if (!message.IsObject() || !message.HasMember("enable") || !message["enable"].IsBool())
                {
                    throw std::runtime_error("enable must be bool.");
                }

                return { message["enable"].GetBool() };
            });
        });
    }

    inline utils::result_throwing<rapidjson::Document> scan_result_to_json(service::ble_scanner::out_message_t&& message) noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        using result_type = utils::result_throwing<rapidjson::Document>;

        rapidjson::Document result;

        result.SetObject();
        result.AddMember("name", rapidjson::StringRef(message.m_name), result.GetAllocator());
        result.AddMember("address", rapidjson::StringRef(message.m_address), result.GetAllocator());

        return result_type::success(std::move(result));
    }

    inline service::mqtt::in_message_t make_mqtt_scan_result_message(utils::result_throwing<rapidjson::Document>&& message) noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        message.get().Accept(writer);
        return { std::string(service::mqtt::MQTT_BLE_SCAN_RESULTS_TOPIC), buffer.GetString() };
    }

    utils::result_throwing<std::weak_ptr<device::device_base>> connect_ble_client(utils::result_throwing<rapidjson::Document>&& message) noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        return utils::bind(std::move(message), [](rapidjson::Document&& message) -> utils::result_throwing<std::weak_ptr<device::device_base>> {
            return utils::invoke([message{ std::move(message) }]() -> std::weak_ptr<device::device_base> {

                if (!message.IsObject() || !message.HasMember("name") || !message.HasMember("address"))
                {
                    throw std::runtime_error("Bad message structure.");
                }

                auto ble_client = device::mappers::make_device(message["name"].GetString());
                ble_client->connect(ble::mac(message["address"].GetString()));
                return ble_client;
            });
        });
    }

    inline utils::result_throwing<service::ble_message_source> make_ble_message_source(utils::result_throwing<std::weak_ptr<device::device_base>>&& client) noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        return utils::bind(std::move(client), [](std::weak_ptr<device::device_base>&& client) {
            return utils::invoke([client{ std::move(client) }]() -> service::ble_message_source { 
                return service::ble_message_source(client); 
            });
        });
    }

    extern "C" void app_main()
    {
        using namespace timing::literals;
        using namespace async::rx::operators;
        using namespace service::operators;

        filesystem::init();

        rapidjson::Document config;

        {
            std::ifstream               config_file("/spiffs/config.json");
            rapidjson::IStreamWrapper   config_ifstream(config_file);

            if (!config_file)
            {
                ESP_LOGE(TAG, "Could not open config.json, aborting...");
                abort();
            }

            config.ParseStream(config_ifstream);
        }

        if (!config.IsObject() || !config["WIFI_SSID"].IsString() || !config["WIFI_PASSWORD"].IsString() || !config["MQTT_URI"].IsString())
        {
            ESP_LOGE(TAG, "Bad config JSON format, aborting...");
            abort();
        }

        wifi::connect(config["WIFI_SSID"].GetString(), config["WIFI_PASSWORD"].GetString());

        wifi::wait_for_connection(10_s);

        if (!ble::init().is_valid())
        {
            abort();
        }

        if (!ble::scanner::init().is_valid())
        {
            abort();
        }

        auto mqtt_service = service::mqtt(config["MQTT_URI"].GetString());

        const auto ble_scan = 
            service::ref(mqtt_service)                                      |
            filter(is_scan_topic)                                           |
            transform(mqtt_message_to_json)                                 |
            transform(make_scanner_message)                                 |
            filter(is_result_valid<service::ble_scanner::in_message_t>)     |
            transform(unwrap_result<service::ble_scanner::in_message_t>)    |
            ble_scanner()                                                   |           
            transform(scan_result_to_json)                                  |
            filter(is_result_valid<rapidjson::Document>)                    |
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
            join()                                                  | 
            sink([](auto&&) { return; });
        
        timing::sleep_for(timing::MAX_DELAY);

        ble::deinit();
        ble::scanner::deinit();
        wifi::disconnect();
        filesystem::deinit();
    }
}