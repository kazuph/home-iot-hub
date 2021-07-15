#ifndef HUB_BLE_SCANNER_HPP
#define HUB_BLE_SCANNER_HPP

#include <string_view>
#include <type_traits>
#include <cstdint>
#include <memory>

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_gap_ble_api.h"

#include "esp_err.h"
#include "esp_log.h"

#include "rxcpp/rx.hpp"
#include "tl/expected.hpp"

#include "utils/mac.hpp"
#include "utils/esp_exception.hpp"

namespace hub::ble::scanner
{
    struct message_type
    {
        std::string_view name;
        std::string_view mac;
    };

    namespace impl
    {
        class state
        {
        public:

            static std::weak_ptr<state>& get_state() noexcept
            {
                return s_scanner_state;
            }

            state();

            state(const state&)             = delete;

            state(state&&)                  = default;

            state& operator=(const state&)  = delete;

            state& operator=(state&&)       = default;

            ~state()                        = default;

            rxcpp::subjects::subject<message_type>& get_subject() noexcept
            {
                return m_subject;
            }

        private:

            static constexpr const char* TAG{ "hub::ble::scanner::impl::state" };

            static constexpr esp_ble_scan_params_t BLE_SCAN_PARAMS{
                BLE_SCAN_TYPE_ACTIVE,           // Scan type
                BLE_ADDR_TYPE_PUBLIC,           // Address type
                BLE_SCAN_FILTER_ALLOW_ALL,      // Filter policy
                0x50,                           // Scan interval
                0x30,                           // Scan window
                BLE_SCAN_DUPLICATE_DISABLE      // Advertise duplicates filter policy
            };

            static std::weak_ptr<state>             s_scanner_state;

            rxcpp::subjects::subject<message_type>  m_subject;
        };
    }

    [[nodiscard]] inline auto get_observable_factory() noexcept
    {
        static constexpr const char* TAG{ "hub::ble::scanner::get_observable_factory" };

        return [](uint16_t scan_duration) -> rxcpp::observable<message_type> {
            if (esp_err_t result = esp_ble_gap_start_scanning(scan_duration); result != ESP_OK)
            {
                return rxcpp::observable<>::error<message_type>(utils::esp_exception("Could not start BLE scan.", result));
            }
            else
            {
                std::shared_ptr<impl::state> local_state{  };

                {
                    auto& weak_state = impl::state::get_state();

                    if (!weak_state.expired())
                    {
                        ESP_LOGD(TAG, "Setting scanner local state.");
                        local_state = weak_state.lock();
                    }
                    else
                    {
                        try
                        {
                            ESP_LOGD(TAG, "Creating scanner global state.");
                            local_state = std::make_shared<impl::state>();
                        }
                        catch (const utils::esp_exception& err)
                        {
                            ESP_LOGE(TAG, "Global state creation failed with error code %i [%s].", err.errc(), esp_err_to_name(err.errc()));
                            return rxcpp::observable<>::error<message_type>(err);
                        }

                        weak_state = local_state;
                    }
                }

                return local_state->get_subject().get_observable() | rxcpp::operators::tap([local_state](message_type) {});
            }
        };
    }
}

#endif