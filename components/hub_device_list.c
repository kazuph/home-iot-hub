#include "hub_device_list.h"

#ifdef CONFIG_SUPPORT_XIAOMI_MI_KETTLE
#include "device_mikettle.h"
#endif
#ifdef CONFIG_SUPPORT_XIAOMI_MI_COMPOSITION_SCALE_2

#endif

const ble_device_type g_ble_supported_devices[] = {

#ifdef CONFIG_SUPPORT_XIAOMI_MI_KETTLE
    {
        .name = MIKETTLE_DEVICE_NAME,
        .connect = &mikettle_connect,
        .notify = NULL,
        .disconnect = NULL
    },
#endif

#ifdef CONFIG_SUPPORT_XIAOMI_MI_COMPOSITION_SCALE_2
    {
        .name = NULL,
        .connect = NULL,
        .notify = NULL,
        .disconnect = NULL
    },
#endif

};