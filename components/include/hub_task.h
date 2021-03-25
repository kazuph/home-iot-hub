#ifndef HUB_TASK_H
#define HUB_TASK_H

#define configUSE_TIME_SLICING 1
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"

#include <functional>
#include <tuple>
#include <memory>
#include <exception>
#include <stdexcept>

#include "hub_timing.h"

namespace hub::utils
{
    class task
    {
    public:

        task() noexcept;

        task(const task&) = delete;

        task(task&& other) noexcept;

        template<typename FunTy, typename... Args>
        task(configSTACK_DEPTH_TYPE stack_size, UBaseType_t priority, FunTy&& fun, Args&&... args) : 
            task_handle     { nullptr },
            event_group     { xEventGroupCreate() }
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);

            struct param_t
            {
                FunTy                   fun;
                std::tuple<Args...>     args;
                EventGroupHandle_t      event_group;
            };

            if (!event_group)
            {
                ESP_LOGE(TAG, "Event group was nullptr.");
                throw std::runtime_error("Event group was nullptr.");
            }

            auto task_code = [](void* param) {
                ESP_LOGD(TAG, "Function: task_code (lambda).");
                {
                    std::unique_ptr<param_t> param_ptr{ reinterpret_cast<param_t*>(param) };          
                    xEventGroupSetBits(param_ptr->event_group, TASK_STARTED_BIT);
                    std::apply(param_ptr->fun, param_ptr->args);
                    xEventGroupSetBits(param_ptr->event_group, TASK_FINISHED_BIT);
                }
                vTaskDelete(nullptr);
            };

            {
                std::unique_ptr<param_t> param_ptr{ new param_t{ std::forward<FunTy>(fun), std::make_tuple(std::forward<Args>(args)...), event_group } };

                if (xTaskCreate(task_code, "task", stack_size, reinterpret_cast<void*>(param_ptr.get()), priority, &task_handle) != pdPASS)
                {
                    ESP_LOGE(TAG, "Task creation failed.");
                    throw std::runtime_error("Task creation failed.");
                }

                param_ptr.release();
            }
        }

        task& operator=(const task&) = delete;

        task& operator=(task&& other);

        bool joinable() const;

        void join();

        ~task();

    private:

        static constexpr const char* TAG                { "TASK" };
        static constexpr EventBits_t TASK_STARTED_BIT   { BIT0 };
        static constexpr EventBits_t TASK_FINISHED_BIT  { BIT1 };

        mutable TaskHandle_t        task_handle;
        mutable EventGroupHandle_t  event_group;
    };
}

#endif