#ifndef HUB_TASK_H
#define HUB_TASK_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include <functional>
#include <tuple>
#include <memory>
#include <exception>
#include <stdexcept>

#include "hub_mutex.h"
#include "hub_lock.h"

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
            mtx             { },
            task_started    { false }
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);

            struct param_t
            {
                FunTy                   fun;
                std::tuple<Args...>     args;
                recursive_mutex&        mtx;
                volatile bool&          task_started;
            };

            auto task_code = [](void* param) {
                ESP_LOGD(TAG, "Function: task_code (lambda).");
                param_t* _param = reinterpret_cast<param_t*>(param);

                {
                    lock<recursive_mutex> mtx_lock{ _param->mtx };
                    _param->task_started = true;
                    std::apply(_param->fun, _param->args);
                }

                delete _param;
                vTaskDelete(nullptr);
            };

            std::unique_ptr<param_t> param_ptr{ new param_t{ std::forward<FunTy>(fun), std::make_tuple(std::forward<Args>(args)...), mtx, task_started } };

            if (xTaskCreate(task_code, "task", stack_size, reinterpret_cast<void*>(param_ptr.get()), priority, &task_handle) != pdPASS)
            {
                ESP_LOGE(TAG, "Task creation failed.");
                throw std::runtime_error("Task creation failed.");
            }

            param_ptr.release();

            ESP_LOGI(TAG, "Task created successfully.");
        }

        task& operator=(const task&) = delete;

        task& operator=(task&& other);

        bool joinable() const;

        void join();

        ~task() = default;

    private:

        static constexpr const char* TAG{ "TASK" };

        mutable TaskHandle_t    task_handle;
        mutable recursive_mutex mtx;
        mutable volatile bool   task_started;
    };
}

#endif