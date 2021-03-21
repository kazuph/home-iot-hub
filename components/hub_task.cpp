#include "hub_task.h"

namespace hub::utils
{
    task::task() noexcept :
        task_handle     { nullptr }, 
        mtx             { },
        task_started    { false }
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
    }

    task::task(task&& other) noexcept :
        task_handle     { nullptr },
        mtx             { },
        task_started    { false }
    {
        ESP_LOGD(TAG, "Function: %s (move constructor).", __func__);
        *this = std::move(other);
    }

    task& task::operator=(task&& other)
    {
        ESP_LOGD(TAG, "Function: %s (move assignment).", __func__);
        if (this == &other)
        {
            return *this;
        }

        task_started    = other.task_started;
        task_handle     = other.task_handle;
        mtx             = std::move(other.mtx);

        other.task_handle   = nullptr;
        other.task_started  = false;

        return *this;
    }

    bool task::joinable() const
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        return task_started;
    }

    void task::join()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        if (!joinable())
        {
            ESP_LOGE(TAG, "Task not joinable.");
            throw std::logic_error("Task not joinable.");
        }
        
        lock<recursive_mutex> mtx_lock{ mtx };
    }
}