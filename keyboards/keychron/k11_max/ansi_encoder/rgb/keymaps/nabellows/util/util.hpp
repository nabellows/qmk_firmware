#pragma once

#include <assert.h>
#include "compat.hpp"
#include "common.hpp"
#include "iterate_view.hpp"
#include "mapping_iterator.hpp"
#include "type_list.hpp"
#include "value_iterator.hpp"

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <functional>
#include <iterator>
#include <optional>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <variant>

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

struct Void{
    constexpr Void(auto&&...){}
};
constexpr inline Void kVoid;

template<class T>
struct ValWrapper {
    T val;
};

template<class T>
struct Construct {
    template<class U>
    constexpr T operator()(U&& value) const
        noexcept(std::is_nothrow_constructible_v<T, U>)
    {
        return T(FWD(value));
    }
};
template<class T>
inline constexpr Construct<T> fConstruct;

template<class T>
constexpr bool is_val_wrapper_v = false;
template<class T>
constexpr bool is_val_wrapper_v<ValWrapper<T>> = true;

template<class T>
requires is_val_wrapper_v<std::remove_cvref_t<T>>
constexpr auto&& unwrap(T&& val) { return FWD(val.val); }
constexpr auto&& unwrap(auto&& val) { return FWD(val); }

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

template<class F, class...A>
constexpr decltype(auto) voidless_invoke(F&& f, A&&...args) {
    if constexpr (std::is_same_v<void, std::invoke_result_t<F&&, A...>>) {
        FWD(f)(FWD(args)...);
        return kVoid;
    } else {
        return FWD(f)(FWD(args)...);
    }
}

template<auto...vals>
constexpr decltype(auto) tinvoke_nttp(auto&& f, auto&&...args) requires requires { FWD(f).template operator()<vals...>(FWD(args)...); } {
    return voidless_invoke([&f, &args...]{ return FWD(f).template operator()<vals...>(FWD(args)...); });
}

template<class... Ts>
concept AllSame =
    sizeof...(Ts) == 0 ||
    (std::same_as<std::tuple_element_t<0, std::tuple<Ts...>>, Ts> && ...);

template<auto...vals, class Proj = std::identity>
constexpr auto ce_for_each_val(auto F, Proj proj = {}) {
    struct LoopState {
        void Break() { m_break = true; }
    private:
        bool m_break = false;
        int i = 0;
    } state;
    auto invoke = [&]<auto val>() {
        if constexpr (requires { tinvoke_nttp<val>(F, state); }) {
            return proj(tinvoke_nttp<val>(F, state));
        } else {
            return proj(tinvoke_nttp<val>(F));
        }
    };
    if constexpr (AllSame<decltype(tinvoke_nttp<vals>(invoke))...>) {
        return std::array{ tinvoke_nttp<vals>(invoke)... };
    } else {
        return std::tuple{ tinvoke_nttp<vals>(invoke)... };
    }
}

namespace detail {

template <
    class Storage,
    auto move,
    bool copyable = true>
struct MoveWrapperImpl : Storage {
    constexpr MoveWrapperImpl(auto&&...args) : Storage{ FWD(args)... }{}

    constexpr MoveWrapperImpl(MoveWrapperImpl const&) requires copyable = default;
    constexpr MoveWrapperImpl& operator=(MoveWrapperImpl const& other) requires copyable = default;

    constexpr MoveWrapperImpl(MoveWrapperImpl&& other) : Storage{ move(unwrap(other)) }{}
    constexpr MoveWrapperImpl& operator=(MoveWrapperImpl&& other) { static_cast<Storage&>(*this) = move(unwrap(other)); }
};

} // detail
template<
    class T,
    auto move, // Expect an actually modifying function, otherwise this class kinda useless //  = [](auto &&other) { std::move(other); },
    bool copyable = false,
    class Storage = std::conditional_t<std::is_class_v<T> && !std::is_final_v<T>,
        T,
        ValWrapper<T>
    >
>
using MoveWrapper = detail::MoveWrapperImpl<Storage, move, copyable>;

template<class Derived, bool copyable = false, bool destruct = true>
struct CrtpMoveWrapper {
    constexpr CrtpMoveWrapper() = default;

    constexpr CrtpMoveWrapper(CrtpMoveWrapper const&) requires copyable = default;
    constexpr CrtpMoveWrapper& operator=(CrtpMoveWrapper const& other) requires copyable = default;

    // 'move_out' must actually be mutating otherwise it is completely useless. Also made it a member function so can access privates
    // Base class presumably already handled default move of actual members, do not cause infinite recursion
    constexpr CrtpMoveWrapper(CrtpMoveWrapper&& other) { ((Derived&&) other).move_out(); }
    constexpr CrtpMoveWrapper& operator=(CrtpMoveWrapper&& other) { ((Derived&&) other).move_out(); }
};

template<class T, class It>
constexpr auto map_iterator(It&& it) {
    return MappingIterator<T, std::remove_cvref_t<It>>{ it };
}

template<class To, class From>
constexpr auto map_val_iterator(From&& val) {
    return MappingIterator<To, ValueIterator<std::remove_cvref_t<From>>>{ val };
}

template<class...Ts>
using DedupVariant = TypeList<Ts...>::template uniq<>::template apply_to<std::variant>;

static_assert(std::same_as<std::variant<int, double, short>, DedupVariant<int, double, int, short, double, short, int>>);

template<class...Ts>
constexpr auto to_variant_array(std::tuple<Ts...> const& tup) {
    std::tuple_element<0, std::tuple<int>>::type a;
    constexpr sz N = sizeof...(Ts);
    return invoke_with_index_seq<N>([&]<sz...is>{
        return std::array<DedupVariant<Ts...>, N>{ std::get<is>(tup)... };
    });
}

template<class T>
constexpr auto repeat_view(T&& val) {
    return iterate_view{
        std::remove_cvref_t<T>(FWD(val)),
        [](auto&){}
    };
    // return std::views::iota(0) | std::views::transform([val_copy = FWD(val)](int){ return val_copy; });
}

//TODO: dope pattern that would have helped in layer_util (though compile times...), would have been a view which can handle elems of Variant<Range1, Range2> etc, which all are
// ranges returning T, and then spawn a new view which is basically concat_view_n, unvariant-ing them


