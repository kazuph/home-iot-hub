#include "hub_wifi.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#define WIFI_CONNECTED_BIT BIT0

#if (CONFIG_MAXIMUM_RETRY == -1)
#define RETRY_INFINITE
#else
#define WIFI_FAIL_BIT BIT1
#endif

static const char *TAG = "HUB WIFI";

static EventGroupHandle_t s_wifi_event_group;

static void hub_wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
#ifndef RETRY_INFINITE
    static int s_retry_num = 0;
#endif
    esp_err_t result = ESP_OK;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        result = esp_wifi_connect();
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "WiFi connection failed.");
        }
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
#ifndef RETRY_INFINITE
        if (s_retry_num < CONFIG_MAXIMUM_RETRY)
        {
            result = esp_wifi_connect();

            if (result != ESP_OK)
            {
                ESP_LOGE(TAG, "WiFi connection failed.");
            }

            s_retry_num++;
            ESP_LOGI(TAG, "Disconnected, retrying...\t[%i/%i]\r", s_retry_num, CONFIG_MAXIMUM_RETRY);
        }
        else
        {
            if (s_wifi_event_group != NULL)
            {
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
                ESP_LOGI(TAG, "Connect to the access point failed.\n");
            }
        }
#else
        result = esp_wifi_connect();

        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "WiFi connection failed.");
        }

        ESP_LOGI(TAG, "Disconnected, retrying...\r");
#endif
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t*)event_data;
        ESP_LOGI(TAG, "Connected. IP: " IPSTR, IP2STR(&event->ip_info.ip));

#ifndef RETRY_INFINITE
        s_retry_num = 0;
#endif

        if (s_wifi_event_group != NULL)
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }
    }
}

esp_err_t hub_wifi_connect(wifi_config_t* config)
{
    esp_err_t result = ESP_OK;

    esp_netif_init();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    result = esp_wifi_init(&wifi_init_config);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Wi-Fi initialization failed.\n");
        return result;
    }

    result = esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_START, &hub_wifi_event_handler, NULL);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Event initialization failed.\n");
        goto cleanup_wifi_init;
    }

    result = esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &hub_wifi_event_handler, NULL);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Event initialization failed.\n");
        goto cleanup_event_handler_register;
    }

    result = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &hub_wifi_event_handler, NULL);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Event initialization failed.\n");
        goto cleanup_event_handler_register;
    }

    result = esp_wifi_set_mode(WIFI_MODE_STA);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Setting WiFi mode failed.\n");
        goto cleanup_event_handler_register;
    }

    result = esp_wifi_set_config(ESP_IF_WIFI_STA, config);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Setting WiFi configuration failed.\n");
        goto cleanup_event_handler_register;
    }

    result = esp_wifi_start();
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "WiFi start failed.\n");
        goto cleanup_event_handler_register;
    }

    return result;

cleanup_event_handler_register:
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &hub_wifi_event_handler);
    esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &hub_wifi_event_handler);
    esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_START, &hub_wifi_event_handler);
cleanup_wifi_init:
    esp_wifi_deinit();

    return result;
}

esp_err_t hub_wifi_disconnect()
{    
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &hub_wifi_event_handler);
    esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &hub_wifi_event_handler);
    esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_START, &hub_wifi_event_handler);
    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_deinit();

    return ESP_OK;
}

esp_err_t hub_wifi_wait_for_connection(int timeout)
{
    esp_err_t result = ESP_OK;

    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL)
    {
        ESP_LOGE(TAG, "Could not create event group.\n");
        return ESP_FAIL;
    }

#ifndef RETRY_INFINITE
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, (TickType_t)timeout / portTICK_PERIOD_MS);
#else
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, (TickType_t)timeout / portTICK_PERIOD_MS);
#endif

    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "WiFi connected.\n");
    }
#ifndef RETRY_INFINITE
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGW(TAG, "WiFi connection failed.\n");
        result = ESP_FAIL;
    }
#endif
    else
    {
        ESP_LOGW(TAG, "Unexpected event.\n");
        result = ESP_FAIL;
    }

    vEventGroupDelete(s_wifi_event_group);
    return result;
}