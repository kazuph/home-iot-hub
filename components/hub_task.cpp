#include "hub_task.h"

namespace hub::utils
{
    task::task() noexcept :
        task_handle     { nullptr }, 
        event_group     { xEventGroupCreate() }
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
    }

    task::task(task&& other) noexcept :
        task_handle     { nullptr },
        event_group     { xEventGroupCreate() }
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

        task_handle = other.task_handle;
        event_group = other.event_group;

        other.task_handle = nullptr;
        other.event_group = nullptr;

        return *this;
    }

    bool task::joinable() const
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        // TASK_STARTED_BIT gets cleared in join() function.
        return ((TASK_STARTED_BIT & xEventGroupGetBits(event_group)) && event_group);
    }

    void task::join()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        if (!joinable())
        {
            return;
        }

        {
            EventBits_t bits = xEventGroupWaitBits(event_group, TASK_STARTED_BIT | TASK_FINISHED_BIT, pdTRUE, pdTRUE, portMAX_DELAY);

            if (!(bits & (TASK_STARTED_BIT | TASK_FINISHED_BIT)))
            {
                ESP_LOGE(TAG, "Task join timeout.");
                throw std::runtime_error("Task join timeout.");
            }
        }
    }

    task::~task()
    {
        if (!event_group)
        {
            return;
        }

        vEventGroupDelete(event_group);
    }
}