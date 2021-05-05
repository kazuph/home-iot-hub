#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "mac.hpp"
#include "timing.hpp"
#include "event.hpp"
#include "service.hpp"
#include "characteristic.hpp"
#include "descriptor.hpp"

#include "esp_gatt_defs.h"
#include "esp_gattc_api.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include <cstdint>
#include <vector>
#include <memory>
#include <new>
#include <functional>
#include <utility>
#include <string_view>

namespace hub::ble
{
    using namespace timing::literals;

    inline constexpr uint16_t   MAX_CLIENTS{ CONFIG_BTDM_CTRL_BLE_MAX_CONN };
    inline constexpr auto       BLE_TIMEOUT{ 5_s };

    namespace event
    {
        struct notify_event_args_t
        {
            uint16_t                m_handle;
            std::vector<uint8_t>    m_data;
        };

        using notify_event_handler_t    = hub::event::event_handler<notify_event_args_t>;
        using notify_function_t         = notify_event_handler_t::function_type;
    }

    /**
     * @brief Class client represents a BLE peripheral.
     */
    class client : public std::enable_shared_from_this<client>
    {
    public:

        friend class service;
        friend class characteristic;
        friend class descriptor;
        friend std::shared_ptr<client> get_client_by_mac(const mac& address) noexcept;

        using shared_client = std::enable_shared_from_this<client>;

        /**
         * @brief Create client object. Constructor of class client does not initialize all the neccessary data
         * and should not be called directly. This function checks whether a new client can be created, assigns application
         * ID and, indirectly, GATT interface ID.
         * 
         * @param id ID of the client.
         * 
         * @return std::shared_ptr<client> 
         */
        static std::shared_ptr<client> make_client(std::string_view id);

        static std::shared_ptr<client> get_client_by_mac(const mac& address) noexcept;

        static std::shared_ptr<client> get_client_by_id(std::string_view id) noexcept;

        client();

        ~client();

        /**
         * @brief Get the shared client object.
         * 
         * @return std::shared_ptr<client> 
         */
        std::shared_ptr<client> get_shared_client()
        {
            return shared_client::shared_from_this();
        }

        /**
         * @brief Connect with the client with the given MAC address.
         * 
         * @param address Address of the client.
         */
        void connect(mac address);

        /**
         * @brief Disconnecy with the client.
         */
        void disconnect();

        /**
         * @brief Get the client services.
         * 
         * @return std::vector<service> 
         */
        std::vector<service> get_services() const;

        /**
         * @brief Subscribe to the BLE characteristic being represented by the current characteristic object.
         * 
         * @param fun Callback function handling event.
         */
        void set_notify_event_handler(event::notify_function_t fun)
        {
            m_notify_event_handler += fun;
        }

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

        uint16_t                                m_connection_id;
        uint16_t                                m_app_id;
        uint16_t                                m_gattc_interface;
        mac                                     m_address;
        EventGroupHandle_t                      m_event_group;
        std::string                             m_id;

        mutable std::vector<service>            m_services_cache;
        mutable std::vector<uint8_t>            m_characteristic_data_cache;
        mutable std::vector<uint8_t>            m_descriptor_data_cache;

        event::notify_event_handler_t           m_notify_event_handler;
    };
}

#endif