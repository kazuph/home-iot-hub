#ifndef HUB_MQTT_H
#define HUB_MQTT_H

#include "mqtt_client.h"

#include "esp_err.h"

#include <functional>
#include <string_view>
#include <string>

#include "hub_event.h"

namespace hub::mqtt
{
    struct data_event_args
    {
        std::string topic;
        std::string data;
    };

    using data_event_handler_t = event_handler<data_event_args>;

    class client
    {
    public:

        data_event_handler_t data_event_handler;

        client();

        explicit client(std::string_view uri, uint16_t port);

        client(client&& other);

        client(const client&) = delete;

        ~client();

        client& operator=(const client&) = delete;

        client& operator=(client&& other);

        esp_err_t start();

        esp_err_t stop();

        esp_err_t publish(std::string_view topic, std::string_view data, bool retain = false);

        esp_err_t subscribe(std::string_view topic);

        esp_err_t unsubscribe(std::string_view topic);

    private:

        static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

        esp_mqtt_client_handle_t    client_handle;
    };
}

#endif