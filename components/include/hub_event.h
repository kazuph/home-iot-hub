#ifndef HUB_EVENT_H
#define HUB_EVENT_H

#include <functional>
#include <algorithm>
#include <list>

#include "hub_dispatch_queue.h"

namespace hub::event
{
    template<std::size_t TasksNum>
    struct dispatcher
    {
        using dispatch_queue_array = std::array<concurrency::dispatch_queue, TasksNum>;

        template<typename Fun, typename... Args>
        void dispatch(Fun&& fun, Args&&... args) const
        {
            if (s_current_queue_iter == s_dispatch_queues.end())
            {
                s_current_queue_iter = s_dispatch_queues.begin();
            }

            s_current_queue_iter->push(std::forward<Fun>(fun), std::forward<Args>(args)...);
            ++s_current_queue_iter;
        }

    private:

        static dispatch_queue_array                     s_dispatch_queues;
        static typename dispatch_queue_array::iterator  s_current_queue_iter;
    };

    template<std::size_t TasksNum>
    typename dispatcher<TasksNum>::dispatch_queue_array dispatcher<TasksNum>::s_dispatch_queues{};

    template<std::size_t TasksNum>
    typename dispatcher<TasksNum>::dispatch_queue_array::iterator dispatcher<TasksNum>::s_current_queue_iter{ s_dispatch_queues.begin() };

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

        void invoke(const SenderT* sender, EventArgsT&& args) const
        {
            std::for_each(m_callback_list.begin(), m_callback_list.end(), 
                [
                    this, 
                    sender, 
                    &args
                ](const function_type& fun) {
                    dispatch(fun, sender, std::forward<EventArgsT>(args));
                }
            );
        }

        void operator+=(function_type&& fun)
        {
            m_callback_list.push_back(std::forward<function_type>(fun));
        }

        operator bool() const
        {
            return !m_callback_list.empty();
        }

    private:

        function_list_type m_callback_list;
    };
}

#endif