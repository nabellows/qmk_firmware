#pragma once

#include "compat.hpp"
#include "key_util.hpp"

#include <stdint.h>

extern "C" {
#include "keycodes.h"
#include "process_underglow.h"
#include "rgb_matrix.h"
}

namespace control {

enum class Var : uint8_t {
    NONE,
    BRIGHTNESS,
    EFFECT,
    HUE,
    SATURATION,
    SPEED,
};

inline Var current_var = Var::NONE;

inline void select_var(qmk_key_t base_keycode) {
    switch (base_keycode) {
        case KC_E: current_var = Var::EFFECT;     break;
        case KC_H: current_var = Var::HUE;        break;
        case KC_A: current_var = Var::SATURATION; break;
        case KC_B: current_var = Var::BRIGHTNESS; break;
        case KC_S: current_var = Var::SPEED;      break;
    }
}

inline qmk_key_t underglow_var_keycode(bool increase) {
    switch (current_var) {
        case Var::NONE:       return KC_NO;
        case Var::BRIGHTNESS: return increase ? UG_VALU : UG_VALD;
        case Var::EFFECT:     return increase ? UG_NEXT : UG_PREV;
        case Var::HUE:        return increase ? UG_HUEU : UG_HUED;
        case Var::SATURATION: return increase ? UG_SATU : UG_SATD;
        case Var::SPEED:      return increase ? UG_SPDU : UG_SPDD;
    }
    __builtin_unreachable(); // until we add non-UG controls
}

inline void adjust_var(bool increase, keyrecord_t *record) {
    const qmk_key_t ug_keycode = underglow_var_keycode(increase);
    if (ug_keycode != KC_NO) process_underglow(ug_keycode, record);
}

inline void reset_var() {
    switch (current_var) {
        case Var::NONE:
            rgb_matrix_toggle();
            break;
        case Var::BRIGHTNESS:
            rgb_matrix_sethsv(rgb_matrix_get_hue(), rgb_matrix_get_sat(), RGB_MATRIX_DEFAULT_VAL);
            break;
        case Var::EFFECT:
            rgb_matrix_mode(RGB_MATRIX_DEFAULT_MODE);
            break;
        case Var::HUE:
            rgb_matrix_sethsv(RGB_MATRIX_DEFAULT_HUE, rgb_matrix_get_sat(), rgb_matrix_get_val());
            break;
        case Var::SATURATION:
            rgb_matrix_sethsv(rgb_matrix_get_hue(), RGB_MATRIX_DEFAULT_SAT, rgb_matrix_get_val());
            break;
        case Var::SPEED:
            rgb_matrix_set_speed(RGB_MATRIX_DEFAULT_SPD);
            break;
    }
}

inline void init() {
    current_var = Var::NONE;
}

} // namespace control
