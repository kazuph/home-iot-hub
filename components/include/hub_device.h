#ifndef HUB_DEVICE_H
#define HUB_DEVICE_H

#include <functional>
#include <string_view>
#include <cstdint>
#include <string>
#include <array>

#include "esp_err.h"

#include "hub_ble.h"
#include "hub_json.h"

namespace hub
{
    class device_base : public ble::client
    {
    public:

        struct connect_event_args {};
        
        struct disconnect_event_args {};

        struct notify_event_args
        {
            const std::string data;
        };

        using connect_event_handler_t       = event::event_handler<device_base, connect_event_args>;
        using disconnect_event_handler_t    = event::event_handler<device_base, disconnect_event_args>;
        using notify_event_handler_t        = event::event_handler<device_base, notify_event_args>;

        using base = ble::client;

        connect_event_handler_t      connect_event_handler;
        disconnect_event_handler_t   disconnect_event_handler;
        notify_event_handler_t       notify_event_handler;

        device_base()                               = delete;

        device_base(std::string_view id);

        device_base(const device_base&)             = delete;

        device_base(device_base&&)                  = default;

        device_base& operator=(const device_base&)  = delete;

        device_base& operator=(device_base&&)       = default;

        virtual ~device_base()                      = default;

        std::string_view get_id() const;

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

    private:

        static constexpr const char* TAG    { "device_base" };

        static constexpr size_t UUID_LENGTH { 36 };

        std::array<char, UUID_LENGTH> id;
    };

    inline std::string_view device_base::get_id() const
    {
        return std::string_view(id.data(), id.size());
    }
}

#endif