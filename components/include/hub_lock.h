#ifndef HUB_LOCK_H
#define HUB_LOCK_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <type_traits>

#include "hub_mutex.h"

namespace hub::concurrency
{
    template<typename MutexT>
    class lock
    {
    public:

        static_assert(std::is_base_of_v<mutex_base, MutexT>, "MutexT must derive from mutex_base.");

        using mutex_type = MutexT;

        explicit lock(mutex_type& mutex) : mtx{ mutex } 
        {
            mtx.lock();
        }

        ~lock()
        {
            mtx.unlock();
        }

        lock(const lock&)             = delete;
        lock(lock&&)                  = default;
        lock& operator=(const lock&)  = delete;
        lock& operator=(lock&&)       = default;

    private:

        mutex_type& mtx;
    };
}

#endif