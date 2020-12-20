#include "hub_test.h"

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

#include "esp_vfs_dev.h"
#include "driver/uart.h"

// TEST_WIFI_CONNECT_DISCONNECT
#define TEST_WIFI_CONNECT_DISCONNECT 0

// TEST_MQTT_ECHO and TEST_MQTT_ECHO_REPEATED
#define TEST_MQTT_ECHO 1
#define TEST_MQTT_ECHO_REPEATED 2
#define TEST_MQTT_ECHO_TIMEOUT (TickType_t)5000 / portTICK_PERIOD_MS
#define TEST_MQTT_ECHO_REPEATED_RERUNS 10

// TEST_MQTT_JSON_RECEIVE
#define TEST_MQTT_JSON_RECEIVE 3

const char* TAG = "HUB_MAIN_TEST";

static EventGroupHandle_t s_mqtt_wait_group;

static esp_err_t stdio_config();
static esp_err_t wifi_config();

// TEST_WIFI_CONNECT_DISCONNECT
static test_err_t test_wifi_connect_disconnect();

// TEST_MQTT_ECHO and TEST_MQTT_ECHO_REPEATED
static test_err_t test_mqtt_echo(int reruns);
static void test_mqtt_echo_data_callback(hub_mqtt_client* client, const char* topic, const void* data, int length);

// TEST_MQTT_JSON_RECEIVE
static test_err_t test_mqtt_json_receive();

void test_run()
{
    stdio_config();

    if (nvs_flash_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "NVS flash initialization failed.");
        esp_restart();
    }

    if (esp_event_loop_create_default() != ESP_OK)
    {
        ESP_LOGE(TAG, "Default event loop creation failed.");
        esp_restart();
    }

    test_err_t test_result = TEST_SUCCESS;
    char test_id = -1;
    unsigned char count = 0;

    while (1)
    {
        printf("TEST ID: ");
        test_id = fgetc(stdin);
        count = 0;

        switch (test_id)
        {
        case TEST_WIFI_CONNECT_DISCONNECT: 
            ESP_LOGI(TAG, "TEST CONNECT DISCONNECT\n");  
            test_result = test_wifi_connect_disconnect(); 
            break;
        case TEST_MQTT_ECHO:               
            ESP_LOGI(TAG, "TEST MQTT ECHO\n");  
            test_result = test_mqtt_echo(1);              
            break;
        case TEST_MQTT_ECHO_REPEATED:      
            ESP_LOGI(TAG, "TEST MQTT ECHO REPEATED\n");  
            test_result = test_mqtt_echo(TEST_MQTT_ECHO_REPEATED_RERUNS);             
            break;
        case TEST_MQTT_JSON_RECEIVE:
            ESP_LOGI(TAG, "TEST MQTT JSON RECEIVE\n");  
            test_result = test_mqtt_json_receive(); 
            break;
        default:
            ESP_LOGW(TAG, "Unknown test ID.\n"); 
            test_result = TEST_UNKNOWN_ID;
            break;
        }

        printf("TEST RESULT: %i\n", (int)test_result);
    }
}

static esp_err_t stdio_config()
{
    // Initialize VFS & UART so we can use std::cout/cin
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
    /* Install UART driver for interrupt-driven reads and writes */
    ESP_ERROR_CHECK(uart_driver_install((uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM, 256, 0, 0, NULL, 0));
    /* Tell VFS to use UART driver */
    esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);
    esp_vfs_dev_uart_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
    /* Move the caret to the beginning of the next line on '\n' */
    esp_vfs_dev_uart_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);
    return ESP_OK;
}

static esp_err_t wifi_config()
{
    esp_err_t result = ESP_OK;

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
        ESP_LOGE(TAG, "Wifi connection failed.\n");
        goto no_cleanup;
    }

    result = hub_wifi_wait_for_connection(5000);    
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Wifi connection failed.\n");
        goto cleanup_wifi;
    }

    ESP_LOGI(TAG, "Wifi initialization success.\n");

    return result;

cleanup_wifi:
    hub_wifi_disconnect();
no_cleanup:
    return result;
}

static test_err_t test_wifi_connect_disconnect()
{
    test_err_t test_result = TEST_SUCCESS;
    esp_err_t result = wifi_config();

    if (result != ESP_OK)
    {
        test_result = TEST_FAILURE;
        goto no_cleanup;
    }

    hub_wifi_disconnect();
no_cleanup:
    return test_result;
}

static test_err_t test_mqtt_echo(int reruns)
{
    test_err_t test_result = TEST_SUCCESS;
    esp_err_t result = ESP_OK;

    s_mqtt_wait_group = xEventGroupCreate();
    if (s_mqtt_wait_group == NULL)
    {
        ESP_LOGE(TAG, "Could not create event group.\n");
        result = ESP_FAIL;
        test_result = TEST_SETUP_FAILURE;
        goto no_cleanup;
    }

    result = wifi_config(); 
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Wifi connection failed.\n");
        test_result = TEST_SETUP_FAILURE;
        goto cleanup_event_group;
    }

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

    // Config done, start testing
    for (int i = 0; i < reruns; i++)
    {
        EventBits_t bits = xEventGroupWaitBits(s_mqtt_wait_group, BIT0, pdFALSE, pdFALSE, TEST_MQTT_ECHO_TIMEOUT);

        if (!(bits & BIT0))
        {
            ESP_LOGE(TAG, "TIMEOUT.\n");
            test_result = TEST_TIMEOUT;
            goto cleanup_mqtt_client;
        }

        xEventGroupClearBits(s_mqtt_wait_group, BIT0);
    }

cleanup_mqtt_client:
    hub_mqtt_client_destroy(&mqtt_client);
cleanup_wifi_connect:
    hub_wifi_disconnect();
cleanup_event_group:
    vEventGroupDelete(s_mqtt_wait_group);
no_cleanup:
    return test_result;
}

static void test_mqtt_echo_data_callback(hub_mqtt_client* client, const char* topic, const void* data, int length)
{
    ESP_LOGI(TAG, "MESSAGE: %s\n", (const char*)data);
    client->publish(client, "/test/subscribe", (const char*)data);
    xEventGroupSetBits(s_mqtt_wait_group, BIT0);
}

static test_err_t test_mqtt_json_receive()
{
    test_err_t test_result = TEST_SUCCESS;
    esp_err_t result = ESP_OK;

    result = wifi_config();    
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Wifi connection failed.\n");
        test_result = TEST_SETUP_FAILURE;
        goto cleanup_wifi_connect;
    }

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

    result = mqtt_client.publish(&mqtt_client, "/test/json", "\
    {\
        \"BLE_devices\": [\
            \"Device1\",\
            \"Device2\",\
            \"Device3\",\
            \"Device4\",\
        ]\
    }");

    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "MQTT client could not publish JSON.\n");
        test_result = TEST_FAILURE;
        goto cleanup_mqtt_client;
    }

cleanup_mqtt_client:
    hub_mqtt_client_destroy(&mqtt_client);
cleanup_wifi_connect:
    hub_wifi_disconnect();

    return test_result;
}