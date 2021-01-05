#include "device_mikettle.h"

#include <string.h>

#include "esp_bt.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_defs.h"
#include "esp_gatt_common_api.h"

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define KEY_LENGTH 4
#define TOKEN_LENGTH 12

const char* TAG = "MIKETTLE";

static void auth_notify_cb(hub_ble_client* ble_client, struct gattc_notify_evt_param* param);
static void cipher_init(uint8_t* key_first, const uint8_t* key_last, uint8_t* out_first, const uint8_t* out_last);
static void cipher_crypt(uint8_t* in_first, const uint8_t* in_last, uint8_t* perm_first, uint8_t* out_first);
static void cipher(uint8_t* key_first, const uint8_t* key_last, uint8_t* in_first, const uint8_t* in_last, uint8_t* out_first);
static void reverse_mac(uint8_t* mac_first, const uint8_t* mac_last, uint8_t* out_first);
static void mix_a(const uint8_t* mac_first, const uint16_t product_id, uint8_t* out_first);
static void mix_b(const uint8_t* mac_first, const uint16_t product_id, uint8_t* out_first);
static void swap(uint8_t* first, uint8_t* second);

static const uint8_t key1[] = { 0x90, 0xCA, 0x85, 0xDE };
static const uint8_t key2[] = { 0x92, 0xAB, 0x54, 0xFA };
static const uint8_t token[] = { 0x91, 0xf5, 0x80, 0x92, 0x24, 0x49, 0xb4, 0x0d, 0x6b, 0x06, 0xd2, 0x8a };
static const uint8_t subscribe[] = { 0x01, 0x00 };

static const esp_bt_uuid_t uuid_service_kettle = {
    .len = ESP_UUID_LEN_16,
    .uuid = {
        .uuid16 = MIKETTLE_GATT_UUID_KETTLE_SRV,
    },
};

static bool auth_notify;

esp_err_t mikettle_authorize(hub_ble_client* ble_client, notify_callback_t callback)
{
    uint16_t descr_count = 0;
    esp_gattc_descr_elem_t* descr = NULL;

    auth_notify = false;

    if (hub_ble_client_connect(ble_client) != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not connect to MiKettle.");
        return ESP_FAIL;
    }

    if (hub_ble_client_search_service(ble_client, &uuid_service_kettle) != ESP_OK)
    {
        ESP_LOGE(TAG, "Could find services.");
        return ESP_FAIL;
    }

    if (hub_ble_client_write_characteristic(ble_client, MIKETTLE_HANDLE_AUTH_INIT, key1, sizeof(key1)) != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not write characteristics.");
        return ESP_FAIL;
    }

    if (hub_ble_client_register_for_notify(ble_client, MIKETTLE_HANDLE_AUTH, &auth_notify_cb) != ESP_OK)
    {
        ESP_LOGE(TAG, "Register for notify failed.");
        return ESP_FAIL;
    }

    if (hub_ble_client_get_descriptors(ble_client, MIKETTLE_HANDLE_AUTH, NULL, &descr_count) != ESP_OK)
    {
        ESP_LOGE(TAG, "Get descriptor count failed.");
        return ESP_FAIL;
    }

    descr = (esp_gattc_descr_elem_t*)malloc(descr_count * sizeof(esp_gattc_descr_elem_t));

    if (hub_ble_client_get_descriptors(ble_client, MIKETTLE_HANDLE_AUTH, descr, &descr_count) != ESP_GATT_OK)
    {
        ESP_LOGE(TAG, "Get descriptors failed.");
        free(descr);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Found %i descriptors.", descr_count);

    if (hub_ble_client_write_descriptor(ble_client, descr[0].handle, subscribe, sizeof(subscribe)) != ESP_OK)
    {
        ESP_LOGE(TAG, "Write descriptor failed.");
        free(descr);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Written to descriptor handle: %i.", descr[0].handle);

    free(descr);

    esp_bd_addr_t* reversed_mac = (esp_bd_addr_t*)malloc(sizeof(esp_bd_addr_t));
    uint8_t* mac_id_mix = (uint8_t*)malloc(sizeof(uint16_t) + sizeof(esp_bd_addr_t));
    uint8_t* ciphered = (uint8_t*)malloc(sizeof(token));

    reverse_mac((uint8_t*)ble_client->remote_bda, (uint8_t*)ble_client->remote_bda + sizeof(esp_bd_addr_t), reversed_mac);
    mix_a(reversed_mac, MIKETTLE_PRODUCT_ID, mac_id_mix);
    cipher(mac_id_mix, mac_id_mix + sizeof(uint16_t) + sizeof(esp_bd_addr_t), token, token + sizeof(token), ciphered);

    if (hub_ble_client_write_characteristic(ble_client, MIKETTLE_HANDLE_AUTH, ciphered, sizeof(token)) != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not write characteristics.");
    }

    free(mac_id_mix);
    free(reversed_mac);

    while (!auth_notify)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    cipher(token, token + sizeof(token), key2, key2 + sizeof(key2), ciphered);

    if (hub_ble_client_write_characteristic(ble_client, MIKETTLE_HANDLE_AUTH, ciphered, sizeof(key2)) != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not write characteristics.");
        free(ciphered);
        return ESP_FAIL;
    }

    free(ciphered);

    if (hub_ble_client_read_characteristic(ble_client, MIKETTLE_HANDLE_VERSION) != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not read characteristics.");
        return ESP_FAIL;
    }

    if (hub_ble_client_unregister_for_notify(ble_client, MIKETTLE_HANDLE_AUTH) != ESP_OK)
    {
        ESP_LOGE(TAG, "Unregister for notify failed.");
        return ESP_FAIL;
    }

    if (hub_ble_client_register_for_notify(ble_client, MIKETTLE_HANDLE_STATUS, callback) != ESP_OK)
    {
        ESP_LOGE(TAG, "Register for notify failed.");
        return ESP_FAIL;
    }

    if (hub_ble_client_write_descriptor(ble_client, MIKETTLE_HANDLE_STATUS + 1, subscribe, sizeof(subscribe)) != ESP_OK)
    {
        ESP_LOGE(TAG, "Write descriptor failed.");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static void auth_notify_cb(hub_ble_client* ble_client, struct gattc_notify_evt_param* param)
{
    auth_notify = true;
}

static void cipher_init(uint8_t* key_first, const uint8_t* key_last, uint8_t* out_first, const uint8_t* out_last)
{
    const uint16_t out_size = out_last - out_first;
    const uint16_t key_size = key_last - key_first;
    uint16_t j = 0;

    for (uint16_t i = 0; i < out_size; i++)
    {
        out_first[i] = i;
    }

    for (uint16_t i = 0; i < out_size; i++)
    {
        j += out_first[i] + key_first[i % key_size];
        j &= 0xff;
        swap(&out_first[i], &out_first[j]);
    }
}

static void cipher_crypt(uint8_t* in_first, const uint8_t* in_last, uint8_t* perm_first, uint8_t* out_first)
{
    uint16_t index1 = 0;
    uint16_t index2 = 0;
    uint16_t index3 = 0;

    for (uint16_t i = 0; i < sizeof(token); i++)
    {
        index1++;
        index1 &= 0xff;
        index2 += perm_first[index1];
        index2 &= 0xff;
        swap(&perm_first[index1], &perm_first[index2]);
        index3 = perm_first[index1] + perm_first[index2];
        index3 &= 0xff;
        out_first[i] = in_first[i] ^ perm_first[index3];
    }
}

static void cipher(uint8_t* key_first, const uint8_t* key_last, uint8_t* in_first, const uint8_t* in_last, uint8_t* out_first)
{
    static const uint16_t perm_length = 256;
    uint8_t* perm_first = (uint8_t*)malloc(perm_length);

    cipher_init(key_first, key_last, perm_first, perm_first + perm_length);
    cipher_crypt(in_first, in_last, perm_first, out_first);

    free(perm_first);
}

static void reverse_mac(uint8_t* mac_first, const uint8_t* mac_last, uint8_t* out_first)
{
    while (mac_last >= mac_first)
    {
        mac_last--;
        *out_first = *mac_last;
        out_first++;
    }
}

static void mix_a(const uint8_t* mac_first, const uint16_t product_id, uint8_t* out_first)
{
    out_first[0] = mac_first[0];
    out_first[1] = mac_first[2];
    out_first[2] = mac_first[5];
    out_first[3] = product_id & 0xff;
    out_first[4] = product_id & 0xff;
    out_first[5] = mac_first[4];
    out_first[6] = mac_first[5];
    out_first[7] = mac_first[1];
}

static void mix_b(const uint8_t* mac_first, const uint16_t product_id, uint8_t* out_first)
{
    out_first[0] = mac_first[0];
    out_first[1] = mac_first[2];
    out_first[2] = mac_first[5];
    out_first[3] = (product_id >> 8) & 0xff;
    out_first[4] = mac_first[4];
    out_first[5] = mac_first[0];
    out_first[6] = mac_first[5];
    out_first[7] = product_id & 0xff;
}

static void swap(uint8_t* first, uint8_t* second)
{
    uint8_t tmp = *second;
    *second = *first;
    *first = tmp;
}