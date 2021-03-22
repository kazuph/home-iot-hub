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
#include "esp_gattc_api.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "hub_ble_mac.h"
#include "hub_event.h"

namespace hub::ble
{
    struct scan_results_event_args
    {
        const std::string device_name;
        const mac device_address;
    };

    using scan_results_event_handler_t = event_handler<std::nullptr_t, scan_results_event_args>;

    extern scan_results_event_handler_t scan_results_event_handler;

    inline constexpr uint16_t MAX_CLIENTS{ CONFIG_BTDM_CTRL_BLE_MAX_CONN };

    esp_err_t init();

    esp_err_t deinit();

    esp_err_t start_scanning(uint32_t scan_time);

    esp_err_t stop_scanning();

    class client
    {
    public:

        friend esp_err_t init();

        using notify_callback_t     = std::function<void(const uint16_t, std::string_view)>;
        using disconnect_callback_t = std::function<void(void)>;

    private:

        static constexpr EventBits_t CONNECT_BIT            { BIT0 };
        static constexpr EventBits_t SEARCH_SERVICE_BIT     { BIT1 };
        static constexpr EventBits_t WRITE_CHAR_BIT         { BIT2 };
        static constexpr EventBits_t READ_CHAR_BIT          { BIT3 };
        static constexpr EventBits_t WRITE_DESCR_BIT        { BIT4 };
        static constexpr EventBits_t READ_DESCR_BIT         { BIT5 };
        static constexpr EventBits_t REG_FOR_NOTIFY_BIT     { BIT6 };
        static constexpr EventBits_t UNREG_FOR_NOTIFY_BIT   { BIT7 };
        static constexpr EventBits_t DISCONNECT_BIT         { BIT8 };

        static std::array<client*, CONFIG_BTDM_CTRL_BLE_MAX_CONN> clients;

        static client* get_client(const esp_gatt_if_t gattc_if);

        static void gattc_callback(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);

        // Connection parameters
        uint16_t            app_id;
        uint16_t            gattc_if;
        uint16_t            conn_id;
        uint16_t            service_start_handle;
        uint16_t            service_end_handle;
        mac                 address;

        EventGroupHandle_t  event_group;

        // Pass data between global GATTC callback and read_* functions
        uint16_t*           buff_length;
        uint8_t*            buff;

    public:

        notify_callback_t       notify_callback;
        disconnect_callback_t   disconnect_callback;

        client();

        client(const client&)               = delete;

        client(client&&)                    = default;

        client& operator=(const client&)    = delete;

        client& operator=(client&&)         = default;

        virtual ~client();
        
        const mac& get_address() const
        {
            return address;
        }

        esp_err_t connect(const mac& address);

        esp_err_t disconnect();

        esp_err_t register_for_notify(uint16_t handle);

        esp_err_t unregister_for_notify(uint16_t handle);

        esp_err_t get_services(esp_gattc_service_elem_t* services, uint16_t* count);

        esp_err_t get_service(const esp_bt_uuid_t* uuid);

        esp_err_t get_characteristics(esp_gattc_char_elem_t* characteristics, uint16_t* count);

        esp_err_t write_characteristic(const uint16_t handle, const uint8_t* value, const uint16_t value_length);

        esp_err_t read_characteristic(const uint16_t handle, uint8_t* value, uint16_t* value_length);

        esp_err_t get_descriptors(const uint16_t char_handle, esp_gattc_descr_elem_t* descr, uint16_t* count);

        esp_err_t write_descriptor(const uint16_t handle, const uint8_t* value, const uint16_t value_length);

        esp_err_t read_descriptor(const uint16_t handle, uint8_t* value, uint16_t* value_length);
    };
}

#endif