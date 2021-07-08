#ifndef HUB_ESP_EXCEPTION_HPP
#define HUB_ESP_EXCEPTION_HPP

#include <stdexcept>
#include <string_view>

#include "esp_err.h"

#include "fmt/core.h"

#define LOG_AND_THROW(tag, except) { auto __except = except; ESP_LOGE(tag, "%s", __except.what()); throw std::move(__except); }

namespace hub::utils
{
    struct esp_exception : public std::runtime_error
    {
        explicit esp_exception(std::string_view message, esp_err_t errc = ESP_FAIL) :
            std::runtime_error(fmt::format("{0} Error code: {1} [{2}]", message, errc, esp_err_to_name(errc))),
            _errc{ errc }
        {
            
        }

        esp_err_t errc() const noexcept
        {
            return _errc;
        }

        esp_err_t _errc;
    };
}

#endif