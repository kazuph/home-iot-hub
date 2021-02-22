#include "device_mikettle.h"

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
    MiKettle::MiKettle(std::string_view id) : 
        device_base             { id },
        last_notify             {},
        keep_warm_type          { 0 },
        keep_warm_temperature   { 0 },
        keep_warm_time_limit    { 0 },
        turn_off_after_boil     { 0 },
        auth_notify             { false }
    {

    }

    esp_err_t MiKettle::connect(const ble::mac& address)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        esp_err_t result{ ESP_OK };

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

        // Read setup data
        {
            uint16_t char_count{ 1 };
            result = ble::client::read_characteristic(HANDLE_TIME_LIMIT, &keep_warm_time_limit, &char_count);
            if (result != ESP_OK)
            {
                ESP_LOGE(TAG, "Read time limit characteristic failed with error code %x [%s].", result, esp_err_to_name(result));
                return result;
            }

            result = ble::client::read_characteristic(HANDLE_BOIL_MODE, &turn_off_after_boil, &char_count);
            if (result != ESP_OK)
            {
                ESP_LOGE(TAG, "Read boil mode characteristic failed with error code %x [%s].", result, esp_err_to_name(result));
                return result;
            }
        }

        return result;
    }

    esp_err_t MiKettle::disconnect()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        return ESP_OK;
    }

    esp_err_t MiKettle::send_data(std::string_view data)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        std::unique_ptr<cJSON, std::function<void(cJSON*)>> json_data{ 
            cJSON_ParseWithLength(data.data(), data.length()),
            [](cJSON* ptr) {
                cJSON_Delete(ptr);
            }
        };


        if (json_data == nullptr)
        {
            auto error_ptr{ cJSON_GetErrorPtr() };
            if (error_ptr != nullptr)
            {
                ESP_LOGE(TAG, "JSON parse error: %s.", error_ptr);
                return ESP_FAIL;
            }
        }

        {
            esp_err_t result = ESP_OK;

            auto obj{ cJSON_GetObjectItemCaseSensitive(json_data.get(), "keep_warm_type") };
            if (obj && cJSON_IsNumber(obj))
            {
                keep_warm_type = obj->valueint;

                uint8_t setup_characteristic[2] = { keep_warm_type, keep_warm_temperature };
                result = ble::client::write_characteristic(HANDLE_KEEP_WARM, setup_characteristic, sizeof(setup_characteristic));
                if (result != ESP_OK)
                {
                    ESP_LOGE(TAG, "Write setup characteristic failed with error code %x [%s].", result, esp_err_to_name(result));
                    return result;
                }
            }

            obj = cJSON_GetObjectItemCaseSensitive(json_data.get(), "keep_warm_temperature");
            if (obj && cJSON_IsNumber(obj))
            {
                keep_warm_temperature = obj->valueint;

                uint8_t setup_characteristic[2] = { keep_warm_type, keep_warm_temperature };
                result = ble::client::write_characteristic(HANDLE_KEEP_WARM, setup_characteristic, sizeof(setup_characteristic));
                if (result != ESP_OK)
                {
                    ESP_LOGE(TAG, "Write setup characteristic failed with error code %x [%s].", result, esp_err_to_name(result));
                    return result;
                }
            }

            obj = cJSON_GetObjectItemCaseSensitive(json_data.get(), "keep_warm_time_limit");
            if (obj && cJSON_IsNumber(obj))
            {
                keep_warm_time_limit = static_cast<uint8_t>(obj->valuedouble) * 2;

                result = ble::client::write_characteristic(HANDLE_TIME_LIMIT, &keep_warm_temperature, sizeof(keep_warm_temperature));
                if (result != ESP_OK)
                {
                    ESP_LOGE(TAG, "Write time limit characteristic failed with error code %x [%s].", result, esp_err_to_name(result));
                    return result;
                }
            }

            obj = cJSON_GetObjectItemCaseSensitive(json_data.get(), "turn_off_after_boil");
            if (obj && cJSON_IsNumber(obj))
            {
                turn_off_after_boil = obj->valueint;

                result = ble::client::write_characteristic(HANDLE_BOIL_MODE, &turn_off_after_boil, sizeof(turn_off_after_boil));
                if (result != ESP_OK)
                {
                    ESP_LOGE(TAG, "Write time limit characteristic failed with error code %x [%s].", result, esp_err_to_name(result));
                    return result;
                }
            }
        }

        return ESP_OK;
    }

    void MiKettle::data_received(const uint16_t char_handle, std::string_view data)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        if (char_handle == HANDLE_STATUS)
        {
            std::string_view result;

            if (std::equal(last_notify.data_array, last_notify.data_array + sizeof(status), data.begin()))
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

                cJSON_AddStringToObject(json_data.get(), "id", get_id().data());
                cJSON_AddNumberToObject(json_data.get(), "action", last_notify.data_struct.action);
                cJSON_AddNumberToObject(json_data.get(), "mode", last_notify.data_struct.mode);
                cJSON_AddNumberToObject(json_data.get(), "temperature_set", last_notify.data_struct.temperature_set);
                cJSON_AddNumberToObject(json_data.get(), "temperature_current", last_notify.data_struct.temperature_current);
                cJSON_AddNumberToObject(json_data.get(), "keep_warm_type", last_notify.data_struct.keep_warm_type);
                cJSON_AddNumberToObject(json_data.get(), "keep_warm_time", last_notify.data_struct.keep_warm_time);
                cJSON_AddNumberToObject(json_data.get(), "keep_warm_time_limit", (static_cast<float>(keep_warm_time_limit) / 2.0f));
                cJSON_AddBoolToObject(json_data.get(), "turn_off_after_boil", (static_cast<bool>(turn_off_after_boil)) ? cJSON_True : cJSON_False);

                result = cJSON_PrintUnformatted(json_data.get());
            }

            if (!notify_callback)
            {
                return;
            }

            // Pass data to app
            notify_callback(result);
        }
        else if (char_handle == HANDLE_AUTH)
        {
            auth_notify = true;
        }
    }

    void MiKettle::device_disconnected()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        if (!disconnect_callback)
        {
            return;
        }

        // Let the app handle reconnection
        disconnect_callback();
    }

    esp_err_t MiKettle::authorize(const ble::mac& address)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        esp_err_t result{ ESP_OK };
        auth_notify = false;

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

        result = ble::client::register_for_notify(HANDLE_AUTH);
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Register for notify failed with error code %x [%s].", result, esp_err_to_name(result));
            return result;
        }

        {
            uint16_t descr_count{ 0 };
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
            std::array<uint8_t, token.size()> ciphered{};

            {
                std::array<uint8_t, 6> reversed_mac{};
                std::array<uint8_t, 8> mac_id_mix{};

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

            result = wait_for_authorization();
            if (result != ESP_OK)
            {
                ESP_LOGE(TAG, "Wait for authorization failed with error code %x [%s].", result, esp_err_to_name(result));
                return result;
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

        return result;
    }

    esp_err_t MiKettle::wait_for_authorization()
    {
        static constexpr TickType_t TIMEOUT_MS    { 2000 };
        static constexpr TickType_t SLEEP_MS      { 50 };

        for (TickType_t i{ 0 }; i < (TIMEOUT_MS / SLEEP_MS); i++)
        {
            vTaskDelay(SLEEP_MS / portTICK_PERIOD_MS);

            if (auth_notify == true)
            {
                return ESP_OK;
            }
        }

        return ESP_ERR_TIMEOUT;
    }

    void MiKettle::cipher_init(const uint8_t* const in_first1, const uint8_t* const in_last1, uint8_t* const out_first)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        const uint16_t key_size{ static_cast<uint16_t>(std::distance(in_first1, in_last1)) };
        uint16_t j{ 0 };

        for (uint16_t i{ 0 }; i < PERM_LENGTH; i++)
        {
            out_first[i] = i;
        }

        for (uint16_t i{ 0 }; i < PERM_LENGTH; i++)
        {
            j += out_first[i] + in_first1[i % key_size];
            j &= 0xff;
            std::swap(out_first[i], out_first[j]);
        }
    }

    void MiKettle::cipher_crypt(const uint8_t* const in_first1, uint8_t* const in_first2, uint8_t* const out_first)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        uint16_t index1{ 0 };
        uint16_t index2{ 0 };
        uint16_t index3{ 0 };

        for (uint16_t i{ 0 }; i < TOKEN_LENGTH; i++)
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