#ifndef HUB_BLE_H
#define HUB_BLE_H

#include <cstdint>
#include <functional>
#include <string_view>
#include <limits>
#include <string>
#include <system_error>

#include "esp_err.h"

#include "esp_bt_defs.h"
#include "esp_gatt_defs.h"

#include "hub_ble_mac.h"

namespace hub::ble
{
    using scan_callback_t = std::function<void(std::string_view, const mac&)>;

    inline constexpr uint16_t MAX_CLIENTS{ CONFIG_BTDM_CTRL_BLE_MAX_CONN };

    esp_err_t init();

    esp_err_t deinit();

    esp_err_t start_scanning(uint32_t scan_time);

    esp_err_t stop_scanning();

    esp_err_t register_scan_callback(scan_callback_t&& callback);

    namespace client
    {
        using handle_t = uint8_t;
        using notify_callback_t = std::function<void(const uint16_t, std::string_view)>;
        using disconnect_callback_t = std::function<void(void)>;

        inline constexpr auto INVALID_HANDLE{ std::numeric_limits<handle_t>::max() };

        esp_err_t init(handle_t* client_handle);

        esp_err_t destroy(const handle_t client_handle);

        esp_err_t connect(const handle_t client_handle, const mac& address);

        esp_err_t disconnect(const handle_t client_handle);

        esp_err_t register_for_notify(const handle_t client_handle, uint16_t handle);

        esp_err_t unregister_for_notify(const handle_t client_handle, uint16_t handle);

        esp_err_t register_notify_callback(const handle_t client_handle, notify_callback_t&& callback);

        esp_err_t register_disconnect_callback(const handle_t client_handle, disconnect_callback_t callback);

        esp_err_t get_services(const handle_t client_handle, esp_gattc_service_elem_t* services, uint16_t* count);

        esp_err_t get_service(const handle_t client_handle, const esp_bt_uuid_t* uuid);

        esp_err_t get_characteristics(const handle_t client_handle, esp_gattc_char_elem_t* characteristics, uint16_t* count);

        esp_err_t write_characteristic(const handle_t client_handle, const uint16_t handle, const uint8_t* value, const uint16_t value_length);

        esp_err_t read_characteristic(const handle_t client_handle, const uint16_t handle, uint8_t* value, uint16_t* value_length);

        esp_err_t get_descriptors(const handle_t client_handle, const uint16_t char_handle, esp_gattc_descr_elem_t* descr, uint16_t* count);

        esp_err_t write_descriptor(const handle_t client_handle, const uint16_t handle, const uint8_t* value, const uint16_t value_length);

        esp_err_t read_descriptor(const handle_t client_handle, const uint16_t handle, uint8_t* value, uint16_t* value_length);
    }
}

#endif