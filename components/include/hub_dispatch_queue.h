#ifndef HUB_DISPATCH_QUEUE_H
#define HUB_DISPATCH_QUEUE_H

#include "esp_log.h"
#include "esp_err.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

#include <functional>
#include <queue>
#include <type_traits>
#include <exception>
#include <string>

namespace hub
{
    class semaphore_lock
    {
        static constexpr const char* TAG = "semaphore_lock";
        SemaphoreHandle_t& mutex;

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
    };

    template<
        configSTACK_DEPTH_TYPE _Stack_size, 
        UBaseType_t _Priority, 
        typename _Ty = std::function<void(void)>, 
        typename _QueueTy = std::queue<_Ty>>
    class dispatch_queue : protected _QueueTy
    {
        static constexpr const char* TAG = "dispatch_queue";
        static constexpr int QUEUE_EMPTY = BIT0;

        static uint16_t queue_count;

        using base = _QueueTy;

        mutable volatile bool exit;
        mutable TaskHandle_t task;
        mutable SemaphoreHandle_t mutex;
        mutable EventGroupHandle_t event_group;

    public:

        using value_type      = typename base::value_type;
        using reference       = typename base::reference;
        using const_reference = typename base::const_reference;
        using size_type       = typename base::size_type;
        using container_type  = typename base::container_type;

        static_assert(std::is_same_v<_Ty, value_type>, "dispatch_queue value type must match its container.");

        dispatch_queue() : exit{ false }
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);

            using namespace std::literals;

            auto dispatch_fun = [](void* arg) -> void {
                ESP_LOGD(TAG, "Function: %s.", __func__);

                dispatch_queue* obj = reinterpret_cast<dispatch_queue*>(arg);
                
                while (!(obj->exit))
                {
                    if (!(obj->empty()))
                    {
                        if (obj->front())
                        {
                            (obj->front())();
                        }

                        obj->pop();
                    }
                    else
                    {
                        xEventGroupWaitBits(obj->event_group, dispatch_queue::QUEUE_EMPTY, pdTRUE, pdFALSE, portMAX_DELAY);
                    }
                }

                vTaskDelete(nullptr);
            };

            mutex = xSemaphoreCreateRecursiveMutex();
            if (mutex == nullptr)
            {
                ESP_LOGE(TAG, "Mutex create failed.");
                abort();
            }

            event_group = xEventGroupCreate();
            if (event_group == nullptr)
            {
                ESP_LOGE(TAG, "Event group create failed.");
                abort();
            }

            if (xTaskCreate(dispatch_fun, std::string("dispatch_queue_"s + std::to_string(queue_count)).c_str(), _Stack_size, reinterpret_cast<void*>(this), _Priority, &task) != pdPASS)
            {
                ESP_LOGE(TAG, "Task create failed.");
                abort();
            }

            queue_count++;
        }

        ~dispatch_queue()
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);

            exit = true;
            xEventGroupSetBits(event_group, QUEUE_EMPTY);
            {
                semaphore_lock{ mutex };
                vTaskDelete(task);
                vEventGroupDelete(event_group);
            }
            vSemaphoreDelete(mutex);
        }

        bool empty() const
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            semaphore_lock{ mutex };
            return base::empty();
        }

        size_type size() const
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            semaphore_lock{ mutex };
            return base::size();
        }

        reference& front()
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            semaphore_lock{ mutex };
            return base::front();
        }

        const_reference& front() const
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            semaphore_lock{ mutex };
            return base::front();
        }

        reference& back()
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            semaphore_lock{ mutex };
            return base::back();
        }

        const_reference& back() const
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            semaphore_lock{ mutex };
            return base::back();
        }

        void push(const value_type& value)
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            {
                semaphore_lock{ mutex };
                base::push(value);
            }
            xEventGroupSetBits(event_group, QUEUE_EMPTY);
        }

        void push(value_type&& value)
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            {
                semaphore_lock{ mutex };
                base::push(std::move(value));
            }
            xEventGroupSetBits(event_group, QUEUE_EMPTY);
        }

        template <class... Args> 
        void emplace (Args&&... args)
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            semaphore_lock{ mutex };
            base::emplace(std::forward(args...));
        }

        void pop()
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            semaphore_lock{ mutex };
            base::pop();
        }

        void swap(dispatch_queue& other) noexcept
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            semaphore_lock{ mutex };
            base::swap(other);
        }
    };

    template<
        configSTACK_DEPTH_TYPE _Stack_size, 
        UBaseType_t _Priority, 
        typename _Ty, 
        typename _QueueTy>
    uint16_t dispatch_queue<_Stack_size, _Priority, _Ty, _QueueTy>::queue_count{ 0 };
}

#endif