#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "mac.hpp"
#include "timing.hpp"

#include "esp_gatt_defs.h"
#include "esp_gattc_api.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include <cstdint>
#include <vector>
#include <memory>
#include <new>

namespace hub::ble
{
    using namespace timing::literals;

    inline constexpr uint16_t   MAX_CLIENTS{ CONFIG_BTDM_CTRL_BLE_MAX_CONN };
    inline constexpr auto       BLE_TIMEOUT{ 5_s };

    class service;
    class characteristic;
    class descriptor;

    class client : public std::enable_shared_from_this<client>
    {
    public:

        friend class service;
        friend class characteristic;
        friend class descriptor;

        using shared_client = std::enable_shared_from_this<client>;

        static std::shared_ptr<client> make_client();

        client();

        ~client();

        std::shared_ptr<client> get_shared_client()
        {
            return shared_client::shared_from_this();
        }

        void connect(mac address);

        void disconnect();

        std::vector<service> get_services() const;

    private:

        static constexpr const char* TAG{ "CLIENT" };

        static constexpr EventBits_t CONNECT_BIT            { BIT0 };
        static constexpr EventBits_t SEARCH_SERVICE_BIT     { BIT1 };
        static constexpr EventBits_t WRITE_CHAR_BIT         { BIT2 };
        static constexpr EventBits_t READ_CHAR_BIT          { BIT3 };
        static constexpr EventBits_t WRITE_DESCR_BIT        { BIT4 };
        static constexpr EventBits_t READ_DESCR_BIT         { BIT5 };
        static constexpr EventBits_t REG_FOR_NOTIFY_BIT     { BIT6 };
        static constexpr EventBits_t UNREG_FOR_NOTIFY_BIT   { BIT7 };
        static constexpr EventBits_t DISCONNECT_BIT         { BIT8 };
        static constexpr EventBits_t FAIL_BIT               { BIT15 };

        static void gattc_callback(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t* param);

        uint16_t                        m_connection_id;
        uint16_t                        m_app_id;
        uint16_t                        m_gattc_interface;
        mac                             m_address;
        EventGroupHandle_t              m_event_group;

        mutable std::vector<service>    m_services_cache;
        mutable std::vector<uint8_t>    m_characteristic_data_cache;
        mutable std::vector<uint8_t>    m_descriptor_data_cache;
    };
}

#endif