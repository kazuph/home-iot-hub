#ifndef HUB_MQTT_CLIENT_HPP
#define HUB_MQTT_CLIENT_HPP

#include "event/event.hpp"

#include "mqtt_client.h"
#include "esp_err.h"

#include <functional>
#include <string_view>
#include <string>

namespace hub::mqtt
{
    namespace event
    {
        struct data_event_args
        {
            std::string topic;
            std::string data;
        };


        using data_event_handler_t      = hub::event::event_handler<data_event_args>;
        using data_event_handler_fun_t  = data_event_handler_t::function_type;
    }

    /**
     * @brief Class representing MQTT client.
     */
    class client
    {
    public:

        client();

        client(client&& other);

        client(const client&) = delete;

        ~client();

        client& operator=(const client&) = delete;

        client& operator=(client&& other);

        /**
         * @brief Connect to the MQTT broker under the specified URI.
         * 
         * @param uri MQTT broker URI in a form "mqtt://<ip>:port".
         * @return esp_err_t ESP_OK on success, other on failure.
         */
        esp_err_t connect(std::string_view uri);

        /**
         * @brief Disconnect from MQTT broker.
         * 
         * @return esp_err_t ESP_OK on success, other on failure.
         */
        esp_err_t disconnect();

        /**
         * @brief Publish data to the MQTT broker. Only valid after successful connection (@see connect).
         * 
         * @param topic Topic under which the data is published.
         * @param data Data to be published.
         * @param retain Retain flag.
         * @return esp_err_t ESP_OK on success, other on failure.
         */
        esp_err_t publish(std::string_view topic, std::string_view data, bool retain = false);

        /**
         * @brief Subscribe to the specified MQTT topic. When data is received on this topic, data_event is fired,
         * calling functions passed to m_data_event_handler (@see set_data_event_handler).
         * 
         * @param topic Topic to subscribe to.
         * @return esp_err_t ESP_OK on success, other on failure.
         */
        esp_err_t subscribe(std::string_view topic);

        /**
         * @brief Unsubscribe from the given topic.
         * 
         * @param topic Topic to unsubscribe from.
         * @return esp_err_t ESP_OK on success, other on failure.
         */
        esp_err_t unsubscribe(std::string_view topic);

        /**
         * @brief Set data event handler. Multiple functions can be submitted. Theese functions get called
         * when data arrives on topics to which client is subscribed to.
         * 
         * @param event_handler Data event callback.
         */
        void set_data_event_handler(event::data_event_handler_fun_t event_handler);

    private:

        static constexpr const char* TAG{ "hub::mqtt::client" };

        static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

        esp_mqtt_client_handle_t    m_client_handle;

        event::data_event_handler_t m_data_event_handler;
    };
}

namespace hub::event
{
    using namespace hub::mqtt::event;
}

#endif