#ifndef HUB_BLE_CLIENT_HPP
#define HUB_BLE_CLIENT_HPP

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include <cstdint>
#include <vector>
#include <memory>
#include <new>
#include <functional>
#include <utility>
#include <string_view>
#include <map>

#include "tl/expected.hpp"

#include "esp_gatt_defs.h"
#include "esp_gattc_api.h"
#include "esp_err.h"

#include "utils/mac.hpp"
#include "timing/timing.hpp"

#include "service.hpp"
#include "characteristic.hpp"
#include "descriptor.hpp"

namespace hub::ble
{
    using namespace timing::literals;

    inline constexpr uint16_t   MAX_CLIENTS{ CONFIG_BTDM_CTRL_BLE_MAX_CONN };
    inline constexpr auto       BLE_TIMEOUT{ 5_s };

    class client : public std::enable_shared_from_this<client>
    {
    public:

        friend class service;
        friend class characteristic;
        friend class descriptor;

        using notify_event_handler_t = std::function<void(const std::vector<uint8_t>&)>;
        using shared_client = std::enable_shared_from_this<client>;

        /**
         * @brief Construct a new client object
         * 
         */
        client();

        virtual ~client();

        /**
         * @brief Get the shared client object
         * 
         * @return std::shared_ptr<client> 
         */
        std::shared_ptr<client> get_shared_client() noexcept
        {
            return shared_client::shared_from_this();
        }

        /**
         * @brief 
         * 
         * @param address 
         * @return tl::expected<void, esp_err_t> 
         */
        tl::expected<void, esp_err_t> connect(utils::mac address) noexcept;

        /**
         * @brief 
         * 
         * @return tl::expected<void, esp_err_t> 
         */
        tl::expected<void, esp_err_t> disconnect() noexcept;

        /**
         * @brief Get the services object
         * 
         * @return tl::expected<std::vector<service>, esp_err_t> 
         */
        tl::expected<std::vector<service>, esp_err_t> get_services() const noexcept;

        /**
         * @brief Get the service by uuid object
         * 
         * @param uuid 
         * @return tl::expected<service, esp_err_t> 
         */
        tl::expected<service, esp_err_t> get_service_by_uuid(const esp_bt_uuid_t* uuid) const noexcept;

        /**
         * @brief Get the address object
         * 
         * @return utils::mac 
         */
        utils::mac get_address() const noexcept
        {
            return m_address;
        }

    private:

        static constexpr const char* TAG{ "hub::ble::client" };

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

        static void gattc_callback(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t* param) noexcept;

        uint16_t                                                    m_connection_id;
        uint16_t                                                    m_app_id;
        uint16_t                                                    m_gattc_interface;
        utils::mac                                                  m_address;
        EventGroupHandle_t                                          m_event_group;

        mutable std::vector<service>                                m_services_cache;
        mutable std::vector<uint8_t>                                m_characteristic_data_cache;
        mutable std::vector<uint8_t>                                m_descriptor_data_cache;

        mutable std::map<uint16_t, notify_event_handler_t>          m_characteristics_callbacks;

        mutable std::shared_ptr<client>                             m_self_ref;
    };
}

#endif