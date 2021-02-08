#ifndef HUB_DEVICE_LIST_H
#define HUB_DEVICE_LIST_H

#include <type_traits>
#include <memory>
#include <map>
#include <string>
#include <utility>
#include <string_view>
#include <algorithm>

#include "hub_ble.h"
#include "stdbool.h"
#include "hub_device.h"

namespace hub
{
    extern const std::map<std::string_view, std::function<std::unique_ptr<device_base>()>> supported_devices;
    
    template<typename _Ty>
    inline constexpr bool Is_device = std::is_base_of_v<device_base, _Ty>;

    template<typename _DeviceTy>
    inline std::enable_if_t<Is_device<_DeviceTy>, std::unique_ptr<device_base>> make_device()
    {
        return std::make_unique<_DeviceTy>();
    }

    template<typename _DeviceTy>
    inline constexpr std::pair<std::string_view, std::function<std::unique_ptr<device_base>()>> register_device_type()
    {
        return std::make_pair(_DeviceTy::device_name, make_device<_DeviceTy>);
    }

    inline bool is_device_supported(std::string_view device_name)
    {
        return (supported_devices.find(device_name) != supported_devices.end());
    }

    inline std::unique_ptr<device_base> device_init(std::string_view device_name)
    {
        return (supported_devices.at(device_name))();
    }
}

#endif