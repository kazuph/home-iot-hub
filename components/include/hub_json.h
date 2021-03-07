#pragma once
#include <utility>
#include <string>
#include <memory>
#include <initializer_list>
#include <exception>
#include <stdexcept>
#include <type_traits>

extern "C"
{
    #include "cJSON.h"
}

namespace hub::json
{
    // Basic types
    using null_type     = std::nullptr_t;
    using bool_type     = bool;
    using integer_type  = int;
    using float_type    = double;
    using string_type   = const char*;

    using key_type      = const char*;
    using index_type    = std::size_t;

    class json_object_item;
    class json_array_item;

    template<bool Is_owning>
    class json_impl;

    using json_ref      = json_impl<false>;
    using json          = json_impl<true>;

    class json_array;
    class json_object;
    class json_value;

    namespace traits
    {
        template<typename T>
        inline constexpr bool is_null                       = std::is_same_v<T, null_type>;

        template<typename T>
        inline constexpr bool is_bool                       = std::is_same_v<T, bool_type>;

        template<typename T>
        inline constexpr bool is_number                     = !is_bool<T> && (std::is_same_v<T, integer_type> || std::is_same_v<T, float_type>);

        template<typename T>
        inline constexpr bool is_integer                    = !is_bool<T> && std::is_same_v<T, integer_type>;

        template<typename T>
        inline constexpr bool is_float                      = !is_bool<T> && std::is_same_v<T, float_type>;

        template<typename T>
        inline constexpr bool is_string                     = std::is_same_v<T, string_type>;

        template<typename T>
        inline constexpr bool is_object                     = std::is_same_v<T, json_object>;

        template<typename T>
        inline constexpr bool is_array                      = std::is_same_v<T, json_array>;

        template<typename From, typename To>
        inline constexpr bool is_explicitly_convertible     = std::is_constructible_v<To, From> && !std::is_convertible_v<From, To>;
    }

    template<typename T, typename JsonT>
    inline std::optional<T> json_cast(JsonT value)
    {
        static_assert(traits::is_explicitly_convertible<JsonT, cJSON*>, "Bad JSON type.");

        if constexpr (traits::is_null<T>)
        {
            if (!cJSON_IsNull(static_cast<cJSON*>(value)))
            {
                return std::nullopt;
            }

            return nullptr;
        }
        else if constexpr (traits::is_bool<T>)
        {
            if (!cJSON_IsBool(static_cast<cJSON*>(value)))
            {
                return std::nullopt;
            }

            return (cJSON_IsTrue(static_cast<cJSON*>(value))) ? true : false;
        }
        else if constexpr (traits::is_integer<T>)
        {
            if (!cJSON_IsNumber(static_cast<cJSON*>(value)))
            {
                return std::nullopt;
            }

            return static_cast<cJSON*>(value)->valueint;
        }
        else if constexpr (traits::is_float<T>)
        {
            if (!cJSON_IsNumber(static_cast<cJSON*>(value)))
            {
                return std::nullopt;
            }

            return static_cast<cJSON*>(value)->valuedouble;
        }
        else if constexpr (traits::is_string<T>)
        {
            if (!cJSON_IsString(static_cast<cJSON*>(value)))
            {
                return std::nullopt;
            }

            return static_cast<cJSON*>(value)->valuestring;
        }
        else
        {
            return std::nullopt;
        }
    }

    template<bool Is_owning>
    class json_ptr
    {
    public:

        using value_type    = cJSON;
        using pointer       = value_type*;

        json_ptr() noexcept :
            data{ nullptr }
        {

        }

        json_ptr(const json_ptr&) = delete;

        json_ptr(json_ptr&& other) noexcept :
            data{ nullptr }
        {
            *this = std::move(other);
        }

        explicit json_ptr(std::nullptr_t) noexcept :
            data{ nullptr }
        {

        }

        explicit json_ptr(pointer data) noexcept
            : data{ data }
        {

        }

        ~json_ptr()
        {
            if constexpr (Is_owning)
            {
                cJSON_Delete(data);
            }

            data = nullptr;
        }

        json_ptr& operator=(const json_ptr&) = delete;

        json_ptr& operator=(json_ptr&& other) noexcept
        {
            if (this == &other)
            {
                return *this;
            }

            if constexpr (Is_owning)
            {
                if (data != nullptr)
                {
                    cJSON_Delete(data);
                    data = nullptr;
                }
            }

            data = other.data;
            other.data = nullptr;

            return *this;
        }

        pointer get() const noexcept
        {
            return data;
        }

        pointer release() noexcept
        {
            pointer tmp = data;
            data = nullptr;
            return tmp;
        }

        explicit operator bool() const noexcept
        {
            return (data != nullptr);
        }

        explicit operator pointer () const noexcept
        {
            return get();
        }

        value_type operator*() noexcept
        {
            return *data;
        }

        value_type operator*() const noexcept
        {
            return *data;
        }

        pointer operator->() noexcept
        {
            return data;
        }

        pointer operator->() const noexcept
        {
            return data;
        }

    private:

        pointer data;
    };

    class json_value final : public json_ptr<false>
    {
    public:

        using base = json_ptr<false>;

        json_value(null_type) noexcept;

        json_value(bool_type value) noexcept;

        json_value(integer_type value) noexcept;

        json_value(float_type value) noexcept;

        json_value(string_type value) noexcept;

        json_value(const json_value&) = delete;

        json_value(json_value&& other) = default;

        json_value& operator=(const json_value&) = delete;

        json_value& operator=(json_value&& other) = default;

        template<typename T>
        json_value& operator=(T value)
        {
            *this = std::move(json_value(value));
            return *this;
        }

        ~json_value() = default;
    };

    class json_object final : public json_ptr<false>
    {
    public:

        using base = json_ptr<false>;

        json_object() noexcept;

        json_object(std::initializer_list<std::pair<key_type, json_value>> object) noexcept;

        json_object(std::initializer_list<std::pair<key_type, json_array>> object) noexcept;

        json_object(std::initializer_list<std::pair<key_type, json_object>> object) noexcept;

        json_object(const json_object&) = delete;

        json_object(json_object&&) = default;

        json_object& operator=(const json_object&) = delete;

        json_object& operator=(json_object&&) = default;

        template<typename T>
        json_object& operator=(T value)
        {
            *this = std::move(json_object(value));
            return *this;
        }

        ~json_object() = default;
    };

    class json_array final : public json_ptr<false>
    {
    public:

        using base = json_ptr<false>;

        json_array() noexcept;

        template<typename T>
        json_array(std::initializer_list<T> init) noexcept :
            base{ cJSON_CreateArray() }
        {
            static_assert(std::is_constructible_v<json_value, T>, "Bad JSON value type.");

            for (const auto& elem : init)
            {
                cJSON_AddItemToArray(get(), json_value(elem).get());
            }
        }

        json_array(std::initializer_list<json_object> init) noexcept;

        json_array(std::initializer_list<json_array> init) noexcept;

        json_array(const json_array&) = delete;

        json_array(json_array&&) = default;

        json_array& operator=(const json_array&) = delete;

        json_array& operator=(json_array&&) = default;

        template<typename T>
        json_array& operator=(T value)
        {
            *this = std::move(json_array(value));
            return *this;
        }

        ~json_array() = default;
    };

    template<bool Is_owning>
    class json_impl : public json_ptr<Is_owning>
    {
    public:

        using base = json_ptr<Is_owning>;

        template<bool _Is_owning>
        friend class json_impl;

        static json_impl parse(const char* str) noexcept
        {
            return json_impl(cJSON_Parse(str));
        }

        json_impl() noexcept :
            base{ nullptr }
        {

        }

        json_impl(const json_impl&) = delete;

        json_impl(json_impl&&) = default;

        json_impl(json_value value) noexcept :
            base{ value.get() }
        {

        }

        json_impl(json_object object) noexcept :
            base{ object.get() }
        {

        }

        json_impl(json_array arr) noexcept :
            base{ arr.get() }
        {

        }

        json_impl& operator=(const json_impl&) = delete;

        json_impl& operator=(json_impl&& other) = default;

        template<typename T>
        json_impl& operator=(T&& value)
        {
            *this = std::move(json_impl(std::move(value)));
            return *this;
        }

        json_object_item operator[](key_type key);

        json_array_item operator[](index_type index);

        template<typename T>
        void push_back(T&& item)
        {
            if (!cJSON_IsArray(base::get()))
            {
                throw std::logic_error("Not an array.");
            }

            cJSON_AddItemToArray(base::get(), json_ref(std::move(item)).get());
        }

        template<typename T>
        void push_back(std::pair<key_type, T>&& item)
        {
            if (!cJSON_IsObject(base::get()))
            {
                throw std::logic_error("Not an object.");
            }

            auto& [key, value] = item;
            cJSON_AddItemToObject(base::get(), key, json_ref(std::move(value)).get());
        }

        bool is_array() const noexcept
        {
            return (cJSON_IsArray(base::get())) ? true : false;
        }

        bool is_object() const noexcept
        {
            return (cJSON_IsObject(base::get())) ? true : false;
        }

        std::string dump() const noexcept
        {
            std::unique_ptr<char, decltype(&cJSON_free)> buff{ cJSON_PrintUnformatted(base::get()), &cJSON_free };
            return std::string(buff.get());
        }

        ~json_impl() = default;

    protected:

        json_impl(cJSON* ptr) noexcept :
            base{ ptr }
        {

        }

    };

    class json_object_item final : public json_ref
    {
    public:

        using base = json_ref;

        json_object_item() = delete;

        json_object_item(json_ptr<false> item, json_ptr<false> parent) noexcept;

        template<typename T>
        json_object_item& operator=(T value)
        {
            cJSON_ReplaceItemInObjectCaseSensitive(parent.get(), base::get()->string_t, json_ref{ std::move(value) }.get());
            return *this;
        }

    private:

        json_ptr<false> parent;
    };

    class json_array_item final : public json_ref
    {
    public:

        using base = json_ref;

        json_array_item() = delete;

        json_array_item(json_ptr<false> item, json_ptr<false> parent, index_type index) noexcept;

        template<typename T>
        json_array_item& operator=(T value)
        {
            cJSON_ReplaceItemInArray(parent.get(), index, json_ref{ std::move(value) }.get());
            return *this;
        }

    private:

        json_ptr<false> parent;
        index_type index;
    };

    template<bool Is_owning>
    json_object_item json_impl<Is_owning>::operator[](key_type key)
    {
        if (!cJSON_IsObject(base::get()))
        {
            throw std::logic_error("Not an object.");
        }

        json_ptr<false> item{ cJSON_GetObjectItemCaseSensitive(base::get(), key) };

        if (!item)
        {
            item = json_ptr<false>(cJSON_CreateNull());
            cJSON_AddItemToObject(base::get(), key, item.get());
        }

        return json_object_item(std::move(item), json_ptr<false>(base::get()));
    }

    template<bool Is_owning>
    json_array_item json_impl<Is_owning>::operator[](index_type index)
    {
        if (!cJSON_IsArray(base::get()))
        {
            throw std::logic_error("Not an array.");
        }

        json_ptr<false> item{ cJSON_GetArrayItem(base::get(), index) };

        if (!item)
        {
            throw std::out_of_range("Array subscript out of range.");
        }

        return json_array_item(std::move(item), json_ptr<false>(base::get()), index);
    }
}