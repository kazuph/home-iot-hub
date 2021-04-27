#ifndef HUB_ERROR_H
#define HUB_ERRPR_H

#include <stdexcept>
#include <string>
#include <string_view>

#include "esp_err.h"
#include "esp_log.h"

namespace hub
{
    using namespace std::literals;

    class esp_exception : public std::runtime_error
    {
    public:
        using base = std::runtime_error;

        explicit esp_exception(esp_err_t error_code, std::string_view message = ""sv) : 
            base("ERROR "s + std::to_string(error_code) + " ["s + esp_err_to_name(error_code) + "] "s + static_cast<std::string>(message)),
            m_error_code(error_code)
        {
            
        }

        void log_error() const noexcept
        {
            ESP_LOGE("EXCEPTION", "%s", what());
        }

        esp_err_t get_error_code() const noexcept
        {
            return m_error_code;
        }

    private:

        esp_err_t m_error_code;
    };
}

#endif