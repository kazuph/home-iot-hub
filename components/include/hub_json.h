#ifndef HUB_JSON_H
#define HUB_JSON_H

#include <utility>
#include <string>
#include <memory>
#include <algorithm>
#include <initializer_list>
#include <string_view>
#include <exception>
#include <stdexcept>
#include <type_traits>
#include <iostream>

extern "C"
{
    #include "cJSON.h"
}

namespace hub::utils
{
    class json_base;
    class json;
    class json_ref;
    class json_object_item;
    class json_array_item;

    class json_base
    {
    public:

        json_base() noexcept;

        json_base(cJSON* json_ptr) noexcept;

        json_base(const json_base&) = delete;

        json_base(json_base&& other) noexcept;

        json_base& operator=(const json_base&) = delete;

        json_base& operator=(json_base&& other);

        cJSON* get() const noexcept
        {
            return json_ptr;
        }

        inline cJSON* release() const noexcept
        {
            cJSON* tmp = json_ptr;
            json_ptr = nullptr;
            return tmp;
        }

        ~json_base();

    private:

        mutable cJSON* json_ptr;
    };

    class json : public json_base
    {
    public:

        using base = json_base;

        // Basic types
        using null_type     = std::nullptr_t;
        using bool_type     = bool;
        using integer_type  = int;
        using float_type    = double;
        using size_type     = integer_type;
        using index_type    = size_type;

        using json_initializer_list = std::initializer_list<json>;

        static json json_object(json_initializer_list init = {});

        static json json_array(json_initializer_list init = {});

        static json parse(std::string_view data)
        {
            return json(cJSON_ParseWithLength(data.data(), static_cast<size_t>(data.length())));
        }

        json() = default;

        json(const json&) = delete;

        json(json&&) = default;

        explicit json(cJSON* json_ptr) noexcept;

        json(null_type) noexcept;

        json(bool_type value) noexcept;

        json(integer_type value) noexcept;

        json(float_type value) noexcept;

        json(const char* value) noexcept;

        json(const std::string& value) noexcept;

        json(std::string_view value) noexcept;

        json(json_initializer_list init = {});

        json& operator=(const json&) = delete;

        json& operator=(json&&) = default;

        json_object_item operator[](const char* key) const;

        json_object_item operator[](const std::string& key) const;

        json_object_item operator[](std::string_view key) const;

        json_array_item operator[](index_type index) const;

        json_object_item at(const char* key) const;

        json_object_item at(const std::string& key) const;

        json_object_item at(std::string_view key) const;

        json_array_item at(index_type index) const;

        void push_back(json&& item);

        void insert(std::pair<const char*, json>&& item);

        void insert(std::pair<const std::string, json>&& item);

        void insert(std::pair<std::string_view, json>&& item);

        size_type size() const
        {
            return cJSON_GetArraySize(get());
        }

        bool is_valid() const
        {
            return (!cJSON_IsInvalid(get()));
        }

        bool is_null() const noexcept
        {
            return (cJSON_IsNull(get()));
        }

        bool is_number() const noexcept
        {
            return (cJSON_IsNumber(get()));
        }

        bool is_bool() const noexcept
        {
            return (cJSON_IsBool(get()));
        }

        bool is_string() const noexcept
        {
            return (cJSON_IsString(get()));
        }

        bool is_array() const noexcept
        {
            return (cJSON_IsArray(get()));
        }

        bool is_object() const noexcept
        {
            return (cJSON_IsObject(get()));
        }

        bool is_object_item() const
        {
            return (get()->string);
        }

        bool is_array_item() const
        {
            return (!is_object_item() && (get()->prev || get()->next));
        }

        bool is_basic_type() const
        {
            return (!is_object_item() && !is_array_item());
        }

        std::string dump(bool formatted = false) const;

        ~json() = default;
    };

    class json_ref : public json
    {
    public:

        using base = json;

        json_ref() noexcept;

        json_ref(cJSON* json_ptr) noexcept;

        json_ref(const json_ref&) = delete;

        json_ref(json_ref&&) = default;

        json_ref& operator=(const json_ref&) = delete;

        json_ref& operator=(json_ref&&) = default;

        ~json_ref();
    };

    class json_object_item final : public json_ref
    {
    public:

        using base = json_ref;

        json_object_item() = delete;

        json_object_item(cJSON* item, cJSON* parent);

        json_object_item(const json_object_item&) = delete;

        json_object_item(json_object_item&&) = default;

        json_object_item& operator=(const json_object_item&) = delete;

        json_object_item& operator=(json_object_item&&) = default;

        json_object_item& operator=(json&& other);

    private:

        cJSON* parent;
    };

    class json_array_item final : public json_ref
    {
    public:

        using index_type = json::index_type;

        using base = json_ref;

        json_array_item() = delete;

        json_array_item(cJSON* item, cJSON* parent, index_type index);

        json_array_item(const json_array_item&) = delete;

        json_array_item(json_array_item&&) = default;

        json_array_item& operator=(const json_array_item&) = delete;

        json_array_item& operator=(json_array_item&&) = default;

        json_array_item& operator=(json&& other);

    private:

        cJSON* parent;
        index_type index;
    };

    namespace traits
    {
        template<typename T>
        inline constexpr bool is_null                       = std::is_same_v<T, json::null_type>;

        template<typename T>
        inline constexpr bool is_bool                       = std::is_same_v<T, json::bool_type>;

        template<typename T>
        inline constexpr bool is_number                     = !is_bool<T> && std::is_integral_v<T>;

        template<typename T>
        inline constexpr bool is_integer                    = !is_bool<T> && std::is_same_v<T, json::integer_type>;

        template<typename T>
        inline constexpr bool is_float                      = std::is_floating_point_v<T>;

        template<typename T>
        inline constexpr bool is_string                     = std::is_convertible_v<T, std::string_view>;
    }

    /*
    *   Cast json object to one of the basic types.
    *   Returns value of the provided type.
    *   Throws std::invalid_argument on error.
    * 
    *   Note:
    *   Currently does not support casting to objects and arrays.
    */
    template<typename T>
    inline T json_cast(const json& value)
    {
        if constexpr (traits::is_null<T>)
        {
            if (!value.is_null())
            {
                throw std::invalid_argument("Value type is not NULL.");
            }

            return nullptr;
        }
        else if constexpr (traits::is_bool<T>)
        {
            if (!value.is_bool())
            {
                throw std::invalid_argument("Value type is not bool.");
            }

            return static_cast<bool>(cJSON_IsTrue(value.get()));
        }
        else if constexpr (traits::is_integer<T>)
        {
            if (!value.is_number())
            {
                throw std::invalid_argument("Value type is not an integer.");
            }

            return static_cast<T>(value.get()->valueint);
        }
        else if constexpr (traits::is_float<T>)
        {
            if (!value.is_number())
            {
                throw std::invalid_argument("Value type is not float.");
            }

            return static_cast<T>(value.get()->valuedouble);
        }
        else if constexpr (traits::is_string<T>)
        {
            if (!value.is_string())
            {
                throw std::invalid_argument("Value type is not string.");
            }

            return T(value.get()->valuestring);
        }
        else
        {
            throw std::invalid_argument("Bad value type.");
        }
    }

    inline std::ostream& operator<<(std::ostream& os, const json& js)
    {
        return os << js.dump(true);
    }

    namespace literals
    {
        inline json operator"" _json(const char* str)
        {
            return json::parse(str);
        }
    }
}

#endif