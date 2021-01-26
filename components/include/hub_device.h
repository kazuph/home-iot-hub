#ifndef HUB_DEVICE_H
#define HUB_DEVICE_H

#include <functional>

#include "esp_err.h"
#include "hub_ble.h"

namespace hub
{
    class device_base
    {

        static constexpr const char* TAG = "device_base";

    protected:

        ble::client::handle_t client_handle;

    public:

        using data_ready_callback_t = std::function<void(const char*, uint16_t)>;

        device_base();

        virtual ~device_base();

        virtual esp_err_t connect(esp_bd_addr_t address) = 0;

        virtual esp_err_t reconnect() = 0;

        virtual esp_err_t register_data_ready_callback(data_ready_callback_t notify_callback) = 0;

        virtual esp_err_t update_data(const char* data, uint16_t data_length) = 0;
    };
}

#endif