#include "hub_device.h"
#include "hub_utils.h"

#include "esp_log.h"

namespace hub
{
    device_base::device_base() :
        notify_callback     { nullptr },
        disconnect_callback { nullptr }
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        base::disconnect_callback = [this]() { 
            device_disconnected(); 
        };

        base::notify_callback = [this](const uint16_t char_handle, std::string_view data) {
            data_received(char_handle, data);
        };
    }
}