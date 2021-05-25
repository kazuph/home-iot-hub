#ifndef HUB_SERVICE_BLE_HPP
#define HUB_SERVICE_BLE_HPP

#include "ble/mac.hpp"
#include "ble/client.hpp"
#include "utils/json.hpp"
#include "device_base.hpp"
#include "async/rx/traits.hpp"

#include "esp_log.h"

#include <cstdint>
#include <variant>
#include <list>
#include <memory>

namespace hub::service
{
    /**
     * @brief Service adaptor for ble::client class. Serves as message source.
     */
    class ble_message_source
    {
    public:

        using tag               = async::rx::tag::two_way_tag;
        using in_message_t      = typename device::device_base::in_message_t;
        using out_message_t     = typename device::device_base::out_message_t;
        using message_handler_t = std::function<void(out_message_t&&)>;

        ble_message_source()                                        = delete;

        explicit ble_message_source(std::weak_ptr<device::device_base> client) :
            m_client{ client }
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
        }

        ble_message_source(const ble_message_source&)               = delete;

        ble_message_source(ble_message_source&&)                    = default;

        ~ble_message_source()                                       = default;

        ble_message_source& operator=(const ble_message_source&)    = delete;

        ble_message_source& operator=(ble_message_source&&)         = default;

        template<typename MessageHandlerT>
        void set_message_handler(MessageHandlerT message_handler)
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);

            m_message_handler = message_handler;

            if (m_client.expired())
            {
                return;
            }

            m_client.lock()->set_message_handler(m_message_handler);
        }

    private:

        static constexpr const char* TAG{ "hub::service::ble" };

        message_handler_t                   m_message_handler;

        std::weak_ptr<device::device_base>  m_client;
    };

    static_assert(async::rx::is_valid_two_way_v<ble_message_source>, "ble is not a valid source service.");
}

#endif