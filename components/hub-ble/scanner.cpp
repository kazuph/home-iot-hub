#include <string>
#include <array>

#include "ble/scanner.hpp"

namespace hub::ble::scanner::impl
{
    std::weak_ptr<state> state::s_scanner_state{};

    state::state() :
        m_subject{  }
    {
        esp_err_t result = ESP_OK;

        result = esp_ble_gap_register_callback([](esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {

            ESP_LOGV(TAG, "GAP event: %i.", event);

            if (s_scanner_state.expired())
            {
                ESP_LOGD(TAG, "Global state expired.");
                return;
            }

            {
                auto state = s_scanner_state.lock();

                if (event == ESP_GAP_BLE_SCAN_RESULT_EVT)
                {
                    ESP_LOGV(TAG, "GAP search event: %i.", param->scan_rst.search_evt);

                    if (param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT)
                    {
                        uint8_t adv_name_len    = 0;
                        uint8_t* adv_name       = esp_ble_resolve_adv_data(param->scan_rst.ble_adv, ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);

                        if (adv_name == nullptr || adv_name_len == 0)
                        {
                            return;
                        }

                        ESP_LOGD(TAG, "Scan result received.");

                        {
                            static std::array<char, utils::mac::MAC_STR_SIZE> cache;
                            utils::mac(param->scan_rst.bda, param->scan_rst.bda + utils::mac::MAC_SIZE).to_charbuff(cache.begin());

                            state->get_subject().get_subscriber().on_next(message_type{
                                std::string_view(reinterpret_cast<const char*>(adv_name), static_cast<size_t>(adv_name_len)),
                                std::string_view(cache.begin(), cache.size())
                            });
                        }
                    }
                    else if (param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_CMPL_EVT)
                    {
                        state->get_subject().get_subscriber().on_completed();
                    }
                }
            }
        });

        if (result != ESP_OK)
        {
            LOG_AND_THROW(TAG, utils::esp_exception("Register GAP callback failed.", result));
        }

        if (result = esp_ble_gap_set_scan_params(const_cast<esp_ble_scan_params_t*>(&BLE_SCAN_PARAMS)); result != ESP_OK)
        {
            LOG_AND_THROW(TAG, utils::esp_exception("Set GAP scan params failed.", result));
        }
    }
}