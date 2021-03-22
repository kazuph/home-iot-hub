#ifndef HUB_TIMING_H
#define HUB_TIMING_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

namespace hub::timing
{
    inline constexpr const char* TAG{ "TIMING" };

    namespace literals
    {
        inline constexpr TickType_t operator"" _s(unsigned long long int value)
        {
            return static_cast<TickType_t>(1000u) * static_cast<TickType_t>(value) / portTICK_PERIOD_MS;
        }

        inline constexpr TickType_t operator"" _ms(unsigned long long int value)
        {
            return static_cast<TickType_t>(value) / portTICK_PERIOD_MS;
        }
    }

    inline void sleep_for(TickType_t value)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        vTaskDelay(value);
    }
}

#endif