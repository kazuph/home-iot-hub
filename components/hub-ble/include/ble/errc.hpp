#ifndef HUB_BLE_ERRC_HPP
#define HUB_BLE_ERRC_HPP

#include "utils/result.hpp"

namespace hub::ble
{
    enum class errc
    {
        ok,
        error,
        timeout,
        no_resources,
        not_connected,
        initialization_failed
    };

    template<typename T>
    using result = utils::result<T, errc>;
}

#endif