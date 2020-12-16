#include "hub_wifi.h"
#include "hub_mqtt.h"

#include <stdio.h>

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

const char* TAG = "HUB_MAIN";

// Enable or disable test mode
#define CONFIG_TEST 1

#if (CONFIG_TEST == 0)

void app_main()
{
    esp_err_t result = ESP_OK;

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

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK
        },
    };

    result = hub_wifi_connect(&wifi_config);    
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Wifi connection failed.");
        goto cleanup_event_loop;
    }

    hub_mqtt_client mqtt_client;
    hub_mqtt_client_config mqtt_client_config = {
        .uri = CONFIG_MQTT_URI,
        .port = CONFIG_MQTT_PORT
    };

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
    hub_wifi_cleanup();
cleanup_event_loop:
    esp_event_loop_delete_default();
cleanup_nvs:
    nvs_flash_deinit();
restart:
    esp_restart();
}

#else

#define TEST_SUCCESS (char)0
#define TEST_FAILURE (char)-1
#define TEST_SETUP_FAILURE (char)-2

// TEST_WIFI_CONNECT_DISCONNECT
#define TEST_WIFI_CONNECT_DISCONNECT 0
static esp_err_t test_wifi_connect_disconnect();

// TEST_MQTT_ECHO and TEST_MQTT_ECHO_REPEATED
#define TEST_MQTT_ECHO 1
#define TEST_MQTT_ECHO_REPEATED 2
#define TEST_ECHO_WAIT_TIMEOUT 5000
static EventGroupHandle_t s_mqtt_wait_group;
static esp_err_t test_mqtt_echo(int reruns);
static void test_mqtt_echo_subscribe_callback(hub_mqtt_client* client, const char* topic, const void* data, int length);

void app_main()
{
    int test_id = -1;

    /* TO DO: GET FROM STDIN */

    switch (test_id)
    {
        case TEST_WIFI_CONNECT_DISCONNECT:  test_wifi_connect_disconnect(); break;
        case TEST_MQTT_ECHO:                test_mqtt_echo(1);              break;
        case TEST_MQTT_ECHO_REPEATED:       test_mqtt_echo(10);             break;
        default: return;
    }
}

static esp_err_t test_wifi_connect_disconnect()
{
    esp_err_t result = ESP_OK;

    result = nvs_flash_init();
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "NVS flash initialization failed.");
        goto no_cleanup;
    }

    result = esp_event_loop_create_default();
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Default event loop creation failed.");
        goto cleanup_nvs;
    }

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK
        },
    };

    result = hub_wifi_connect(&wifi_config);    
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Wifi connection failed.");
        goto cleanup_event_loop;
    }

    ESP_LOGI(TAG, "Wifi initialization success.");

    hub_wifi_disconnect();
    hub_wifi_cleanup();

cleanup_event_loop:
    esp_event_loop_delete_default();
cleanup_nvs:
    nvs_flash_deinit();
no_cleanup:
    return result;
}

static esp_err_t test_mqtt_echo(int reruns)
{
    esp_err_t result = ESP_OK;

    s_mqtt_wait_group = xEventGroupCreate();
    if (s_mqtt_wait_group == NULL)
    {
        ESP_LOGE(TAG, "Could not create event group.");
        result = ESP_FAIL;
        goto no_cleanup;
    }

    result = nvs_flash_init();
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "NVS flash initialization failed.");
        goto cleanup_event_group;
    }

    result = esp_event_loop_create_default();
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Default event loop creation failed.");
        goto cleanup_nvs;
    }

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK
        },
    };

    result = hub_wifi_connect(&wifi_config);    
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Wifi connection failed.");
        goto cleanup_event_loop;
    }

    hub_mqtt_client mqtt_client;
    hub_mqtt_client_config mqtt_client_config = {
        .uri = CONFIG_MQTT_URI,
        .port = CONFIG_MQTT_PORT
    };

    result = hub_mqtt_client_initialize(&mqtt_client, &mqtt_client_config);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "MQTT client initialization failed.");
        goto cleanup_wifi_connect;
    }

    result = mqtt_client.start(&mqtt_client);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "MQTT client start failed.");
        goto cleanup_mqtt_client;
    }

    result = mqtt_client.subscribe(&mqtt_client, "test/publish");
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "MQTT client could not subscribe.");
        goto cleanup_mqtt_client;
    }

    result = mqtt_client.register_subscribe_callback(&mqtt_client, &test_mqtt_echo_subscribe_callback);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "MQTT client could not register subscribe callback.");
        goto cleanup_mqtt_client;
    }

    // Config done, start testing
    for (int i = 0; i < reruns; i++)
    {
        EventBits_t bits = xEventGroupWaitBits(s_mqtt_wait_group, BIT0, pdFALSE, pdFALSE, TEST_ECHO_WAIT_TIMEOUT);

        if (!(bits & BIT0))
        {
            result = TEST_FAILURE;
            break;
        }
        xEventGroupClearBits(s_mqtt_wait_group, BIT0);
    }


cleanup_mqtt_client:
    hub_mqtt_client_destroy(&mqtt_client);
cleanup_wifi_connect:
    hub_wifi_disconnect();
    hub_wifi_cleanup();
cleanup_event_loop:
    esp_event_loop_delete_default();
cleanup_nvs:
    nvs_flash_deinit();
cleanup_event_group:
    vEventGroupDelete(s_mqtt_wait_group);
no_cleanup:
    return result;
}

static void test_mqtt_echo_subscribe_callback(hub_mqtt_client* client, const char* topic, const void* data, int length)
{
    client->publish(client, "test/subscribe", data);
    xEventGroupSetBits(s_mqtt_wait_group, BIT0);
}

#endif