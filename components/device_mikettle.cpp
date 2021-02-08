#include "device_mikettle.h"
#include "hub_utils.h"

#include <string>
#include <algorithm>
#include <memory>

#include "esp_bt.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_defs.h"
#include "esp_gatt_common_api.h"

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C"
{
    #include "cJSON.h"
}

namespace hub
{
    esp_err_t MiKettle::connect(const ble::mac& address)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        esp_err_t result = ESP_OK;

        result = ble::client::connect(address);
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Connect failed with error code %x [%s].", result, esp_err_to_name(result));
            return result;
        }

        result = authorize(address);
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Authorize failed with error code %x [%s].", result, esp_err_to_name(result));
            return result;
        }

        return result;
    }

    esp_err_t MiKettle::update_data(std::string_view)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        return ESP_OK;
    }

    esp_err_t MiKettle::authorize(const ble::mac& address)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        esp_err_t result = ESP_OK;
        volatile bool auth_notify = false;

        result = ble::client::get_service(&uuid_service_kettle);
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Find services failed with error code %x [%s].", result, esp_err_to_name(result));
            return result;
        }

        result = ble::client::write_characteristic(HANDLE_AUTH_INIT, key1.data(), key1.size());
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Write characteristic failed with error code %x [%s].", result, esp_err_to_name(result));
            return result;
        }

        ble::client::notify_callback = 
            [&auth_notify](const uint16_t char_handle, std::string_view data) {
                ESP_LOGD(TAG, "Function: %s.", __func__);

                if (char_handle != HANDLE_AUTH)
                {
                    return;
                }

                auth_notify = true;
            };

        result = ble::client::register_for_notify(HANDLE_AUTH);
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Register for notify failed with error code %x [%s].", result, esp_err_to_name(result));
            return result;
        }

        {
            uint16_t descr_count = 0;
            std::vector<esp_gattc_descr_elem_t> descr{};

            result = ble::client::get_descriptors(HANDLE_AUTH, nullptr, &descr_count);
            if (result != ESP_OK)
            {
                ESP_LOGE(TAG, "Get descriptor count failed with error code %x [%s].", result, esp_err_to_name(result));
                return result;
            }

            descr.resize(descr_count * sizeof(esp_gattc_descr_elem_t));

            result = ble::client::get_descriptors(HANDLE_AUTH, descr.data(), &descr_count);
            if (result != ESP_GATT_OK)
            {
                ESP_LOGE(TAG, "Get descriptors failed.");
                return result;
            }

            ESP_LOGI(TAG, "Found %i descriptors.", descr_count);

            result = ble::client::write_descriptor(descr.at(0).handle, subscribe, sizeof(subscribe));
            if (result != ESP_OK)
            {
                ESP_LOGE(TAG, "Write descriptor failed with error code %x [%s].", result, esp_err_to_name(result));
                return result;
            }
        }

        {
            std::array<uint8_t, token.size()> ciphered;

            {
                std::array<uint8_t, 6> reversed_mac;
                std::array<uint8_t, 8> mac_id_mix;

                std::reverse_copy(address.begin(), address.end(), reversed_mac.begin());
                mix_a(reversed_mac.data(), PRODUCT_ID, mac_id_mix.data());
                cipher(mac_id_mix.data(), mac_id_mix.data() + mac_id_mix.size(), token.data(), ciphered.data());
            }

            result = ble::client::write_characteristic(HANDLE_AUTH, ciphered.data(), ciphered.size());
            if (result != ESP_OK)
            {
                ESP_LOGE(TAG, "Write characteristic failed with error code %x [%s].", result, esp_err_to_name(result));
                return result;
            }

            for (uint8_t i = 0; !auth_notify; i++)
            {
                vTaskDelay(50 / portTICK_PERIOD_MS);

                if (i == 60) // Timeout after 3s
                {
                    result = ESP_ERR_TIMEOUT;
                    return result;
                }
            }

            cipher(token.data(), token.data() + token.size(), key2.data(), ciphered.data());

            result = ble::client::write_characteristic(HANDLE_AUTH, ciphered.data(), KEY_LENGTH);
            if (result != ESP_OK)
            {
                ESP_LOGE(TAG, "Write characteristic failed with error code %x [%s].", result, esp_err_to_name(result));
                return result;
            }
        }

        result = ble::client::read_characteristic(HANDLE_VERSION, NULL, NULL);
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Read characteristic failed with error code %x [%s].", result, esp_err_to_name(result));
            return result;
        }

        result = ble::client::unregister_for_notify(HANDLE_AUTH);
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Unregister for notify failed with error code %x [%s].", result, esp_err_to_name(result));
            return result;
        }

        result = ble::client::register_for_notify(HANDLE_STATUS);
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Register for notify failed with error code %x [%s].", result, esp_err_to_name(result));
            return result;
        }

        result = ble::client::write_descriptor(HANDLE_STATUS + 1, subscribe, sizeof(subscribe));
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Write descriptor failed with error code %x [%s].", result, esp_err_to_name(result));
            return result;
        }

        ble::client::notify_callback = [this](const uint16_t char_handle, std::string_view data) {
            ble_notify_callback(char_handle, data);
        };

        return result;
    }

    void MiKettle::ble_notify_callback(const uint16_t char_handle, std::string_view data)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        std::string_view result;

        if (std::equal(last_notify.data_array, last_notify.data_array + sizeof(data_model), data.begin()))
        {
            return;
        }

        std::copy(data.begin(), data.end(), last_notify.data_array);

        {
            std::unique_ptr<cJSON, std::function<void(cJSON*)>> json_data{ 
                cJSON_CreateObject(),
                [](cJSON* ptr) {
                    cJSON_Delete(ptr);
                }
            };

            cJSON_AddNumberToObject(json_data.get(), "action", last_notify.data_struct.action);
            cJSON_AddNumberToObject(json_data.get(), "mode", last_notify.data_struct.mode);
            cJSON_AddNumberToObject(json_data.get(), "temperature_set", last_notify.data_struct.temperature_set);
            cJSON_AddNumberToObject(json_data.get(), "temperature_current", last_notify.data_struct.temperature_current);
            cJSON_AddNumberToObject(json_data.get(), "keep_warm_type", last_notify.data_struct.keep_warm_type);
            cJSON_AddNumberToObject(json_data.get(), "keep_warm_time", last_notify.data_struct.keep_warm_time);

            result = cJSON_PrintUnformatted(json_data.get());
        }

        if (!notify_callback)
        {
            return;
        }

        // Pass data to app
        notify_callback(result);
    }

    void MiKettle::cipher_init(const uint8_t* const in_first1, const uint8_t* const in_last1, uint8_t* const out_first)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        const uint16_t key_size = std::distance(in_first1, in_last1);
        uint16_t j = 0;

        for (uint16_t i = 0; i < PERM_LENGTH; i++)
        {
            out_first[i] = i;
        }

        for (uint16_t i = 0; i < PERM_LENGTH; i++)
        {
            j += out_first[i] + in_first1[i % key_size];
            j &= 0xff;
            std::swap(out_first[i], out_first[j]);
        }
    }

    void MiKettle::cipher_crypt(const uint8_t* const in_first1, uint8_t* const in_first2, uint8_t* const out_first)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        uint16_t index1 = 0;
        uint16_t index2 = 0;
        uint16_t index3 = 0;

        for (uint16_t i = 0; i < TOKEN_LENGTH; i++)
        {
            index1++;
            index1 &= 0xff;
            index2 += in_first2[index1];
            index2 &= 0xff;
            std::swap(in_first2[index1], in_first2[index2]);
            index3 = in_first2[index1] + in_first2[index2];
            index3 &= 0xff;
            out_first[i] = in_first1[i] ^ in_first2[index3];
        }
    }

    void MiKettle::cipher(const uint8_t* const in_first1, const uint8_t* const in_last1, const uint8_t* const in_first2, uint8_t* const out_first)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        std::vector<uint8_t> perm(PERM_LENGTH);

        cipher_init(in_first1, in_last1, perm.data());
        cipher_crypt(in_first2, perm.data(), out_first);
    }

    void MiKettle::mix_a(const uint8_t* in_first, const uint16_t product_id, uint8_t* out_first)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        out_first[0] = in_first[0];
        out_first[1] = in_first[2];
        out_first[2] = in_first[5];
        out_first[3] = product_id & 0xff;
        out_first[4] = product_id & 0xff;
        out_first[5] = in_first[4];
        out_first[6] = in_first[5];
        out_first[7] = in_first[1];
    }

    void MiKettle::mix_b(const uint8_t* in_first, const uint16_t product_id, uint8_t* out_first)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        out_first[0] = in_first[0];
        out_first[1] = in_first[2];
        out_first[2] = in_first[5];
        out_first[3] = (product_id >> 8) & 0xff;
        out_first[4] = in_first[4];
        out_first[5] = in_first[0];
        out_first[6] = in_first[5];
        out_first[7] = product_id & 0xff;
    }
}