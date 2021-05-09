#ifndef RESULT_H
#define RESULT_H

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
        static result success(ArgsT&&... args) noexcept
        {
            return result(variant_type(value_helper{ value_type(std::forward<ArgsT>(args)...) }));
        }

        template<typename... ArgsT>
        static result failure(ArgsT&&... args) noexcept
        {
            return result(variant_type(error_helper{ error_type(std::forward<ArgsT>(args)...) }));
        }

        result(const result& other) :
            m_result{ other.m_result }
        {

        }

        result(result&& other) :
            m_result{ std::move(other.m_result) }
        {

        }

        ~result() = default;

        result& operator=(const result& other)
        {
            m_result = other.m_result;
            return *this;
        }

        result& operator=(result&& other)
        {
            if (this == &other)
            {
                return *this;
            }

            m_result = std::move(other.m_result);
            return *this;
        }

        value_type* operator->()
        {
            return &get();
        }

        const value_type* operator->() const
        {
            return &get();
        }

        operator bool() const noexcept
        {
            return is_valid();
        }

        operator std::optional<value_type>() noexcept
        {
            return is_valid() ? std::optional<value_type>(get_unchecked()) : std::nullopt;
        }

        operator std::optional<value_type>() const noexcept
        {
            return is_valid() ? std::optional<value_type>(get_unchecked()) : std::nullopt;
        }

        bool is_valid() const noexcept
        {
            return std::holds_alternative<value_helper>(m_result);
        }

        ResultT& get()
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

        const ResultT& get() const
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

        error_type& error()
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

        const error_type& error() const
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

        template<typename FunT>
        void visit(FunT fun)
        {
            if (is_valid())
            {
                std::invoke(fun, get_unchecked());
            }
            else
            {
                std::invoke(fun, error_unchecked());
            }
        }

        void swap(result& other) noexcept
        {
            std::swap(m_result, other.m_result);
        }

#ifndef NO_EXCEPTIONS
        std::enable_if_t<
            std::is_same_v<error_type, std::exception_ptr>,
            value_type> get_or_throw() const
        {
            if (is_valid())
            {
                return get_unchecked();
            }

            std::rethrow_exception(error_unchecked());
        }
#endif

    private:

        variant_type m_result;

        result() = default;

        result(const variant_type& var) :
            m_result{ var }
        {

        }

        result(variant_type&& var) :
            m_result{ std::move(var) }
        {

        }

        value_type& get_unchecked() noexcept
        {
            return std::get<value_helper>(m_result).m_value;
        }

        const value_type& get_unchecked() const noexcept
        {
            return std::get<value_helper>(m_result).m_value;
        }

        error_type& error_unchecked() noexcept
        {
            return std::get<error_helper>(m_result).m_error;
        }

        const error_type& error_unchecked() const noexcept
        {
            return std::get<error_helper>(m_result).m_error;
        }
    };

#ifndef NO_EXCEPTIONS

    template<typename ResultT>
    using result_throwing = result<ResultT, std::exception_ptr>;

    template<typename FunT, typename ResultT, typename ErrorT>
    decltype(auto) bind(result<ResultT, ErrorT> value, FunT fun) noexcept
    {
        using result_type = std::invoke_result_t<FunT, ResultT>;

        if (!value.is_valid())
        {
            return result_type::failure(value.error());
        }

        return std::invoke(fun, value.get());
    }

    template<typename FunT>
    decltype(auto) catch_as_result(FunT fun) noexcept
    {
        using result_type = result_throwing<std::invoke_result_t<FunT>>;

        try
        {
            return result_type::success(std::invoke(fun));
        }
        catch (...)
        {
            return result_type::failure(std::current_exception());
        }

    }

#endif
}

#endif