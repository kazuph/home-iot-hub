#include "utils/esp_exception.hpp"

#include "fmt/format.h"

namespace hub::utils
{
    esp_exception::esp_exception(std::string_view message, esp_err_t errc = ESP_FAIL) :
        std::runtime_error(fmt::format("{0} Error code: {1} [{2}]", message, errc, esp_err_to_name(errc))),
        _errc{ errc }
    {
        
    }

    esp_err_t esp_exception::errc() const noexcept
    {
        return _errc;
    }
}
