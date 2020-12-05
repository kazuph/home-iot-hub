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

    result = hub_wifi_connect();    
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Wifi connection failed.");
        goto cleanup_event_loop;
    }

    hub_mqtt_client mqtt_client = create_hub_mqtt_client();

    result = mqtt_client.initialize(&mqtt_client);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "MQTT client initialization failed.");
        goto cleanup_wifi_connect;
    }

    result = mqtt_client.subscribe(&mqtt_client, "/mikettle/#");
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "MQTT client initialization failed.");
        goto cleanup_wifi_connect;
    }

    for (int i = 3600; i >= 0; i--)
    {
        if (i <= 10)
        {
            printf("Restarting in %d seconds...\n", i);
        }
        
        result = mqtt_client.publish(&mqtt_client, "/mikettle/temperature/current", "42");
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "MQTT client publish failed.");
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    mqtt_client.destroy(&mqtt_client);

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