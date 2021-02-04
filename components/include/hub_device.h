#ifndef HUB_DEVICE_H
#define HUB_DEVICE_H

#include <functional>
#include <string_view>

#include "esp_err.h"
#include "hub_ble.h"

namespace hub
{
    class device_base
    {
    public:

        static constexpr const char* TAG = "device_base";

        using notify_callback_t = std::function<void(std::string_view)>;
        using disconnect_callback_t = std::function<void(void)>;

    protected:

        ble::client::handle_t client_handle;

        notify_callback_t notify_callback;

    public:

        device_base();

        virtual ~device_base();

        virtual std::string_view get_device_name() = 0;

        virtual esp_err_t connect(std::string_view address) = 0;

        virtual esp_err_t update_data(std::string_view data) = 0;

        esp_err_t register_notify_callback(notify_callback_t&& notify_callback);

        esp_err_t register_disconnect_callback(disconnect_callback_t&& disconnect_callback);
    };
}

#endif