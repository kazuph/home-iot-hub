#include "hub_wifi.h"
#include "hub_mqtt.h"
#include "hub_ble.h"
#include "hub_device_list.h"
#include "hub_dispatch_queue.h"

#include <cstdio>
#include <cstddef>
#include <map>
#include <string>
#include <sstream>
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

constexpr uint16_t WIFI_CONNECTION_TIMEOUT{ 5000U };
constexpr uint16_t BLE_SCAN_TIME{ 30U };

constexpr const char* MQTT_BLE_SCAN_ENABLE_TOPIC{ "/scan/enable" };
constexpr const char* MQTT_BLE_SCAN_RESULTS_TOPIC{ "/scan/results" };
constexpr const char* MQTT_BLE_CONNECT_TOPIC{ "/connect" };

constexpr const char* TAG = "HUB_MAIN";

static esp_err_t app_init();
static esp_err_t app_cleanup();
static void ble_scan_callback(const char* name, esp_bd_addr_t address, esp_ble_addr_type_t address_type, int rssi, int result_index);

static hub_mqtt_client mqtt_client{};
static hub_dispatch_queue connect_queue{};
static std::map<std::string, esp_bd_addr_t> scan_results{};
static std::map<std::string, hub_ble_client_handle_t> connected_devices{};

extern "C" void app_main()
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

    if (app_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "Application initialization failed.");
        goto restart;
    }
    ESP_LOGI(TAG, "Application initialization success.");

    hub_ble_start_scanning(BLE_SCAN_TIME);
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
    strcpy((char*)wifi_config.sta.ssid, CONFIG_WIFI_SSID);
    strcpy((char*)wifi_config.sta.password, CONFIG_WIFI_PASSWORD);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    
    hub_mqtt_client_config mqtt_client_config{};
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

    result = hub_mqtt_client_init(&mqtt_client, &mqtt_client_config);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "MQTT client initialization failed.");
        goto cleanup_wifi_connect;
    }

    result = hub_mqtt_client_start(&mqtt_client);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "MQTT client initialization failed.");
        goto cleanup_mqtt_client;
    }

    result = hub_ble_init();
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "BLE initialization failed.");
        goto cleanup_mqtt_client;
    }

    result = hub_ble_register_scan_callback(&ble_scan_callback);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "BLE scan callback register failed.");
        goto cleanup_ble;
    }

    result = hub_dispatch_queue_init(&connect_queue);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not initialze dispatch queue.");
        goto cleanup_ble;
    }

    return result;

cleanup_ble:
    hub_ble_deinit();
cleanup_mqtt_client:
    hub_mqtt_client_destroy(&mqtt_client);
cleanup_wifi_connect:
    hub_wifi_disconnect();
cleanup_event_loop:
    esp_event_loop_delete_default();
cleanup_nvs:
    nvs_flash_deinit();

    return result;
}

static esp_err_t app_cleanup()
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

    hub_dispatch_queue_destroy(&connect_queue);
    hub_ble_deinit();
    hub_mqtt_client_destroy(&mqtt_client);
    hub_wifi_disconnect();
    esp_event_loop_delete_default();
    nvs_flash_deinit();

    return ESP_OK;
}

static void ble_scan_callback(const char* name, esp_bd_addr_t address, esp_ble_addr_type_t address_type, int rssi, int result_index)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

    if (scan_results.find(name) != scan_results.end())
    {
        return;
    }

    if (is_device_supported(name))
    {
        std::string buff;
        std::copy(address, address + sizeof(esp_bd_addr_t), scan_results[name]);

        buff += '[';

        for (const auto& [device_name, device_address] : scan_results)
        {
            buff += "{\"name\":\"";
            buff += device_name;
            buff += "\"},";
        }

        buff.pop_back(); // Remove last ','
        buff += ']';

        ESP_LOGI(TAG, "Published data: %s", buff.c_str());

        if (hub_mqtt_client_publish(&mqtt_client, MQTT_BLE_SCAN_RESULTS_TOPIC, buff.c_str(), true) != ESP_OK)
        {
            ESP_LOGE(TAG, "Client publish failed.");
        }
    }
}