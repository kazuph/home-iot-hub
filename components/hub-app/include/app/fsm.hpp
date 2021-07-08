#ifndef HUB_APP_FSM_HPP
#define HUB_APP_FSM_HPP

#include <type_traits>
#include <variant>
#include <cassert>

#ifndef HUB_FSM_NOEXCEPT
    #ifndef NO_EXCEPTIONS
        #define HUB_FSM_NOEXCEPT
    #else
        #define HUB_FSM_NOEXCEPT noexcept
    #endif
#endif

namespace hub
{
    struct state_check {};

    struct no_state_check {};

    template<typename StateCheckT>
    inline constexpr bool is_state_check_type_v = std::is_same_v<StateCheckT, state_check> || std::is_same_v<StateCheckT, no_state_check>;

    template<typename... StatesT>
    class fsm
    {
    public:

        static_assert(std::conjunction_v<std::is_move_constructible<StatesT>...>, "States are not move constructible.");
        static_assert(std::conjunction_v<std::is_move_assignable<StatesT>...>, "States are not move assignable.");

        using state_t = std::variant<StatesT...>;

        template<typename T>
        static inline constexpr bool is_state_type_v = std::disjunction_v<std::is_same<T, StatesT>...>;

        fsm()                       = default;

        fsm(const fsm&)             = default;

        fsm(fsm&&)                  = default;

        fsm& operator=(const fsm&)  = default;

        fsm& operator=(fsm&&)       = default;

        ~fsm()                      = default;

        template<typename StateT>
        bool is_state() const noexcept
        {
            static_assert(is_state_type_v<StateT>, "Not a valid state type.");
            return std::holds_alternative<StateT>(m_state);
        }

        template<typename StateT, typename... ArgsT>
        StateT& set_state(ArgsT&&... args) noexcept
        {
            static_assert(is_state_type_v<StateT>, "Not a valid state type.");
            static_assert(std::is_constructible_v<StateT, ArgsT...>, "State is not constructible with supplied arguments.");
            m_state = StateT(std::forward<ArgsT>(args)...);
            return std::get<StateT>(m_state);
        }

        template<typename StateT, typename StateCheckT = state_check>
        StateT& get_state() HUB_FSM_NOEXCEPT
        {
            static_assert(is_state_type_v<StateT>, "Not a valid state type.");
            static_assert(is_state_check_type_v<StateCheckT>, "Available options for state check are: state_check and no_state_check.");

            if constexpr (std::is_same_v<StateCheckT, state_check>)
            {
                throw_on_invalid_state<StateT>();
            }

            return std::get<StateT>(m_state);
        }

    private:

        state_t m_state;

        template<typename StateT>
        void throw_on_invalid_state() HUB_FSM_NOEXCEPT
        {
            static_assert(is_state_type_v<StateT>, "Not a valid state type.");
            if (!is_state<StateT>())
            {
#ifndef NO_EXCEPTIONS
                throw std::logic_error("Invalid state.");
#else
                abort();
#endif
            } 
        }
    };
}

#endif