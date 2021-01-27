#include "hub_device.h"

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
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

    esp_err_t device_base::register_data_ready_callback(data_ready_callback_t data_ready_callback)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        this->data_ready_callback = data_ready_callback;
        return ESP_OK;
    }
}