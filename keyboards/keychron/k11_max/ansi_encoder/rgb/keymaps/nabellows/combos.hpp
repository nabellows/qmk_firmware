#pragma once

#include <array>
#include <utility>
#include "compat.hpp"
#include "key_util.hpp"
#include "keys.hpp"
#include "util.hpp"

extern "C" {
#include "process_combo.h"
}

enum class Combo {
    BOTH_SHIFT,

    COMBO_NAME_ENUM_END,
};
constexpr static sz kNumCombos = sz(Combo::COMBO_NAME_ENUM_END);

template<Combo combo>
struct ComboDef;

template<Key act, Key...ks>
struct ComboBase {
    constexpr static qmk_key_t keys[] = { ks.to_qmk()..., COMBO_END };
    constexpr static qmk_key_t action = act.to_qmk();
};
#define DEF_COMBO(action, name, ...) template<> struct ComboDef<Combo::name> : ComboBase<action, __VA_ARGS__> {};

DEF_COMBO(T_CAPS_WORD, BOTH_SHIFT, KC_LSFT, KC_RSFT);

// Could implement runtime->constexpr lookup table but probably not worth it
// Per-combo timeouts
QMK_INLINE uint16_t get_combo_term(uint16_t combo_index, combo_t *combo) {
    using enum Combo;
    switch (combo_index) {
        case int(BOTH_SHIFT):
            return UINT16_MAX;
    }
    return COMBO_TERM;
}
