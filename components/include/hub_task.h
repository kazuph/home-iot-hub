#ifndef HUB_TASK_H
#define HUB_TASK_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include <functional>
#include <tuple>
#include <memory>

namespace hub::utils
{
    template<typename _FunTy, typename... _Args>
    class task
    {
        using param_t           = std::pair<_FunTy, std::tuple<_Args...>>;
        using param_ptr_t       = std::unique_ptr<param_t>;

    public:

        task() noexcept :
            _task       { nullptr }, 
            _param_ptr  { nullptr }
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
        }

        task(const task&)               = delete;

        task& operator=(const task&)    = delete;

        task(task&&)                    = default;

        task& operator=(task&&)         = default;

        task(configSTACK_DEPTH_TYPE stack_size, UBaseType_t priority, _FunTy&& fun, _Args&&... args) noexcept : 
            _task       { nullptr },
            _param_ptr  { std::make_unique<param_t>(std::make_pair(std::forward<_FunTy>(fun), std::make_tuple(std::forward<_Args>(args)...))) }
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);

            auto task_code = [](void* param) {
                auto& [fun, args] = *(reinterpret_cast<param_t*>(param));
                std::apply(fun, args);
                vTaskDelete(nullptr);
            };

            if (xTaskCreate(task_code, "task", stack_size, reinterpret_cast<void*>(_param_ptr.get()), priority, &_task) != pdPASS)
            {
                ESP_LOGE(TAG, "Could not create task.");
                abort();
            }
        }

        ~task() = default;

    private:

        static constexpr const char* TAG{ "task_templ" };

        mutable TaskHandle_t    _task;
        mutable param_ptr_t     _param_ptr;
    };

    template<typename _FunTy, typename... _Args>
    task<_FunTy, _Args...> make_task(configSTACK_DEPTH_TYPE stack_size, UBaseType_t priority, _FunTy&& fun, _Args&&... args)
    {
        return task<_FunTy, _Args...>(stack_size, priority, std::forward<_FunTy>(fun), std::forward<_Args>(args)...);
    }
}

#endif