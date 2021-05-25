#ifndef HUB_DEVICE_MAPPERS_HPP
#define HUB_DEVICE_MAPPERS_HPP

#include "utils/json.hpp"
#include "utils/const_map.hpp"

#include <cstdint>
#include <memory>
#include <string_view>
#include <variant>
#include <type_traits>

// DEVICE INCLUDES
#include "device_base.hpp"
#include "xiaomi-mikettle.hpp"

namespace hub::device::mappers
{
    using device_name_type              = std::string_view;
    using device_factory_function_type  = std::shared_ptr<device_base>(*)();

    template<typename DeviceT>
    inline constexpr auto map_device() noexcept
    {
        static_assert(std::is_base_of_v<device_base, DeviceT>, "Provided device class does not derive from device_base.");
        return std::make_pair(
            DeviceT::device_name, 
            []() -> std::shared_ptr<device_base> { 
                return std::static_pointer_cast<device_base>(std::make_shared<DeviceT>()); 
            }
        );
    }

    // Device-specific functions are mapped here
    inline constexpr auto g_device_mapper = utils::make_const_map<device_name_type, device_factory_function_type>(
        map_device<xiaomi::mikettle>()
    );

    inline auto get_device_factory(device_name_type device_name)
    {
        return g_device_mapper.at(device_name);
    }

    inline auto make_device(device_name_type device_name)
    {
        return get_device_factory(device_name)();
    }
}

#endif