#include "wifi/wifi.hpp"

#include <cstring>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"

#include "nvs.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

namespace hub::wifi
{
    constexpr const char* TAG                   { "hub::wifi" };

    constexpr EventBits_t WIFI_CONNECTED_BIT    { BIT0 };

    static EventGroupHandle_t wifi_event_group;

    static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        esp_err_t result = ESP_OK;

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
            result = esp_wifi_connect();

            if (result != ESP_OK)
            {
                ESP_LOGE(TAG, "WiFi connection failed with error code %x [%s].", result, esp_err_to_name(result));
            }

            ESP_LOGI(TAG, "Disconnected, retrying...");
        }
        else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
        {
            ip_event_got_ip_t* event = reinterpret_cast<ip_event_got_ip_t*>(event_data);
            ESP_LOGI(TAG, "Connected. IP: " IPSTR, IP2STR(&event->ip_info.ip));

            if (wifi_event_group != nullptr)
            {
                xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
            }
        }
    }

    tl::expected<void, esp_err_t> connect(std::string_view ssid, std::string_view password, timing::duration_t timeout) noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        esp_err_t result = ESP_OK;
        wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();

        if (result = nvs_flash_init(); result != ESP_OK)
        {
            ESP_LOGE(TAG, "NVS flash initialization failed.");
            tl::expected<void, esp_err_t>(tl::unexpect, result);
        }

        if (result = esp_event_loop_create_default(); result != ESP_OK)
        {
            ESP_LOGE(TAG, "Default event loop creation failed.");
            goto cleanup_nvs;
        }

        if (wifi_event_group = xEventGroupCreate(); wifi_event_group == nullptr)
        {
            ESP_LOGE(TAG, "Could not create event group.");
            result = ESP_FAIL;
            goto cleanup_event_loop;
        }

        esp_netif_init();
        esp_netif_create_default_wifi_sta();

        if (result = esp_wifi_init(&wifi_init_config); result != ESP_OK)
        {
            ESP_LOGE(TAG, "Wi-Fi initialization failed with error code %x [%s].", result, esp_err_to_name(result));
            return tl::expected<void, esp_err_t>(tl::unexpect, result);
        }

        if (result = esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_START, &event_handler, nullptr); result != ESP_OK)
        {
            ESP_LOGE(TAG, "Event initialization failed with error code %x [%s].", result, esp_err_to_name(result));
            goto cleanup_wifi_init;
        }

        if (result = esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &event_handler, nullptr); result != ESP_OK)
        {
            ESP_LOGE(TAG, "Event initialization failed with error code %x [%s].", result, esp_err_to_name(result));
            goto cleanup_event_handler_register;
        }

        if (result = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, nullptr); result != ESP_OK)
        {
            ESP_LOGE(TAG, "Event initialization failed with error code %x [%s].", result, esp_err_to_name(result));
            goto cleanup_event_handler_register;
        }

        if (result = esp_wifi_set_mode(WIFI_MODE_STA); result != ESP_OK)
        {
            ESP_LOGE(TAG, "Setting WiFi mode failed with error code %x [%s].", result, esp_err_to_name(result));
            goto cleanup_event_handler_register;
        }

        {
            wifi_config_t config{};

            std::strcpy(reinterpret_cast<char*>(config.sta.ssid), ssid.data());
            std::strcpy(reinterpret_cast<char*>(config.sta.password), password.data());

            if (result = esp_wifi_set_config(static_cast<wifi_interface_t>(ESP_IF_WIFI_STA), &config); result != ESP_OK)
            {
                ESP_LOGE(TAG, "Setting WiFi configuration failed with error code %x [%s].", result, esp_err_to_name(result));
                goto cleanup_event_handler_register;
            }
        }

        if (result = esp_wifi_start(); result != ESP_OK)
        {
            ESP_LOGE(TAG, "WiFi start failed with error code %x [%s].", result, esp_err_to_name(result));
            goto cleanup_event_handler_register;
        }

        {
            EventBits_t bits = xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdTRUE, pdFALSE, static_cast<TickType_t>(timeout));

            if (bits & WIFI_CONNECTED_BIT)
            {
                ESP_LOGI(TAG, "WiFi connected.");
            }
            else
            {
                result = ESP_ERR_TIMEOUT;
                ESP_LOGE(TAG, "WiFi connection failed with error code %x [%s].", result, esp_err_to_name(result));
                goto cleanup_wifi_start;
            }
        }

        return tl::expected<void, esp_err_t>();

    cleanup_wifi_start:
        esp_wifi_stop();
    cleanup_event_handler_register:
        esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler);
        esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &event_handler);
        esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_START, &event_handler);
    cleanup_wifi_init:
        esp_wifi_deinit();
    cleanup_event_loop:
        esp_event_loop_delete_default();
    cleanup_nvs:
        nvs_flash_deinit();

        return tl::expected<void, esp_err_t>(tl::unexpect, result);
    }

    tl::expected<void, esp_err_t> disconnect() noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler);
        esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &event_handler);
        esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_START, &event_handler);
        esp_wifi_disconnect();
        esp_wifi_stop();
        esp_wifi_deinit();
        vEventGroupDelete(wifi_event_group);
        esp_event_loop_delete_default();
        nvs_flash_deinit();

        return tl::expected<void, esp_err_t>();
    }
}