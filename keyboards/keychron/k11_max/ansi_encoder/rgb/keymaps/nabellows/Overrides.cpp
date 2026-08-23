#include <stdint.h>
#include "compat.hpp"

#include "caps_word.hpp"
#include "control.hpp"
#include "key_util.hpp"
#include "keys.hpp"
#include "mouse.hpp"

extern "C" {
#include "action.h"
#include "caps_word.h"
#include "keymap_introspection.h"
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
    mouse::load_speeds();
}

void eeconfig_init_user() {
    eeconfig::invalidate();
    mouse::init_eeprom();
}

void housekeeping_task_user() {
    mouse::housekeeping_task();
}

static qmk_key_t shift_state = 0;

// clang-format on
bool process_record_user(qmk_key_t keycode, keyrecord_t *record) {
    switch (keycode) {
        case CONTROL_VAR:
            if (record->event.pressed) {
                control::select_var(keycode_at_keymap_location_raw(
                    layer_state_t(Layer::BASE),
                    record->event.key.row,
                    record->event.key.col
                ));
            }
            return false;

        case VAR_MINUS:
        case VAR_PLUS:
            if (record->event.pressed) {
                const bool increase = keycode == VAR_PLUS;
                if (layer_state_is(Layer::MOUSE)) {
                    mouse::adjust_speed(increase);
                } else if (layer_state_is(Layer::CONTROL)) {
                    control::adjust_var(increase, record);
                }
            }
            return false;

        case VAR_RESET:
            if (record->event.pressed) {
                if (layer_state_is(Layer::MOUSE)) {
                    mouse::reset_speed();
                } else if (layer_state_is(Layer::CONTROL)) {
                    control::reset_var();
                }
            }
            return false;

        case MS_ACL0:
            mouse::set_speed_modifier(mouse::SpeedIndex::SLOW, record->event.pressed);
            break;

        case MS_ACL2:
            mouse::set_speed_modifier(mouse::SpeedIndex::FAST, record->event.pressed);
            break;

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

layer_state_t layer_state_set_user(layer_state_t state) {
    constexpr layer_state_t control_layer = layer_state_t(1) << layer_state_t(Layer::CONTROL);
    if ((state ^ layer_state) & control_layer) {
        control::init();
    }
    return state;
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

#define EECONFIG_IMPL
#include "eeconfig/eeconfig_impl.hpp"
