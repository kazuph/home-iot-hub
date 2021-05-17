#ifndef HUB_SERVICE_BLE_HPP
#define HUB_SERVICE_BLE_HPP

#include "traits.hpp"

#include "ble/mac.hpp"
#include "ble/client.hpp"
#include "utils/json.hpp"

#include "esp_log.h"

#include <cstdint>
#include <variant>
#include <list>
#include <memory>

namespace hub::service
{
    /**
     * @brief BLE actor class. Represents BLE endpoint. Operates both as source and sink.
     * Handles BLE scan, device connection, disconnection and updates from both directions.
     */
    class ble
    {
    public:

        struct scan_control_t
        {
            bool m_enable;
        };

        struct scan_result_t
        {
            std::string m_name;
            std::string m_address;
        };

        struct device_connect_t
        {
            hub::ble::mac   m_address;
            std::string     m_id;
            std::string     m_name;
        };

        struct device_disconnect_t
        {
            std::string m_id;
        }; 
    
        struct device_update_t
        {
            std::string m_id;
            utils::json m_data;
        };

        using service_tag       = service_tag::two_way_service_tag;
        using in_message_t      = std::variant<scan_control_t, device_connect_t, device_disconnect_t, device_update_t>;
        using out_message_t     = std::variant<scan_result_t, device_update_t>;
        using message_handler_t = std::function<void(out_message_t&&)>;

        ble();

        ble(const ble&) = delete;

        ble(ble&&) = default;

        ~ble();

        void process_message(in_message_t&& message) const;

        template<typename MessageHandlerT>
        void set_message_handler(MessageHandlerT&& message_handler)
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);

            m_message_handler = std::forward<MessageHandlerT>(message_handler);
        }

    private:

        static constexpr const char* TAG{ "hub::service::ble" };

        message_handler_t                                       m_message_handler;

        mutable std::list<std::shared_ptr<hub::ble::client>>    m_client_list;

        void scan_control_handler(scan_control_t&& scan_start_args) const;

        void device_connect_handler(device_connect_t&& device_connect_args) const;

        void device_disconnect_handler(device_disconnect_t&& device_disconnect_args) const;

        void device_update_handler(device_update_t&& device_update_args) const;
    };

    static_assert(is_valid_two_way_service_v<ble>, "ble is not a valid two way service.");
}

#endif