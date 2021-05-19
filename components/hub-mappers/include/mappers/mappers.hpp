#ifndef HUB_DEVICE_MAPPERS_HPP
#define HUB_DEVICE_MAPPERS_HPP

#include "utils/json.hpp"
#include "utils/const_map.hpp"
#include "ble/client.hpp"

#include <cstdint>
#include <memory>
#include <string_view>
#include <variant>

// DEVICE INCLUDES
#include "xiaomi-mikettle.hpp"

namespace hub::device::mappers
{
    //inline constexpr std::size_t SUPPORTED_DEVICE_COUNT{ 1 };

    using device_name_type                  = std::string_view;
    using on_after_connect_function_type    = void(*)(std::shared_ptr<ble::client>);
    using device_data_to_json_function_type = utils::json(*)(const ble::event::notify_event_args_t&);
    using send_data_to_device_function_type = void(*)(utils::json&& data);

    using event_handler_function_type = std::variant<
        on_after_connect_function_type, 
        device_data_to_json_function_type, 
        send_data_to_device_function_type>;

    enum class operation_type
    {
        on_after_connect,
        device_data_to_json,
        send_data_to_device
    };

    using operation_mapper_type = utils::const_map<operation_type, event_handler_function_type, 3>;

    template<typename DeviceT>
    inline constexpr std::pair<device_name_type, operation_mapper_type> map_device() noexcept
    {
        return {
            DeviceT::device_name, { {
                { operation_type::on_after_connect,     &DeviceT::on_after_connect    }, 
                { operation_type::device_data_to_json,  &DeviceT::device_data_to_json }, 
                { operation_type::send_data_to_device,  &DeviceT::send_data_to_device } 
        } } };
    }

    // Device-specific functions are mapped here
    inline constexpr auto g_device_mapper = utils::make_const_map<device_name_type, operation_mapper_type>(
        map_device<xiaomi::mikettle>()
    );

    template<operation_type Operation>
    inline auto get_operation_for(device_name_type device_name)
    {
        return std::get<static_cast<std::size_t>(Operation)>(g_device_mapper.at(device_name)[Operation]);
    }
}

#endif