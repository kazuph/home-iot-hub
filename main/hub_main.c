#include <stdio.h>
#include "sdkconfig.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "mqtt_client.h"
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
    hub_wifi_connect();
}
