#include "hub_mutex.h"

#include <cassert>
#include <stdexcept>
#include <new>

namespace hub::concurrency
{
    mutex_base::mutex_base(const SemaphoreHandle_t handle) : 
        m_mutex_handle{ handle }
    {
        if (!m_mutex_handle)
        {
            throw std::bad_alloc();
        }
    }

    mutex_base::~mutex_base()
    {
        vSemaphoreDelete(m_mutex_handle);
    }

    mutex::mutex() : base{ xSemaphoreCreateMutex() }
    {
        
    }

    void mutex::lock()
    {
        assert(m_mutex_handle);

        if (xSemaphoreTake(m_mutex_handle, portMAX_DELAY) != pdTRUE)
        {
            throw std::runtime_error("Mutex lock failed.");
        }
    }

    void mutex::unlock()
    {
        assert(m_mutex_handle);

        if (xSemaphoreGive(m_mutex_handle) != pdTRUE)
        {
            throw std::runtime_error("Mutex unlock failed.");
        }
    }

    recursive_mutex::recursive_mutex() : 
        base{ xSemaphoreCreateRecursiveMutex() }
    {

    }

    void recursive_mutex::lock()
    {
        assert(m_mutex_handle);

        if (xSemaphoreTakeRecursive(m_mutex_handle, portMAX_DELAY) != pdTRUE)
        {
            throw std::runtime_error("Mutex lock failed.");
        }
    }

    void recursive_mutex::unlock()
    {
        assert(m_mutex_handle);

        if (xSemaphoreGiveRecursive(m_mutex_handle) != pdTRUE)
        {
            throw std::runtime_error("Mutex unlock failed.");
        }
    }
}