#include "hub_mutex.h"

#include "esp_log.h"

namespace hub::utils
{
    static constexpr const char* TAG{ "mutex" };

    mutex_base::mutex_base(const SemaphoreHandle_t handle) noexcept : mutex_handle{ handle }
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        if (!mutex_handle)
        {
            ESP_LOGE(TAG, "Could not create mutex.");
            abort();
        }
    }

    mutex_base::~mutex_base()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        vSemaphoreDelete(mutex_handle);
    }

    void mutex_base::lock_from_isr() noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        BaseType_t higherPriorityTaskWoken{ 0 };
        if (xSemaphoreTakeFromISR(mutex_handle, &higherPriorityTaskWoken) != pdTRUE)
        {
            ESP_LOGE(TAG, "Could not lock mutex.");
            abort();
        }
    }

    void mutex_base::unlock_from_isr() noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        BaseType_t higherPriorityTaskWoken{ 0 };
        if (xSemaphoreGiveFromISR(mutex_handle, &higherPriorityTaskWoken) != pdTRUE)
        {
            ESP_LOGE(TAG, "Could not unlock mutex.");
            abort();
        }
    }

    mutex::mutex() noexcept : base{ xSemaphoreCreateMutex() }
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
    }

    void mutex::lock() noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        if (xSemaphoreTake(mutex_handle, portMAX_DELAY) != pdTRUE)
        {
            ESP_LOGE(TAG, "Could not lock mutex.");
            abort();
        }
    }

    void mutex::unlock() noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        if (xSemaphoreGive(mutex_handle) != pdTRUE)
        {
            ESP_LOGE(TAG, "Could not unlock mutex.");
            abort();
        }
    }

    recursive_mutex::recursive_mutex() noexcept : base{ xSemaphoreCreateRecursiveMutex() }
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
    }

    void recursive_mutex::lock() noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        if (xSemaphoreTakeRecursive(mutex_handle, portMAX_DELAY) != pdTRUE)
        {
            ESP_LOGE(TAG, "Could not lock mutex.");
            abort();
        }
    }

    void recursive_mutex::unlock() noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        if (xSemaphoreGiveRecursive(mutex_handle) != pdTRUE)
        {
            ESP_LOGE(TAG, "Could not unlock mutex.");
            abort();
        }
    }
}