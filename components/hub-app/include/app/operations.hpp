#ifndef HUB_OPERATIONS_HPP
#define HUB_OPERATIONS_HPP

#include <memory>
#include <string_view>

#include "rapidjson/document.h"

#include "utils/mac.hpp"

namespace hub
{
    inline bool is_empty_sv(std::string_view str) noexcept
    {
        return str.length() == 0;
    }

    inline std::shared_ptr<rapidjson::Document> parse_json(std::string_view message) noexcept
    {
        auto result = std::make_shared<rapidjson::Document>();
        result->Parse(message.data(), message.length());
        return result;
    }

    inline bool is_parse_success(std::shared_ptr<rapidjson::Document> message_ptr) noexcept
    {
        return !(message_ptr->HasParseError());
    }

    inline bool is_connect_message(std::shared_ptr<rapidjson::Document> message_ptr) noexcept
    {
        const auto &message = *message_ptr;

        return (
            message.IsObject() &&
            message.HasMember("name") &&
            message.HasMember("address") &&
            message.HasMember("id") &&
            message["name"].IsString() &&
            message["address"].IsString() &&
            message["id"].IsString());
    }

    inline bool is_disconnect_message(std::shared_ptr<rapidjson::Document> message_ptr) noexcept
    {
        return is_connect_message(message_ptr); // Same requirements
    }

    inline auto connect_message_predicate(std::string_view device_name, const utils::mac& device_address) noexcept
    {
        return [=](std::shared_ptr<rapidjson::Document> message_ptr) {
            const auto &message = *message_ptr;

            std::array<char, utils::mac::MAC_STR_SIZE> address_buff;
            std::string_view address{ address_buff.begin(), address_buff.size() };

            device_address.to_charbuff(address_buff.begin());

            return (
                (device_name == message["name"].GetString()) &&
                (address == message["address"].GetString()));
        };
    }

    inline auto disconnect_message_predicate(std::string_view device_name, std::string_view device_id, const utils::mac& device_address) noexcept
    {
        return [=](std::shared_ptr<rapidjson::Document> message_ptr) {
            const auto &message = *message_ptr;

            std::array<char, utils::mac::MAC_STR_SIZE> address_buff;
            std::string_view address{ address_buff.begin(), address_buff.size() };

            device_address.to_charbuff(address_buff.begin());

            return (
                (device_name == message["name"].GetString()) &&
                (address == message["address"].GetString()) &&
                (device_id == message["id"].GetString()));
        };
    }
}

#endif