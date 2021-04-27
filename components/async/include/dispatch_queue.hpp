#ifndef DISPATCH_QUEUE_HPP
#define DISPATCH_QUEUE_HPP

#include "lock.hpp"
#include "mutex.hpp"
#include "task.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include <functional>
#include <queue>
#include <tuple>
#include <utility>

namespace hub::concurrency
{
    /**
     * @brief The dispatch_queue class dispatches callable objects to be called on seperate task.
     */
    class dispatch_queue : private std::queue<std::function<void(void)>>
    {
    public:

        using base = std::queue<std::function<void(void)>>;

        using value_type      = typename base::value_type;
        using reference       = typename base::reference;
        using const_reference = typename base::const_reference;
        using size_type       = typename base::size_type;
        using container_type  = typename base::container_type;

        /**
         * @brief Constructor.
         * 
         * @param stack_size Stack size to be allocated by the internal FreeRTOS task.
         * @param priority Priority on which dispatched function objects should be run.
         */
        dispatch_queue(configSTACK_DEPTH_TYPE stack_size = 4096, UBaseType_t priority = tskIDLE_PRIORITY);

        dispatch_queue(const dispatch_queue&) = delete;

        dispatch_queue(dispatch_queue&& other);

        dispatch_queue& operator=(const dispatch_queue&) = delete;

        dispatch_queue& operator=(dispatch_queue&& other);

        ~dispatch_queue();

        /**
         * @brief Check if the queue is empty.
         * 
         * @return true if the queue is empty
         * @return false otherwise
         */
        bool empty() const;

        /**
         * @brief Get current size of the queue. This method is thread safe.
         * 
         * @return size_type Size of the queue.
         */
        size_type size() const;

        /**
         * @brief Push function object and arguments to be passed to it to the end of the queue.
         * 
         * @tparam FunT Type of the function to be called.
         * @tparam ArgsT Types of the arguments to be passed to the function.
         * @param fun Function to be called.
         * @param args Arguments to be passed to the function.
         */
        template<typename FunT, typename... ArgsT>
        void push(FunT&& fun, ArgsT&&... args)
        {
            {
                lock<recursive_mutex> _lock{ m_mutex };
                base::push(
                    [
                        fun     { std::forward<FunT>(fun) }, 
                        args    { std::make_tuple(std::forward<ArgsT>(args)...) }
                    ]() { 
                        std::apply(fun, args);
                    });
            }

            xEventGroupSetBits(m_event_group, QUEUE_EMPTY_BIT);
        }

        void pop();

        void swap(dispatch_queue& other) noexcept;

    private:

        static constexpr EventBits_t QUEUE_EMPTY_BIT    { BIT0 };

        mutable volatile bool       m_exit_flag;
        mutable recursive_mutex     m_mutex;
        mutable EventGroupHandle_t  m_event_group;
        mutable task                m_dispatch_task;

        /**
         * @brief Task function being responsible for fetching function objects
         * and arguments from the queue and executing them.
         */
        void task_code() noexcept;

        /**
         * @brief Get reference to the first element of the queue.
         * @return reference Reference to the first element.
         */
        reference front();

        /**
         * @brief Get const reference to the first element of the queue.
         * @return const_reference Const reference to the first element.
         */
        const_reference front() const;

        /**
         * @brief Get reference to the last element of the queue.
         * @return reference Reference to the last element.
         */
        reference back();

        /**
         * @brief Get const reference to the last element of the queue.
         * @return reference Const reference to the last element.
         */
        const_reference back() const;
    };
}

#endif