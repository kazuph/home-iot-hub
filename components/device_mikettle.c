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

#define KEY_LENGTH      4
#define TOKEN_LENGTH    12
#define PERM_LENGTH     256

const char* TAG = "MIKETTLE";

static void auth_notify_cb(hub_ble_client* ble_client, struct gattc_notify_evt_param* param);
static void cipher_init(uint8_t* key_first, const uint8_t* key_last, uint8_t* out_first, const uint8_t* out_last);
static void cipher_crypt(uint8_t* in_first, const uint8_t* in_last, uint8_t* perm_first, uint8_t* out_first);
static void cipher(uint8_t* key_first, const uint8_t* key_last, uint8_t* in_first, const uint8_t* in_last, uint8_t* out_first);
static void reverse_mac(uint8_t* mac_first, const uint8_t* mac_last, uint8_t* out_first);
static void mix_a(const uint8_t* mac_first, const uint16_t product_id, uint8_t* out_first);
static void mix_b(const uint8_t* mac_first, const uint16_t product_id, uint8_t* out_first);
static void swap(uint8_t* first, uint8_t* second);

static const uint8_t key1[KEY_LENGTH]       = { 0x90, 0xCA, 0x85, 0xDE };
static const uint8_t key2[KEY_LENGTH]       = { 0x92, 0xAB, 0x54, 0xFA };
static const uint8_t token[TOKEN_LENGTH]    = { 0x91, 0xf5, 0x80, 0x92, 0x24, 0x49, 0xb4, 0x0d, 0x6b, 0x06, 0xd2, 0x8a };
static const uint8_t subscribe[]            = { 0x01, 0x00 };

static const esp_bt_uuid_t uuid_service_kettle = {
    .len = ESP_UUID_LEN_16,
    .uuid = {
        .uuid16 = MIKETTLE_GATT_UUID_KETTLE_SRV,
    },
};

static bool auth_notify;

esp_err_t mikettle_authorize(hub_ble_client* ble_client, notify_callback_t callback)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

    esp_err_t result = ESP_OK;
    uint16_t descr_count = 0;
    esp_gattc_descr_elem_t* descr = NULL;
    esp_bd_addr_t* reversed_mac = NULL;
    uint8_t* mac_id_mix = NULL;
    uint8_t* ciphered = NULL;

    auth_notify = false;

    result = hub_ble_client_connect(ble_client);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not connect to MiKettle.");
        return result;
    }

    result = hub_ble_client_search_service(ble_client, &uuid_service_kettle);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Could find services.");
        return result;
    }

    result = hub_ble_client_write_characteristic(ble_client, MIKETTLE_HANDLE_AUTH_INIT, key1, KEY_LENGTH);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not write characteristics.");
        return result;
    }

    result = hub_ble_client_register_for_notify(ble_client, MIKETTLE_HANDLE_AUTH, &auth_notify_cb);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Register for notify failed.");
        return result;
    }

    result = hub_ble_client_get_descriptors(ble_client, MIKETTLE_HANDLE_AUTH, NULL, &descr_count);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Get descriptor count failed.");
        return result;
    }

    descr = (esp_gattc_descr_elem_t*)malloc(descr_count * sizeof(esp_gattc_descr_elem_t));
    if (descr == NULL)
    {
        ESP_LOGE(TAG, "Could not allocate memory.");
        return ESP_FAIL;
    }

    if (hub_ble_client_get_descriptors(ble_client, MIKETTLE_HANDLE_AUTH, descr, &descr_count) != ESP_GATT_OK)
    {
        ESP_LOGE(TAG, "Get descriptors failed.");
        goto cleanup;
    }

    ESP_LOGI(TAG, "Found %i descriptors.", descr_count);

    result = hub_ble_client_write_descriptor(ble_client, descr[0].handle, subscribe, sizeof(subscribe));
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Write descriptor failed.");
        goto cleanup;
    }

    ESP_LOGI(TAG, "Written to descriptor handle: %i.", descr[0].handle);

    reversed_mac = (esp_bd_addr_t*)malloc(sizeof(esp_bd_addr_t));
    if (reversed_mac == NULL)
    {
        ESP_LOGE(TAG, "Could not allocate memory.");
        goto cleanup;
    }

    mac_id_mix = (uint8_t*)malloc(sizeof(uint16_t) + sizeof(esp_bd_addr_t));
    if (mac_id_mix == NULL)
    {
        ESP_LOGE(TAG, "Could not allocate memory.");
        goto cleanup;
    }

    ciphered = (uint8_t*)malloc(sizeof(token));
    if (ciphered == NULL)
    {
        ESP_LOGE(TAG, "Could not allocate memory.");
        goto cleanup;
    }

    reverse_mac((uint8_t*)ble_client->remote_bda, (uint8_t*)ble_client->remote_bda + sizeof(esp_bd_addr_t), reversed_mac);
    mix_a(reversed_mac, MIKETTLE_PRODUCT_ID, mac_id_mix);
    cipher(mac_id_mix, mac_id_mix + sizeof(uint16_t) + sizeof(esp_bd_addr_t), token, token + TOKEN_LENGTH, ciphered);

    result = hub_ble_client_write_characteristic(ble_client, MIKETTLE_HANDLE_AUTH, ciphered, TOKEN_LENGTH);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not write characteristics.");
        goto cleanup;
    }

    while (!auth_notify)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    cipher(token, token + TOKEN_LENGTH, key2, key2 + KEY_LENGTH, ciphered);

    result = hub_ble_client_write_characteristic(ble_client, MIKETTLE_HANDLE_AUTH, ciphered, KEY_LENGTH);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not write characteristics.");
        goto cleanup;
    }

    result = hub_ble_client_read_characteristic(ble_client, MIKETTLE_HANDLE_VERSION);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not read characteristics.");
        goto cleanup;
    }

    result = hub_ble_client_unregister_for_notify(ble_client, MIKETTLE_HANDLE_AUTH);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Unregister for notify failed.");
        goto cleanup;
    }

    result = hub_ble_client_register_for_notify(ble_client, MIKETTLE_HANDLE_STATUS, callback);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Register for notify failed.");
        goto cleanup;
    }

    result = hub_ble_client_write_descriptor(ble_client, MIKETTLE_HANDLE_STATUS + 1, subscribe, sizeof(subscribe));
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Write descriptor failed.");
        goto cleanup;
    }

cleanup:

    if (descr != NULL)
    {
        free(descr);
        descr = NULL;
    }

    if (reversed_mac != NULL)
    {
        free(descr);
        reversed_mac = NULL;
    }

    if (mac_id_mix != NULL)
    {
        free(descr);
        mac_id_mix = NULL;
    }

    if (ciphered != NULL)
    {
        free(descr);
        ciphered = NULL;
    }

    return result;
}

static void auth_notify_cb(hub_ble_client* ble_client, struct gattc_notify_evt_param* param)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

    auth_notify = true;
}

static void cipher_init(uint8_t* key_first, const uint8_t* key_last, uint8_t* out_first, const uint8_t* out_last)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

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
    ESP_LOGD(TAG, "Function: %s.", __func__);

    uint16_t index1 = 0;
    uint16_t index2 = 0;
    uint16_t index3 = 0;

    for (uint16_t i = 0; i < TOKEN_LENGTH; i++)
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
    ESP_LOGD(TAG, "Function: %s.", __func__);

    uint8_t* perm_first = NULL;

    perm_first = (uint8_t*)malloc(PERM_LENGTH);
    if (perm_first == NULL)
    {
        return;
    }

    cipher_init(key_first, key_last, perm_first, perm_first + PERM_LENGTH);
    cipher_crypt(in_first, in_last, perm_first, out_first);

    free(perm_first);
}

static void reverse_mac(uint8_t* mac_first, const uint8_t* mac_last, uint8_t* out_first)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

    while (mac_last >= mac_first)
    {
        mac_last--;
        *out_first = *mac_last;
        out_first++;
    }
}

static void mix_a(const uint8_t* mac_first, const uint16_t product_id, uint8_t* out_first)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

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
    ESP_LOGD(TAG, "Function: %s.", __func__);

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
    ESP_LOGD(TAG, "Function: %s.", __func__);
    
    uint8_t tmp = *second;
    *second = *first;
    *first = tmp;
}