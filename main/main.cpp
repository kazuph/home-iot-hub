#include "hub_wifi.h"
#include "hub_mqtt.h"
#include "hub_ble.h"
#include "hub_device_factory.h"
#include "hub_utils.h"

#include <cstdio>
#include <cstring>
#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <algorithm>
#include <list>
#include <utility>
#include <vector>

#include "esp_system.h"
#include "esp_event.h"

#include "esp_log.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "nvs.h"
#include "nvs_flash.h"

extern "C"
{
    #include "cJSON.h"
}

namespace hub
{
    using namespace std::literals;

    using device_ptr_t = std::unique_ptr<hub::device_base>;

    constexpr uint16_t  WIFI_CONNECTION_TIMEOUT { 10000U }; // ms
    constexpr uint16_t  BLE_SCAN_TIME           { 20U }; // s

    constexpr auto      MQTT_BLE_SCAN_ENABLE_TOPIC  { "/scan/enable"sv };
    constexpr auto      MQTT_BLE_SCAN_RESULTS_TOPIC { "/scan/results"sv };
    constexpr auto      MQTT_BLE_CONNECT_TOPIC      { "/connect"sv };
    constexpr auto      MQTT_BLE_DISCONNECT_TOPIC   { "/disconnect"sv };
    static const auto   MQTT_BLE_DEVICE_TOPIC       { "/device/"s }; // Device IDs are appended to this topic

    constexpr const char* TAG{ "HUB_MAIN" };

    static esp_err_t    app_init();

    static void         app_cleanup();

    static void         ble_start_scanning(uint16_t scan_time = BLE_SCAN_TIME);

    static void         ble_stop_scanning();

    static void         ble_scan_callback(std::string_view name, const ble::mac& address);

    static esp_err_t    ble_connect(std::string_view id, std::string_view name, const ble::mac& address);

    static void         mqtt_data_callback(std::string_view topic, std::string_view data);

    static mqtt::client                                 mqtt_client{};

    static std::map<ble::mac, std::string_view>         scan_results{};

    static std::map<std::string_view, device_ptr_t>     connected_devices{};
    static std::map<std::string_view, device_ptr_t>     disconnected_devices{};

    utils::dispatch_queue<4096, tskIDLE_PRIORITY>       task_queue{};

    extern "C" void app_main()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        if (app_init() != ESP_OK)
        {
            ESP_LOGE(TAG, "Application initialization failed.");
            goto restart;
        }
        ESP_LOGI(TAG, "Application initialization success.");

        mqtt_client.subscribe(MQTT_BLE_SCAN_ENABLE_TOPIC);
        mqtt_client.subscribe(MQTT_BLE_CONNECT_TOPIC);
        mqtt_client.subscribe(MQTT_BLE_DISCONNECT_TOPIC);

        return;

    restart:
        app_cleanup();
        esp_restart();
    }

    static esp_err_t app_init()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        esp_err_t result = ESP_OK;

        result = nvs_flash_init();
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "NVS flash initialization failed.");
            return ESP_FAIL;
        }

        result = esp_event_loop_create_default();
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Default event loop creation failed.");
            goto cleanup_nvs;
        }

        {
            wifi_config_t wifi_config{};
            std::strcpy(reinterpret_cast<char*>(wifi_config.sta.ssid), CONFIG_WIFI_SSID);
            std::strcpy(reinterpret_cast<char*>(wifi_config.sta.password), CONFIG_WIFI_PASSWORD);
            wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

            result = wifi::connect(&wifi_config);
            if (result != ESP_OK)
            {
                ESP_LOGE(TAG, "Wifi initialization failed.");
                goto cleanup_event_loop;
            }
        }

        result = wifi::wait_for_connection(WIFI_CONNECTION_TIMEOUT);
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Wifi connection failed.");
            goto cleanup_wifi_connect;
        }

        {
            mqtt::client_config mqtt_client_config{};
            mqtt_client_config.uri  = CONFIG_MQTT_URI;
            mqtt_client_config.port = CONFIG_MQTT_PORT;

            mqtt_client = mqtt::client(&mqtt_client_config);
        }

        result = mqtt_client.start();
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "MQTT client initialization failed.");
            goto cleanup_wifi_connect;
        }

        result = mqtt_client.register_data_callback([](std::string_view topic, std::string_view data) { 
            mqtt_data_callback(topic, data);
        });

        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "MQTT data callback set failed.");
            goto cleanup_wifi_connect;
        }

        result = ble::init();
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "BLE initialization failed.");
            goto cleanup_wifi_connect;
        }

        result = ble::register_scan_callback([](std::string_view name, const ble::mac& address) {
            ble_scan_callback(name, address);
        });

        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "BLE scan callback register failed.");
            goto cleanup_ble;
        }

        return result;

    cleanup_ble:
        ble::deinit();
    cleanup_wifi_connect:
        wifi::disconnect();
    cleanup_event_loop:
        esp_event_loop_delete_default();
    cleanup_nvs:
        nvs_flash_deinit();

        return result;
    }

    static void app_cleanup()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        ble::deinit();
        wifi::disconnect();
        esp_event_loop_delete_default();
        nvs_flash_deinit();
    }

    static void ble_start_scanning(uint16_t scan_time)
    {
        scan_results.clear();
        ble::start_scanning(scan_time);
    }

    static void ble_stop_scanning()
    {
        ble::stop_scanning();
        scan_results.clear();
    }
    
    static void ble_scan_callback(std::string_view name, const ble::mac& address)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        // Return if already in scan_results.
        if (scan_results.find(address) != scan_results.end())
        {
            return;
        }

        // Reconnect to disconnected devices.
        if (auto iter = std::find_if(
                disconnected_devices.begin(), 
                disconnected_devices.end(), 
                [&name](const auto& elem) { 
                    return ((elem.second)->get_device_name() == name); 
                });
            iter != disconnected_devices.end())
        {
            auto& [id, ptr] = *iter;
            task_queue.push([id, &ptr]() {
                if (ptr->connect(ptr->get_address()) != ESP_OK)
                {
                    return;
                }

                connected_devices[id].swap(disconnected_devices[id]);
            });
        }

        if (!device_factory::supported(name))
        {
            return;
        }

        scan_results[address] = device_factory::view_on_name(name);

        {
            std::unique_ptr<cJSON, std::function<void(cJSON*)>> json_data{ 
                cJSON_CreateArray(),
                [](cJSON* ptr) {
                    cJSON_Delete(ptr);
                }
            };

            for (const auto& [device_address, device_name] : scan_results)
            {
                cJSON* obj{ cJSON_CreateObject() };

                cJSON_AddStringToObject(obj, "name", device_name.data());
                cJSON_AddStringToObject(obj, "address", (device_address.to_string()).c_str());
                cJSON_AddItemToArray(json_data.get(), obj);
            }

            if (mqtt_client.publish(MQTT_BLE_SCAN_RESULTS_TOPIC, cJSON_PrintUnformatted(json_data.get()), true) != ESP_OK)
            {
                ESP_LOGE(TAG, "Client publish failed.");
                return;
            }
        }
    }

    static esp_err_t ble_connect(std::string_view id, std::string_view name, const ble::mac& address)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        esp_err_t result{ ESP_OK };
        std::string device_mqtt_topic{ MQTT_BLE_DEVICE_TOPIC + std::string(id) };

        auto device{ device_factory::make_device(name, id) };
        if (device == nullptr)
        {
            result = ESP_FAIL;
            ESP_LOGE(TAG, "Could not connect to %s.", name.data());
            return result;
        }

        result = device->connect(address);
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Could not connect to %s.", name.data());
            return result;
        }

        result = mqtt_client.subscribe(device_mqtt_topic);
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Client subscribe failed.");
            return result;
        }

        /* 
            Publish on device topic when new data arrives
        */
        device->notify_callback = [device_mqtt_topic](std::string_view data) {
            if (mqtt_client.publish(device_mqtt_topic, data) != ESP_OK)
            {
                ESP_LOGE(TAG, "Client publish failed.");
                return;
            }
        };

        device->disconnect_callback = [id]() {      
            connected_devices[id].swap(disconnected_devices[id]);
            ble_start_scanning(60 /* seconds */);
        };

        device.swap(connected_devices[id]);
        return result;
    }

    static void mqtt_data_callback(std::string_view topic, std::string_view data)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        std::unique_ptr<cJSON, std::function<void(cJSON*)>> json_data{ 
            cJSON_ParseWithLength(data.data(), data.length()),
            [](cJSON* ptr) {
                cJSON_Delete(ptr);
            }
        };

        ESP_LOGI(TAG, "Received data: %s.", cJSON_PrintUnformatted(json_data.get()));

        if (json_data == nullptr)
        {
            auto error_ptr{ cJSON_GetErrorPtr() };
            if (error_ptr != nullptr)
            {
                ESP_LOGE(TAG, "JSON parse error: %s.", error_ptr);
                return;
            }
        }

        if (topic == MQTT_BLE_SCAN_ENABLE_TOPIC)
        {
            auto enable{ cJSON_GetObjectItemCaseSensitive(json_data.get(), "enable") };

            if (cJSON_IsTrue(enable))
            {
                ESP_LOGI(TAG, "Enabling BLE scan...");
                ble_start_scanning();
            }
            else if (cJSON_IsFalse(enable))
            {
                ESP_LOGI(TAG, "Disabling BLE scan...");
                ble_stop_scanning();
            }
        }
        else if (topic == MQTT_BLE_CONNECT_TOPIC)
        {
            auto device_name    { cJSON_GetObjectItemCaseSensitive(json_data.get(), "name") };
            auto device_id      { cJSON_GetObjectItemCaseSensitive(json_data.get(), "id") };
            auto device_addr    { cJSON_GetObjectItemCaseSensitive(json_data.get(), "address") };

            if (!device_name                            || 
                (!cJSON_IsString(device_name)           && 
                (device_name->valuestring != nullptr))  || 
                !device_factory::supported(device_name->valuestring))
            {
                ESP_LOGE(TAG, "Bad device name.");
                return;
            }

            if (!device_id || (!cJSON_IsString(device_id) && (device_id->valuestring != nullptr)))
            {
                ESP_LOGE(TAG, "Bad device ID.");
                return;
            }

            if (!device_addr || (!cJSON_IsString(device_addr) && (device_addr->valuestring != nullptr)))
            {
                ESP_LOGE(TAG, "Bad device address.");
                return;
            }

            task_queue.push(
                [
                    id  { std::string{ device_id->valuestring } },
                    addr{ ble::mac(device_addr->valuestring) },
                    name{ device_factory::view_on_name(device_name->valuestring) }
                ]() {
                    if (ble_connect(id, name, addr) != ESP_OK)
                    {
                        ESP_LOGE(TAG, "Failed to connect to %s.", name.data());
                        return;
                    }
                });
        }
        else if (topic == MQTT_BLE_DISCONNECT_TOPIC)
        {
            auto device_id{ cJSON_GetObjectItemCaseSensitive(json_data.get(), "id") };

            if (!device_id || (!cJSON_IsString(device_id) && (device_id->valuestring != nullptr)))
            {
                return;
            }

            if (auto device_iter = connected_devices.find(device_id->valuestring); device_iter == connected_devices.end())
            {
                ESP_LOGE(TAG, "Device with ID: %s not connected.", device_id->valuestring);
                return;
            }

            mqtt_client.unsubscribe(MQTT_BLE_DEVICE_TOPIC + device_id->valuestring);
            connected_devices.erase(device_id->valuestring);
            disconnected_devices.erase(device_id->valuestring);

            ESP_LOGI(TAG, "Disonnected with %s.", device_id->valuestring);
        }
    }
}