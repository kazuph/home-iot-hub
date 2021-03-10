#ifndef HUB_SEMAPHORElock_impl_H
#define HUB_SEMAPHORElock_impl_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_log.h"

#include <type_traits>

namespace hub::utils
{
    enum class lock_context
    {
        non_isr,
        isr
    };

    template<typename _Mutex, lock_context _Context>
    class lock_impl
    {
    public:

        using mutex_type = _Mutex;

        explicit lock_impl(mutex_type& mutex) : mtx{ mutex } 
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);

            if constexpr (_Context == lock_context::non_isr)
            {
                mtx.lock();
            }
            else
            {
                mtx.lock_from_isr();
            }
        }

        ~lock_impl()
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);

            if constexpr (_Context == lock_context::non_isr)
            {
                mtx.unlock();
            }
            else
            {
                mtx.unlock_from_isr();
            }
        }

        lock_impl(const lock_impl&)             = delete;
        lock_impl(lock_impl&&)                  = default;
        lock_impl& operator=(const lock_impl&)  = delete;
        lock_impl& operator=(lock_impl&&)       = default;

    private:

        static constexpr const char* TAG{ "lock" };
        mutex_type& mtx;
    };

    template<typename _Mutex>
    using lock      = lock_impl<_Mutex, lock_context::non_isr>;

    template<typename _Mutex>
    using isr_lock  = lock_impl<_Mutex, lock_context::isr>;
}

#endif