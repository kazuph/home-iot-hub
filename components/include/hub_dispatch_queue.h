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
#include <utility>

namespace hub::utils
{
    class dispatch_queue : private std::queue<std::function<void(void)>>
    {
    public:

        using base = std::queue<std::function<void(void)>>;

        using value_type      = typename base::value_type;
        using reference       = typename base::reference;
        using const_reference = typename base::const_reference;
        using size_type       = typename base::size_type;
        using container_type  = typename base::container_type;

        dispatch_queue(configSTACK_DEPTH_TYPE stack_size = 4096, UBaseType_t priority = tskIDLE_PRIORITY);

        dispatch_queue(const dispatch_queue&) = delete;

        dispatch_queue(dispatch_queue&& other);

        dispatch_queue& operator=(const dispatch_queue&) = delete;

        dispatch_queue& operator=(dispatch_queue&& other);

        ~dispatch_queue();

        bool empty() const;

        size_type size() const;

        reference front();

        const_reference front() const;

        reference back();

        const_reference back() const;

        template<typename _FunTy, typename... _Args>
        void push(_FunTy&& fun, _Args&&... args)
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            
            {
                lock<recursive_mutex> _lock{ mtx };
                base::push(
                    [
                        fun{ std::forward<_FunTy>(fun) }, 
                        args{ std::make_tuple(std::forward<_Args>(args)...) }
                    ]() { 
                        std::apply(fun, args);
                    });
            }

            xEventGroupSetBits(event_group, QUEUE_EMPTY_BIT);
        }

        template<typename _FunTy, typename... _Args>
        void emplace(_FunTy&& fun, _Args&&... args)
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            
            {
                lock<recursive_mutex> _lock{ mtx };
                base::emplace(                    
                    [
                        fun{ std::forward<_FunTy>(fun) }, 
                        args{ std::make_tuple(std::forward<_Args>(args)...) }
                    ]() { 
                        std::apply(fun, args);
                    }
                );
            }

            xEventGroupSetBits(event_group, QUEUE_EMPTY_BIT);
        }

        void pop();

        void swap(dispatch_queue& other) noexcept;

    private:

        void task_code() noexcept;

        static constexpr const char* TAG                { "DISPATCH QUEUE" };
        static constexpr EventBits_t QUEUE_EMPTY_BIT    { BIT0 };

        mutable volatile bool       exit_flag;
        mutable recursive_mutex     mtx;
        mutable EventGroupHandle_t  event_group;
        mutable task                dispatch_task;
    };
}

#endif