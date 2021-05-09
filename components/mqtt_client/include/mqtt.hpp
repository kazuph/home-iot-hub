#ifndef MQTT_HPP
#define MQTT_HPP

#include "event.hpp"

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
     * @brief Class representing a single MQTT client.
     */
    class client
    {
    public:

        client();

        explicit client(std::string_view uri);

        client(client&& other);

        client(const client&) = delete;

        ~client();

        client& operator=(const client&) = delete;

        client& operator=(client&& other);

        esp_err_t connect(std::string_view uri);

        esp_err_t publish(std::string_view topic, std::string_view data, bool retain = false);

        esp_err_t subscribe(std::string_view topic);

        esp_err_t unsubscribe(std::string_view topic);

        void set_data_event_handler(event::data_event_handler_fun_t event_handler);

    private:

        static constexpr const char *TAG{ "HUB MQTT" };

        static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

        esp_mqtt_client_handle_t    client_handle;

        event::data_event_handler_t data_event_handler;

        esp_err_t start();

        esp_err_t stop();
    };
}

#endif