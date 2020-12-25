#include "hub_test_mqtt.h"
#include "hub_test_utils.h"

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

#include "esp_vfs_dev.h"
#include "driver/uart.h"

static const char* TAG = "HUB_MQTT_TEST";

#define TEST_MQTT_ECHO_TIMEOUT (TickType_t)10000 / portTICK_PERIOD_MS

static EventGroupHandle_t s_mqtt_wait_group;

static void test_mqtt_echo_data_callback(hub_mqtt_client* client, const char* topic, const void* data, int length)
{
    ESP_LOGI(TAG, "MESSAGE: %s\n", (const char*)data);
    client->publish(client, "/test/subscribe", (const char*)data);
    xEventGroupSetBits(s_mqtt_wait_group, BIT0);
}

static test_err_t run_test_mqtt_echo()
{
    test_err_t test_result = TEST_SUCCESS;
    esp_err_t result = ESP_OK;

    s_mqtt_wait_group = xEventGroupCreate();
    if (s_mqtt_wait_group == NULL)
    {
        ESP_LOGE(TAG, "Could not create event group.\n");
        result = ESP_FAIL;
        test_result = TEST_SETUP_FAILURE;
        return test_result;
    }

    result = wifi_config(); 
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Wifi connection failed.\n");
        test_result = TEST_SETUP_FAILURE;
        goto cleanup_event_group;
    }

    ESP_LOGI(TAG, "Connecting to %s:%i.\n", CONFIG_MQTT_URI, CONFIG_MQTT_PORT);

    hub_mqtt_client mqtt_client;
    hub_mqtt_client_config mqtt_client_config = {
        .uri = CONFIG_MQTT_URI,
        .port = CONFIG_MQTT_PORT
    };

    result = hub_mqtt_client_init(&mqtt_client, &mqtt_client_config);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "MQTT client initialization failed.\n");
        test_result = TEST_FAILURE;
        goto cleanup_wifi_connect;
    }

    result = mqtt_client.start(&mqtt_client);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "MQTT client start failed.\n");
        test_result = TEST_FAILURE;
        goto cleanup_mqtt_client;
    }

    result = mqtt_client.subscribe(&mqtt_client, "/test/publish");
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "MQTT client could not subscribe.\n");
        test_result = TEST_FAILURE;
        goto cleanup_mqtt_client;
    }

    result = mqtt_client.register_data_callback(&mqtt_client, &test_mqtt_echo_data_callback);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "MQTT client could not register subscribe callback.\n");
        test_result = TEST_FAILURE;
        goto cleanup_mqtt_client;
    }

    EventBits_t bits = xEventGroupWaitBits(s_mqtt_wait_group, BIT0, pdFALSE, pdFALSE, TEST_MQTT_ECHO_TIMEOUT);

    if (!(bits & BIT0))
    {
        ESP_LOGE(TAG, "TIMEOUT.\n");
        test_result = TEST_TIMEOUT;
        goto cleanup_mqtt_client;
    }

    xEventGroupClearBits(s_mqtt_wait_group, BIT0);

cleanup_mqtt_client:
    hub_mqtt_client_destroy(&mqtt_client);
cleanup_wifi_connect:
    hub_wifi_disconnect();
cleanup_event_group:
    vEventGroupDelete(s_mqtt_wait_group);

    return test_result;
}

test_case test_mqtt_echo = {
    .name = "TEST MQTT ECHO",
    .run = &run_test_mqtt_echo
};