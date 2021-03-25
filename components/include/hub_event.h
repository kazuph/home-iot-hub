#ifndef HUB_EVENT_H
#define HUB_EVENT_H

#include "esp_log.h"

#include <functional>
#include <algorithm>
#include <list>

#include "hub_dispatch_queue.h"

namespace hub
{
    template<std::size_t TasksNum>
    struct dispatcher
    {
        using dispatch_queue_array = std::array<utils::dispatch_queue, TasksNum>;

        static constexpr const char* TAG{ "DISPATCHER" };

        template<typename Fun, typename... Args>
        void dispatch(Fun&& fun, Args&&... args)
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);

            if (current_queue_iter == dispatch_queues.end())
            {
                current_queue_iter = dispatch_queues.begin();
            }

            ESP_LOGI(TAG, "Pushing to task %i.", std::distance(current_queue_iter, dispatch_queues.end()));

            current_queue_iter->push(std::forward<Fun>(fun), std::forward<Args>(args)...);
            ++current_queue_iter;
        }

    private:

        static dispatch_queue_array                     dispatch_queues;
        static typename dispatch_queue_array::iterator  current_queue_iter;
    };

    template<std::size_t TasksNum>
    typename dispatcher<TasksNum>::dispatch_queue_array dispatcher<TasksNum>::dispatch_queues{};

    template<std::size_t TasksNum>
    typename dispatcher<TasksNum>::dispatch_queue_array::iterator dispatcher<TasksNum>::current_queue_iter{ dispatch_queues.begin() };

    template<typename SenderT, typename EventArgsT>
    class event_handler : protected dispatcher<4>
    {
    public:

        using function_type         = std::function<void(const SenderT*, EventArgsT)>;
        using function_list_type    = std::list<function_type>;

        event_handler()                                 = default;

        event_handler(const event_handler&)             = delete;

        event_handler(event_handler&&)                  = default;

        event_handler& operator=(const event_handler&)  = delete;

        event_handler& operator=(event_handler&&)       = default;

        ~event_handler()                                = default;

        void invoke(const SenderT* sender, EventArgsT&& args)
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);

            std::for_each(callback_list.begin(), callback_list.end(), [this, sender, args](const auto& fun) {
                dispatch(fun, sender, args);
            });
        }

        void operator+=(function_type&& fun)
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            callback_list.push_back(std::forward<function_type>(fun));
        }

        operator bool() const
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            return !callback_list.empty();
        }

    private:

        static constexpr const char* TAG { "EVENT HANDLER" };

        function_list_type callback_list;
    };
}

#endif