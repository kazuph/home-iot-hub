#ifndef HUB_MQTT_CLIENT_HPP
#define HUB_MQTT_CLIENT_HPP

#include <string_view>
#include <memory>

#include "mqtt_client.h"
#include "esp_err.h"
#include "esp_log.h"

#include "rxcpp/rx.hpp"
#include "tl/expected.hpp"

#include "utils/esp_exception.hpp"

namespace hub::mqtt
{
    using config_t = esp_mqtt_client_config_t;

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

            client_state(const config_t& config);

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

            static constexpr const char* TAG{ "hub::mqtt::client_state" };

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
            at_most_once  = 0,
            at_least_once = 1,
            exactly_once  = 2
        };

        /**
         * @brief Data type being passed to the rxcpp::subscribers and rxcpp::observables.
         * 
         */
        using message_t = std::string_view;

        static constexpr const char* TAG{ "hub::mqtt::client" };

        /**
         * @brief Mutable MQTT state.
         * 
         */
        std::shared_ptr<impl::client_state> m_state;

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
        [[nodiscard]] rxcpp::observable<message_t> subscribe(std::string_view topic, qos_t qos = qos_t::at_most_once) noexcept;

        /**
         * @brief Get the MQTT subscriber. Publishes messages under the given topic with the specified parameters.
         * 
         * @param topic 
         * @param qos 
         * @param retain 
         * @return auto 
         */
        [[nodiscard]] auto publish(std::string_view topic, qos_t qos = qos_t::at_most_once, bool retain = false) noexcept
        {
            namespace rx = rxcpp;
            namespace rxo = rx::operators;

            using namespace rxo;

            return [topic, qos, retain, local_state{ m_state }](rx::observable<message_t> observable) {
                return observable |
                    map([topic, qos, retain, local_state](message_t message) {
                        ESP_LOGD(TAG, "Publishing on topic: %.*s.", topic.length(), topic.data());
                        int result = esp_mqtt_client_publish(
                            local_state->get_handle(), 
                            topic.data(), 
                            message.data(), 
                            message.length(), 
                            static_cast<int>(qos),
                            static_cast<int>(retain));

                        if (result == ESP_FAIL)
                        {
                            LOG_AND_THROW(TAG, utils::esp_exception("Data publish failed."));
                        }

                        ESP_LOGV(TAG, "Published data: %.*s.", message.length(), message.data());
                        return result;
                    });
            };
        }
    };

    /**
     * @brief Create MQTT subscriber/observable factory.
     * 
     * @param uri Broker uri.
     * @return tl::expected<client, esp_err_t> 
     */
    [[nodiscard]] tl::expected<client, esp_err_t> make_client(std::string_view uri) noexcept;

    /**
     * @brief Create MQTT subscriber/observable factory.
     * 
     * @param config Client configuration.
     * @return tl::expected<client, esp_err_t> 
     */
    [[nodiscard]] tl::expected<client, esp_err_t> make_client(const config_t& config) noexcept;
}

#endif