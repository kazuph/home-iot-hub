#include "hub_device.h"
#include "hub_utils.h"

#include "esp_log.h"

namespace hub
{
    device_base::device_base()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        if (ble::client::init(&client_handle) != ESP_OK)
        {
            ESP_LOGE(TAG, "BLE client initialization failed.");
            abort();
        }
    }

    device_base::~device_base()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        ble::client::destroy(client_handle);
    }

    esp_err_t device_base::register_notify_callback(notify_callback_t&& notify_callback)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        this->notify_callback = std::move(notify_callback);
        return ESP_OK;
    }

    esp_err_t device_base::register_disconnect_callback(disconnect_callback_t&& disconnect_callback)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        /*
            Delegate callback to the task_queue to avoid calling hub::ble methods on the same
            thread as BLE stack callbacks. Register callback directly in hub::ble layer.
        */
        esp_err_t result = ble::client::register_disconnect_callback(
            client_handle, 
            [fun{ std::move(disconnect_callback) }]() { 
                task_queue.push([fun{ std::move(fun) }]() { 
                    if (!fun) 
                    {
                        return;
                    } 

                    fun();
                });
            });

        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Register disconnect callback failed.");
            return result;
        }

        return result;
    }
}