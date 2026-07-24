#pragma once

#include "key_util.hpp"
#include "keycodes.h"
#include "modifiers.h"
#include "quantum_keycodes.h"

typedef enum {
    //TODO: make it still send KC_CTRL when pressed (like with mouse ctrl click)
    CTL_ESC = LCTL_T(KC_ESC),
    GUI_SPC = MT(MOD_LGUI, KC_SPC),
    T_CAPS_WORD = QK_CAPS_WORD_TOGGLE,
    RSPACE = KC_SPACE,
} kc_aliases_t;

typedef enum {
    LSPACE = SAFE_RANGE,
    FN1,
    FN2,
    KNOB_PRESS,
    KNOB_CCW,
    KNOB_CW,
} custom_kc_t;

namespace key_defs {

template<int N>
constexpr Key F = Key(qmk_key_t(KC_F1 + N - 1));
constexpr KeyList arrows_hjkl = { KC_LEFT, KC_DOWN, KC_UP, KC_RIGHT };
constexpr KeyList arrows_wasd = { KC_UP, KC_LEFT, KC_DOWN, KC_RIGHT };
constexpr KeyList mouse_hjkl = { MS_LEFT, MS_DOWN, MS_UP, MS_RGHT };
constexpr KeyList mouse_wasd = { MS_UP, MS_LEFT, MS_DOWN, MS_RGHT };

template<int first, int last, qmk_key_t qbegin, qmk_key_t qend>
constexpr auto norm_1_index_keys = []{
    constexpr auto first_key = qbegin + first-1;
    constexpr auto last_key = qbegin + last-1;
    static_assert(first <= last && first_key >= qbegin && last_key <= qend, "Given 1-indexed key indices do not fit within defined range");
    return kKeyRange<first_key, last_key>;
}();
template<int first = 1, int last = 24>
constexpr auto f_keys = norm_1_index_keys<first, last, KC_F1, KC_F24>;

//TODO: remove
#include <ranges>
static_assert(std::ranges::distance(f_keys<1, 12>) == 12);

template<int first = 1, int last = 8>
constexpr auto mouse_buttons = norm_1_index_keys<first, last, MS_BTN1, MS_BTN8>;

}
