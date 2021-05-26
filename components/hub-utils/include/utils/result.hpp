#ifndef HUB_UTILS_RESULT_H
#define HUB_UTILS_RESULT_H

#include <exception>
#include <stdexcept>
#include <functional>
#include <optional>
#include <type_traits>
#include <variant>

namespace hub::utils
{
    template<typename ResultT, typename ErrorT>
    class result
    {
    public:

        using value_type = ResultT;
        using error_type = ErrorT;

        struct value_helper
        {
            value_type m_value;
        };

        struct error_helper
        {
            error_type m_error;
        };

        using variant_type = std::variant<value_helper, error_helper>;

        template<typename... ArgsT>
        constexpr static result success(ArgsT&&... args) noexcept
        {
            return result(variant_type(value_helper{ value_type(std::forward<ArgsT>(args)...) }));
        }

        template<typename... ArgsT>
        constexpr static result failure(ArgsT&&... args) noexcept
        {
            return result(variant_type(error_helper{ error_type(std::forward<ArgsT>(args)...) }));
        }

        constexpr result(const result& other) :
            m_result{ other.m_result }
        {

        }

        constexpr result(result&& other) :
            m_result{ std::move(other.m_result) }
        {

        }

        ~result() = default;

        constexpr result& operator=(const result& other) noexcept 
        {
            m_result = other.m_result;
            return *this;
        }

        constexpr result& operator=(result&& other) noexcept
        {
            if (this == &other)
            {
                return *this;
            }

            m_result = std::move(other.m_result);
            return *this;
        }

        constexpr value_type* operator->()
        {
            return &get();
        }

        constexpr const value_type* operator->() const
        {
            return &get();
        }

        constexpr operator bool() const noexcept
        {
            return is_valid();
        }

        constexpr operator std::optional<value_type>() noexcept
        {
            return is_valid() ? std::optional<value_type>(get_unchecked()) : std::nullopt;
        }

        constexpr operator std::optional<value_type>() const noexcept
        {
            return is_valid() ? std::optional<value_type>(get_unchecked()) : std::nullopt;
        }

        constexpr bool is_valid() const noexcept
        {
            return std::holds_alternative<value_helper>(m_result);
        }

        constexpr ResultT& get()
        {
            if (!is_valid())
            {
#ifdef NO_EXCEPTIONS
                std::terminate();
#else 
                throw std::logic_error("Not a valid result.");
#endif
            }

            return get_unchecked();
        }

        constexpr const ResultT& get() const
        {
            if (!is_valid())
            {
#ifdef NO_EXCEPTIONS
                std::terminate();
#else 
                throw std::logic_error("Not a valid result.");
#endif
            }

            return get_unchecked();
        }

        constexpr error_type& error()
        {
            if (is_valid())
            {
#ifdef NO_EXCEPTIONS
                std::terminate();
#else 
                throw std::logic_error("Not an error.");
#endif
            }

            return error_unchecked();
        }

        constexpr const error_type& error() const
        {
            if (is_valid())
            {
#ifdef NO_EXCEPTIONS
                std::terminate();
#else 
                throw std::logic_error("Not an error.");
#endif
            }

            return error_unchecked();
        }

        void swap(result& other) noexcept
        {
            std::swap(m_result, other.m_result);
        }

    private:

        variant_type m_result;

        constexpr result() = delete;

        constexpr result(const variant_type& var) noexcept :
            m_result{ var }
        {

        }

        constexpr result(variant_type&& var) noexcept :
            m_result{ std::move(var) }
        {

        }

        constexpr value_type& get_unchecked() noexcept
        {
            return std::get<value_helper>(m_result).m_value;
        }

        constexpr const value_type& get_unchecked() const noexcept
        {
            return std::get<value_helper>(m_result).m_value;
        }

        constexpr error_type& error_unchecked() noexcept
        {
            return std::get<error_helper>(m_result).m_error;
        }

        constexpr const error_type& error_unchecked() const noexcept
        {
            return std::get<error_helper>(m_result).m_error;
        }
    };

    /**
     * @brief Template specialization for void value type.
     * 
     * @tparam ErrorT Type of the error.
     */
    template<typename ErrorT>
    class result<void, ErrorT>
    {
    public:

        using error_type = ErrorT;

        using optional_type = std::optional<ErrorT>;

        template<typename... ArgsT>
        constexpr static result success() noexcept
        {
            return result(std::nullopt);
        }

        template<typename... ArgsT>
        constexpr static result failure(ArgsT&&... args) noexcept
        {
            return result(optional_type(error_type(std::forward<ArgsT>(args)...)));
        }

        constexpr result(const result& other) :
            m_result{ other.m_result }
        {

        }

        constexpr result(result&& other) :
            m_result{ std::move(other.m_result) }
        {

        }

        ~result() = default;

        constexpr result& operator=(const result& other) noexcept 
        {
            m_result = other.m_result;
            return *this;
        }

        constexpr result& operator=(result&& other) noexcept
        {
            if (this == &other)
            {
                return *this;
            }

            m_result = std::move(other.m_result);
            return *this;
        }

        constexpr operator bool() const noexcept
        {
            return is_valid();
        }

        constexpr bool is_valid() const noexcept
        {
            return !m_result.has_value();
        }

        constexpr error_type& error()
        {
            if (is_valid())
            {
#ifdef NO_EXCEPTIONS
                std::terminate();
#else 
                throw std::logic_error("Not an error.");
#endif
            }

            return error_unchecked();
        }

        constexpr const error_type& error() const
        {
            if (is_valid())
            {
#ifdef NO_EXCEPTIONS
                std::terminate();
#else 
                throw std::logic_error("Not an error.");
#endif
            }

            return error_unchecked();
        }

        void swap(result& other) noexcept
        {
            std::swap(m_result, other.m_result);
        }

    private:

        optional_type m_result;

        constexpr result() = delete;

        constexpr result(const optional_type& opt) noexcept :
            m_result{ opt }
        {

        }

        constexpr result(optional_type&& opt) noexcept :
            m_result{ std::move(opt) }
        {

        }

        constexpr error_type& error_unchecked() noexcept
        {
            return m_result.value();
        }

        constexpr const error_type& error_unchecked() const noexcept
        {
            return m_result.value();
        }
    };

    template<typename FunT, typename ResultT, typename ErrorT>
    std::invoke_result_t<FunT, ResultT> bind(result<ResultT, ErrorT>&& value, FunT&& fun) noexcept
    {
        if (!value.is_valid())
        {
            return std::invoke_result_t<FunT, ResultT>::failure(std::move(value.error()));
        }

        return std::invoke(std::forward<FunT>(fun), std::move(value.get()));
    }

    template<typename FunT, typename ResultT, typename ErrorT>
    std::invoke_result_t<FunT, ResultT> bind(const result<ResultT, ErrorT>& value, FunT&& fun) noexcept
    {
        if (!value.is_valid())
        {
            return std::invoke_result_t<FunT, ResultT>::failure(value.error());
        }

        return std::invoke(std::forward<FunT>(fun), value.get());
    }

#ifndef NO_EXCEPTIONS

    template<typename ResultT>
    using result_throwing = result<ResultT, std::exception_ptr>;

    template<typename FunT>
    auto invoke(FunT&& fun) noexcept
    {
        using result_type = result_throwing<std::invoke_result_t<FunT>>;

        try
        {
            return result_type::success(std::invoke(std::forward<FunT>(fun)));
        }
        catch (...)
        {
            return result_type::failure(std::current_exception());
        }

    }

#endif
}

#endif