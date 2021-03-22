#include "hub_dispatch_queue.h"
#include "hub_timing.h"

#include <exception>
#include <stdexcept>
#include <cstring>

namespace hub::utils
{
    dispatch_queue::dispatch_queue(configSTACK_DEPTH_TYPE stack_size, UBaseType_t priority) : 
        _exit           { false }, 
        _mutex          {  }, 
        _event_group    { xEventGroupCreate() },
        _task           { stack_size, priority, [this]() mutable { task_code(); } }
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
    }

    dispatch_queue::dispatch_queue(dispatch_queue&& other) :
        _exit           { false }, 
        _mutex          {  }, 
        _event_group    {  },
        _task           {  }
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

        _exit           = other._exit;
        _mutex          = std::move(other._mutex);
        _event_group    = other._event_group;
        _task           = std::move(other._task);

        other._exit         = false;
        other._event_group  = nullptr;

        return *this;
    }

    dispatch_queue::~dispatch_queue()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        using namespace timing::literals;

        lock<recursive_mutex> _lock{ _mutex };
        _exit = true;

        while (!_task.joinable()) 
        {
            timing::sleep_for(20_ms);
        }

        _task.join();
        xEventGroupSetBits(_event_group, QUEUE_EMPTY);
        vEventGroupDelete(_event_group);
    }

    bool dispatch_queue::empty() const
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        lock<recursive_mutex> _lock{ _mutex };
        return base::empty();
    }

    dispatch_queue::size_type dispatch_queue::size() const
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        lock<recursive_mutex> _lock{ _mutex };
        return base::size();
    }

    dispatch_queue::reference dispatch_queue::front()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        lock<recursive_mutex> _lock{ _mutex };
        return base::front();
    }

    dispatch_queue::const_reference dispatch_queue::front() const
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        lock<recursive_mutex> _lock{ _mutex };
        return base::front();
    }

    dispatch_queue::reference dispatch_queue::back()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        lock<recursive_mutex> _lock{ _mutex };
        return base::back();
    }

    dispatch_queue::const_reference dispatch_queue::back() const
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        lock<recursive_mutex> _lock{ _mutex };
        return base::back();
    }

    void dispatch_queue::pop()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        lock<recursive_mutex> _lock{ _mutex };
        base::pop();
    }

    void dispatch_queue::swap(dispatch_queue& other) noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        lock<recursive_mutex> _lock{ _mutex };
        base::swap(other);
    }

    void dispatch_queue::task_code() noexcept
    {
        ESP_LOGD(TAG, "Function: main task (lambda).");

        using namespace timing::literals;
        
        while (!_exit)
        {
            if (!empty())
            {
                front()();
                pop();

                timing::sleep_for(20_ms);
            }
            else
            {
                ESP_LOGD(TAG, "Sleep.");
                xEventGroupWaitBits(_event_group, QUEUE_EMPTY, pdTRUE, pdFALSE, portMAX_DELAY);
            }
        }
    }
}