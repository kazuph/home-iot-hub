#ifndef HUB_DEVICE_H
#define HUB_DEVICE_H

#include <functional>
#include <string_view>

#include "esp_err.h"
#include "hub_ble.h"

namespace hub
{
    class device_base : public ble::client
    {
    public:

        static constexpr const char* TAG = "device_base";

        using notify_callback_t         = std::function<void(std::string_view)>;
        using disconnect_callback_t     = std::function<void(void)>;

    protected:

        /*
            This function is to be called every time the fresh data is received from the device
            and formatted to the JSON string. This is done by setting ble::client::notify_callback
            to the device specific function that handles data.
        */
        notify_callback_t notify_callback;

    public:

        device_base()                               = default;

        device_base(const device_base&)             = delete;

        device_base(device_base&&)                  = default;

        device_base& operator=(const device_base&)  = delete;

        device_base& operator=(device_base&&)       = default;

        virtual ~device_base()                      = default;

        /* These three virtual methods are to be implemented by the device implementation. */

        /*
            Return device name in a form of std::string_view. Every device implementation has to have field
            of type static constexpr std::string_view containing its name.
        */
        virtual std::string_view get_device_name() = 0;

        /*
            Perform device-specific connection.
        */
        virtual esp_err_t connect(const ble::mac& address) = 0;

        /*
            Send data to the device in a form of JSON formatted string.
        */
        virtual esp_err_t update_data(std::string_view data) = 0;

        /*
            Set notify callback. This is set by the app.
        */
        esp_err_t register_notify_callback(notify_callback_t&& notify_callback);

        /*
            Set disconnect callback. This is set by the app.
        */
        esp_err_t register_disconnect_callback(disconnect_callback_t&& disconnect_callback);
    };
}

#endif