#pragma once

#include <type_traits>
#include <utility>

#include "common.hpp"

template<class...Ts>
struct TypeList;

#define HAS_PACK_ELEM_BUILTIN 0
#if defined (__has_builtin)
#if __has_builtin(__type_pack_element)
#define HAS_PACK_ELEM_BUILTIN 1
#endif
#endif

namespace detail {

#if HAS_PACK_ELEM_BUILTIN
template<sz I, class...Ts>
using pack_element_impl = __type_pack_element<I, Ts...>;
#else
template<sz I, class T, class...Ts>
constexpr auto pack_get_() {
    static_assert(sizeof...(Ts) > 0, "Cannot call get<>() on empty tuple/type-list type"); // Should always have 'void' sentinel in Ts...
    if constexpr (I == 0) return std::declval<T>();
    else return pack_get_<I-1, Ts...>();
}
template<sz I, class...Ts>
using pack_element_impl = decltype(pack_get_<I, Ts..., void>());
#endif
template<template<class...>class TL, class...Ts>
constexpr auto type_list_from_(TL<Ts...> const&) {
    return TypeList<Ts...>{};
}
template<template<auto...>class TL, auto...vals>
constexpr auto type_list_from_(TL<vals...> const&) {
    return TypeList<NTTP<vals>...>{};
}
template<template<class T, T...>class TL, class T, T...vals>
constexpr auto type_list_from_(TL<T, vals...> const&) {
    return TypeList<NTTP<vals>...>{};
}
template<class...Ts> struct ToTypeList_{ using type = TypeList<Ts...>; };
template<class...Ts> struct ToTypeList_<TypeList<Ts...>>{ using type = TypeList<Ts...>; };
} // detail

template<sz I, class...Ts>
using pack_element_t = detail::pack_element_impl<I, Ts...>;

template<class TL>
using TypeListFrom = decltype(detail::type_list_from_(std::declval<TL>()));

constexpr inline auto lMakeTypeList = []<class...Ts>(){ return TypeList<Ts...>{}; };

template<class...Ts>
using ToTypeList = detail::ToTypeList_<Ts...>::type;

template<auto...vals>
using ValTypeList = TypeList<NTTP<vals>...>;

template<sz N, auto GenF = []<sz I>(){ return Sz<I>{}; }>
using GenTypeList = TypeListFrom<std::make_index_sequence<N>>::template transform_i<GenF>;

template<class...Ts>
struct TypeList {
    template<sz I>
    using at = pack_element_t<I, Ts...>;
    constexpr static auto size = sizeof...(Ts);

    template<template<class...>class Tup>
    using apply_to = Tup<Ts...>;

    template<template<class> class Pred>
    constexpr static decltype(auto) any() { return (Pred<Ts>::value || ...); }
    template<template<class> class Pred>
    constexpr static decltype(auto) all() { return (Pred<Ts>::value && ...); }
    template<template<class> class Pred>
    constexpr static decltype(auto) none() { return !any<Pred>(); }

    constexpr static decltype(auto) any(auto&& f) { return (FWD(f).template operator()<Ts>() || ...); }
    constexpr static decltype(auto) all(auto&& f) { return (FWD(f).template operator()<Ts>() && ...); }
    constexpr static decltype(auto) none(auto&& f) { return !any(FWD(f)); }

    template<class T, sz sub_begin = 0, sz N = size> // Try to reduce recursive template depth
    constexpr static bool contains = any([]<class U>{ return std::is_same_v<T, U>; });

    constexpr static decltype(auto) apply_t(auto&& f){
        return FWD(f).template operator()<Ts...>();
    }
    constexpr static decltype(auto) apply_each_t(auto&& f, auto&& acc){
        return FWD(acc)(FWD(f).template operator()<Ts>()...);
    }
    constexpr static decltype(auto) apply_each_t(auto&& f){
        return TypeList<decltype(FWD(f).template operator()<Ts>())...>{};
    }
    constexpr static decltype(auto) apply_i(auto&& f){
        return [&]<sz...is>()->decltype(auto){
            return FWD(f).template operator()<is...>();
        }(std::make_index_sequence<size>{});
    }
    constexpr static decltype(auto) apply_each_i(auto&& f){
        return [&]<sz...is>(std::index_sequence<is...>)->decltype(auto){
            return TypeList<decltype(FWD(f).template operator()<is>())...>{};
        }(std::make_index_sequence<size>{});
    }
    constexpr static decltype(auto) apply_each_i(auto&& f, auto&& acc){
        return [&]<sz...is>(std::index_sequence<is...>)->decltype(auto){
            return FWD(acc)(FWD(f).template operator()<is>()...);
        }(std::make_index_sequence<size>{});
    }
    constexpr static decltype(auto) apply_each_it(auto&& f){
        return apply_each_i([&]<sz i>(){ return FWD(f).template operator()<i, at<i>>(); });
    }
    constexpr static decltype(auto) apply_each_it(auto&& f, auto&& acc){
        return apply_each_i([&]<sz i>(){ return FWD(f).template operator()<i, at<i>>(); }, FWD(acc));
    }

    template<class...Us> using append = TypeList<Ts..., Us...>;
    template<class...Us> using prepend = TypeList<Us..., Ts...>;
    template<auto...vals> using append_val = append<NTTP<vals>...>;
    template<auto...vals> using prepend_val = prepend<NTTP<vals>...>;

    template<auto F>
    using filter_i = decltype([]<class Empty = TypeList<>>(){
        auto it = []<sz i>(auto it){
            if constexpr (i == size) return Empty{};
            else {
                using Tail = decltype(it.template operator()<i+1>(it));
                if constexpr (F.template operator()<i>()) return typename Tail::template prepend<at<i>>{};
                else return Tail{};
            }
        };
        return it.template operator()<0>(it);
    }());

    template<auto F>
    using filter_it = filter_i<[]<sz i>() { return F.template operator()<i, at<i>>(); }>;
    template<auto F>
    using filter_t = filter_i<[]<sz i>() { return F.template operator()<at<i>>(); }>;

    template<auto F>
    using transform_t = decltype(apply_each_t(F));
    template<auto F>
    using transform_i = decltype(apply_each_i(F));
    template<auto F>
    using transform_it = decltype(apply_each_it(F));

    template<class TL>
    using cat = TL::template apply_to<append>;

    template<sz I, class T>
    using set = transform_i<[]<sz i>(){ return std::declval<std::conditional_t<i==I, T, at<i>>>(); }>;

    template<sz I, auto val>
    using set_val = set<I, NTTP<val>>;

    template<sz I>
    using remove = filter_i<[]<sz i>{ return i != I; }>;

    template<int I, sz N=size>
    using sublist = filter_i<[]<sz i>{
        const sz begin = I < 0 ? I + size : I;
        const sz end = begin + N;
        return begin <= i && i < end;
    }>;

    template<class=void> // to make this lazier
    using uniq = filter_it<[]<sz i, class T>{
        if constexpr (size == 0) return true; // Avoid recursion
        else return !sublist<0, i>::template contains<T>;
    }>;
};

static_assert(std::same_as<TypeList<int, float, short, long, long, double>::sublist<2, 3>, TypeList<short, long, long>>);
static_assert(std::same_as<TypeList<int, float, short, long, long, double, int>::uniq<>, TypeList<int, float, short, long, double>>);
