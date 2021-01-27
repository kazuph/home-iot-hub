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

        using data_ready_callback_t = std::function<void(std::string_view)>;

    protected:

        ble::client::handle_t client_handle;
        data_ready_callback_t data_ready_callback;

    public:

        device_base();

        virtual ~device_base();

        virtual esp_err_t connect(esp_bd_addr_t address) = 0;

        virtual esp_err_t update_data(std::string_view data) = 0;

        esp_err_t register_data_ready_callback(data_ready_callback_t data_ready_callback);
    };
}

#endif