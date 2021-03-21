#include "hub_device_manager.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"

#include <exception>
#include <stdexcept>
#include <fstream>

#include "hub_ble.h"
#include "hub_filesystem.h"
#include "hub_device_factory.h"
#include "hub_const_map.h"

namespace hub
{
    device_manager::device_manager() :
        mqtt_client             {  },
        scan_results            {  },
        connected_devices       {  },
        disconnected_devices    {  }
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
    }

    esp_err_t device_manager::mqtt_start(std::string_view mqtt_uri, const uint16_t mqtt_port)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        esp_err_t result{ ESP_OK };

        try 
        {
            mqtt_client = mqtt::client(mqtt_uri, mqtt_port);
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
            throw std::runtime_error("MQTT client start failed.");
        }

        mqtt_client.subscribe(MQTT_BLE_SCAN_ENABLE_TOPIC);
        mqtt_client.subscribe(MQTT_BLE_CONNECT_TOPIC);
        mqtt_client.subscribe(MQTT_BLE_DISCONNECT_TOPIC);
        mqtt_client.subscribe(MQTT_BLE_DEVICE_WRITE_TOPIC);

        mqtt_client.data_event_handler += [this](mqtt::data_event_args args) {
            mqtt_data_callback(args.topic, args.data);
        };

        return result;
    }

    esp_err_t device_manager::mqtt_stop()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        return mqtt_client.stop();
    }

    void device_manager::ble_scan_start(uint16_t ble_scan_time)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        scan_results.clear();
        ble::start_scanning(ble_scan_time);
    }

    void device_manager::ble_scan_stop()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        ble::stop_scanning();
        scan_results.clear();
    }

    void device_manager::dump_connected_devices()
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

    void device_manager::load_connected_devices()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        std::string devices_dumped;
        utils::json json_data{ utils::json::json_array() };

        {
            std::ifstream ifs;
            ifs.open("/spiffs/devices.json");

            if (!ifs)
            {
                ESP_LOGE(TAG, "Could not open file.");
                return;
            }

            ifs >> devices_dumped;
        }

        json_data = utils::json::parse(devices_dumped);

        for (int i = 0; i < json_data.size(); i++)
        {
            std::string_view id;
            std::string_view name;
            std::string_view address;

            try
            {
                id = utils::json_cast<std::string_view>(json_data["id"]);
                name = utils::json_cast<std::string_view>(json_data["name"]);
                address = utils::json_cast<std::string_view>(json_data["address"]);
            }
            catch (const std::invalid_argument& err)
            {
                ESP_LOGE(TAG, "%s", err.what());
                continue;
            }

            ble_device_connect(id, name, ble::mac(address));
        }
    }

    device_manager::~device_manager()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
    }

    void device_manager::ble_scan_callback(std::string_view name, const ble::mac& address)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        // Return if already in scan_results.
        if (scan_results.find(address) != scan_results.end())
        {
            return;
        }

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

        scan_results[address] = device_factory::view_on_name(name);

        {
            utils::json scan_results_json{ utils::json::json_array() };

            std::for_each(scan_results.begin(), scan_results.end(), [&scan_results_json](const auto& elem) {

                const auto& [device_address, device_name] = elem;

                scan_results_json.push_back({ {
                    { { "name",       device_name                 } },
                    { { "address",    device_address.to_string()  } }
                } });
            });

            if (mqtt_client.publish(MQTT_BLE_SCAN_RESULTS_TOPIC, scan_results_json.dump(), true) != ESP_OK)
            {
                ESP_LOGE(TAG, "Client publish failed.");
                return;
            }
        }
    }

    void device_manager::ble_device_connect(std::string_view id, std::string_view name, const ble::mac& address)
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

        /* 
            Publish on device topic when new data arrives
        */
        device->notify_callback = [this](std::string_view data) {
            if (mqtt_client.publish(MQTT_BLE_DEVICE_READ_TOPIC, data) != ESP_OK)
            {
                ESP_LOGE(TAG, "Client publish failed.");
                return;
            }
        };

        device->disconnect_callback = [this, id{ device->get_id() }]() {
            connected_devices[id].swap(disconnected_devices[id]);
            ble_scan_start(60 /* seconds */);
        };

        device.swap(connected_devices[device->get_id()]);
        dump_connected_devices();
        return;
    }

    void device_manager::ble_device_disconnect(std::string_view id) noexcept
    {
        connected_devices.erase(id);
        disconnected_devices.erase(id);
    }

    void device_manager::mqtt_data_callback(std::string_view topic, std::string_view data)
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

    void device_manager::mqtt_scan_enable_topic_callback(const utils::json& data)
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

    void device_manager::mqtt_connect_topic_callback(const utils::json& data)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        using namespace std::literals;

        std::string_view name;
        std::string_view id;
        std::string_view address;

        try 
        {
            name = utils::json_cast<std::string_view>(data["name"]);
            id = utils::json_cast<std::string_view>(data["id"]);
            address = utils::json_cast<std::string_view>(data["address"]);
        }
        catch (const std::invalid_argument& err)
        {
            ESP_LOGE(TAG, "%s", err.what());
            return;
        }

        ble_device_connect(id, name, ble::mac(address));
    }

    void device_manager::mqtt_disconnect_topic_callback(const utils::json& data)
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

    void device_manager::mqtt_device_write_topic_callback(const utils::json& data)
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