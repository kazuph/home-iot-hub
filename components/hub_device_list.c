#include "hub_device_list.h"
#include "string.h"

#ifdef CONFIG_SUPPORT_XIAOMI_MI_KETTLE
#include "device_mikettle.h"
#endif
#ifdef CONFIG_SUPPORT_XIAOMI_MI_COMPOSITION_SCALE_2

#endif

static const ble_device_type s_ble_supported_devices[] = {

#ifdef CONFIG_SUPPORT_XIAOMI_MI_KETTLE
    {
        .name = XIAOMI_MI_KETTLE_DEVICE_NAME,
        .connect = &mikettle_connect,
        .reconnect = &mikettle_reconnect,
        .register_data_ready_callback = &mikettle_register_data_ready_callback,
        .update_data = &mikettle_update_data
    },
#endif

#ifdef CONFIG_SUPPORT_XIAOMI_MI_COMPOSITION_SCALE_2
    {
        .name = NULL,
        .connect = NULL,
        .reconnect = NULL,
        .register_data_ready_callback = NULL,
        .update_data = NULL
    },
#endif

};

const ble_device_type* get_type_for_device_name(const char* name)
{
    for (uint16_t i = 0; i < (uint16_t)(sizeof(s_ble_supported_devices) / sizeof(ble_device_type)); i++)
    {
        if (strcmp(name, s_ble_supported_devices[i].name) == 0)
        {
            return &s_ble_supported_devices[i];
        }
    }

    return NULL;
}

bool is_device_supported(const char* name)
{
    for (uint16_t i = 0; i < (uint16_t)(sizeof(s_ble_supported_devices) / sizeof(ble_device_type)); i++)
    {
        if (strcmp(name, s_ble_supported_devices[i].name) == 0)
        {
            return true;
        }
    }

    return false;
}