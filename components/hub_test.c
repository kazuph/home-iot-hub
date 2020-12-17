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

#define TEST_MSG_MAX_LEN (2 * sizeof(char))

// TEST_WIFI_CONNECT_DISCONNECT
#define TEST_WIFI_CONNECT_DISCONNECT 0

// TEST_MQTT_ECHO and TEST_MQTT_ECHO_REPEATED
#define TEST_MQTT_ECHO 1
#define TEST_MQTT_ECHO_REPEATED 2
#define TEST_MQTT_ECHO_TIMEOUT 10000
#define TEST_MQTT_ECHO_REPEATED_RERUNS 10

const char* TAG = "HUB_MAIN_TEST";

static EventGroupHandle_t s_mqtt_wait_group;

static esp_err_t stdin_stdout_config();

// TEST_WIFI_CONNECT_DISCONNECT
static test_err_t test_wifi_connect_disconnect();

// TEST_MQTT_ECHO and TEST_MQTT_ECHO_REPEATED
static test_err_t test_mqtt_echo(int reruns);
static void test_mqtt_echo_subscribe_callback(hub_mqtt_client* client, const char* topic, const void* data, int length);

void test_run()
{
    stdin_stdout_config();

    test_err_t test_result = TEST_SUCCESS;
    char test_id = -1;
    unsigned char count = 0;
    char msg[TEST_MSG_MAX_LEN];

    while (1)
    {
        while (count < TEST_MSG_MAX_LEN) 
        {
            char c = fgetc(stdin);
            if (c == '\n') 
            {
                printf("%i\n", (int)c);
                msg[count] = '\0';
                break;
            } 
            else if (c > 0 && c < 127) 
            {
                printf("%i\n", (int)c);
                msg[count] = c;
                ++count;
            }

            vTaskDelay(10 / portTICK_PERIOD_MS);
        }

        test_id = msg[0];

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
            default: goto restart;
        }
    }

restart:
    esp_restart();
}

static esp_err_t stdin_stdout_config()
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

static test_err_t test_wifi_connect_disconnect()
{
    test_err_t test_result = TEST_SUCCESS;
    esp_err_t result = ESP_OK;

    result = nvs_flash_init();
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "NVS flash initialization failed.\n");
        test_result = TEST_SETUP_FAILURE;
        goto no_cleanup;
    }

    result = esp_event_loop_create_default();
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Default event loop creation failed.\n");
        test_result = TEST_SETUP_FAILURE;
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
        ESP_LOGE(TAG, "Wifi connection failed.\n");
        test_result = TEST_FAILURE;
        goto cleanup_event_loop;
    }

    ESP_LOGI(TAG, "Wifi initialization success.\n");

    hub_wifi_disconnect();
    hub_wifi_cleanup();

cleanup_event_loop:
    esp_event_loop_delete_default();
cleanup_nvs:
    nvs_flash_deinit();
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

    result = nvs_flash_init();
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "NVS flash initialization failed.\n");
        test_result = TEST_SETUP_FAILURE;
        goto cleanup_event_group;
    }

    result = esp_event_loop_create_default();
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Default event loop creation failed.\n");
        test_result = TEST_SETUP_FAILURE;
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
        ESP_LOGE(TAG, "Wifi connection failed.\n");
        test_result = TEST_SETUP_FAILURE;
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

    result = mqtt_client.register_subscribe_callback(&mqtt_client, &test_mqtt_echo_subscribe_callback);
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
            test_result = TEST_FAILURE;
            goto cleanup_mqtt_client;
        }

        xEventGroupClearBits(s_mqtt_wait_group, BIT0);
    }

    ESP_LOGI(TAG, "MQTT client initialization success.\n");

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
    return test_result;
}

static void test_mqtt_echo_subscribe_callback(hub_mqtt_client* client, const char* topic, const void* data, int length)
{
    ESP_LOGI(TAG, "MESSAGE: %s\n", (const char*)data);
    client->publish(client, "/test/subscribe", (const char*)data);
    xEventGroupSetBits(s_mqtt_wait_group, BIT0);
}