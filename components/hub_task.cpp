#include "hub_task.h"

namespace hub::concurrency
{
    uint32_t task::s_task_counter{ 0U };

    task::task() noexcept :
        m_task_handle{ nullptr }, 
        m_event_group{ nullptr }
    {

    }

    task::task(task&& other) noexcept :
        m_task_handle{ nullptr },
        m_event_group{ nullptr }
    {
        *this = std::move(other);
    }

    task& task::operator=(task&& other)
    {
        if (this == &other)
        {
            return *this;
        }

        m_task_handle = other.m_task_handle;
        m_event_group = other.m_event_group;

        other.m_task_handle = nullptr;
        other.m_event_group = nullptr;

        return *this;
    }

    void task::join()
    {
        assert(m_event_group);

        EventBits_t bits = xEventGroupWaitBits(m_event_group, TASK_FINISHED_BIT, pdTRUE, pdTRUE, portMAX_DELAY);

        if (!(bits & TASK_FINISHED_BIT))
        {
            throw std::runtime_error("Task join timeout.");
        }

        vEventGroupDelete(m_event_group);
        m_event_group = nullptr;
    }

    task::~task()
    {
        if (joinable())
        {
            abort();
        }
    }
}