#ifndef LOCK_HPP
#define LOCK_HPP

#include "mutex.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <type_traits>

namespace hub::concurrency
{
    template<typename MutexT>
    class lock
    {
    public:

        static_assert(std::is_base_of_v<mutex_base, MutexT>, "MutexT must derive from mutex_base.");

        using mutex_type = MutexT;

        explicit lock(mutex_type& mutex) : m_mutex_ref{ mutex } 
        {
            m_mutex_ref.lock();
        }

        ~lock()
        {
            m_mutex_ref.unlock();
        }

        lock(const lock&)             = delete;
        lock(lock&&)                  = default;
        lock& operator=(const lock&)  = delete;
        lock& operator=(lock&&)       = default;

    private:

        mutex_type& m_mutex_ref;
    };
}

#endif