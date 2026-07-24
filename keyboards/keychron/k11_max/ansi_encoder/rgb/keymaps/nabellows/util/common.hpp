#pragma once
#define restrict

#include <cstddef>
#include <type_traits>

#define FWD(...) ::std::forward<decltype(__VA_ARGS__)>((__VA_ARGS__))

#define DEDUCE_BODY(...) noexcept(noexcept(__VA_ARGS__)) requires (requires { (__VA_ARGS__); }) { return (__VA_ARGS__); }

using sz = size_t;

template<auto V>
struct NTTP {
    using T = std::remove_cvref_t<decltype(V)>;
    static constexpr T val = V;
    constexpr operator T() const { return val; }
    constexpr static T get() { return val; }
};

template<sz S>
struct Sz : std::integral_constant<sz, S> {};
