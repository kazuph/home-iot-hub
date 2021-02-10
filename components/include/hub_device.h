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

        using base = ble::client;
        using notify_callback_t         = std::function<void(std::string_view)>;
        using disconnect_callback_t     = std::function<void(void)>;

        /*
            This function object is to be called every time the fresh data is received from the device
            and formatted to the JSON string. This is done by setting ble::client::notify_callback
            to the device specific function that handles data.
        */
        notify_callback_t       notify_callback;

        /*
            This function object should be called whenever device disconnects and after device-specific (if any)
            cleanup/retry actions are finished.
        */
        disconnect_callback_t   disconnect_callback;

        device_base();

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
        virtual std::string_view get_device_name() const                                = 0;

        /*
            Perform device-specific connection.
        */
        virtual esp_err_t connect(const ble::mac& address)                              = 0;

        /*
            Disconnect from the device.
        */
        virtual esp_err_t disconnect()                                                  = 0;

        /*
            Send data to the device in a form of JSON formatted string. Implementation
            should convert the data into device-specific array of bytes and use ble::client
            methods to send data to the device.
        */
        virtual esp_err_t send_data(std::string_view data)                              = 0;

    protected:

        /*
            Data received event handler.
        */
        virtual void data_received(const uint16_t char_handle, std::string_view data)   = 0;

        /*
            Device disconnected event handler.
        */
        virtual void device_disconnected()                                              = 0;
    };
}

#endif