#include <stdio.h>
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "hub_wifi.h"

void app_main()
{
    ESP_ERROR_CHECK(hub_wifi_connect(CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD, CONFIG_MAXIMUM_RETRY));
}
