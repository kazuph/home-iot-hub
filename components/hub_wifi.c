#include "hub_wifi.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_log.h"

#include "nvs.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static const char *TAG = "HUB WIFI";

static EventGroupHandle_t s_wifi_event_group;

static void hub_wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    static int s_retry_num = 0;
    esp_err_t result = ESP_OK;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        result = esp_wifi_connect();
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, esp_err_to_name(result));
        }
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < CONFIG_MAXIMUM_RETRY)
        {
            result = esp_wifi_connect();

            if (result != ESP_OK)
            {
                ESP_LOGE(TAG, esp_err_to_name(result));
            }

            s_retry_num++;
            ESP_LOGI(TAG, "Disconnected, retrying...[%i/%i]\r", s_retry_num, CONFIG_MAXIMUM_RETRY);
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGI(TAG, "Connect to the access point failed.");
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t*)event_data;
        ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t hub_wifi_connect()
{
    esp_err_t result = ESP_OK;
    s_wifi_event_group = NULL;

    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL)
    {
        ESP_LOGE(TAG, "Could not create event group.");
        result = ESP_FAIL;
        goto cleanup_event_group;
    }

    tcpip_adapter_init();

    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    result = esp_wifi_init(&wifi_init_config);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Wi-Fi initialization failed.");
        goto cleanup_wifi_init;
    }

    result = esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_START, &hub_wifi_event_handler, NULL);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Event initialization failed.");
        goto cleanup_wifi_init;
    }

    result = esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &hub_wifi_event_handler, NULL);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Event initialization failed.");
        goto cleanup_wifi_init;
    }

    result = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &hub_wifi_event_handler, NULL);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Event initialization failed.");
        goto cleanup_event_handler_register;
    }

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD
        },
    };

    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    result = esp_wifi_set_mode(WIFI_MODE_STA);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Setting WiFi mode failed.");
        goto cleanup_wifi_init;
    }

    result = esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Setting WiFi configuration failed.");
        goto cleanup_wifi_init;
    }

    result = esp_wifi_start();
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "WiFi start failed.");
        goto cleanup_wifi_init;
    }

    ESP_LOGI(TAG, "Wi-Fi started.");

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "Connected to SSID: %s.", wifi_config.sta.ssid);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGW(TAG, "Failed connecting to SSID: %s.", wifi_config.sta.ssid);
        result = ESP_FAIL;
        goto cleanup_wifi_connect;
    }
    else
    {
        ESP_LOGW(TAG, "Unexpected event.");
        result = ESP_FAIL;
        goto cleanup_wifi_connect;
    }

    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &hub_wifi_event_handler);
    esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &hub_wifi_event_handler);
    esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_START, &hub_wifi_event_handler);
    vEventGroupDelete(s_wifi_event_group);

    return result;

cleanup_wifi_connect:
    esp_wifi_stop();
cleanup_wifi_init:
    esp_wifi_deinit();
cleanup_event_handler_register:
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &hub_wifi_event_handler);
    esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &hub_wifi_event_handler);
    esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_START, &hub_wifi_event_handler);
cleanup_event_group:
    vEventGroupDelete(s_wifi_event_group);

    return result;
}

esp_err_t hub_wifi_disconnect()
{
    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_deinit();

    return ESP_OK;
}

esp_err_t hub_wifi_cleanup()
{
    return ESP_OK;
}