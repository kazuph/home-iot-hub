#include "dispatch_queue.hpp"
#include "timing.hpp"

#include <exception>
#include <stdexcept>

namespace hub::concurrency
{
    dispatch_queue::dispatch_queue(configSTACK_DEPTH_TYPE stack_size, UBaseType_t priority) : 
        m_exit_flag     { false }, 
        m_mutex         {  }, 
        m_event_group   { xEventGroupCreate() },
        m_dispatch_task { stack_size, priority, [this]() mutable { task_code(); } }
    {
        
    }

    dispatch_queue::dispatch_queue(dispatch_queue&& other) :
        m_exit_flag     { false }, 
        m_mutex         {  }, 
        m_event_group   {  },
        m_dispatch_task {  }
    {
        *this = std::move(other);
    }

    dispatch_queue& dispatch_queue::operator=(dispatch_queue&& other)
    {
        if (this == &other)
        {
            return *this;
        }

        m_exit_flag       = other.m_exit_flag;
        m_mutex           = std::move(other.m_mutex);
        m_event_group     = other.m_event_group;
        m_dispatch_task   = std::move(other.m_dispatch_task);

        other.m_exit_flag     = false;
        other.m_event_group   = nullptr;

        return *this;
    }

    dispatch_queue::~dispatch_queue()
    {
        using namespace timing::literals;

        m_exit_flag = true;
        m_dispatch_task.join();
        xEventGroupSetBits(m_event_group, QUEUE_EMPTY_BIT);
        vEventGroupDelete(m_event_group);
    }

    bool dispatch_queue::empty() const
    {
        lock<recursive_mutex> _lock{ m_mutex };
        return base::empty();
    }

    dispatch_queue::size_type dispatch_queue::size() const
    {
        lock<recursive_mutex> _lock{ m_mutex };
        return base::size();
    }

    void dispatch_queue::pop()
    {
        lock<recursive_mutex> _lock{ m_mutex };
        base::pop();
    }

    void dispatch_queue::swap(dispatch_queue& other) noexcept
    {
        lock<recursive_mutex> _lock{ m_mutex };
        base::swap(other);
    }

    dispatch_queue::reference dispatch_queue::front()
    {
        lock<recursive_mutex> _lock{ m_mutex };
        return base::front();
    }

    dispatch_queue::const_reference dispatch_queue::front() const
    {
        lock<recursive_mutex> _lock{ m_mutex };
        return base::front();
    }

    dispatch_queue::reference dispatch_queue::back()
    {
        lock<recursive_mutex> _lock{ m_mutex };
        return base::back();
    }

    dispatch_queue::const_reference dispatch_queue::back() const
    {
        lock<recursive_mutex> _lock{ m_mutex };
        return base::back();
    }

    void dispatch_queue::task_code() noexcept
    {
        using namespace timing::literals;
        
        while (!m_exit_flag)
        {
            if (!empty())
            {
                front()();
                pop();
            }
            else
            {
                xEventGroupWaitBits(m_event_group, QUEUE_EMPTY_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
            }
        }
    }
}