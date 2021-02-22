#ifndef HUB_MUTEX_H
#define HUB_MUTEX_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace hub::utils
{
    struct mutex_base
    {
        SemaphoreHandle_t mutex_handle;

        mutex_base()                                = delete;

        explicit mutex_base(const SemaphoreHandle_t handle) noexcept;

        mutex_base(const mutex_base&)               = delete;

        mutex_base(mutex_base&&)                    = default;

        mutex_base& operator=(const mutex_base&)    = delete;

        mutex_base& operator=(mutex_base&&)         = default;

        virtual ~mutex_base();

        virtual void lock() noexcept    = 0;

        virtual void unlock() noexcept  = 0;

        void lock_from_isr() noexcept;

        void unlock_from_isr() noexcept;
    };

    struct mutex : public mutex_base
    {
        using base = mutex_base;

        mutex() noexcept;

        mutex(const mutex&)               = delete;

        mutex(mutex&&)                    = default;

        mutex& operator=(const mutex&)    = delete;

        mutex& operator=(mutex&&)         = default;

        void lock() noexcept override;

        void unlock() noexcept override;
    };

    struct recursive_mutex : public mutex_base
    {
        using base = mutex_base;

        recursive_mutex() noexcept;

        recursive_mutex(const recursive_mutex&)               = delete;

        recursive_mutex(recursive_mutex&&)                    = default;

        recursive_mutex& operator=(const recursive_mutex&)    = delete;

        recursive_mutex& operator=(recursive_mutex&&)         = default;

        void lock() noexcept override;

        void unlock() noexcept override;
    };
}

#endif