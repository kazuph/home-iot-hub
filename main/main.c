#include "hub_wifi.h"
#include "hub_mqtt.h"
#include "hub_ble.h"
#include "hub_dispatch_queue.h"

#include <stdio.h>
#include <stddef.h>

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

#include "device_mikettle.h"

/* HUB macros */
#define HUB_UUID "0072513f-dbec-4a5c-a6f5-d4bf89ba13a4"

/* WiFi macros */
#define WIFI_CONNECTION_TIMEOUT 5000U // ms

/* BLE macros */
#define BLE_SCAN_TIME 3600U // s

/* MQTT macros */
#define MQTT_BLE_SCAN_TOPIC "scan/" HUB_UUID
#define MQTT_DEVICE_TOPIC_FORMAT "device/%s"

static const char* TAG = "HUB_MAIN";

static esp_err_t app_init();
static esp_err_t app_cleanup();
static void ble_scan_callback(esp_bd_addr_t address, const char* device_name, esp_ble_addr_type_t address_type);
static void ble_notify_callback(hub_ble_client* ble_client, uint16_t handle, uint8_t *value, uint16_t value_length);
static void ble_disconnect_callback(hub_ble_client* ble_client);
static void ble_start_scanning();
static void ble_stop_scanning();
static void mikettle_connect();

static hub_mqtt_client mqtt_client;
static hub_ble_client mikettle;
static hub_dispatch_queue connect_queue;

void app_main()
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

    if (app_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "Application initialization failed.");
        goto restart;
    }
    ESP_LOGI(TAG, "Setup successful");

    if (hub_dispatch_queue_push(&connect_queue, &ble_start_scanning) != ESP_OK)
    {
        ESP_LOGE(TAG, "Push to dispatch queue failed.");
        goto cleanup;
    }

    ESP_LOGI(TAG, "Scanning started...");

    return;

cleanup:
    app_cleanup();
restart:
    esp_restart();
}

static esp_err_t app_init()
{
    ESP_LOGD(TAG, "Function: %s.", __func__);
    esp_err_t result = ESP_OK;

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK
        },
    };
    
    hub_mqtt_client_config mqtt_client_config = {
        .uri = CONFIG_MQTT_URI,
        .port = CONFIG_MQTT_PORT
    };

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

    result = hub_ble_client_init(&mikettle);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not initialze MiKettle.");
        goto cleanup_ble;
    }

    result = hub_dispatch_queue_init(&connect_queue);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not initialze dispatch queue.");
        goto cleanup_ble_client;
    }

    return result;

cleanup_ble_client:
    hub_ble_client_destroy(&mikettle);
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
    hub_dispatch_queue_destroy(&connect_queue);
    hub_ble_client_destroy(&mikettle);
    hub_ble_deinit();
    hub_mqtt_client_destroy(&mqtt_client);
    hub_wifi_disconnect();
    esp_event_loop_delete_default();
    nvs_flash_deinit();

    return ESP_OK;
}

static void ble_scan_callback(esp_bd_addr_t address, const char* device_name, esp_ble_addr_type_t address_type)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

    char* buff = NULL;

    buff = (char*)malloc(64 * sizeof(char));
    if (buff == NULL)
    {
        ESP_LOGE(TAG, "Could not allocate memory.");
        return;
    }

    if (sprintf(buff, "{\"name\":\"%s\"}", device_name) < 0)
    {
        ESP_LOGE(TAG, "JSON formatting failed.");
        goto cleanup_buff;
    }

    if (hub_mqtt_client_publish(&mqtt_client, MQTT_BLE_SCAN_TOPIC, buff, true) != ESP_OK)
    {
        ESP_LOGE(TAG, "Client publish failed.");
        goto cleanup_buff;
    }

    if (strncmp(device_name, MIKETTLE_DEVICE_NAME, sizeof(MIKETTLE_DEVICE_NAME)) == 0)
    {
        memcpy(mikettle.remote_bda, address, sizeof(mikettle.remote_bda));
        mikettle.addr_type = address_type;

        if (hub_dispatch_queue_push(&connect_queue, &ble_stop_scanning) != ESP_OK)
        {
            ESP_LOGE(TAG, "Push to dispatch queue failed.");
            goto cleanup_buff;
        }

        if (hub_dispatch_queue_push(&connect_queue, &mikettle_connect) != ESP_OK)
        {
            ESP_LOGE(TAG, "Push to dispatch queue failed.");
            goto cleanup_buff;
        }
    }

cleanup_buff:
    free(buff);
}

static void ble_notify_callback(hub_ble_client* ble_client, uint16_t handle, uint8_t *value, uint16_t value_length)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

    if (handle == MIKETTLE_HANDLE_STATUS)
    {
        static mikettle_status status;
        char* buff = NULL;

        if (value_length <= sizeof(mikettle_status))
        {
            ESP_LOGE(TAG, "Bad data size.");
            return;
        }

        // Avoid sending the same status several times
        if (memcmp(status.data, value, sizeof(mikettle_status)) == 0)
        {
            return;
        }

        memcpy(status.data, value, sizeof(mikettle_status));
        
        buff = (char*)malloc(256);
        if (buff == NULL)
        {
            ESP_LOGE(TAG, "Could not allocate memory.");
            return;
        }

        if (sprintf(
            buff, 
            MIKETTLE_JSON_FORMAT, 
            status.temperature_current,
            status.temperature_set,
            status.action,
            status.mode,
            status.keep_warm_type,
            status.keep_warm_time,
            12,
            1) < 0)
        {
            ESP_LOGE(TAG, "JSON formatting failed.");
            goto cleanup_buff;
        }

        if (hub_mqtt_client_publish(&mqtt_client, MIKETTLE_MQTT_TOPIC, buff, true) != ESP_OK)
        {
            ESP_LOGE(TAG, "Client publish failed.");
            goto cleanup_buff;
        }

        ESP_LOGI(TAG, "Received data: %s.", buff);

cleanup_buff:
        free(buff);
    }
}

static void ble_disconnect_callback(hub_ble_client* ble_client)
{
    if (hub_dispatch_queue_push(&connect_queue, &ble_start_scanning) != ESP_OK)
    {
        ESP_LOGE(TAG, "Push to dispatch queue failed.");
    }
}

static void ble_start_scanning()
{
    if (hub_ble_start_scanning(BLE_SCAN_TIME) != ESP_OK)
    {
        ESP_LOGE(TAG, "Scan start failed.");
    }
}

static void ble_stop_scanning()
{
    if (hub_ble_stop_scanning() != ESP_OK)
    {
        ESP_LOGE(TAG, "Scan stop failed.");
    }
}

static void mikettle_connect()
{
    if (hub_ble_client_connect(&mikettle) != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not connect to MiKettle.");
        goto retry;
    }

    if (mikettle_authorize(&mikettle) != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not authorize to MiKettle.");
        goto retry;
    }

    if (hub_ble_client_register_notify_callback(&mikettle, &ble_notify_callback) != ESP_OK)
    {
        ESP_LOGE(TAG, "Register notify callback failed.");
        goto retry;
    }

    if (hub_ble_client_register_disconnect_callback(&mikettle, &ble_disconnect_callback) != ESP_OK)
    {
        ESP_LOGE(TAG, "Disconnect callback register failed.");
        goto retry;
    }

    return;

retry:
    ESP_LOGI(TAG, "Retrying...");
    if (hub_dispatch_queue_push(&connect_queue, &ble_start_scanning) != ESP_OK)
    {
        ESP_LOGE(TAG, "Push to dispatch queue failed.");
    }
}