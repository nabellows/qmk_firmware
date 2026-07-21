#pragma once

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
