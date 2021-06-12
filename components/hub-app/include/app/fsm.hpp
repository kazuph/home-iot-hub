#ifndef HUB_APP_FSM_HPP
#define HUB_APP_FSM_HPP

#include <type_traits>
#include <variant>
#include <cassert>

namespace hub
{
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
        void set_state(ArgsT&&... args) noexcept
        {
            static_assert(is_state_type_v<StateT>, "Not a valid state type.");
            static_assert(std::is_constructible_v<StateT, ArgsT...>, "State is not constructible with supplied arguments.");
            m_state = StateT(std::forward<ArgsT>(args)...);
        }

        template<typename StateT>
        StateT& get_state() noexcept
        {
            static_assert(is_state_type_v<StateT>, "Not a valid state type.");
            assert(is_state<StateT>());
            return std::get<StateT>(m_state);
        }

    private:

        state_t m_state;
    };
}

#endif