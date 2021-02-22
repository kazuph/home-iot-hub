#ifndef HUB_DISPATCH_QUEUE_H
#define HUB_DISPATCH_QUEUE_H

#include "hub_lock.h"
#include "hub_mutex.h"
#include "hub_task.h"

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include <functional>
#include <queue>
#include <tuple>
#include <type_traits>
#include <exception>
#include <string>
#include <utility>

namespace hub::utils
{
    template<
        typename _FunTy,
        typename... _ArgTy>
    class dispatch_queue : private std::queue<std::pair<_FunTy, std::tuple<_ArgTy...>>>
    {
    public:

        using base = std::queue<std::pair<_FunTy, std::tuple<_ArgTy...>>>;

        using value_type      = typename base::value_type;
        using reference       = typename base::reference;
        using const_reference = typename base::const_reference;
        using size_type       = typename base::size_type;
        using container_type  = typename base::container_type;

        using task_function_type    = std::function<void(void)>;
        using task_internal_type    = task<task_function_type>;

        dispatch_queue(configSTACK_DEPTH_TYPE stack_size = 2048, UBaseType_t priority = tskIDLE_PRIORITY) : 
            _exit           { false }, 
            _task           {  }, 
            _mutex          {  }, 
            _event_group    { xEventGroupCreate() }
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);

            using namespace std::literals;

            if (_event_group == nullptr)
            {
                abort();
            }

            _task = make_task<task_function_type>(stack_size, priority, [this]() { task_code(); });
        }

        ~dispatch_queue()
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            lock<recursive_mutex> _lock{ _mutex };
            _exit = true;
            xEventGroupSetBits(_event_group, QUEUE_EMPTY);
            vEventGroupDelete(_event_group);
        }

        bool empty() const
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            lock<recursive_mutex> _lock{ _mutex };
            return base::empty();
        }

        size_type size() const
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            lock<recursive_mutex> _lock{ _mutex };
            return base::size();
        }

        reference front()
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            lock<recursive_mutex> _lock{ _mutex };
            return base::front();
        }

        const_reference front() const
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            lock<recursive_mutex> _lock{ _mutex };
            return base::front();
        }

        reference back()
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            lock<recursive_mutex> _lock{ _mutex };
            return base::back();
        }

        const_reference back() const
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            lock<recursive_mutex> _lock{ _mutex };
            return base::back();
        }

        void push(const value_type& value)
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            {
                lock<recursive_mutex> _lock{ _mutex };
                base::push(value);
            }
            xEventGroupSetBits(_event_group, QUEUE_EMPTY);
        }

        template<typename... _Args>
        void push(_FunTy&& fun, _Args&&... args)
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            {
                lock<recursive_mutex> _lock{ _mutex };
                base::push(
                    std::make_pair<_FunTy, std::tuple<_Args...>>(
                        std::forward<_FunTy>(fun), std::make_tuple(std::forward<_Args>(args)...)
                    )
                );
            }
            xEventGroupSetBits(_event_group, QUEUE_EMPTY);
        }

        template<typename... _Args>
        void emplace(_FunTy&& fun, _Args&&... args)
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            lock<recursive_mutex> _lock{ _mutex };
            base::emplace(
                std::make_pair<_FunTy, std::tuple<_Args...>>(
                    std::forward<_FunTy>(fun), std::make_tuple(std::forward<_Args>(args)...)
                )
            );
        }

        void pop()
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            lock<recursive_mutex> _lock{ _mutex };
            base::pop();
        }

        void swap(dispatch_queue& other) noexcept
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            lock<recursive_mutex> _lock{ _mutex };
            base::swap(other);
        }

    private:

        void task_code()
        {
            ESP_LOGD(TAG, "Function: main task (lambda).");
            
            while (true)
            {
                {
                    lock<recursive_mutex> _lock{ _mutex };
                    if (_exit)
                    {
                        return;
                    }
                }

                if (!empty())
                {
                    const auto& [fun, args] = front();
                    std::apply(fun, args);

                    pop();
                }
                else
                {
                    xEventGroupWaitBits(_event_group, QUEUE_EMPTY, pdTRUE, pdFALSE, portMAX_DELAY);
                }
            }
        }

        static constexpr const char* TAG    { "dispatch_queue" };
        static constexpr int QUEUE_EMPTY    { BIT0 };

        mutable volatile bool       _exit;
        mutable task_internal_type  _task;
        mutable recursive_mutex     _mutex;
        mutable EventGroupHandle_t  _event_group;
    };
}

#endif