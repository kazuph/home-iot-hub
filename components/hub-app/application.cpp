#include "app/application.hpp"

#include <string_view>

#include "esp_err.h"
#include "esp_log.h"

#include "tl/expected.hpp"

#include "app/configuration.hpp"

namespace hub
{
    void application::run()
    {
        constexpr std::string_view CONFIG_FILE = "/spiffs/config.json";

        configuration config;

        {
            ESP_LOGI(TAG, "Entering state: init_t.");
            ESP_LOGD(TAG, "Initializing filesystem.");
            
            auto exp_config = get_state<init_t>().initialize_filesystem()
                .and_then([this]() {
                    ESP_LOGD(TAG, "Initializing BLE.");
                    return get_state<init_t, utils::no_state_check>().initialize_ble(); 
                })
                .and_then([=]() {
                    ESP_LOGD(TAG, "Reading configuration data from: %s.", CONFIG_FILE.data());
                    return get_state<init_t, utils::no_state_check>().read_config(CONFIG_FILE); 
                })
                .or_else([this](esp_err_t err) {
                    ESP_LOGE(TAG, "Initialization failed. Error code: %i [%s].", err, esp_err_to_name(err));
                    set_state<cleanup_t>().cleanup(); 
                });

            config = std::move(exp_config.value());

            get_state<init_t>().connect_to_wifi(config)
                .or_else([this](esp_err_t err) {
                    ESP_LOGE(TAG, "WiFi connection failed. Error code: %i [%s].", err, esp_err_to_name(err));
                    set_state<cleanup_t>().cleanup(); 
                });
        }

        while (!is_state<cleanup_t>())
        {
            ESP_LOGI(TAG, "Entering state: not_connected_t.");

            set_state<not_connected_t>(config).wait_for_id_assignment()
                .or_else([this](esp_err_t err) {
                    ESP_LOGE(TAG, "Hub connection failed. Error code: %i [%s].", err, esp_err_to_name(err));
                    ESP_LOGI(TAG, "Entering state: cleanup_t.");
                    set_state<cleanup_t>();
                })
                .and_then([&, this](std::string&& id) {
                    config.hub.id = std::move(id);

                    ESP_LOGI(TAG, "Entering state: connected_t.");
                    return set_state<connected_t>(config).process_events();
                });
        }

        ESP_LOGI(TAG, "Exiting...");
    }
}
