#include <string_view>

#include "esp_err.h"
#include "esp_log.h"

#include "tl/expected.hpp"

#include "app/consts.hpp"
#include "app/configuration.hpp"
#include "app/application.hpp"

namespace hub
{
    void application::run()
    {
        ESP_LOGI(TAG, "Entering state: init_t.");

        auto config = get_state<init_t>()
            .initialize_filesystem()
            .and_then([](init_t& state) {
                return state.read_config(CONFIG_FILE_PATH); 
            })
            .or_else([](esp_err_t err) {
                ESP_LOGE(TAG, "Reading configuration file failed.");
            })
            .value();

        get_state<init_t>()
            .connect_to_wifi(config)
            .and_then([&config](init_t& state) {
                return state.send_mqtt_config(config);
            })
            .and_then([](init_t& state) {
                return state.initialize_ble();
            })
            .or_else([](esp_err_t err) {
                ESP_LOGE(TAG, "Initialization failed.");
            })
            .value();

        set_state<running_t>(config)
            .run()
            .or_else([](esp_err_t err) {
                ESP_LOGE(TAG, "Critical error occured.");
            });
    }
}
