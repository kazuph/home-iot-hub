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
     * @brief Service adaptor for ble::client class. Serves as message source.
     */
    class ble_message_source
    {
    public:

        using service_tag       = service_tag::source_service_tag;
        using out_message_t     = ble::event::notify_event_args_t;
        using message_handler_t = std::function<void(out_message_t&&)>;

        ble_message_source()                                        = delete;

        explicit ble_message_source(std::weak_ptr<ble::client> client);

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

            auto client = m_client.lock();

            if (!client)
            {
                ESP_LOGE(TAG, "Client expired.");
                return;
            }

            client->set_notify_event_handler([this](const out_message_t& message) {
                m_message_handler(out_message_t(message));
            });
        }

    private:

        static constexpr const char* TAG{ "hub::service::ble" };

        message_handler_t               m_message_handler;

        std::weak_ptr<hub::ble::client> m_client;
    };

    static_assert(is_valid_source_service_v<ble_message_source>, "ble is not a valid source service.");
}

#endif