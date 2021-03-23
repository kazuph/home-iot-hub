#ifndef HUB_TIMING_H
#define HUB_TIMING_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

namespace hub::timing
{
    inline constexpr const char* TAG{ "TIMING" };

    /**
     * @brief Type representing time duration in a type-safe manner.
     * Explicitly convertible to FreeRTOS TickType_t.
     * 
     * @see miliseconds, seconds
     */
    enum class duration_t : TickType_t {};

    /**
     * @brief Function converting time duration from miliseconds to its representation in duration_t.
     * 
     * @param duration Number of miliseconds to be converted.
     * @return constexpr duration_t 
     */
    inline constexpr duration_t miliseconds(unsigned long long int duration) noexcept
    {
        return static_cast<duration_t>(static_cast<TickType_t>(value) / portTICK_PERIOD_MS);
    }

    /**
     * @brief Function converting time duration from seconds to its representation in duration_t.
     * 
     * @param duration Number of seconds to be converted.
     * @return constexpr duration_t 
     */
    inline constexpr duration_t seconds(unsigned long long int duration) noexcept
    {
        return static_cast<duration_t>((1000u * static_cast<TickType_t>(value)) / portTICK_PERIOD_MS);
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
            return miliseconds(value);
        }

        /**
         * @brief Literal for duration_t representation of seconds.
         * 
         */
        inline constexpr duration_t operator"" _s(unsigned long long int duration) noexcept
        {
            return seconds(value);
        }
    }

    /**
     * @brief Delay current task for the specified duration.
     * 
     * @param duration Requested delay duration.
     */
    inline void sleep_for(duration_t duration) noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        vTaskDelay(static_cast<TickType_t>(value));
    }
}

#endif