#ifndef HUB_SEMAPHORE_LOCK_H
#define HUB_SEMAPHORE_LOCK_H

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace hub::utils
{
    class semaphore_lock
    {
    public:

        explicit semaphore_lock(SemaphoreHandle_t& mutex) : mutex{ mutex } 
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);

            if (xSemaphoreTakeRecursive(mutex, portMAX_DELAY) != pdTRUE)
            {
                ESP_LOGE(TAG, "Mutex lock failed.");
                abort();
            }
        }

        ~semaphore_lock() noexcept
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);

            if (xSemaphoreGiveRecursive(mutex) != pdTRUE)
            {
                ESP_LOGE(TAG, "Mutex release failed.");
                abort();
            }
        }

        semaphore_lock(const semaphore_lock&) = delete;
        semaphore_lock& operator=(const semaphore_lock&) = delete;

    private:

        static constexpr const char* TAG = "semaphore_lock";
        SemaphoreHandle_t& mutex;
    };
}

#endif