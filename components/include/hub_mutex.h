#ifndef HUB_MUTEX_H
#define HUB_MUTEX_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace hub::concurrency
{
    struct mutex_base
    {
        SemaphoreHandle_t m_mutex_handle;

        mutex_base()                                = delete;

        explicit mutex_base(const SemaphoreHandle_t handle);

        mutex_base(const mutex_base&)               = delete;

        mutex_base(mutex_base&&)                    = default;

        mutex_base& operator=(const mutex_base&)    = delete;

        mutex_base& operator=(mutex_base&&)         = default;

        virtual ~mutex_base();

        virtual void lock()    = 0;

        virtual void unlock()  = 0;
    };

    /**
     * @brief Struct representing a basic mutex.
     */
    struct mutex : public mutex_base
    {
        using base = mutex_base;

        mutex();

        mutex(const mutex&)               = delete;

        mutex(mutex&&)                    = default;

        mutex& operator=(const mutex&)    = delete;

        mutex& operator=(mutex&&)         = default;

        /**
         * @brief Take ownership of mutex.
         */
        void lock() override;

        /**
         * @brief Release ownership of the mutex.
         */
        void unlock() override;
    };

    /**
     * @brief Struct representing mutex that can be acquired in recursive calls.
     */
    struct recursive_mutex : public mutex_base
    {
        using base = mutex_base;

        recursive_mutex();

        recursive_mutex(const recursive_mutex&)               = delete;

        recursive_mutex(recursive_mutex&&)                    = default;

        recursive_mutex& operator=(const recursive_mutex&)    = delete;

        recursive_mutex& operator=(recursive_mutex&&)         = default;

        /**
         * @brief Take ownership of mutex.
         */
        void lock() override;

        /**
         * @brief Release ownership of the mutex.
         */
        void unlock() override;
    };
}

#endif