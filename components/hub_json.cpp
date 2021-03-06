#include "hub_json.h"

namespace hub::json
{
    json_value::json_value(null_type) noexcept :
        base{ cJSON_CreateNull() }
    {

    }

    json_value::json_value(bool_type value) noexcept :
        base{ (value) ? cJSON_CreateTrue() : cJSON_CreateFalse() }
    {

    }

    json_value::json_value(integer_type value) noexcept :
        base{ cJSON_CreateNumber(static_cast<double>(value)) }
    {

    }

    json_value::json_value(float_type value) noexcept :
        base{ cJSON_CreateNumber(value) }
    {

    }

    json_value::json_value(string_type value) noexcept :
        base{ cJSON_CreateString(value) }
    {

    }

    json_object::json_object() noexcept :
        base{ cJSON_CreateObject() }
    {

    }

    json_object::json_object(std::initializer_list<std::pair<key_type, json_value>> object) noexcept :
        base{ cJSON_CreateObject() }
    {
        for (const auto& [key, value] : object)
        {
            cJSON_AddItemToObject(get(), key, value.get());
        }
    }

    json_object::json_object(std::initializer_list<std::pair<key_type, json_array>> object) noexcept :
        base{ cJSON_CreateObject() }
    {
        for (const auto& [key, value] : object)
        {
            cJSON_AddItemToObject(get(), key, value.get());
        }
    }

    json_object::json_object(std::initializer_list<std::pair<key_type, json_object>> object) noexcept :
        base{ cJSON_CreateObject() }
    {
        for (const auto& [key, value] : object)
        {
            cJSON_AddItemToObject(get(), key, value.get());
        }
    }

    json_array::json_array() noexcept :
        base{ cJSON_CreateArray() }
    {

    }

    json_array::json_array(std::initializer_list<json_object> init) noexcept :
        base{ cJSON_CreateArray() }
    {
        for (const auto& obj : init)
        {
            cJSON_AddItemToArray(get(), obj.get());
        }
    }

    json_array::json_array(std::initializer_list<json_array> init) noexcept :
        base{ cJSON_CreateArray() }
    {
        for (const auto& arr : init)
        {
            cJSON_AddItemToArray(get(), arr.get());
        }
    }

    json_object_item::json_object_item(json_ptr<false> item, json_ptr<false> parent) noexcept :
        base{ item.get() },
        parent{ std::move(parent) }
    {

    }

    json_array_item::json_array_item(json_ptr<false> item, json_ptr<false> parent, index_type index) noexcept :
        base{ item.get() },
        parent{ std::move(parent) },
        index{ index }
    {

    }
}