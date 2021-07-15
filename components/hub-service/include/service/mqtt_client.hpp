#ifndef HUB_SERVICE_MQTT_CLIENT_HPP
#define HUB_SERVICE_MQTT_CLIENT_HPP

#include <atomic>
#include <string>
#include <string_view>
#include <exception>
#include <type_traits>
#include <memory>
#include <list>
#include <mutex>
#include <functional>
#include <set>

#include "mqtt_client.h"
#include "esp_err.h"

#include "rxcpp/rx.hpp"

namespace hub::service::mqtt
{
    namespace impl
    {
        class client_state
        {
        public:

            struct mqtt_message_t
            {
                std::string_view topic;
                std::string_view data;
            };

            client_state() = delete;

            client_state(std::string_view uri);

            client_state(const client_state&)               = delete;

            client_state(client_state&&)                    = default;

            client_state& operator=(const client_state&)    = delete;

            client_state& operator=(client_state&&)         = default;

            ~client_state();

            inline rxcpp::observable<mqtt_message_t> get_observable() const noexcept
            {
                return m_subject.get_observable();
            }

            inline esp_mqtt_client_handle_t get_handle() noexcept
            {
                return m_handle;
            }

        private:

            static constexpr const char* TAG{ "hub::service::mqtt::client_state" };

            esp_mqtt_client_handle_t                    m_handle;

            rxcpp::subjects::subject<mqtt_message_t>    m_subject;
        };
    }

    /**
     * @brief Factory class managing MQTT observables and subscribers.
     * Constructed via make_client function.
     * Upon construction connects to the broker under the given URI.
     * Each rxcpp::observable produced observes one MQTT topic.
     * Each rxcpp::subscriber publishes to one MQTT topic. Data exchanged with
     * rxcpp::obserables and rxcpp::subscribers is of type client::message_t, which
     * is currently defined as std::string_view.
     * 
     */
    struct client
    {
        /**
         * @brief Quality of service.
         * 
         */
        enum class qos_t : int
        {
            at_most_once    = 0,
            at_least_once   = 1,
            exactly_once    = 2
        };

        /**
         * @brief Data type being passed to the rxcpp::subscribers and rxcpp::observables.
         * 
         */
        using message_t = std::string_view;

        static constexpr const char* TAG{ "hub::service::mqtt::client" };

        /**
         * @brief Mutable MQTT state.
         * 
         */
        std::unique_ptr<impl::client_state> m_state;

        [[nodiscard]] static inline constexpr auto topic_predicate(std::string_view topic) noexcept
        {
            return [=](impl::client_state::mqtt_message_t message) {
                return topic == message.topic;
            };
        }

        /**
         * @brief Get the MQTT observable for the specified topic.
         * 
         * @param topic 
         * @param qos 
         * @return rxcpp::observable<message_t> 
         */
        [[nodiscard]] rxcpp::observable<message_t> get_observable(std::string_view topic, qos_t qos = qos_t::at_most_once) noexcept;

        /**
         * @brief Get the MQTT subscriber. Publishes messages under the given topic with the specified parameters.
         * 
         * @param topic 
         * @param qos 
         * @param retain 
         * @return rxcpp::subscriber<message_t> 
         */
        [[nodiscard]] rxcpp::subscriber<message_t> get_subscriber(std::string_view topic, qos_t qos = qos_t::at_most_once, bool retain = false);
    };

    /**
     * @brief Create MQTT subscriber/observable factory.
     * 
     * @param uri URI of the broker.
     * @return client MQTT subscriber/observable factory.
     */
    [[nodiscard]] inline client make_client(std::string_view uri)
    {
        return client{ std::make_unique<impl::client_state>(uri) };
    }
}

#endif