#pragma once

#include "compat.hpp"

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <optional>
#include <type_traits>

#define FWD(...) ::std::forward<decltype(__VA_ARGS__)>((__VA_ARGS__))

using sz = size_t;

template<sz N>
constexpr decltype(auto) invoke_with_index_seq(auto f){
    return [&]<sz...is>(std::index_sequence<is...>)->decltype(auto){
        return f.template operator()<is...>();
    }(std::make_index_sequence<N>{});
}

template<sz N>
constexpr decltype(auto) invoke_with_each_index(auto f){
    return invoke_with_index_seq<N>([&]<sz...is>(){
        return (f.template operator()<is>(), ...);
    });
}

struct Void{};

template<class=void>
constexpr bool TFalse = false;

template<auto=0>
constexpr bool VFalse = false;

template<class T>
struct remove_extents {
    using type = std::remove_all_extents_t<T>;
};

template<class T>
using remove_extents_t = remove_extents<T>::type;

template<class T, sz N>
struct remove_extents<std::array<T, N>> {
    using type = remove_extents_t<T>;
};

template<class S, class D>
requires std::is_assignable_v<D&, S>
constexpr void ce_copy(S const& src, D& dst) {
    dst = src;
}

template<class S, class D, sz N>
requires std::is_assignable_v<remove_extents_t<D>&, remove_extents_t<S>>
constexpr void ce_copy(S const (&src)[N], D (&dst)[N]) {
    for (sz i = 0; i < N; ++i) {
        ce_copy(src[i], dst[i]);
    }
}

static_assert(std::same_as<remove_extents_t<std::array<std::array<int, 3>, 2>>, int>);

namespace detail {
template<sz N, class V>
constexpr void ce_fill_(V const& val, auto& dst);
}

template<class T, sz N, class V>
requires std::is_assignable_v<remove_extents_t<T>&, V const&>
constexpr void ce_fill(V const& val, T (&dst)[N]) { detail::ce_fill_<N>(val, dst); }

template<class T, sz N, class V>
requires std::is_assignable_v<remove_extents_t<T>&, V const&>
constexpr void ce_fill(V const& val, std::array<T, N>& dst) { detail::ce_fill_<N>(val, dst); }

namespace detail {
template<sz N, class V>
constexpr void ce_fill_(V const& val, auto& dst) {
    for (sz i = 0; i < N; ++i) {
        if constexpr (std::is_assignable_v<decltype(dst[i]), V const&>) {
            dst[i] = val;
        } else {
            ::ce_fill(val, dst[i]);
        }
    }
}
}


template<sz N>
using md_sz = std::conditional_t<N == 1, sz, std::array<sz, N>>;
template<sz N>
using md_index = md_sz<N>;

namespace detail {

template<class Arr, class V, sz Rank>
constexpr bool index_of_impl(
    Arr const& arr,
    V const& value,
    std::array<sz, Rank>& indices,
    sz depth
) {
    for (auto& i = indices[depth]; i < std::extent_v<Arr>; ++i) {
        using Element = std::remove_reference_t<decltype(arr[i])>;
        if constexpr (std::is_array_v<Element>) {
            if (index_of_impl(arr[i], value, indices, depth + 1)) return true;
        } else {
            if (arr[i] == value) return true;
        }
    }
    indices[depth] = 0;
    return false;
}

template<class T>
struct IndexType;

template<class T, sz N>
struct IndexType<T[N]> {
    using type = md_sz<std::rank_v<T[N]>>;
};

} // namespace detail

//TODO: technically we want one with operator++ and a way to apply(array)
template<class T>
using index_t = detail::IndexType<T>::type;

template<class T, sz N, class V>
constexpr bool index_of(T const (&arr)[N], V const& value, index_t<T[N]>& indices) {
    return detail::index_of_impl(arr, value, indices, 0);
}

template<class T, sz N, class V>
constexpr std::optional<index_t<T[N]>> index_of(T const (&arr)[N], V const& value) {
    index_t<T[N]> indices{};
    if (index_of(arr, value, indices)) {
        return indices;
    }
    return std::nullopt;
}

namespace util_test {
constexpr auto res = []{
    int arr[3][3] = { { 1, 2, 3 }, { 1, 4, 3 }, { 3, 3, 9 } };
    md_index<2> index{};
    std::array<md_index<2>, 9> res{};
    int i = 0;
    while (index_of(arr, 3, index)) {
        res[i++] = index;
        ++index.back();
    }
    return res;
}();
static_assert(res == std::array<md_index<2>, 9>{{ {0,2}, {1,2}, {2,0}, {2,1} }});
}

// Individual iterations return true to short circuit (final return value is if any return true)
template<auto...vals>
constexpr bool ce_for_each_val(auto F) {
    auto invoke_as_bool_r = [&]<auto val>()->decltype(auto){
        if constexpr (std::is_same_v<void, decltype(F.template operator()<val>())>) {
            F.template operator()<val>();
            return false;
        } else {
            return F.template operator()<val>();
        }
    };
    return (invoke_as_bool_r.template operator()<vals>() || ...);
}
