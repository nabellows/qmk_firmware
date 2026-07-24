#pragma once

#include "common.hpp"

#include <functional>
#include <iterator>
#include <ranges>
#include <type_traits>

template<class State, class Update>
class iterate_view
    : public std::ranges::view_interface<iterate_view<State, Update>>
{
    State initial_;
    [[no_unique_address]] Update update_;

    constexpr static bool is_finite = requires (State s) { !std::invoke(*update_, s); };

    struct finite_sentinel { };

    // To make it externaly detectible
    using sentinel = std::conditional_t<is_finite, finite_sentinel, std::unreachable_sentinel_t>;

    class iterator {
        State state_;
        Update* update_;
        bool end = false; // Unfortunately using update_ and setting to nullptr gives me issues with constexpr for some reason

    public:
        using value_type = State;
        using difference_type = std::ptrdiff_t;
        using iterator_concept = std::input_iterator_tag;

        constexpr iterator(State state, Update& update)
            : state_(std::move(state)), update_(&update) {}

        constexpr State const& operator*() const noexcept {
            return state_;
        }

        constexpr iterator& operator++() {
            if constexpr (is_finite) {
                if (!std::invoke(*update_, state_)) {
                    end = true;
                }
            } else {
                std::invoke(*update_, state_);
            }
            return *this;
        }

        constexpr void operator++(int) {
            ++*this;
        }

        constexpr bool operator==(sentinel) const {
            return end;
        }
    };

public:
    constexpr iterate_view(State initial, Update update)
        : initial_(std::move(initial)),
          update_(std::move(update)) {}

    constexpr auto begin() {
        return iterator{initial_, update_};
    }

    constexpr auto end() const noexcept {
        return sentinel{};
    }
};

template<class State, class Update>
iterate_view(State&&, Update) -> iterate_view<std::remove_cvref_t<State>, Update>;
