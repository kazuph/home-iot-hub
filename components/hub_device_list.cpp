#include "hub_device_list.h"

#ifdef CONFIG_SUPPORT_XIAOMI_MI_KETTLE
#include "device_mikettle.h"
#endif

namespace hub
{
    const std::map<std::string_view, std::function<std::unique_ptr<device_base>()>> supported_devices
    {
#ifdef CONFIG_SUPPORT_XIAOMI_MI_KETTLE
        register_device_type<MiKettle>()
#endif
    };
}