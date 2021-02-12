#include "hub_device.h"
#include "hub_utils.h"

#include "esp_log.h"

namespace hub
{
    device_base::device_base(std::string_view id) :
        notify_callback     { nullptr },
        disconnect_callback { nullptr },
        id{ "00000000-0000-0000-0000-000000000000" }
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        std::copy(id.begin(), id.end(), this->id.begin());

        base::disconnect_callback = [this]() { 
            device_disconnected(); 
        };

        base::notify_callback = [this](const uint16_t char_handle, std::string_view data) {
            data_received(char_handle, data);
        };
    }
}