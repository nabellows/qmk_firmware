#pragma once

#include "compat.hpp"
#include "key_util.hpp"
#include "layers.hpp"

extern "C" {
#include "action_util.h"
#include "process_key_override.h"
}

namespace caps_word {

static inline bool enable_shift_space_underscore = false;
constexpr key_override_t shift_space_override = {
    .trigger                                = KC_SPACE,
    .trigger_mods                           = MOD_MASK_SHIFT,
    .layers                                 = kAllLayers,
    .negative_mod_mask                      = 0,
    .suppressed_mods                        = MOD_MASK_SHIFT,
    .replacement                            = KC_UNDERSCORE,
    .options                                = ko_options_default,
    .custom_action                          = NULL,
    .context                                = NULL,
    .enabled                                = &enable_shift_space_underscore
};

}

// Override: just adding space support (will do shift-space without cancelling, then will add custom override for shift-space for caps-word mode)
QMK_INLINE bool caps_word_press_user(qmk_key_t keycode) {
    switch (keycode) {
        // Keycodes that continue Caps Word, with shift applied.
        case KC_A ... KC_Z:
        case KC_SPACE:
            add_weak_mods(MOD_BIT(KC_LSFT));  // Apply shift to next key.
            return true;

        // Keycodes that continue Caps Word, without shifting.
        case KC_1 ... KC_0:
        case KC_MINUS:
        case KC_BSPC:
        case KC_DEL:
        case KC_UNDERSCORE:
            return true;

        default:
            return false;  // Deactivate Caps Word.
    }
}

QMK_INLINE void caps_word_set_user(bool active) {
    caps_word::enable_shift_space_underscore = active;
}
