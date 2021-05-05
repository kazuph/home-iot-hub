#include "scanner.hpp"
#include "error.hpp"
#include "timing.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_gap_ble_api.h"

#include "esp_err.h"
#include "esp_log.h"

#include <list>
#include <new>

namespace hub::ble::scanner
{
    using namespace timing::literals;

    static constexpr auto           BLE_TIMEOUT     { 10_s };

    static constexpr EventBits_t    SCAN_START_BIT  { BIT0 };
    static constexpr EventBits_t    SCAN_STOP_BIT   { BIT1 };
    static constexpr EventBits_t    FAIL_BIT        { BIT2 };

    static constexpr esp_ble_scan_params_t BLE_SCAN_PARAMS{
        BLE_SCAN_TYPE_ACTIVE,           // Scan type
        BLE_ADDR_TYPE_PUBLIC,           // Address type
        BLE_SCAN_FILTER_ALLOW_ALL,      // Filter policy
        0x50,                           // Scan interval
        0x30,                           // Scan window
        BLE_SCAN_DUPLICATE_DISABLE      // Advertise duplicates filter policy
    };

    static EventGroupHandle_t                       s_scan_event_group              { nullptr };

    static event::scan_results_event_handler_t      s_scan_results_event_handler    {  };

    static void gap_callback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
    {
        if (event == ESP_GAP_BLE_SCAN_START_COMPLETE_EVT)
        {
            if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
            {
                xEventGroupSetBits(s_scan_event_group, FAIL_BIT);
                return;
            }

            xEventGroupSetBits(s_scan_event_group, SCAN_START_BIT);
        }
        else if (event == ESP_GAP_BLE_SCAN_RESULT_EVT && param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT)
        {
            uint8_t adv_name_len    = 0;
            uint8_t* adv_name       = esp_ble_resolve_adv_data(param->scan_rst.ble_adv, ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);

            if (adv_name == nullptr || adv_name_len == 0)
            {
                return;
            }

            if (!s_scan_results_event_handler)
            {
                return;
            }

            s_scan_results_event_handler.invoke({
                std::string(reinterpret_cast<const char*>(adv_name), static_cast<size_t>(adv_name_len)),
                mac(param->scan_rst.bda, param->scan_rst.ble_addr_type)
            });
        }
        else if (event == ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT)
        {
            if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS)
            {
                xEventGroupSetBits(s_scan_event_group, FAIL_BIT);
                return;
            }

            xEventGroupSetBits(s_scan_event_group, SCAN_STOP_BIT);
        }
    }

    void init()
    {
        if (esp_err_t result = esp_ble_gap_register_callback(&gap_callback); result != ESP_OK)
        {
            throw esp_exception(result);
        }

        s_scan_event_group = xEventGroupCreate();
        if (!s_scan_event_group)
        {
            throw std::bad_alloc();
        }
    }

    void deinit()
    {
        vEventGroupDelete(s_scan_event_group);
    }

    void start(uint16_t duration)
    {
        if (esp_err_t result = esp_ble_gap_start_scanning(duration); result != ESP_OK)
        {
            throw esp_exception(result);
        }

        EventBits_t bits = xEventGroupWaitBits(s_scan_event_group, SCAN_START_BIT | FAIL_BIT, pdTRUE, pdFALSE, static_cast<TickType_t>(BLE_TIMEOUT));

        if (bits & SCAN_START_BIT)
        {
            return;
        }
        else if (bits & FAIL_BIT)
        {
            throw esp_exception(ESP_FAIL, "Scan start failed.");
        }
        else
        {
            throw esp_exception(ESP_ERR_TIMEOUT, "Scan start timed out.");
        }
    }

    void stop()
    {
        if (esp_err_t result = esp_ble_gap_stop_scanning(); result != ESP_OK)
        {
            throw esp_exception(result);
        }

        EventBits_t bits = xEventGroupWaitBits(s_scan_event_group, SCAN_STOP_BIT | FAIL_BIT, pdTRUE, pdFALSE, static_cast<TickType_t>(BLE_TIMEOUT));

        if (bits & SCAN_STOP_BIT)
        {
            return;
        }
        else if (bits & FAIL_BIT)
        {
            throw esp_exception(ESP_FAIL, "Scan stop failed.");
        }
        else
        {
            throw esp_exception(ESP_ERR_TIMEOUT, "Scan stop timed out.");
        }
    }

    void set_scan_results_event_handler(event::scan_results_event_handler_t::function_type scan_callback)
    {
        s_scan_results_event_handler += scan_callback;
    }
}