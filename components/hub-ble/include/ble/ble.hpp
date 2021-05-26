#ifndef HUB_BLE_HPP
#define HUB_BLE_HPP

#include "errc.hpp"

namespace hub::ble
{
    result<void> init() noexcept;

    result<void> deinit() noexcept;
}

#endif