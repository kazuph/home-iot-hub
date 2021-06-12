#ifndef HUB_SERVICE_DEVICE_INTERFACE_HPP
#define HUB_SERVICE_DEVICE_INTERFACE_HPP

#include "ble/client.hpp"
#include "ble/mac.hpp"
#include "utils/json.hpp"

#include "esp_log.h"

#include <functional>
#include <memory>

namespace hub::device
{
    class device_base : public ble::client
    {
    public:

        using in_message_t      = rapidjson::Document;
        using out_message_t     = rapidjson::Document;
        using message_handler_t = std::function<void(out_message_t&&)>;

        device_base() :
            m_message_handler{  }
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
        }

        template<typename MessageHandlerT>
        void set_message_handler(MessageHandlerT message_handler)
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            m_message_handler = message_handler;
        }

        virtual void connect(ble::mac address)          = 0;

        virtual void disconnect()                       = 0;

        virtual void process_message(in_message_t&&)    = 0;

        virtual ~device_base()
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
        }

    protected:

        std::shared_ptr<ble::client> get_client() noexcept
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            return get_shared_client();
        }

        void invoke_message_handler(out_message_t&& message) const
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);

            if (!m_message_handler)
            {
                ESP_LOGW(TAG, "Invalid message handler.");
                return;
            }

            std::invoke(m_message_handler, std::move(message));
        }

    private:

        static constexpr const char* TAG{ "hub::device::device_base" };

        message_handler_t m_message_handler;
    };
}

#endif