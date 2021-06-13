#ifndef HUB_APPLICATION_HPP
#define HUB_APPLICATION_HPP

#include <variant>
#include <cassert>
#include <type_traits>

#include "configuration.hpp"
#include "init.hpp"
#include "not_connected.hpp"
#include "connected.hpp"
#include "fsm.hpp"

namespace hub
{
    class application final : public fsm<init_t, not_connected_t, connected_t>
    {
    public:

        application()                               = default;

        application(const application&)             = delete;

        application(application&&)                  = default;

        application& operator=(const application&)  = delete;

        application& operator=(application&&)       = default;

        void run() noexcept
        {
            configuration config;

            {
                auto& state = get_state<init_t>();

                state.initialize_filesystem();
                config = state.read_config("spiffs/config.json");
                
                state.initialize_ble();
                state.connect_to_wifi(config);
                state.publish_scan_message(config);
            }

            while (true)
            {
                set_state<not_connected_t>(config);

                {
                    auto& state = get_state<not_connected_t>();
                    config.hub.id = state.wait_for_id_assignment();
                }

                set_state<connected_t>(config);

                {
                    auto& state = get_state<connected_t>();
                    state.process_events();
                }
            }
        }
    };
}

#endif