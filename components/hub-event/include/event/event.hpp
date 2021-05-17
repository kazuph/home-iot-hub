#ifndef HUB_EVENT_H
#define HUB_EVENT_H

#include "async/dispatch_queue.hpp"

#include <functional>
#include <algorithm>
#include <list>
#include <type_traits>

namespace hub::event
{
    /**
     * @brief Struct dispatcher handles managing execution of event handlers and their dispatch to
     * dispatch queues. 
     * 
     * @tparam TasksNum Number of concurrent tasks to which event handler function are dispatched.
     */
    template<std::size_t TasksNum>
    struct dispatcher
    {
        using dispatch_queue_array = std::array<async::dispatch_queue, TasksNum>;

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

    /**
     * @brief Generic implementation of event handler. New event handlers are added via operator+=. 
     * Adding a new concrete type of event handler is performed by specifying the type of the sender
     * and the type of the event arguments.
     * 
     * @tparam SenderT Sender type.
     * @tparam EventArgsT Argument type.
     */
    template<typename EventArgsT>
    class event_handler : protected dispatcher<4>
    {
    public:

        using argument_type         = std::remove_cv_t<std::remove_reference_t<std::remove_pointer_t<EventArgsT>>>;
        using function_type         = std::function<void(const argument_type&)>;

        event_handler()                                 = default;

        event_handler(const event_handler&)             = delete;

        event_handler(event_handler&&)                  = default;

        event_handler& operator=(const event_handler&)  = delete;

        event_handler& operator=(event_handler&&)       = default;

        ~event_handler()                                = default;

        bool empty() const noexcept
        {
            return m_callback_list.empty();
        }

        void clear() noexcept
        {
            m_callback_list.clear();
        }

        void add_callback(function_type fun) noexcept
        {
            m_callback_list.push_back(fun);
        }

        void invoke(argument_type&& args) const noexcept
        {
            if (empty())
            {
                return;
            }

            std::for_each(m_callback_list.cbegin(), m_callback_list.cend(), [this, args{ std::forward<argument_type>(args) }](const function_type& fun) {
                dispatch(fun, args);
            });
        }

        void operator+=(function_type fun) noexcept
        {
            add_callback(fun);
        }

        operator bool() const noexcept
        {
            return empty();
        }

    private:

        std::list<function_type> m_callback_list;
    };
}

#endif