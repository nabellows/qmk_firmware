#include <stdint.h>
#include "compat.hpp"

#include "caps_word.hpp"
#include "key_util.hpp"
#include "keys.hpp"

extern "C" {
#include "action.h"
#include "caps_word.h"
#include "keycodes.h"
#include "layers.hpp"
#include "rgb_matrix.h"
}

struct OsState {
    enum Value { MAC, WIN };
    static Value set(Value v) {
        val = v;
        default_layer_set(val == MAC ? Layer::MAC_BASE : Layer::WIN_BASE); // Override keychron
        return val;
    }
    static Value get() { return val; }
    static bool is_mac() { return val == MAC; }
    static bool is_win() { return val == WIN; }
    static Layer default_layer() { return val == MAC ? Layer::MAC_BASE : Layer::WIN_BASE; }
private:
    inline static Value val = MAC; // Not defined, but the dip_switch_update_user will be called in dip_switch_init on keyboard init
};

extern "C" {
#include "action_tapping.h"
#include "debug.h"
#include "keychron_common.h"

//TODO: tbh, should i just revert and let keychon steal this? This currently provides no value right
bool dip_switch_update_user(uint8_t index, bool active) {
    if (index == 0) {
        OsState::set(active ? OsState::MAC : OsState::WIN);
    }
    return true;
}

typedef enum {
    DEFAULT,
    PERMISSIVE_HOLD,
    HOLD_ON_OTHER_KEY_PRESS,
} mod_tap_behavior_t;

mod_tap_behavior_t get_mod_tap_behavior(qmk_key_t keycode, keyrecord_t* record) {
    switch (keycode) {
        case CTL_ESC: return HOLD_ON_OTHER_KEY_PRESS; // Considered this for permissive hold. Upside is that fast esc typistry works better, downside is that the effect of a ctrl+key is delayed until the key release
        case GUI_SPC: return PERMISSIVE_HOLD; // Adds lag to the hold/chord action, but allows space to work much better especially in heavy-handed stuff like left hand hitting shift and space where apparently I lag
        // If desired, I could make PERM_HOLD the default for windows but hold-on-other the default for mac (since I use cmd way more on mac than on win key)
        default: return DEFAULT;
    }
}

bool get_permissive_hold(qmk_key_t keycode, keyrecord_t* record) {
    return get_mod_tap_behavior(keycode, record) == PERMISSIVE_HOLD;
}
bool get_hold_on_other_key_press(qmk_key_t keycode, keyrecord_t* record) {
    return get_mod_tap_behavior(keycode, record) == HOLD_ON_OTHER_KEY_PRESS;
}

void keyboard_post_init_user() {
#ifdef DEBUG
    debug_enable = true;
#endif
}

static qmk_key_t shift_state = 0;

// clang-format on
bool process_record_user(qmk_key_t keycode, keyrecord_t *record) {
    switch (keycode) {
        case KC_LSFT:
        case KC_RSFT:
            shift_state ^= keycode;
            if (shift_state == (KC_LSFT ^ KC_RSFT)) {
                caps_word_toggle();
            }
            break;
    }
    if (!process_record_keychron_common(keycode, record)) {
        return false;
    }
    return true;
}

bool rgb_matrix_indicators_user() {
    if (is_caps_word_on()) {
        //FIXME: with effects like raindrops, takes a bit to reset the colors, any way to save/pause and resume state?
        rgb_matrix_set_color_all(RGB_WHITE);
    }
    return true;
}

uint16_t get_tapping_term(qmk_key_t keycode, keyrecord_t *record) {
    switch (keycode) {
        case GUI_SPC:
            return UINT16_MAX;
        default:
            break;
    }
#ifdef DYNAMIC_TAPPING_TERM_ENABLE
    return g_tapping_term;
#else
    return TAPPING_TERM;
#endif
}

}
