#include "hub_wifi.h"
#include "hub_mqtt.h"
#include "hub_ble.h"
#include "hub_device_list.h"
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

    static void         ble_scan_callback(std::string_view name, const ble::mac& address);

    static esp_err_t    ble_connect(std::string_view id, std::string_view name, const ble::mac& address);

    static void         mqtt_data_callback(std::string_view topic, std::string_view data);

    static std::unique_ptr<mqtt::client>                            mqtt_client;

    static std::map<ble::mac, std::string_view>                     scan_results{};
    static std::map<std::string, std::unique_ptr<hub::device_base>> connected_devices{};
    static std::map<std::string_view, std::string>                  disconnected_devices{};

    dispatch_queue<4096, tskIDLE_PRIORITY>                          task_queue{};

    extern "C" void app_main()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        std::atexit(&app_cleanup);

        if (app_init() != ESP_OK)
        {
            ESP_LOGE(TAG, "Application initialization failed.");
            goto restart;
        }
        ESP_LOGI(TAG, "Application initialization success.");

        mqtt_client->subscribe(MQTT_BLE_SCAN_ENABLE_TOPIC);
        mqtt_client->subscribe(MQTT_BLE_CONNECT_TOPIC);
        mqtt_client->subscribe(MQTT_BLE_DISCONNECT_TOPIC);

        return;

    restart:
        app_cleanup();
        esp_restart();
    }

    static esp_err_t app_init()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        esp_err_t result = ESP_OK;

        wifi_config_t wifi_config{};
        std::strcpy(reinterpret_cast<char*>(wifi_config.sta.ssid), CONFIG_WIFI_SSID);
        std::strcpy(reinterpret_cast<char*>(wifi_config.sta.password), CONFIG_WIFI_PASSWORD);
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        
        mqtt::client_config mqtt_client_config{};
        mqtt_client_config.uri  = CONFIG_MQTT_URI;
        mqtt_client_config.port = CONFIG_MQTT_PORT;

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

        result = wifi::connect(&wifi_config);
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Wifi initialization failed.");
            goto cleanup_event_loop;
        }

        result = wifi::wait_for_connection(WIFI_CONNECTION_TIMEOUT);
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Wifi connection failed.");
            goto cleanup_wifi_connect;
        }

        mqtt_client = std::make_unique<mqtt::client>(&mqtt_client_config);
        result = mqtt_client->start();
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "MQTT client initialization failed.");
            goto cleanup_wifi_connect;
        }

        result = mqtt_client->register_data_callback([](std::string_view topic, std::string_view data) { 
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

    static void ble_scan_callback(std::string_view name, const ble::mac& address)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        // Return if already in scan_results.
        if (scan_results.find(address) != scan_results.end())
        {
            return;
        }

        // Reconnect to disconnected devices.
        if (auto iter = disconnected_devices.find(name); iter != disconnected_devices.end())
        {
            task_queue.push([id{ (*iter).second }, name{ (*iter).first }, address]() {
                if (ble_connect(id, name, address) != ESP_OK)
                {
                    ESP_LOGE(TAG, "Failed to connect to %s.", name.data());
                    return;
                }

                disconnected_devices.erase(name);
            });
        }

        // Is device supported?
        if (const auto& device_iter{ supported_devices.find(name) }; device_iter != supported_devices.end())
        {
            /* 
                Device name here is a temporary string_view, so we need to get the 
                name from supported_devices as these are constexpr.
            */
            scan_results[address] = (*device_iter).first;

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
                cJSON_AddStringToObject(obj, "address", device_address.c_str());
                cJSON_AddItemToArray(json_data.get(), obj);
            }

            if (mqtt_client->publish(MQTT_BLE_SCAN_RESULTS_TOPIC, cJSON_PrintUnformatted(json_data.get()), true) != ESP_OK)
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
        std::string device_id{ id };
        std::string device_mqtt_topic{ MQTT_BLE_DEVICE_TOPIC + device_id };

        auto device{ device_init(name) };

        result = device->connect(address);    
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Could not connect to %s.", name.data());
            return result;
        }

        result = mqtt_client->subscribe(device_mqtt_topic);
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Client subscribe failed.");
            return result;
        }

        /* 
            Publish on device topic when new data arrives
        */
        result = device->register_notify_callback([device_mqtt_topic](std::string_view data) {
            ESP_LOGD(TAG, "Function: %s.", __func__);

            if (mqtt_client->publish(device_mqtt_topic, data) != ESP_OK)
            {
                ESP_LOGE(TAG, "Client publish failed.");
                return;
            }
        });

        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Notify callback register failed.");
            return result;
        }

        /*
            When device disconnects, add its ID and name to disconnected_devices, unsubscribe from its MQTT topic,
            erase it from connected_devices and start BLE scan. When device comes back into range, ble_connect function
            will be called by ble_scan_callback.
        */
        result = device->register_disconnect_callback([device_id, device_mqtt_topic{ std::move(device_mqtt_topic) }]() {      
            ESP_LOGD(TAG, "Function: %s.", __func__);

            auto device_iter{ connected_devices.find(device_id) };

            if (device_iter == connected_devices.end())
            {
                ESP_LOGE(TAG, "Device not found.");
                return;
            }

            {
                std::string_view device_name{ ((*device_iter).second)->get_device_name() };

                mqtt_client->unsubscribe(device_mqtt_topic);
                disconnected_devices[device_name] = std::move(device_id);
                connected_devices.erase(device_iter);

                scan_results.clear();
                ble::start_scanning(BLE_SCAN_TIME);
            }
        });

        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Disconnect callback register failed.");
            return result;
        }

        device.swap(connected_devices[std::move(device_id)]);
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
                scan_results.clear();
                task_queue.push([]() {
                    ble::start_scanning(BLE_SCAN_TIME);
                });
            }
            else if (cJSON_IsFalse(enable))
            {
                ESP_LOGI(TAG, "Disabling BLE scan...");
                task_queue.push([]() {
                    ble::stop_scanning();
                });
            }
        }
        else if (topic == MQTT_BLE_CONNECT_TOPIC)
        {
            auto device_id      { cJSON_GetObjectItemCaseSensitive(json_data.get(), "id") };
            auto device_addr    { cJSON_GetObjectItemCaseSensitive(json_data.get(), "address") };

            if (!device_id || (!cJSON_IsString(device_id) && (device_id->valuestring != nullptr)))
            {
                ESP_LOGE(TAG, "Bad JSON data.");
                return;
            }

            if (!device_addr || (!cJSON_IsString(device_addr) && (device_addr->valuestring != nullptr)))
            {
                ESP_LOGE(TAG, "Bad JSON data.");
                return;
            }

            {   // Check if device with the given ID is already connected
                std::string id{ device_id->valuestring };

                if (connected_devices.find(device_id->valuestring) != connected_devices.end())
                {
                    ESP_LOGW(TAG, "Device %s already connected.", device_id->valuestring);
                    return;
                }

                {   // Find device in scan_results and get device name
                    ble::mac addr           { device_addr->valuestring };
                    auto scan_result_iter   { scan_results.find(addr) };

                    if (scan_result_iter == scan_results.end())
                    {
                        ESP_LOGE(TAG, "Device not found.");
                    }

                    task_queue.push(
                        [
                            id{ std::move(id) },
                            addr,
                            name{ (*scan_result_iter).second }
                        ]() {
                            ESP_LOGD(TAG, "Function: %s.", __func__);

                            if (ble_connect(id, name, addr) != ESP_OK)
                            {
                                ESP_LOGE(TAG, "Failed to connect to %s.", name.data());
                                return;
                            }

                            scan_results.erase(addr);
                        });
                }
            }
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

            mqtt_client->unsubscribe(MQTT_BLE_DEVICE_TOPIC + device_id->valuestring);
            connected_devices.erase(device_id->valuestring);

            ESP_LOGI(TAG, "Disonnected with %s.", device_id->valuestring);
        }
    }
}