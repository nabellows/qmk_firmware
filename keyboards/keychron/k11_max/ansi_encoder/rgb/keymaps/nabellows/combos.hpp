#pragma once

#include <array>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "common.hpp"
#include "compat.hpp"
#include "key_util.hpp"
#include "keys.hpp"
#include "type_list.hpp"
#include "util.hpp"

extern "C" {
#include "process_combo.h"
}

enum class Combo {
    BOTH_SHIFT,
    COMBO_NAME_ENUM_END,
};
constexpr static sz kNumCombosRaw = sz(Combo::COMBO_NAME_ENUM_END);

template<Combo combo>
struct ComboDef {
    static_assert(VFalse<>, "Missing definition for combo");
};
template<Combo combo>
constexpr inline ComboDef<combo> kComboDef;

struct ComboBase {
    constexpr ComboBase(){}
    // TODO: kinda lame pattern, could add cooler versions but we literally have no need rn
    // Actually yeah why tf did I write such machinery for filtering and all... you can just re-order after ENUM_END like we do for layers
    bool enabled = true;
    int combo_term = COMBO_TERM;
    qmk_key_t action;
    bool (*should_trigger)() = nullptr;
};

constexpr inline auto kComboBaseDefaults = []{
    ComboBase res;
    res.action = KC_NO;
    return res;
}();

template<Key...ks>
struct ComboBaseWithKeys : ComboBase {
    constexpr static qmk_key_t keys[] = { ks.to_qmk()..., COMBO_END };
};

#define DEF_COMBO(name_, keys, ...) template<> struct ComboDef<Combo::name_> : ComboBaseWithKeys<UNPAREN keys> { static constexpr Combo name = Combo::name_; consteval ComboDef() __VA_ARGS__ };

//-----------------------------------------------------------------------------
// DEFINITIONS
//-----------------------------------------------------------------------------

DEF_COMBO(BOTH_SHIFT, (KC_LSFT, KC_RSFT), {
    action = T_CAPS_WORD;
    combo_term = UINT16_MAX;
    enabled = true;
});

//-----------------------------------------------------------------------------
// DETAIL / QMK Integ
//-----------------------------------------------------------------------------

constexpr decltype(auto) unpack_combos_raw(auto f) {
    return invoke_with_index_seq<kNumCombosRaw>([&]<sz...is>() -> decltype(auto) {
        return tinvoke_nttp<Combo{ is }...>(f);
    });
}

constexpr decltype(auto) unpack_combo_defs_raw(auto f) {
    return unpack_combos_raw([&]<Combo...combos>() -> decltype(auto) {
        return tinvoke_nttp_ref<kComboDef<combos>...>(f);
    });
}

constexpr static sz kNumCombos = unpack_combo_defs_raw([]<auto...defs>{
    return (int(defs.enabled) + ... + 0);
});

constexpr decltype(auto) unpack_combo_defs(auto f) {
    return unpack_combo_defs_raw([&]<auto&...defs>{
        using FilteredList = ValTypeList<&defs...>::template filter_t<[]<class C>{ return C::val->enabled; }>;
        return FilteredList::apply_t([&]<class...NTTPs>() -> decltype(auto) {
            return tinvoke_nttp_ref<*NTTPs::val...>(f);
        });
    });
}

constexpr decltype(auto) unpack_combos(auto f) {
    return unpack_combo_defs([&]<auto&...defs>() -> decltype(auto) {
        return tinvoke_nttp_ref<defs.name...>(f);
    });
}

constexpr auto kComboDefs = unpack_combo_defs([]<auto&...defs>{
    return std::tuple<decltype(defs)...>{ defs... };
});

template<class Visitor>
using ComboVisitResult = decltype(unpack_combo_defs([]<auto&...defs>(){
    using R = std::conditional_t<(kNumCombos > 0),
        std::common_type<decltype(tinvoke_nttp_ref<defs>(std::declval<Visitor>()))...>,
        std::type_identity<decltype(tinvoke_nttp_ref<kComboBaseDefaults>(std::declval<Visitor>()))>
    >;
    return R{};
}))::type;

template<sz I = 0, class Vis>
[[gnu::always_inline]]
constexpr ComboVisitResult<Vis> visit_combo(uint16_t combo_index, Vis visitor) {
    const Combo combo_name{ combo_index };
    if constexpr (I < kNumCombos) {
        if (I == combo_index) {
            return tinvoke_nttp_ref<get<I>(kComboDefs)>(visitor);
        } else {
            return visit_combo<I + 1>(combo_index, visitor);
        }
    } else {
        constexpr_fail("Invalid combo index!");
        if constexpr (kNumCombos == 0) { // I mean.... lol
            return tinvoke_nttp_ref<kComboBaseDefaults>(visitor);
        } else {
            // Hopefully impossible with runtime indexing!!!!
            __builtin_unreachable();
        }
    }
}

template<auto member>
constexpr decltype(auto) combo_field(uint16_t combo_index) {
    return visit_combo(combo_index, []<auto& def>() -> decltype(auto){ return def.*member; });
}

//TODO: add extra introspection to enforce size and stuff (like make sure COMBO_BUFFER_LEN >= len and all that)

// Could implement runtime->constexpr lookup table but probably not worth it
// Per-combo timeouts
QMK_INLINE uint16_t get_combo_term(uint16_t combo_index, combo_t *combo) {
    return combo_field<&ComboBase::combo_term>(combo_index);
}

QMK_INLINE bool combo_should_trigger(uint16_t combo_index, combo_t *combo, uint16_t keycode, keyrecord_t *record) {
    return visit_combo(combo_index, []<auto& def>{
        if constexpr (def.should_trigger != nullptr) {
            return def.should_trigger();
        } else {
            return true;
        }
    });
}

constexpr inline auto kQmkComboArray = unpack_combo_defs([]<auto&...defs>{
    return std::array<combo_t, kNumCombos> {{ COMBO(defs.keys, defs.action)... }};
});

