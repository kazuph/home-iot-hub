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

static EventGroupHandle_t wifi_event_group;

static void hub_wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

    esp_err_t result = ESP_OK;

#ifndef RETRY_INFINITE
    static int s_retry_num = 0;
#endif

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        result = esp_wifi_connect();
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "WiFi connection failed with error code %x [%s].", result, esp_err_to_name(result));
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
                xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
                ESP_LOGI(TAG, "Connect to the access point failed.");
            }
        }
#else
        result = esp_wifi_connect();

        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "WiFi connection failed with error code %x [%s].", result, esp_err_to_name(result));
        }

        ESP_LOGI(TAG, "Disconnected, retrying...");
#endif
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t*)event_data;
        ESP_LOGI(TAG, "Connected. IP: " IPSTR, IP2STR(&event->ip_info.ip));

#ifndef RETRY_INFINITE
        s_retry_num = 0;
#endif

        if (wifi_event_group != NULL)
        {
            xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        }
    }
}

esp_err_t hub_wifi_connect(wifi_config_t* config)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

    esp_err_t result = ESP_OK;

    wifi_event_group = xEventGroupCreate();
    if (wifi_event_group == NULL)
    {
        ESP_LOGE(TAG, "Could not create event group.");
        return ESP_FAIL;
    }

    esp_netif_init();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();

    result = esp_wifi_init(&wifi_init_config);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Wi-Fi initialization failed with error code %x [%s].", result, esp_err_to_name(result));
        return result;
    }

    result = esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_START, &hub_wifi_event_handler, NULL);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Event initialization failed with error code %x [%s].", result, esp_err_to_name(result));
        goto cleanup_wifi_init;
    }

    result = esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &hub_wifi_event_handler, NULL);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Event initialization failed with error code %x [%s].", result, esp_err_to_name(result));
        goto cleanup_event_handler_register;
    }

    result = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &hub_wifi_event_handler, NULL);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Event initialization failed with error code %x [%s].", result, esp_err_to_name(result));
        goto cleanup_event_handler_register;
    }

    result = esp_wifi_set_mode(WIFI_MODE_STA);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Setting WiFi mode failed with error code %x [%s].", result, esp_err_to_name(result));
        goto cleanup_event_handler_register;
    }

    result = esp_wifi_set_config(ESP_IF_WIFI_STA, config);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Setting WiFi configuration failed with error code %x [%s].", result, esp_err_to_name(result));
        goto cleanup_event_handler_register;
    }

    result = esp_wifi_start();
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "WiFi start failed with error code %x [%s].", result, esp_err_to_name(result));
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
    ESP_LOGD(TAG, "Function: %s.", __func__);

    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &hub_wifi_event_handler);
    esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &hub_wifi_event_handler);
    esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_START, &hub_wifi_event_handler);
    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_deinit();
    vEventGroupDelete(wifi_event_group);

    return ESP_OK;
}

esp_err_t hub_wifi_wait_for_connection(int timeout)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);
    
    esp_err_t result = ESP_OK;

#ifndef RETRY_INFINITE
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdTRUE, pdFALSE, (TickType_t)timeout / portTICK_PERIOD_MS);
#else
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdTRUE, pdFALSE, (TickType_t)timeout / portTICK_PERIOD_MS);
#endif

    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "WiFi connected.");
    }
#ifndef RETRY_INFINITE
    else if (bits & WIFI_FAIL_BIT)
    {
        result = ESP_FAIL;
        ESP_LOGE(TAG, "WiFi connection failed with error code %x [%s].", result, esp_err_to_name(result));
    }
#endif
    else
    {
        result = ESP_ERR_TIMEOUT;
        ESP_LOGE(TAG, "WiFi connection failed with error code %x [%s].", result, esp_err_to_name(result));
    }

    return result;
}