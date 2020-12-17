#ifndef CONFIG_TEST

#include "hub_wifi.h"
#include "hub_mqtt.h"

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

static const char* TAG = "HUB_MAIN";
static hub_mqtt_client mqtt_client;

void app_main()
{
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
        goto restart;
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

    result = hub_wifi_wait_for_connection(5000);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Wifi connection failed.");
        goto cleanup_wifi_connect;
    }

    result = hub_mqtt_client_initialize(&mqtt_client, &mqtt_client_config);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "MQTT client initialization failed.");
        goto cleanup_wifi_connect;
    }

    result = mqtt_client.start(&mqtt_client);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "MQTT client initialization failed.");
        goto cleanup_mqtt_client;
    }

    ESP_LOGI(TAG, "MQTT client initialization success.");

cleanup_mqtt_client:
    hub_mqtt_client_destroy(&mqtt_client);
cleanup_wifi_connect:
    hub_wifi_disconnect();
cleanup_event_loop:
    esp_event_loop_delete_default();
cleanup_nvs:
    nvs_flash_deinit();
restart:
    esp_restart();
}

#else

#include "hub_test.h"

void app_main()
{
    test_run();
}

#endif