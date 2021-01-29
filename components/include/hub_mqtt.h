#ifndef HUB_MQTT_H
#define HUB_MQTT_H

#include "esp_err.h"
#include "mqtt_client.h"
#include <functional>
#include <string_view>

namespace hub::mqtt
{

    class client;

    using client_config = esp_mqtt_client_config_t;
    using data_callback_t = std::function<void(std::string_view, std::string_view)>;

    class client
    {
        static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

        esp_mqtt_client_handle_t client_handle;
        data_callback_t data_callback;

    public:

        explicit client(const client_config* const config);
        ~client();

        esp_err_t start();
        esp_err_t stop();
        esp_err_t publish(std::string_view topic, std::string_view data, bool retain);
        esp_err_t subscribe(std::string_view topic);
        esp_err_t unsubscribe(std::string_view topic);
        esp_err_t register_data_callback(data_callback_t callback);
    };

}

#endif