#include "ble/ble.hpp"

#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_gap_ble_api.h"

namespace hub::ble
{
    static constexpr const char* TAG{ "hub::ble" };

    tl::expected<void, esp_err_t> init() noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        using result_type = tl::expected<void, esp_err_t>;

        esp_err_t result = ESP_OK;

        esp_bt_controller_config_t bt_config = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
        result = esp_bt_controller_init(&bt_config);
        if (result != ESP_OK)
        {
            goto error;
        }

        result = esp_bt_controller_enable(ESP_BT_MODE_BLE);
        if (result != ESP_OK)
        {
            goto cleanup_bt_controller_init;
        }

        result = esp_bluedroid_init();
        if (result != ESP_OK)
        {
            goto cleanup_bt_controller_enable;
        }

        result = esp_bluedroid_enable();
        if (result != ESP_OK)
        {
            goto cleanup_bluedroid_init;
        }

        ESP_LOGI(TAG, "BLE module initialized.");

        return result_type();

    cleanup_bluedroid_init:
        esp_bluedroid_deinit();
    cleanup_bt_controller_enable:
        esp_bt_controller_disable();
    cleanup_bt_controller_init:
        esp_bt_controller_deinit();
    error:

        ESP_LOGE(TAG, "BLE module initialization failed.");
        return result_type(tl::unexpect, result);
    }

    tl::expected<void, esp_err_t> deinit() noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        using result_type = tl::expected<void, esp_err_t>;
        
        esp_bluedroid_disable();
        esp_bluedroid_deinit();
        esp_bt_controller_disable();
        esp_bt_controller_deinit();

        ESP_LOGI(TAG, "BLE module deinitialized.");

        return result_type();
    }
}