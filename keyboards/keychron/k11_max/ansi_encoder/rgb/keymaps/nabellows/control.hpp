#pragma once

#include "compat.hpp"
#include "key_util.hpp"
#include "rgb.hpp"

#include <stdint.h>

extern "C" {
#include "action.h"
#include "color.h"
#include "eeconfig_snap_click.h"
#include "keycodes.h"
#include "process_underglow.h"
#include "rgb_matrix.h"
#include "snap_click.h"
}

namespace control {

enum class Var : uint8_t {
    NONE,
    BRIGHTNESS,
    EFFECT,
    HUE,
    SATURATION,
    SPEED,
    SNAP_CLICK,
};

inline Var current_var = Var::NONE;

Var key_to_var(qmk_key_t base_key);

inline Var select_var(qmk_key_t base_key) {
    current_var = key_to_var(base_key);
    return current_var;
}

extern "C" snap_click_config_t snap_click_pair[SNAP_CLICK_COUNT];

// WARNING: For now, do NOT disable via control then write a new one via launcher. Well, unless swapping is what you want.
inline void adjust_snap_click(bool reset) {
    static bool enabled = true;
    if (!(reset && enabled)) { // reset just re-enables
        static uint8_t old_types[SNAP_CLICK_COUNT];
        for (int i = 0; i < SNAP_CLICK_COUNT; ++i) {
            std::swap(old_types[i], snap_click_pair[i].type);
        }
        enabled = !enabled;
    }
    rgb::flash(enabled ? rgb_t{RGB_GREEN} : rgb_t{RGB_RED}, 500);
};

inline void adjust_var(bool increase, keyrecord_t *record) {
    switch (current_var) {
        // Written this way to make a single switch statement force all values to have an impl for now
        case Var::NONE: return;
        case Var::BRIGHTNESS: process_underglow(increase ? UG_VALU : UG_VALD, record); return;
        case Var::EFFECT:     process_underglow(increase ? UG_NEXT : UG_PREV, record); return;
        case Var::HUE:        process_underglow(increase ? UG_HUEU : UG_HUED, record); return;
        case Var::SATURATION: process_underglow(increase ? UG_SATU : UG_SATD, record); return;
        case Var::SPEED:      process_underglow(increase ? UG_SPDU : UG_SPDD, record); return;
        case Var::SNAP_CLICK: return adjust_snap_click(false);
    }
}

inline void reset_var() {
    switch (current_var) {
        case Var::NONE:
            return rgb_matrix_toggle();
        case Var::BRIGHTNESS:
            return rgb_matrix_sethsv(rgb_matrix_get_hue(), rgb_matrix_get_sat(), RGB_MATRIX_DEFAULT_VAL);
        case Var::EFFECT:
            return rgb_matrix_mode(RGB_MATRIX_DEFAULT_MODE);
        case Var::HUE:
            return rgb_matrix_sethsv(RGB_MATRIX_DEFAULT_HUE, rgb_matrix_get_sat(), rgb_matrix_get_val());
        case Var::SATURATION:
            return rgb_matrix_sethsv(rgb_matrix_get_hue(), RGB_MATRIX_DEFAULT_SAT, rgb_matrix_get_val());
        case Var::SPEED:
            return rgb_matrix_set_speed(RGB_MATRIX_DEFAULT_SPD);
        case Var::SNAP_CLICK:
            return adjust_snap_click(true);
    }
}

inline void init() {
    current_var = Var::NONE;
}

} // namespace control
