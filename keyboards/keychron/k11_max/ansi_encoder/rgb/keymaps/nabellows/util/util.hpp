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
#include <span>
#include <tuple>
#include <type_traits>
#include <variant>

template<sz N>
[[gnu::always_inline]]
constexpr decltype(auto) invoke_with_index_seq(auto f){
    return [&]<sz...is>(std::index_sequence<is...>)->decltype(auto){
        return f.template operator()<is...>();
    }(std::make_index_sequence<N>{});
}

template<sz N, class Agg = decltype([](auto&&...){})>
[[gnu::always_inline]]
constexpr decltype(auto) invoke_with_each_index(auto f, Agg agg = {}){
    return invoke_with_index_seq<N>([&]<sz...is>(){
        return agg(f.template operator()<is>()...);
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

template<class T, sz rows, sz cols>
using Matrix = std::array<std::array<T, cols>, rows>;

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

template<class T, sz N>
struct remove_extents<std::array<T, N>&> {
    using type = remove_extents_t<T>;
};

template<class T>
using span_t = decltype(std::span{std::declval<T&>()});

template<class T>
concept FixedSpan = requires(T& value) {
    std::span{value};
    requires span_t<T>::extent != std::dynamic_extent;
};

template<class S, class D>
requires std::is_assignable_v<D&, S>
constexpr void ce_copy(S const& src, D& dst) {
    dst = src;
}

template<FixedSpan S, FixedSpan D>
requires(not std::is_assignable_v<D&, S>
    && span_t<S>::extent == span_t<D>::extent
    && std::is_assignable_v<remove_extents_t<D>&, remove_extents_t<S>>)
constexpr void ce_copy(S const &src, D &dst) {
    if constexpr (FixedSpan<std::remove_cvref_t<S>>) {
        auto src_span = std::span{src};
        auto dst_span = std::span{dst};
        static_assert(src_span.extent == dst_span.extent);
        for (sz i = 0; i < src_span.extent; ++i) ce_copy(src_span[i], dst_span[i]);
    } else {
        dst = src;
    }
}

static_assert(std::same_as<remove_extents_t<Matrix<int, 2, 3>>, int>);
static_assert(std::is_assignable_v<remove_extents_t<Matrix<int, 3, 5>>&, remove_extents_t<int[3][5]>>);

template<class Arr, class V>
requires FixedSpan<std::remove_cvref_t<Arr>>
    && std::is_assignable_v<remove_extents_t<std::remove_cvref_t<Arr>>&, V const&>
constexpr void ce_fill(V const& val, Arr& dst) {
    for (auto& element : dst) {
        if constexpr (std::is_assignable_v<decltype(element), V const&>) {
            element = val;
        } else {
            ce_fill(val, element);
        }
    }
}

template<sz N>
using md_sz = std::conditional_t<N == 1, sz, std::array<sz, N>>;
template<sz N>
using md_index = md_sz<N>;

namespace detail {

template<class T>
constexpr sz array_rank = [] {
    using Arr = std::remove_cvref_t<T>;
    if constexpr (FixedSpan<Arr>) {
        return sz{1} + array_rank<typename span_t<Arr>::element_type>;
    } else {
        return sz{0};
    }
}();

template<sz Rank>
constexpr sz& index_at(md_sz<Rank>& indices, sz depth) {
    if constexpr (Rank == 1) return indices;
    else return indices[depth];
}

template<class Arr, class V, sz Rank>
constexpr bool index_of_impl(
    Arr const& arr,
    V const& value,
    md_sz<Rank>& indices,
    sz depth
) {
    auto span = std::span{arr};
    for (auto& i = index_at<Rank>(indices, depth); i < span.extent; ++i) {
        using Element = std::remove_cvref_t<decltype(span[i])>;
        if constexpr (array_rank<Element> != 0) {
            if (index_of_impl<decltype(span[i]), V, Rank>(span[i], value, indices, depth + 1)) return true;
        } else {
            if (span[i] == value) return true;
        }
    }
    index_at<Rank>(indices, depth) = 0;
    return false;
}

template<class T>
struct IndexType;

template<FixedSpan T>
struct IndexType<T> {
    using type = md_sz<array_rank<T>>;
};

} // namespace detail

//TODO: technically we want one with operator++ and a way to apply(array)
template<class T>
using index_t = detail::IndexType<std::remove_cvref_t<T>>::type;

template<class Arr, class V>
requires FixedSpan<std::remove_cvref_t<Arr>>
constexpr bool index_of(Arr const& arr, V const& value, index_t<Arr>& indices) {
    return detail::index_of_impl<Arr, V, detail::array_rank<std::remove_cvref_t<Arr>>>(arr, value, indices, 0);
}

template<class Arr, class V>
requires FixedSpan<std::remove_cvref_t<Arr>>
constexpr std::optional<index_t<Arr>> index_of(Arr const& arr, V const& value) {
    index_t<Arr> indices{};
    if (index_of(arr, value, indices)) {
        return indices;
    }
    return std::nullopt;
}

namespace util_test {
constexpr auto res = []{
    int arr[3][3] = { { 1, 2, 3 }, { 1, 4, 3 }, { 3, 3, 9 } };
    md_index<2> index{};
    std::array<md_index<2>, sizeof(arr)/sizeof(int)> res{};
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

template<class...Ts>
constexpr decltype(auto) tinvoke_t(auto&& f, auto&&...args) requires requires { FWD(f).template operator()<Ts...>(FWD(args)...); } {
    return FWD(f).template operator()<Ts...>(FWD(args)...);
}

template<auto...vals>
constexpr decltype(auto) tinvoke_nttp(auto&& f, auto&&...args) requires requires { FWD(f).template operator()<vals...>(FWD(args)...); } {
    return FWD(f).template operator()<vals...>(FWD(args)...);
}

template<auto&...vals>
constexpr decltype(auto) tinvoke_nttp_ref(auto&& f, auto&&...args) requires requires { FWD(f).template operator()<vals...>(FWD(args)...); } {
    return FWD(f).template operator()<vals...>(FWD(args)...);
}

template<class Enum>
constexpr inline bool kIsPackedEnum = requires { Enum::ENUM_END; };

template<class Enum>
constexpr inline bool kEnumLen = sz(Enum::ENUM_END);

template<class Enum>
requires (kIsPackedEnum<Enum> && requires{ kEnumLen<Enum>; })
constexpr decltype(auto) unpack_enum(auto&& f) {
    return invoke_with_index_seq<kEnumLen<Enum>>([&]<sz...is>() -> decltype(auto) {
        return tinvoke_nttp<Enum(is)...>(f);
    });
}

template<class... Ts>
concept AllSame =
    sizeof...(Ts) == 0 ||
    (std::same_as<std::tuple_element_t<0, std::tuple<Ts...>>, Ts> && ...);

template<auto...vals, class Proj = std::identity>
constexpr auto ce_for_each_val(auto F, Proj proj = {}) {
    struct LoopState {
        void Break() { m_break = true; }
        bool m_break = false;
        int i = -1;
    } state;
    auto invoke = [&]<auto val>() -> decltype(auto) {
        if constexpr (requires { tinvoke_nttp<val>(F, state); }) {
            ++state.i;
            auto voidless = [&]->decltype(auto){ return voidless_invoke([&]{ return tinvoke_nttp<val>(F, state); }); };
            using R = decltype(proj(voidless()));
            if (state.m_break) return R{}; // For now, return type has to be default constructible, if you wanna use break, consider std::optional projection
            return proj(voidless());
        } else {
            auto voidless = [&]->decltype(auto){ return voidless_invoke([&]{ return tinvoke_nttp<val>(F); }); };
            return proj(voidless());
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
    return iterate_view{ std::remove_cvref_t<T>(FWD(val)), [](auto&){} };
}

template<class R>
concept InfiniteRange = std::ranges::range<R> && requires (R r) {
    { std::ranges::end(r) } -> std::same_as<std::unreachable_sentinel_t>;
};
static_assert(InfiniteRange<std::ranges::iota_view<int>>);
static_assert(InfiniteRange<decltype(repeat_view(1))>);
static_assert(!InfiniteRange<std::array<int, 10>>);

template<std::ranges::view V>
class infinite_view : public std::ranges::view_interface<infinite_view<V>> {
    V base_;
public:
    constexpr infinite_view() requires std::default_initializable<V> = default;
    constexpr explicit infinite_view(V base) : base_(std::move(base)) {}

    constexpr auto begin() { return std::ranges::begin(base_); }
    constexpr auto begin() const requires std::ranges::range<const V> { return std::ranges::begin(base_); }

    constexpr std::unreachable_sentinel_t end() const noexcept { return std::unreachable_sentinel; }
    static_assert(InfiniteRange<infinite_view<V>>);
};

template<std::ranges::viewable_range R>
infinite_view(R&&) -> infinite_view<std::views::all_t<R>>;

template<std::ranges::viewable_range R>
constexpr auto infinite_range(R&& r)
{
    return infinite_view<std::views::all_t<R>>{
        std::views::all(FWD(r))
    };
}

//TODO: dope pattern that would have helped in layer_util (though compile times...), would have been a view which can handle elems of Variant<Range1, Range2> etc, which all are
// ranges returning T, and then spawn a new view which is basically concat_view_n, unvariant-ing them
