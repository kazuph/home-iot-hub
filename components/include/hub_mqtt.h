#ifndef HUB_MQTT_H
#define HUB_MQTT_H

#include "esp_err.h"
#include "mqtt_client.h"
#include <functional>
#include <string_view>

namespace hub::mqtt
{
    using client_config     = esp_mqtt_client_config_t;

    class client
    {
    public:

        using data_callback_t   = std::function<void(std::string_view, std::string_view)>;

    private:

        static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

        esp_mqtt_client_handle_t    client_handle;
        data_callback_t             data_callback;

    public:

        client() : client_handle{ nullptr }, data_callback{ nullptr }
        {};

        explicit client(const client_config* const config);

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
        
        esp_err_t register_data_callback(data_callback_t callback);
    };
}

#endif