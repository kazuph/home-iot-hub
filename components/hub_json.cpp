#include "hub_json.h"

namespace hub::utils
{
    json_base::json_base() noexcept :
        json_ptr{ nullptr }
    {

    }

    json_base::json_base(cJSON* json_ptr) noexcept :
        json_ptr{ json_ptr }
    {

    }

    json_base::json_base(json_base&& other) noexcept :
        json_ptr{ nullptr }
    {
        *this = std::move(other);
    }

    json_base& json_base::operator=(json_base&& other)
    {
        if (this == &other)
        {
            return *this;
        }

        cJSON_Delete(json_ptr);

        json_ptr = other.json_ptr;
        other.json_ptr = nullptr;

        return *this;
    }

    json_base::~json_base()
    {
        cJSON_Delete(json_ptr);
    }

    json json::json_object(json_initializer_list init)
    {
        auto result = json_base(cJSON_CreateObject());

        for (const auto& item : init)
        {
            if (!item.is_array() || item.size() != 2 || !item[0].is_string())
            {
                throw std::invalid_argument("Bad object type.");
            }

            cJSON* key = cJSON_GetArrayItem(item.get(), 0);
            cJSON* value = cJSON_DetachItemFromArray(item.get(), 1);

            cJSON_AddItemToObject(result.get(), key->valuestring, value);
        }

        return json(result.release());
    }

    json json::json_array(json_initializer_list init)
    {
        auto result = json_base(cJSON_CreateArray());

        for (const auto& elem : init)
        {
            cJSON_AddItemToArray(result.get(), elem.release());
        }

        return json(result.release());
    }

    json::json(cJSON* json_ptr) noexcept :
        base{ json_ptr }
    {

    }

    json::json(null_type) noexcept :
        base{ cJSON_CreateNull() }
    {

    }

    json::json(bool_type value) noexcept :
        base{ (value) ? cJSON_CreateTrue() : cJSON_CreateFalse() }
    {

    }

    json::json(integer_type value) noexcept :
        base{ cJSON_CreateNumber(static_cast<double>(value)) }
    {

    }

    json::json(float_type value) noexcept :
        base{ cJSON_CreateNumber(static_cast<double>(value)) }
    {

    }

    json::json(const char* value) noexcept :
        base{ cJSON_CreateString(value) }
    {

    }

    json::json(const std::string& value) noexcept :
        base{ cJSON_CreateString(value.c_str()) }
    {
        
    }

    json::json(std::string_view value) noexcept :
        base{ cJSON_CreateString(value.data()) }
    {

    }

    json::json(json_initializer_list init) :
        base{ nullptr }
    {
        if (std::all_of(init.begin(), init.end(), [](const json& elem) { return (elem.is_array() && elem.size() == 2 && elem[0].is_string()); }))
        {
            *this = std::move(json_object(init));
        }
        else
        {
            *this = std::move(json_array(init));
        }
    }

    json_object_item json::operator[](const char* key) const
    {
        if (!is_object())
        {
            throw std::logic_error("Not an object.");
        }

        cJSON* item{ cJSON_GetObjectItemCaseSensitive(get(), key) };

        if (!item)
        {
            item = cJSON_AddNullToObject(get(), key);
        }

        return json_object_item(item, get());
    }

    json_object_item json::operator[](const std::string& key) const
    {
        if (!is_object())
        {
            throw std::logic_error("Not an object.");
        }

        cJSON* item{ cJSON_GetObjectItemCaseSensitive(get(), key.c_str()) };

        if (!item)
        {
            item = cJSON_AddNullToObject(get(), key.c_str());
        }

        return json_object_item(item, get());
    }

    json_object_item json::operator[](std::string_view key) const
    {
        if (!is_object())
        {
            throw std::logic_error("Not an object.");
        }

        cJSON* item{ cJSON_GetObjectItemCaseSensitive(get(), key.data()) };

        if (!item)
        {
            item = cJSON_AddNullToObject(get(), key.data());
        }

        return json_object_item(item, get());
    }

    json_array_item json::operator[](index_type index) const
    {
        if (!is_array())
        {
            throw std::logic_error("Not an array.");
        }

        cJSON* item{ cJSON_GetArrayItem(get(), index) };

        if (!item)
        {
            throw std::out_of_range("Array subscript out of range.");
        }

        return json_array_item(item, get(), index);
    }

    json_object_item json::at(const char* key) const
    {
        return (*this)[key];
    }

    json_object_item json::at(const std::string& key) const
    {
        return (*this)[key];
    }

    json_object_item json::at(std::string_view key) const
    {
        return (*this)[key];
    }

    json_array_item json::at(index_type index) const
    {
        return (*this)[index];
    }

    void json::push_back(json&& item)
    {
        if (!is_array())
        {
            throw std::logic_error("Not an array.");
        }

        cJSON_AddItemToArray(get(), item.release());
    }

    void json::insert(std::pair<const char*, json>&& item)
    {
        if (!is_object())
        {
            throw std::logic_error("Not an object.");
        }

        auto&& [key, value] = item;
        cJSON_AddItemToObject(get(), key, value.release());
    }

    void json::insert(std::pair<const std::string, json>&& item)
    {
        if (!is_object())
        {
            throw std::logic_error("Not an object.");
        }

        auto&& [key, value] = item;
        cJSON_AddItemToObject(get(), key.c_str(), value.release());
    }

    void json::insert(std::pair<std::string_view, json>&& item)
    {
        if (!is_object())
        {
            throw std::logic_error("Not an object.");
        }

        auto&& [key, value] = item;
        cJSON_AddItemToObject(get(), key.data(), value.release());
    }

    std::string json::dump(bool formatted) const
    {
        auto buff = std::unique_ptr<char>((formatted) ? cJSON_Print(get()) : cJSON_PrintUnformatted(get()));
        return buff.get();
    }

    json_ref::json_ref() noexcept :
        base{ nullptr }
    {

    }

    json_ref::json_ref(cJSON* json_ptr) noexcept :
        base{ base(json_ptr) }
    {

    }

    json_ref::~json_ref()
    {
        // This is just a reference type, avoid freeing memory
        base::release();
    }

    json_object_item::json_object_item(cJSON* item, cJSON* parent) :
        base{ item },
        parent{ parent }
    {

    }

    json_object_item& json_object_item::operator=(json&& other)
    {
        const char* key = get()->string;

        if (!cJSON_ReplaceItemInObject(parent, key, other.release()))
        {
            throw std::invalid_argument("Invalid object item.");
        }

        return *this;
    }

    json_array_item::json_array_item(cJSON* item, cJSON* parent, index_type index) :
        base{ item },
        parent{ parent },
        index{ index }
    {

    }
    
    json_array_item& json_array_item::operator=(json&& other)
    {
        if (!cJSON_ReplaceItemInArray(parent, index, other.release()))
        {
            throw std::invalid_argument("Invalid array item.");
        }

        return *this;
    }
}