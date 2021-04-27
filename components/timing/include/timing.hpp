#ifndef TIMING_HPP
#define TIMING_HPP

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace hub::timing
{
    /**
     * @brief Type representing time duration in a type-safe manner.
     * Explicitly convertible to FreeRTOS TickType_t.
     * 
     * @see miliseconds, seconds
     */
    enum class duration_t : TickType_t {};

    inline constexpr duration_t MAX_DELAY{ static_cast<duration_t>(portMAX_DELAY) };

    /**
     * @brief Function converting time duration from miliseconds to its representation in duration_t.
     * 
     * @param duration Number of miliseconds to be converted.
     * @return constexpr duration_t 
     */
    inline constexpr duration_t miliseconds(unsigned long long int duration) noexcept
    {
        return static_cast<duration_t>(static_cast<TickType_t>(duration) / portTICK_PERIOD_MS);
    }

    /**
     * @brief Function converting time duration from seconds to its representation in duration_t.
     * 
     * @param duration Number of seconds to be converted.
     * @return constexpr duration_t 
     */
    inline constexpr duration_t seconds(unsigned long long int duration) noexcept
    {
        return static_cast<duration_t>((1000u * static_cast<TickType_t>(duration)) / portTICK_PERIOD_MS);
    }

    /**
     * @brief Convert duration_t to TickType_t.
     * Effectively does static_cast<TickType_t>(duration).
     * 
     * @param duration Duration to be converted to TickType_t.
     * @return constexpr TickType_t Duration representation in TickType_t.
     */
    inline constexpr TickType_t to_ticks(duration_t duration) noexcept
    {
        return static_cast<TickType_t>(duration);
    }

    /**
     * @brief Delay current task for the specified duration.
     * 
     * @param duration Requested delay duration.
     */
    inline void sleep_for(duration_t duration) noexcept
    {
        vTaskDelay(static_cast<TickType_t>(duration));
    }

    /**
     * @brief Namespace contains literals fot time representation.
     * 
     */
    namespace literals
    {
        /**
         * @brief Literal for duration_t representation of miliseconds.
         * 
         */
        inline constexpr duration_t operator"" _ms(unsigned long long int duration) noexcept
        {
            return miliseconds(duration);
        }

        /**
         * @brief Literal for duration_t representation of seconds.
         * 
         */
        inline constexpr duration_t operator"" _s(unsigned long long int duration) noexcept
        {
            return seconds(duration);
        }
    }
}

#endif