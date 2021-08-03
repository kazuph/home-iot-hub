#ifndef HUB_ESP_EXCEPTION_HPP
#define HUB_ESP_EXCEPTION_HPP

#include <stdexcept>
#include <string_view>

#include "esp_err.h"

#define LOG_AND_THROW(tag, except) do { auto __except = except; ESP_LOGE(tag, "%s", __except.what()); throw std::move(__except); } while (false);

namespace hub::utils
{
    struct esp_exception : public std::runtime_error
    {
        explicit esp_exception(std::string_view message, esp_err_t errc = ESP_FAIL);

        esp_err_t errc() const noexcept;

        esp_err_t _errc;
    };
}

#endif