#include "application.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"

#include <stdexcept>
#include <fstream>
#include <string>

#include "hub_ble.h"
#include "hub_filesystem.h"
#include "hub_device_factory.h"
#include "hub_const_map.h"
#include "hub_wifi.h"
#include "hub_timing.h"

namespace hub
{
    application::application() :
        mqtt_client             {  },
        scan_results            {  },
        connected_devices       {  },
        disconnected_devices    {  }
    {
        using namespace std::literals;
        using namespace timing::literals;

        ESP_LOGD(TAG, "Function: %s.", __func__);

        filesystem::init();

        {
            utils::json config{};

            {
                constexpr const char* CONFIG_FILE   { "/spiffs/config.json" };
                std::ifstream ifs                   { CONFIG_FILE };

                if (!ifs)
                {
                    throw std::runtime_error("Could not open "s + CONFIG_FILE + "."s);
                }

                ifs >> config;
            }
            
            if (wifi::connect(
                utils::json_cast<std::string_view>(config["WIFI_SSID"sv]), 
                utils::json_cast<std::string_view>(config["WIFI_PASSWORD"sv])) != ESP_OK)
            {
                ESP_LOGE(TAG, "Wifi initialization failed.");
                throw std::runtime_error("WiFi initialization failed.");
            }

            if (wifi::wait_for_connection(10_s) != ESP_OK)
            {
                ESP_LOGE(TAG, "WiFi connection failed.");
                throw std::runtime_error("WiFi connection failed.");
            }

            if (mqtt_start(utils::json_cast<std::string_view>(config["MQTT_URI"sv])) != ESP_OK)
            {
                ESP_LOGE(TAG, "MQTT start failed.");
                throw std::runtime_error("MQTT start failed.");
            }
        }

        if (ble::init() != ESP_OK)
        {
            ESP_LOGE(TAG, "BLE initialization failed.");
            throw std::runtime_error("BLE initialization failed.");
        }

        ble::scan_results_event_handler += [this](auto sender, auto args) {
            ESP_LOGD(TAG, "Function: scan_results_event_handler (lambda).");
            ble_scan_callback(args.device_name, args.device_address);
        };

        load_connected_devices();
        ble_scan_start();
    }

    esp_err_t application::mqtt_start(std::string_view mqtt_uri)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        esp_err_t result{ ESP_OK };

        try 
        {
            mqtt_client = mqtt::client(mqtt_uri);
        }
        catch (const std::runtime_error& err)
        {
            ESP_LOGE(TAG, "%s", err.what());
            return ESP_FAIL;
        }

        result = mqtt_client.start();

        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "MQTT client start failed.");
            return result;
        }

        mqtt_client.subscribe(MQTT_BLE_SCAN_ENABLE_TOPIC);
        mqtt_client.subscribe(MQTT_BLE_CONNECT_TOPIC);
        mqtt_client.subscribe(MQTT_BLE_DISCONNECT_TOPIC);
        mqtt_client.subscribe(MQTT_BLE_DEVICE_WRITE_TOPIC);

        mqtt_client.data_event_handler += [this](auto client, auto args) {
            mqtt_data_callback(args.topic, args.data);
        };

        return result;
    }

    esp_err_t application::mqtt_stop()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        return mqtt_client.stop();
    }

    void application::ble_scan_start(uint16_t ble_scan_time)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        scan_results.clear();
        ble::start_scanning(ble_scan_time);
    }

    void application::ble_scan_stop()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        ble::stop_scanning();
        scan_results.clear();
    }

    void application::dump_connected_devices()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        utils::json devices_to_dump{ utils::json::json_array() };

        auto push_device = [&devices_to_dump](const auto& elem) {

            const auto& [device_id, device_ptr] = elem;

            devices_to_dump.push_back({ {
                { { "id",         device_id                             } },
                { { "name",       device_ptr->get_device_name()         } },
                { { "address",    device_ptr->get_address().to_string() } }
            } });
        };

        std::for_each(connected_devices.begin(), connected_devices.end(), push_device);
        std::for_each(disconnected_devices.begin(), disconnected_devices.end(), push_device);

        {
            std::ofstream ofs;
            ofs.open("/spiffs/devices.json");

            if (!ofs)
            {
                ESP_LOGE(TAG, "Could not open file.");
                return;
            }

            ofs << devices_to_dump.dump();
        }
    }

    void application::run()
    {
        timing::sleep_for(timing::MAX_DELAY);
    }

    void application::load_connected_devices()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        utils::json json_data;

        {
            std::ifstream ifs;
            ifs.open("/spiffs/devices.json");

            if (!ifs)
            {
                ESP_LOGE(TAG, "Could not open file.");
                return;
            }

            ifs >> json_data;
        }

        for (int i = 0; i < json_data.size(); i++)
        {
            std::string_view id;
            std::string_view name;
            std::string_view address;
            auto json_device_object = json_data[i];

            try
            {
                id      = utils::json_cast<std::string_view>(json_device_object["id"]);
                name    = utils::json_cast<std::string_view>(json_device_object["name"]);
                address = utils::json_cast<std::string_view>(json_device_object["address"]);
            }
            catch (const std::invalid_argument& err)
            {
                ESP_LOGE(TAG, "%s", err.what());
                continue;
            }

            ble_device_connect(id, name, ble::mac(address));
        }
    }

    application::~application()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        filesystem::deinit();
        ble::deinit();
        wifi::disconnect();
    }

    void application::ble_scan_callback(std::string_view name, const ble::mac& address)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        // Reconnect to disconnected devices.
        if (auto iter = std::find_if(
                disconnected_devices.begin(), 
                disconnected_devices.end(), 
                [&name](const auto& elem) { 
                    return ((elem.second)->get_device_name() == name); 
                });
            iter != disconnected_devices.end())
        {
            auto& [device_id, device_ptr] = *iter;

            if (device_ptr->connect(device_ptr->get_address()) != ESP_OK)
            {
                return;
            }

            connected_devices[device_id].swap(disconnected_devices[device_id]);
        }

        if (!device_factory::supported(name))
        {
            return;
        }

        scan_results[address.to_string()] = device_factory::view_on_name(name);

        {
            utils::json scan_results_json = utils::json::json_array();

            std::for_each(scan_results.begin(), scan_results.end(), [&scan_results_json](const auto& elem) {

                const auto& [device_address, device_name] = elem;

                scan_results_json.push_back({
                    { "name",    device_name     },
                    { "address", device_address  }
                });
            });

            if (mqtt_client.publish(MQTT_BLE_SCAN_RESULTS_TOPIC, scan_results_json.dump(), true) != ESP_OK)
            {
                ESP_LOGE(TAG, "Client publish failed.");
                return;
            }
        }
    }

    void application::ble_device_connect(std::string_view id, std::string_view name, const ble::mac& address)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        esp_err_t result{ ESP_OK };

        auto device{ device_factory::make_device(name, id) };
        
        if (device == nullptr)
        {
            result = ESP_FAIL;
            ESP_LOGE(TAG, "Could not connect to %s.", name.data());
            return;
        }

        result = device->connect(address);
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Could not connect to %s.", name.data());
            return;
        }

        device->disconnect_event_handler += [this, id{ std::string(id) }](auto sender, auto args) {
            ESP_LOGD(TAG, "Function: disconnect_event_handler (lambda).");
            connected_devices[id].swap(disconnected_devices[id]);
            ble_scan_start();
        };

        device->notify_event_handler += [this](auto sender, auto args) {
            ESP_LOGD(TAG, "Function: notify_event_handler (lambda).");
            if (mqtt_client.publish(MQTT_BLE_DEVICE_READ_TOPIC, args.data) != ESP_OK)
            {
                ESP_LOGE(TAG, "Client publish failed.");
                return;
            }
        };

        device.swap(connected_devices[device->get_id()]);
        dump_connected_devices();
        return;
    }

    void application::ble_device_disconnect(std::string_view id) noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        if (auto iter = connected_devices.find(id); iter != connected_devices.end())
        {
            ESP_LOGI(TAG, "Disconnecting with %s.", (iter->second)->get_device_name().data());
            (iter->second)->disconnect();
            connected_devices.erase(iter);
        }
        else if (auto iter = disconnected_devices.find(id); iter != disconnected_devices.end())
        {
            disconnected_devices.erase(iter);
        }

        dump_connected_devices();
    }

    void application::mqtt_data_callback(std::string_view topic, std::string_view data)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        static const utils::const_map<std::string_view, std::function<void(const utils::json&)>, 4> topic_callback_map{ {
            { MQTT_BLE_SCAN_ENABLE_TOPIC,   [this](const utils::json& data) { mqtt_scan_enable_topic_callback(data);  } },
            { MQTT_BLE_CONNECT_TOPIC,       [this](const utils::json& data) { mqtt_connect_topic_callback(data);      } },
            { MQTT_BLE_DISCONNECT_TOPIC,    [this](const utils::json& data) { mqtt_disconnect_topic_callback(data);   } },
            { MQTT_BLE_DEVICE_WRITE_TOPIC,  [this](const utils::json& data) { mqtt_device_write_topic_callback(data); } },
        } };

        if (auto iter = topic_callback_map.find(topic); iter != topic_callback_map.cend())
        {
            iter->second((utils::json::parse(data)));
        }
    }

    void application::mqtt_scan_enable_topic_callback(const utils::json& data)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        bool enable{ false };

        try 
        {
            enable = utils::json_cast<bool>(data["enable"]);
        } 
        catch (const std::invalid_argument& err) 
        {
            ESP_LOGE(TAG, "%s", err.what());
            return;
        }
        
        if (enable)
        {
            ESP_LOGI(TAG, "Enabling BLE scan...");
            ble_scan_start();
        }
        else
        {
            ESP_LOGI(TAG, "Disabling BLE scan...");
            ble_scan_stop();
        }
    }

    void application::mqtt_connect_topic_callback(const utils::json& data)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        using namespace std::literals;

        std::string_view name;
        std::string_view id;
        std::string_view address;

        try 
        {
            name    = utils::json_cast<std::string_view>(data["name"]);
            id      = utils::json_cast<std::string_view>(data["id"]);
            address = utils::json_cast<std::string_view>(data["address"]);
        }
        catch (const std::invalid_argument& err)
        {
            ESP_LOGE(TAG, "%s", err.what());
            return;
        }

        ble_device_connect(id, name, ble::mac(address));
    }

    void application::mqtt_disconnect_topic_callback(const utils::json& data)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        using namespace std::literals;

        std::string_view device_id;

        try
        {
            device_id = utils::json_cast<std::string_view>(data["id"]);
        }
        catch (const std::invalid_argument& err)
        {
            ESP_LOGE(TAG, "%s", err.what());
            return;
        }

        ble_device_disconnect(device_id);

        ESP_LOGI(TAG, "Disonnected with %s.", device_id.data());
    }

    void application::mqtt_device_write_topic_callback(const utils::json& data)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        std::string_view id;

        try 
        {
            id = utils::json_cast<std::string_view>(data["id"]);
        } 
        catch (const std::invalid_argument& err) 
        {
            ESP_LOGE(TAG, "%s", err.what());
        }

        if (auto iter = connected_devices.find(id); iter != connected_devices.end())
        {
            if (esp_err_t result = (iter->second)->send_data(data.dump()); result != ESP_OK)
            {
                ESP_LOGE(TAG, "Send data failed with error code %x [%s].", result, esp_err_to_name(result));
                return;
            }

            ESP_LOGI(TAG, "Data sent to device with ID: %s", id.data());
        }
        else
        {
            ESP_LOGW(TAG, "Device with ID: %s not found.", id.data());
        }
    }
}