#ifndef TASK_HPP
#define TASK_HPP

#include "timing.hpp"

#ifndef configUSE_TIME_SLICING
#define configUSE_TIME_SLICING 1
#endif


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include <functional>
#include <tuple>
#include <memory>
#include <exception>
#include <stdexcept>
#include <new>

namespace hub::concurrency
{
    /**
     * @brief Class wrapper for FreeRTOS task.
     */
    class task
    {
    public:

        task() noexcept;

        task(const task&) = delete;

        task(task&& other) noexcept;

        /**
         * @brief Constructor creating and starting a new task.
         * 
         * @tparam FunTy Type of the function to be executed.
         * @tparam Args Types of the arguments passed to the executed function.
         * @param stack_size Stack size described in words to be allocated for task execution.
         * @param priority Priority of the task.
         * @param fun Function to be executed in task.
         * @param args Arguments passed tu the function being executed in task.
         */
        template<typename FunTy, typename... Args>
        task(configSTACK_DEPTH_TYPE stack_size, UBaseType_t priority, FunTy&& fun, Args&&... args) : 
            m_task_handle{ nullptr },
            m_event_group{ xEventGroupCreate() }
        {
            using namespace std::literals;

            struct param_t
            {
                FunTy                   fun;
                std::tuple<Args...>     args;
                EventGroupHandle_t      m_event_group;
            };

            if (!m_event_group)
            {
                throw std::bad_alloc();
            }

            auto task_code = [](void* param) {
                {
                    std::unique_ptr<param_t> param_ptr{ reinterpret_cast<param_t*>(param) };          
                    std::apply(param_ptr->fun, param_ptr->args);
                    xEventGroupSetBits(param_ptr->m_event_group, TASK_FINISHED_BIT);
                }
                vTaskDelete(nullptr);
            };

            {
                std::unique_ptr<param_t> param_ptr{ new param_t{ std::forward<FunTy>(fun), std::make_tuple(std::forward<Args>(args)...), m_event_group } };

                if (xTaskCreate(task_code, ("task_"s + std::to_string(s_task_counter)).c_str(), stack_size, reinterpret_cast<void*>(param_ptr.get()), priority, &m_task_handle) != pdPASS)
                {
                    throw std::runtime_error("Task creation failed.");
                }

                // When the task has started correctly, the allocated memory gets released inside the task code
                param_ptr.release();
            }

            s_task_counter++;
        }

        task& operator=(const task&) = delete;

        task& operator=(task&& other);

        /**
         * @brief Check whether the task is in joinable state. 
         * 
         * @return true when the task has not yet been joined.
         * @return false otherwise.
         */
        bool joinable() const
        {
            return (m_event_group);
        }

        /**
         * @brief Synchronously wait for the task to finish its execution.
         */
        void join();

        ~task();

    private:

        static constexpr EventBits_t    TASK_FINISHED_BIT  { BIT0 };

        static uint32_t                 s_task_counter;

        TaskHandle_t                    m_task_handle;
        EventGroupHandle_t              m_event_group;
    };
}

#endif