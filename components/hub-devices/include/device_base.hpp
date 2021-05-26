#ifndef HUB_SERVICE_DEVICE_INTERFACE_HPP
#define HUB_SERVICE_DEVICE_INTERFACE_HPP

#include "async/rx/traits.hpp"
#include "ble/client.hpp"
#include "ble/mac.hpp"
#include "utils/json.hpp"

#include <functional>
#include <memory>

namespace hub::device
{
    class device_base
    {
    public:

        using service_tag       = async::rx::tag::two_way_tag;
        using in_message_t      = rapidjson::Document;
        using out_message_t     = rapidjson::Document;
        using message_handler_t = std::function<void(out_message_t&&)>;

        device_base() :
            m_client            { ble::client::make_client().get() },
            m_message_handler   {  }
        {

        }

        template<typename MessageHandlerT>
        void set_message_handler(MessageHandlerT message_handler)
        {
            m_message_handler = message_handler;
        }

        virtual void connect(ble::mac address)          = 0;

        virtual void disconnect()                       = 0;

        virtual void process_message(in_message_t&&)    = 0;

        virtual ~device_base()
        {

        }

    protected:

        std::shared_ptr<ble::client> get_client()
        {
            return m_client;
        }

        const std::shared_ptr<ble::client> get_client() const
        {
            return m_client;
        }

        void invoke_message_handler(out_message_t&& message) const
        {
            if (!m_message_handler)
            {
                return;
            }

            std::invoke(m_message_handler, std::move(message));
        }

    private:

        std::shared_ptr<ble::client>    m_client;

        message_handler_t               m_message_handler;
    };
}

#endif