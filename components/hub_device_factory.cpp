#include "hub_device_factory.h"

#ifdef CONFIG_SUPPORT_XIAOMI_MI_KETTLE
#include "device_mikettle.h"
#endif

namespace hub::device_factory
{
    template<typename _Ty>
    static constexpr bool Is_device = std::is_base_of_v<device_base, _Ty>;

    template<typename _DeviceTy>
    static std::enable_if_t<Is_device<_DeviceTy>, device_ptr_t> _make_device(device_id_t id)
    {
        return std::make_unique<_DeviceTy>(id);
    }

    template<typename _DeviceTy>
    static constexpr map_type::value_type register_device_type()
    {
        return std::make_pair(_DeviceTy::device_name, _make_device<_DeviceTy>);
    }

    static constexpr utils::const_map<std::string_view, std::function<std::unique_ptr<device_base>(std::string_view)>, 1> supported_devices{ {
#ifdef CONFIG_SUPPORT_XIAOMI_MI_KETTLE
        register_device_type<MiKettle>(),
#endif
    } };

    std::unique_ptr<device_base> make_device(std::string_view device_name, std::string_view id)
    {
        return (supported_devices[device_name])(id);
    }

    bool supported(std::string_view device_name)
    {
        return supported_devices.contains(device_name);
    }

    std::string_view view_on_name(std::string_view device_name)
    {
        return (*supported_devices.find(device_name)).first;
    }
}