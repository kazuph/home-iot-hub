#include "hub_dispatch_queue.h"
#include "hub_timing.h"

#include <exception>
#include <stdexcept>
#include <cstring>

namespace hub::utils
{
    dispatch_queue::dispatch_queue(configSTACK_DEPTH_TYPE stack_size, UBaseType_t priority) : 
        exit_flag       { false }, 
        mtx             {  }, 
        event_group     { xEventGroupCreate() },
        dispatch_task   { stack_size, priority, [this]() mutable { task_code(); } }
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
    }

    dispatch_queue::dispatch_queue(dispatch_queue&& other) :
        exit_flag       { false }, 
        mtx             {  }, 
        event_group     {  },
        dispatch_task   {  }
    {
        ESP_LOGD(TAG, "Function: %s (move constructor).", __func__);
        *this = std::move(other);
    }

    dispatch_queue& dispatch_queue::operator=(dispatch_queue&& other)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        if (this == &other)
        {
            return *this;
        }

        exit_flag       = other.exit_flag;
        mtx             = std::move(other.mtx);
        event_group     = other.event_group;
        dispatch_task   = std::move(other.dispatch_task);

        other.exit_flag     = false;
        other.event_group   = nullptr;

        return *this;
    }

    dispatch_queue::~dispatch_queue()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        using namespace timing::literals;

        exit_flag = true;
        dispatch_task.join();
        xEventGroupSetBits(event_group, QUEUE_EMPTY_BIT);
        vEventGroupDelete(event_group);
    }

    bool dispatch_queue::empty() const
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        lock<recursive_mutex> _lock{ mtx };
        return base::empty();
    }

    dispatch_queue::size_type dispatch_queue::size() const
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        lock<recursive_mutex> _lock{ mtx };
        return base::size();
    }

    dispatch_queue::reference dispatch_queue::front()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        lock<recursive_mutex> _lock{ mtx };
        return base::front();
    }

    dispatch_queue::const_reference dispatch_queue::front() const
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        lock<recursive_mutex> _lock{ mtx };
        return base::front();
    }

    dispatch_queue::reference dispatch_queue::back()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        lock<recursive_mutex> _lock{ mtx };
        return base::back();
    }

    dispatch_queue::const_reference dispatch_queue::back() const
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        lock<recursive_mutex> _lock{ mtx };
        return base::back();
    }

    void dispatch_queue::pop()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        lock<recursive_mutex> _lock{ mtx };
        base::pop();
    }

    void dispatch_queue::swap(dispatch_queue& other) noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        lock<recursive_mutex> _lock{ mtx };
        base::swap(other);
    }

    void dispatch_queue::task_code() noexcept
    {
        ESP_LOGD(TAG, "Function: main task (lambda).");

        using namespace timing::literals;
        
        while (!exit_flag)
        {
            if (!empty())
            {
                front()();
                pop();
            }
            else
            {
                xEventGroupWaitBits(event_group, QUEUE_EMPTY_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
            }
        }
    }
}