#include "hub_device.h"
#include "hub_utils.h"

#include "esp_log.h"

namespace hub
{
    esp_err_t device_base::register_notify_callback(notify_callback_t&& notify_callback)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        this->notify_callback = std::move(notify_callback);
        return ESP_OK;
    }

    esp_err_t device_base::register_disconnect_callback(disconnect_callback_t&& disconnect_callback)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        if (disconnect_callback == nullptr)
        {
            ble::client::disconnect_callback = nullptr;
            return ESP_OK;
        }

        /*
            Delegate callback to the task_queue to avoid calling hub::ble methods on the same
            thread as BLE stack callbacks. Register callback directly in hub::ble layer.
        */
        ble::client::disconnect_callback = [fun{ std::move(disconnect_callback) }]() { 
            task_queue.push([fun{ std::move(fun) }]() { 
                if (!fun) 
                {
                    return;
                } 

                fun();
            });
        };

        return ESP_OK;
    }
}