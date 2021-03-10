#include "hub_wifi.h"
#include "hub_ble.h"
#include "hub_device_manager.h"

#include <cstring>
#include <cstdint>
#include <cstddef>
#include <string_view>

#include "esp_system.h"
#include "esp_event.h"

#include "esp_log.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "nvs.h"
#include "nvs_flash.h"

namespace hub
{
    static constexpr const char* TAG{ "HUB_MAIN" };

    static constexpr uint16_t WIFI_CONNECTION_TIMEOUT{ 10000U }; // ms

    static device_manager manager{  };

    static esp_err_t app_init();

    static void app_cleanup();

    extern "C" void app_main()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        std::atexit(&app_cleanup);

        if (app_init() != ESP_OK)
        {
            ESP_LOGE(TAG, "Application initialization failed.");
            goto restart;
        }

        ESP_LOGI(TAG, "Application initialization success.");

        return;

    restart:

        app_cleanup();
        esp_restart();
    }

    static esp_err_t app_init()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        esp_err_t result = ESP_OK;

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

        result = wifi::connect(CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD);
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Wifi initialization failed.");
            goto cleanup_event_loop;
        }

        result = wifi::wait_for_connection(WIFI_CONNECTION_TIMEOUT);
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Wifi connection failed.");
            goto cleanup_wifi_connect;
        }

        result = ble::init();
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "BLE initialization failed.");
            goto cleanup_wifi_connect;
        }

        result = manager.mqtt_start(CONFIG_MQTT_URI, CONFIG_MQTT_PORT);
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "BLE initialization failed.");
            goto cleanup_ble;
        }

        return result;

    cleanup_ble:
        ble::deinit();
    cleanup_wifi_connect:
        wifi::disconnect();
    cleanup_event_loop:
        esp_event_loop_delete_default();
    cleanup_nvs:
        nvs_flash_deinit();

        return result;
    }

    static void app_cleanup()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        manager.mqtt_stop();
        ble::deinit();
        wifi::disconnect();
        esp_event_loop_delete_default();
        nvs_flash_deinit();
    }
}