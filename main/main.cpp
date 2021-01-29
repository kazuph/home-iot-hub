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

    constexpr uint16_t WIFI_CONNECTION_TIMEOUT{ 5000U };
    constexpr uint16_t BLE_SCAN_TIME{ 10U };

    constexpr auto MQTT_BLE_SCAN_ENABLE_TOPIC{ "/scan/enable"sv };
    constexpr auto MQTT_BLE_SCAN_RESULTS_TOPIC{ "/scan/results"sv };
    constexpr auto MQTT_BLE_CONNECT_TOPIC{ "/connect"sv };
    constexpr auto MQTT_BLE_DISCONNECT_TOPIC{ "/disconnect"sv };
    static const auto MQTT_BLE_DEVICE_TOPIC{ "/device/"s }; // Device IDs are appended to this topic

    constexpr const char* TAG = "HUB_MAIN";

    static esp_err_t app_init();
    static void app_cleanup();
    static void ble_scan_callback(std::string_view name, const esp_bd_addr_t address, esp_ble_addr_type_t address_type, int rssi);
    static void mqtt_data_callback(std::string_view topic, std::string_view data);

    static std::unique_ptr<mqtt::client> mqtt_client;
    static std::map<const std::string_view /* Device name */, esp_bd_addr_t /* Device address */> scan_results{};
    static std::map<const std::string /* Device ID */, std::unique_ptr<hub::device_base> /* Device object */> connected_devices{};

    dispatch_queue<4096, tskIDLE_PRIORITY> task_queue{};

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

        mqtt_client->register_data_callback([](std::string_view topic, std::string_view data) { 
            mqtt_data_callback(topic, data);
        });

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
        mqtt_client_config.uri = CONFIG_MQTT_URI;
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

        result = hub_wifi_connect(&wifi_config);
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Wifi initialization failed.");
            goto cleanup_event_loop;
        }

        result = hub_wifi_wait_for_connection(WIFI_CONNECTION_TIMEOUT);
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

        result = ble::init();
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "BLE initialization failed.");
            goto cleanup_wifi_connect;
        }

        result = ble::register_scan_callback(&ble_scan_callback);
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "BLE scan callback register failed.");
            goto cleanup_ble;
        }

        return result;

    cleanup_ble:
        ble::deinit();
    cleanup_wifi_connect:
        hub_wifi_disconnect();
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
        hub_wifi_disconnect();
        esp_event_loop_delete_default();
        nvs_flash_deinit();
    }

    static void ble_scan_callback(std::string_view name, const esp_bd_addr_t address, esp_ble_addr_type_t address_type, int rssi)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        // Return if already in scan_results.
        if (scan_results.find(name) != scan_results.end())
        {
            return;
        }

        // Is device supported?
        if (const auto& device_iter{ supported_devices.find(name) }; device_iter != supported_devices.end())
        {
            /* 
                Take string_view from supported_devices as key because the name corresponds to a temporary
                std::string object.
            */
            std::copy(address, address + sizeof(esp_bd_addr_t), scan_results[(*device_iter).first]);

            std::unique_ptr<cJSON, std::function<void(cJSON*)>> json_data{ 
                cJSON_CreateArray(),
                [](cJSON* ptr) {
                    cJSON_Delete(ptr);
                }
            };

            for (const auto& [device_name, device_address] : scan_results)
            {
                cJSON* obj = cJSON_CreateObject();

                cJSON_AddStringToObject(obj, "name", device_name.data());
                cJSON_AddNumberToObject(obj, "rssi", rssi);
                cJSON_AddItemToArray(json_data.get(), obj);
            }

            if (mqtt_client->publish(MQTT_BLE_SCAN_RESULTS_TOPIC, cJSON_PrintUnformatted(json_data.get()), true) != ESP_OK)
            {
                ESP_LOGE(TAG, "Client publish failed.");
                return;
            }
        }
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

        if (json_data == nullptr)
        {
            auto error_ptr = cJSON_GetErrorPtr();
            if (error_ptr != nullptr)
            {
                ESP_LOGE(TAG, "JSON parse error: %s.", error_ptr);
            }
        }

        if (topic == MQTT_BLE_SCAN_ENABLE_TOPIC)
        {
            auto enable = cJSON_GetObjectItemCaseSensitive(json_data.get(), "enable");

            if (cJSON_IsTrue(enable))
            {
                ESP_LOGI(TAG, "Enabling BLE scan...");
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
            auto device_name = cJSON_GetObjectItemCaseSensitive(json_data.get(), "name");
            auto device_id = cJSON_GetObjectItemCaseSensitive(json_data.get(), "id");

            if (!cJSON_IsString(device_name) && (device_name->valuestring != nullptr))
            {
                ESP_LOGE(TAG, "Bad JSON data.");
                return;
            }

            if (!cJSON_IsString(device_id) && (device_id->valuestring != nullptr))
            {
                ESP_LOGE(TAG, "Bad JSON data.");
                return;
            }

            if (connected_devices.find(device_id->valuestring) != connected_devices.end())
            {
                ESP_LOGW(TAG, "Device %s already connected.", device_name->valuestring);
                return;
            }

            task_queue.push(
                [device_iter{ scan_results.find(device_name->valuestring) }, device_id{ std::string(device_id->valuestring) }]() {
                    const auto& [device_name, device_address] = *device_iter;
                    auto device = device_init(device_name);
                    
                    if (device->connect(device_address) != ESP_OK)
                    {
                        ESP_LOGE(TAG, "Could not connect to %s.", device_name.data());
                        return;
                    }

                    {
                        auto device_mqtt_topic = MQTT_BLE_DEVICE_TOPIC + device_id;
                        mqtt_client->subscribe(device_mqtt_topic);

                        device->register_data_ready_callback([topic{ std::move(device_mqtt_topic) }](std::string_view data) {
                            if (mqtt_client->publish(topic, data, true) != ESP_OK)
                            {
                                ESP_LOGE(TAG, "Client publish failed.");
                                return;
                            }
                        });
                    }

                    scan_results.erase(device_iter);
                    device.swap(connected_devices[std::move(device_id)]);

                    ESP_LOGI(TAG, "Connected to %s.", device_name.data());
                });
        }
        else if (topic == MQTT_BLE_DISCONNECT_TOPIC)
        {
            auto device_id = cJSON_GetObjectItemCaseSensitive(json_data.get(), "id");

            if (!cJSON_IsString(device_id) && (device_id->valuestring != nullptr))
            {
                return;
            }

            auto device_iter = connected_devices.find(device_id->valuestring);

            if (device_iter == connected_devices.end())
            {
                ESP_LOGE(TAG, "Device with ID: %s not connected.", device_id->valuestring);
                return;
            }

            mqtt_client->unsubscribe(MQTT_BLE_DEVICE_TOPIC + (*device_iter).first);

            connected_devices.erase(device_id->valuestring);
            ESP_LOGI(TAG, "Disonnected with %s.", device_id->valuestring);
        }
    }
}