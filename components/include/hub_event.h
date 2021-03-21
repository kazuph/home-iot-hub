#ifndef HUB_EVENT_H
#define HUB_EVENT_H

#include "esp_log.h"

#include <functional>
#include <algorithm>
#include <list>
#include <any>

#include "hub_dispatch_queue.h"

namespace hub
{
    struct dispatcher
    {
        static constexpr const char* TAG{ "DISPATCHER" };
        static utils::dispatch_queue dispatch_queue;

        template<typename Fun, typename... Args>
        void dispatch(Fun&& fun, Args&&... args)
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            dispatch_queue.push(std::forward<Fun>(fun), std::forward<Args>(args)...);
        }
    };

    template<typename EventArgsT>
    class event_handler : protected dispatcher
    {
    public:

        using function_type         = std::function<void(EventArgsT)>;
        using function_list_type    = std::list<function_type>;

        event_handler()                                 = default;

        event_handler(const event_handler&)             = delete;

        event_handler(event_handler&&)                  = default;

        event_handler& operator=(const event_handler&)  = delete;

        event_handler& operator=(event_handler&&)       = default;

        ~event_handler()                                = default;

        void invoke(EventArgsT args)
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);

            std::for_each(callback_list.begin(), callback_list.end(), [this, args](const auto& fun) {
                dispatch(fun, args);
            });
        }

        void operator+=(function_type fun)
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