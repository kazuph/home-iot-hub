#include "hub_test_utils.h"
#include "hub_wifi.h"

#include "esp_vfs_dev.h"
#include "driver/uart.h"

static const char* TAG = "HUB_MQTT_UTILS";

esp_err_t stdio_config()
{
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
    ESP_ERROR_CHECK(uart_driver_install((uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM, 256, 0, 0, NULL, 0));
    esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);
    esp_vfs_dev_uart_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
    esp_vfs_dev_uart_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);
    return ESP_OK;
}

esp_err_t wifi_config()
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