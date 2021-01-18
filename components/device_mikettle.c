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

#define MIKETTLE_MQTT_TOPIC                 "mikettle"
#define MIKETTLE_JSON_FORMAT                "{\"temperature\":{\"current\":%i,\"set\":%i},\"action\":%i,\"mode\":%i,\"keep_warm\":{\"type\":%i,\"time\":%i,\"time_limit\":%i},\"boil_mode\":%i}"
#define MIKETTLE_PRODUCT_ID                 275

/* MiKettle services */
#define MIKETTLE_GATT_UUID_KETTLE_SRV       0xfe95
#define MIKETTLE_GATT_UUID_KETTLE_DATA_SRV  { 0x56, 0x61, 0x23, 0x37, 0x28, 0x26, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x36, 0x47, 0x34, 0x01 }

/* MiKettle characteristics */
#define MIKETTLE_GATT_UUID_AUTH_INIT        0x0010
#define MIKETTLE_GATT_UUID_AUTH             0x0001
#define MIKETTLE_GATT_UUID_VERSION          0x0004
#define MIKETTLE_GATT_UUID_SETUP            0xaa01
#define MIKETTLE_GATT_UUID_STATUS           0xaa02
#define MIKETTLE_GATT_UUID_TIME             0xaa04
#define MIKETTLE_GATT_UUID_BOIL_MODE        0xaa05
#define MIKETTLE_GATT_UUID_MCU_VERSION      0x2a28

/* MiKettle handles */
#define MIKETTLE_HANDLE_READ_FIRMWARE_VERSION   26
#define MIKETTLE_HANDLE_READ_NAME               20
#define MIKETTLE_HANDLE_AUTH_INIT               44
#define MIKETTLE_HANDLE_AUTH                    37
#define MIKETTLE_HANDLE_VERSION                 42
#define MIKETTLE_HANDLE_KEEP_WARM               58
#define MIKETTLE_HANDLE_STATUS                  61
#define MIKETTLE_HANDLE_TIME_LIMIT              65
#define MIKETTLE_HANDLE_BOIL_MODE               68

#define KEY_LENGTH      4
#define TOKEN_LENGTH    12
#define PERM_LENGTH     256

const char* TAG = XIAOMI_MI_KETTLE_DEVICE_NAME;

static esp_err_t mikettle_authorize(const hub_ble_client_handle_t client_handle, esp_bd_addr_t address);
static void ble_notify_callback(const hub_ble_client_handle_t client_handle, uint16_t handle, uint8_t *value, uint16_t value_length);
static void auth_notify_cb(const hub_ble_client_handle_t client_handle, uint16_t handle, uint8_t *value, uint16_t value_length);
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

static volatile bool auth_notify = false;
static data_ready_callback_t s_data_ready_callback = NULL;

esp_err_t mikettle_connect(const hub_ble_client_handle_t client_handle, esp_bd_addr_t address, esp_ble_addr_type_t address_type)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);
    esp_err_t result = ESP_OK;

    result = hub_ble_client_connect(client_handle, address, address_type);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Connect failed with error code %x [%s].", result, esp_err_to_name(result));
        return result;
    }

    result = mikettle_authorize(client_handle, address);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Authorize failed with error code %x [%s].", result, esp_err_to_name(result));
        return result;
    }

    return result;
}

esp_err_t mikettle_reconnect(const hub_ble_client_handle_t client_handle)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);
    esp_err_t result = ESP_OK;
    esp_bd_addr_t address;

    result = hub_ble_client_get_address(client_handle, &address);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Get address failed with error code %x [%s].", result, esp_err_to_name(result));
        return result;
    }

    result = hub_ble_client_reconnect(client_handle);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Reconnect failed with error code %x [%s].", result, esp_err_to_name(result));
        return result;
    }

    result = mikettle_authorize(client_handle, address);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Authorize failed with error code %x [%s].", result, esp_err_to_name(result));
        return result;
    }

    return result;
}

esp_err_t mikettle_register_data_ready_callback(const hub_ble_client_handle_t client_handle, data_ready_callback_t data_ready_callback)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);
    s_data_ready_callback = data_ready_callback;
    return ESP_OK;
}

esp_err_t mikettle_update_data(const hub_ble_client_handle_t client_handle, const char* data, uint16_t data_length)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);
    return ESP_OK;
}

static esp_err_t mikettle_authorize(const hub_ble_client_handle_t client_handle, esp_bd_addr_t address)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

    esp_err_t result = ESP_OK;
    uint16_t descr_count = 0;
    esp_gattc_descr_elem_t* descr = NULL;
    esp_bd_addr_t* reversed_mac = NULL;
    uint8_t* mac_id_mix = NULL;
    uint8_t* ciphered = NULL;

    result = hub_ble_client_get_service(client_handle, &uuid_service_kettle);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Find services failed with error code %x [%s].", result, esp_err_to_name(result));
        return result;
    }

    result = hub_ble_client_write_characteristic(client_handle, MIKETTLE_HANDLE_AUTH_INIT, key1, KEY_LENGTH);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Write characteristic failed with error code %x [%s].", result, esp_err_to_name(result));
        return result;
    }

    result = hub_ble_client_register_for_notify(client_handle, MIKETTLE_HANDLE_AUTH);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Register for notify failed with error code %x [%s].", result, esp_err_to_name(result));
        return result;
    }

    result = hub_ble_client_register_notify_callback(client_handle, &auth_notify_cb);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Register for notify failed with error code %x [%s].", result, esp_err_to_name(result));
        return result;
    }

    result = hub_ble_client_get_descriptors(client_handle, MIKETTLE_HANDLE_AUTH, NULL, &descr_count);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Get descriptor count failed with error code %x [%s].", result, esp_err_to_name(result));
        return result;
    }

    descr = (esp_gattc_descr_elem_t*)malloc(descr_count * sizeof(esp_gattc_descr_elem_t));
    if (descr == NULL)
    {
        result = ESP_ERR_NO_MEM;
        ESP_LOGE(TAG, "Memory allocate failed with error code %x [%s].", result, esp_err_to_name(result));
        return ESP_FAIL;
    }

    if (hub_ble_client_get_descriptors(client_handle, MIKETTLE_HANDLE_AUTH, descr, &descr_count) != ESP_GATT_OK)
    {
        ESP_LOGE(TAG, "Get descriptors failed.");
        goto cleanup;
    }

    ESP_LOGI(TAG, "Found %i descriptors.", descr_count);

    result = hub_ble_client_write_descriptor(client_handle, descr[0].handle, subscribe, sizeof(subscribe));
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Write descriptor failed with error code %x [%s].", result, esp_err_to_name(result));
        goto cleanup;
    }

    ESP_LOGI(TAG, "Written to descriptor handle: %i.", descr[0].handle);

    reversed_mac = (esp_bd_addr_t*)malloc(sizeof(esp_bd_addr_t));
    if (reversed_mac == NULL)
    {
        result = ESP_ERR_NO_MEM;
        ESP_LOGE(TAG, "Memory allocate failed with error code %x [%s].", result, esp_err_to_name(result));
        goto cleanup;
    }

    mac_id_mix = (uint8_t*)malloc(sizeof(uint16_t) + sizeof(esp_bd_addr_t));
    if (mac_id_mix == NULL)
    {
        result = ESP_ERR_NO_MEM;
        ESP_LOGE(TAG, "Memory allocate failed with error code %x [%s].", result, esp_err_to_name(result));
        goto cleanup;
    }

    ciphered = (uint8_t*)malloc(sizeof(token));
    if (ciphered == NULL)
    {
        result = ESP_ERR_NO_MEM;
        ESP_LOGE(TAG, "Memory allocate failed with error code %x [%s].", result, esp_err_to_name(result));
        goto cleanup;
    }

    reverse_mac((uint8_t*)address, (uint8_t*)address + sizeof(esp_bd_addr_t), reversed_mac);
    mix_a(reversed_mac, MIKETTLE_PRODUCT_ID, mac_id_mix);
    cipher(mac_id_mix, mac_id_mix + sizeof(uint16_t) + sizeof(esp_bd_addr_t), token, token + TOKEN_LENGTH, ciphered);

    result = hub_ble_client_write_characteristic(client_handle, MIKETTLE_HANDLE_AUTH, ciphered, TOKEN_LENGTH);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Write characteristic failed with error code %x [%s].", result, esp_err_to_name(result));
        goto cleanup;
    }

    while (!auth_notify)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    
    cipher(token, token + TOKEN_LENGTH, key2, key2 + KEY_LENGTH, ciphered);

    result = hub_ble_client_write_characteristic(client_handle, MIKETTLE_HANDLE_AUTH, ciphered, KEY_LENGTH);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Write characteristic failed with error code %x [%s].", result, esp_err_to_name(result));
        goto cleanup;
    }

    result = hub_ble_client_read_characteristic(client_handle, MIKETTLE_HANDLE_VERSION, NULL, NULL);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Read characteristic failed with error code %x [%s].", result, esp_err_to_name(result));
        goto cleanup;
    }

    result = hub_ble_client_unregister_for_notify(client_handle, MIKETTLE_HANDLE_AUTH);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Unregister for notify failed with error code %x [%s].", result, esp_err_to_name(result));
        goto cleanup;
    }

    result = hub_ble_client_register_for_notify(client_handle, MIKETTLE_HANDLE_STATUS);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Register for notify failed with error code %x [%s].", result, esp_err_to_name(result));
        goto cleanup;
    }

    result = hub_ble_client_write_descriptor(client_handle, MIKETTLE_HANDLE_STATUS + 1, subscribe, sizeof(subscribe));
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Write descriptor failed with error code %x [%s].", result, esp_err_to_name(result));
        goto cleanup;
    }

    if (hub_ble_client_register_notify_callback(client_handle, &ble_notify_callback) != ESP_OK)
    {
        ESP_LOGE(TAG, "Register notify callback failed.");
        goto cleanup;
    }

cleanup:

    auth_notify = false;

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

static void ble_notify_callback(const hub_ble_client_handle_t client_handle, uint16_t handle, uint8_t *value, uint16_t value_length)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

    if (s_data_ready_callback == NULL)
    {
        return;
    }

    s_data_ready_callback(client_handle, NULL, 0);
}

static void auth_notify_cb(const hub_ble_client_handle_t client_handle, uint16_t handle, uint8_t *value, uint16_t value_length)
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