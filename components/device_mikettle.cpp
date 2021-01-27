#include "device_mikettle.h"

#include <string>
#include <algorithm>

#include "esp_bt.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_defs.h"
#include "esp_gatt_common_api.h"

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace hub
{
    esp_err_t MiKettle::connect(esp_bd_addr_t address)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        esp_err_t result = ESP_OK;

        [[unlikely]] // Reconnecting to the same address is more likely than first-time setup
        if (!std::equal(address, address + sizeof(esp_bd_addr_t), this->address))
        {
            std::copy(address, address + sizeof(esp_bd_addr_t), this->address);
        }

        result = ble::client::connect(client_handle, address);
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Connect failed with error code %x [%s].", result, esp_err_to_name(result));
            return result;
        }

        result = authorize();
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

    esp_err_t MiKettle::authorize()
    {
        using namespace ble::client;

        ESP_LOGD(TAG, "Function: %s.", __func__);

        esp_err_t result = ESP_OK;
        uint16_t descr_count = 0;

        result = get_service(client_handle, &uuid_service_kettle);
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Find services failed with error code %x [%s].", result, esp_err_to_name(result));
            return result;
        }

        result = write_characteristic(client_handle, HANDLE_AUTH_INIT, key1.data(), key1.size());
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Write characteristic failed with error code %x [%s].", result, esp_err_to_name(result));
            return result;
        }

        {
            result = register_notify_callback(
                client_handle, 
                [this](const uint16_t char_handle, std::string_view data) {

                    esp_err_t result = ESP_OK;

                    {
                        std::array<uint8_t, token.size()> ciphered;

                        cipher(token.data(), token.data() + token.size(), key2.data(), ciphered.data());

                        result = write_characteristic(client_handle, HANDLE_AUTH, ciphered.data(), KEY_LENGTH);
                        if (result != ESP_OK)
                        {
                            ESP_LOGE(TAG, "Write characteristic failed with error code %x [%s].", result, esp_err_to_name(result));
                            return;
                        }
                    }

                    result = read_characteristic(client_handle, HANDLE_VERSION, NULL, NULL);
                    if (result != ESP_OK)
                    {
                        ESP_LOGE(TAG, "Read characteristic failed with error code %x [%s].", result, esp_err_to_name(result));
                        return;
                    }

                    result = unregister_for_notify(client_handle, HANDLE_AUTH);
                    if (result != ESP_OK)
                    {
                        ESP_LOGE(TAG, "Unregister for notify failed with error code %x [%s].", result, esp_err_to_name(result));
                        return;
                    }

                    result = register_for_notify(client_handle, HANDLE_STATUS);
                    if (result != ESP_OK)
                    {
                        ESP_LOGE(TAG, "Register for notify failed with error code %x [%s].", result, esp_err_to_name(result));
                        return;
                    }

                    result = write_descriptor(client_handle, HANDLE_STATUS + 1, subscribe, sizeof(subscribe));
                    if (result != ESP_OK)
                    {
                        ESP_LOGE(TAG, "Write descriptor failed with error code %x [%s].", result, esp_err_to_name(result));
                        return;
                    }

                    result = register_notify_callback(
                        client_handle, 
                        [this](const uint16_t char_handle, std::string_view data) {
                            notify_callback(char_handle, data);
                        });

                    if (result != ESP_OK)
                    {
                        ESP_LOGE(TAG, "Register notify callback failed.");
                        return;
                    }

                    result = register_disconnect_callback(
                        client_handle, 
                        [this]() { connect(address); });

                    if (result != ESP_OK)
                    {
                        ESP_LOGE(TAG, "Register notify callback failed.");
                        return;
                    }
                });
        }

        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Register notify callback failed.");
            return result;
        }

        result = register_for_notify(client_handle, HANDLE_AUTH);
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Register for notify failed with error code %x [%s].", result, esp_err_to_name(result));
            return result;
        }

        result = get_descriptors(client_handle, HANDLE_AUTH, NULL, &descr_count);
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Get descriptor count failed with error code %x [%s].", result, esp_err_to_name(result));
            return result;
        }

        {
            std::vector<esp_gattc_descr_elem_t> descr(descr_count * sizeof(esp_gattc_descr_elem_t));

            result = get_descriptors(client_handle, HANDLE_AUTH, descr.data(), &descr_count);
            if (result != ESP_GATT_OK)
            {
                ESP_LOGE(TAG, "Get descriptors failed.");
                return result;
            }

            ESP_LOGI(TAG, "Found %i descriptors.", descr_count);

            result = write_descriptor(client_handle, descr[0].handle, subscribe, sizeof(subscribe));
            if (result != ESP_OK)
            {
                ESP_LOGE(TAG, "Write descriptor failed with error code %x [%s].", result, esp_err_to_name(result));
                return result;
            }
        }

        {
            std::array<uint8_t, token.size()> ciphered;

            {
                esp_bd_addr_t reversed_mac;
                std::array<uint8_t, sizeof(uint16_t) + sizeof(esp_bd_addr_t)> mac_id_mix;

                std::reverse_copy((uint8_t*)address, (uint8_t*)address + sizeof(esp_bd_addr_t), (uint8_t*)reversed_mac);
                mix_a((uint8_t*)reversed_mac, PRODUCT_ID, mac_id_mix.data());
                cipher(mac_id_mix.data(), mac_id_mix.data() + mac_id_mix.size(), token.data(), ciphered.data());
            }

            result = write_characteristic(client_handle, HANDLE_AUTH, ciphered.data(), ciphered.size());
            if (result != ESP_OK)
            {
                ESP_LOGE(TAG, "Write characteristic failed with error code %x [%s].", result, esp_err_to_name(result));
                return result;
            }
        }

        return result;
    }

    void MiKettle::notify_callback(const uint16_t char_handle, std::string_view data)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
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