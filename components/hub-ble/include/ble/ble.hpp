#ifndef HUB_BLE_HPP
#define HUB_BLE_HPP

#include "esp_err.h"

#include "tl/expected.hpp"

namespace hub::ble
{
    tl::expected<void, esp_err_t> init() noexcept;

    tl::expected<void, esp_err_t> deinit() noexcept;
}

#endif