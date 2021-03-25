#include "hub_device.h"

#include "esp_log.h"

namespace hub
{
    device_base::device_base(std::string_view id) :
        connect_event_handler       {  },
        disconnect_event_handler    {  },
        notify_event_handler        {  },
        id                          {  }
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        std::copy(id.begin(), id.end(), this->id.begin());

        base::connect_event_handler += [this](const base* sender, base::connect_event_args args) {
            connect_event_handler.invoke(this, {});
        };

        base::disconnect_event_handler += [this](const base* sender, base::disconnect_event_args args) {
            device_disconnected();
        };

        base::notify_event_handler += [this](const base* sender, base::notify_event_args args) {
            data_received(args.handle, args.data);
        };
    }
}