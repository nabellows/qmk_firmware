/* Copyright 2024 @ Keychron (https://www.keychron.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <assert.h>
#include <stdint.h>
#include "action.h"
#include "action_tapping.h"
#include "action_util.h"
#include "keymap_us.h"
#include "modifiers.h"
#include "report.h"

#include QMK_KEYBOARD_H
#include "keychron_common.h"
#include "keycodes.h"
#include "process_key_override.h"
#include "progmem.h"
#include "process_combo.h"
#include "quantum_keycodes.h"

typedef enum {
    BASE,
	MAC_BASE = BASE,
	WIN_BASE = 2, // So far, no separate keys
	MAC_FN1,
	WIN_FN1,
	FN2,
    //TODO: Add more layers or use for bottom left two pinky keys and right cmd? (or, keep right-cmd for the rare right-only override?)
    // One could be a leader key... Or macro? Might as well still try to use layers though
} layer_t;
// assert layer size for dynamic layers? Wtf do they even do in via if you use more layers here

typedef enum {
    //TODO: make it still send KC_CTRL when pressed (like with mouse ctrl click)
    CTL_ESC = LCTL_T(KC_ESC),
    GUI_SPC = MT(MOD_LGUI, KC_SPC),
    T_CAPS_WORD = QK_CAPS_WORD_TOGGLE,
} custom_kc_t;

bool dip_switch_update_user(uint8_t index, bool active) {
    if (index == 0) {
        int layer = active ? MAC_BASE : WIN_BASE;
        default_layer_set(1UL << layer);
    }
    return true;
}

typedef enum {
    DEFAULT,
    PERMISSIVE_HOLD,
    HOLD_ON_OTHER_KEY_PRESS,
} mod_tap_behavior_t;

mod_tap_behavior_t get_mod_tap_behavior(uint16_t keycode, keyrecord_t* record) {
    switch (keycode) {
        case CTL_ESC: return HOLD_ON_OTHER_KEY_PRESS; // Considered this for permissive hold. Upside is that fast esc typistry works better, downside is that the effect of a ctrl+key is delayed until the key release
        case GUI_SPC: return PERMISSIVE_HOLD; // Adds lag to the hold/chord action, but allows space to work much better especially in heavy-handed stuff like left hand hitting shift and space where apparently I lag
        // If desired, I could make PERM_HOLD the default for windows but hold-on-other the default for mac (since I use cmd way more on mac than on win key)
        default: return DEFAULT;
    }
}

bool get_permissive_hold(uint16_t keycode, keyrecord_t* record) {
    return get_mod_tap_behavior(keycode, record) == PERMISSIVE_HOLD;
}
bool get_hold_on_other_key_press(uint16_t keycode, keyrecord_t* record) {
    return get_mod_tap_behavior(keycode, record) == HOLD_ON_OTHER_KEY_PRESS;
}

//TODO: perhaps some double tap fn1 fn2 keys to toggle the layer instead of one-shot (with timeout? gets unset if pressed once? )
// Decide how to make hjkl and wasd useful. Which layer for numpad? unfortunately i find fn2 unergonomic at least right now, prefer a left hand key
// But I would also prefer said left hand key for hjkl
// clang-format off
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [BASE] = LAYOUT_69_ansi(
        KC_GRV, /**/ KC_1,  KC_2,  KC_3,  KC_4,  KC_5,  KC_6, /**/   KC_7,   KC_8,   KC_9,   KC_0,   KC_MINS, KC_EQL,       /**/KC_BSPC,   /**/   KC_MUTE,
        /*------------------------------------------------------------------------------------------------------------------------------------------------*/
        KC_TAB, /**/   KC_Q,   KC_W,   KC_E,   KC_R,   KC_T,  /**/ KC_Y,  KC_U,   KC_I,   KC_O,   KC_P,    KC_LBRC, KC_RBRC,/**/ KC_BSLS,  /**/   KC_DEL,
        CTL_ESC,/**/   KC_A,   KC_S,   KC_D,   KC_F,   KC_G,  /**/        KC_H,   KC_J,   KC_K,   KC_L,    KC_SCLN, KC_QUOT,/**/ KC_ENT,   /**/   KC_HOME,
        KC_LSFT,/**/   KC_Z,   KC_X,   KC_C,   KC_V,   KC_B,  /**/ KC_B,  KC_N,   KC_M,  KC_COMM, KC_DOT,  KC_SLSH,    /**/ KC_RSFT,      KC_UP,
        /*------------------------------------------------------------------------------------------------------------------------------------------------*/
        KC_LCTL, KC_LGUI,  KC_LOPT,      GUI_SPC,     MO(MAC_FN1),/**/MO(FN2),     KC_SPC,      KC_RCMMD,              /**/      KC_LEFT, KC_DOWN, KC_RGHT
    ),

    #define MAP(KEY, NEW_MAP) NEW_MAP

    //TODO: make something like FOR_EACH_BASE() and apply to both base and FOR_EACH_BASE_FN1 and inside use MO(WHICH(FN1)) or other keys which recursively expand to something
    // if that key acts differently in the two bases? Or nah, just make WIN_FN1 and WIN always exist, and fallthrough 99% of time
    //TODO: Refactor to make the win duplicates (a) not exist or (b) reuse mac definition
    #if WIN_BASE != BASE
    [WIN_BASE] = LAYOUT_69_ansi(
        KC_GRV,  KC_1,	   KC_2,	 KC_3,	  KC_4,    KC_5,	KC_6,	  KC_7,    KC_8,	KC_9,	 KC_0,	   KC_MINS,  KC_EQL,   KC_BSPC,          KC_MUTE,
        KC_TAB,  KC_Q,	   KC_W,	 KC_E,	  KC_R,    KC_T,	KC_Y,	  KC_U,    KC_I,	KC_O,	 KC_P,	   KC_LBRC,  KC_RBRC,  KC_BSLS,          KC_DEL,
        CTL_ESC, KC_A,	   KC_S,	 KC_D,	  KC_F,    KC_G,              KC_H,    KC_J,	KC_K,	 KC_L,	   KC_SCLN,  KC_QUOT,  KC_ENT,           KC_HOME,
        KC_LSFT, KC_Z,	   KC_X,	 KC_C,    KC_V,	   KC_B,	KC_B,     KC_N,	   KC_M,	KC_COMM, KC_DOT,   KC_SLSH,  KC_RSFT,           KC_UP,
        KC_LCTL, KC_LWIN,  KC_LALT,           KC_SPC,           MO(WIN_FN1), MO(FN2),       KC_SPC,            KC_RALT,            KC_LEFT, KC_DOWN, KC_RGHT),
    #endif

    // Could create a layer underneath these two for common features like f1-f12... Or create only a mac_fn1 layer or only a win_fn1 layer and make the larger one fallback to the other
    // Or just a utility to reduce duplication. Or, just copy paste and suck it up.
    // Also dont love how this macro takes away the arr-of-arr indexing such that I dont have a 'row' concept to do some progammatic access of all keys in a 'row', unless there is a way?
    // But, the default "enable layer win_fn1", does it have a way to enable multiple layers? Because it wont work to make win fallback to mac if mac isn't also enabled (and if so, make FN1 FN1_DEFAULT rather than 'mac')
    [MAC_FN1] = LAYOUT_69_ansi(
        KC_GRV,  KC_BRID,  KC_BRIU, KC_MCTRL, KC_LNPAD,UG_VALD, UG_VALU,  KC_MPRV, KC_MPLY, KC_MNXT, KC_MUTE,  KC_VOLD,  KC_VOLU,  _______,          UG_TOGG,
        _______, BT_HST1,  BT_HST2,  BT_HST3, P2P4G,   _______, _______,  _______, _______, _______, _______,  _______,  _______,  _______,          KC_INS,
        UG_TOGG, UG_NEXT,  UG_VALU,  UG_HUEU, UG_SATU, UG_SPDU,           _______, _______, _______, _______,  _______,  _______,  _______,          KC_END,
        _______, UG_PREV, UG_VALD,  UG_HUED, UG_SATD, UG_SPDD, _______,  NK_TOGG, _______, _______,  _______, _______,  _______,           KC_PGUP,
        _______, _______,  _______,           _______,          _______,  _______,          _______,           _______,            _______, KC_PGDN, _______),

    [WIN_FN1] = LAYOUT_69_ansi(
        KC_GRV,  KC_BRID,  KC_BRIU,  KC_TASK, KC_FILE, UG_VALD, UG_VALU,  KC_MPRV, KC_MPLY, KC_MNXT, KC_MUTE,  KC_VOLD,  KC_VOLU,  _______,          UG_TOGG,
        _______, BT_HST1,  BT_HST2,  BT_HST3, P2P4G,   _______, _______,  _______, _______, _______, _______,  _______,	 _______,  _______,          KC_INS,
        UG_TOGG, UG_NEXT,  UG_VALU,  UG_HUEU, UG_SATU, UG_SPDU,           _______, _______, _______, _______,  _______,  _______,  _______,          KC_END,
        _______, UG_PREV, UG_VALD,  UG_HUED, UG_SATD, UG_SPDD, _______,  NK_TOGG, _______, _______,  _______, _______,  _______,           KC_PGUP,
        _______, _______,  _______,           _______,          _______,  _______,          _______,           _______,            _______, KC_PGDN, _______),

    //TODO: might be nice to similarly define a macro or const-init function which lets you map from 'base' layer to new key so quite simply I specify pairs like KC_1->KC_F1
    // Might need hella magic, but this idea would be really really helpful and could isolate logical groups rather than position like ADD_GROUP(MAP(J, DOWN)) etc
    // Ideally, we integrate one magic cpp file which exposes C extern linkage symbols?
    [FN2] = LAYOUT_69_ansi(
        KC_TILD, KC_F1,    KC_F2,	 KC_F3,   KC_F4,   KC_F5,	KC_F6,	  KC_F7,   KC_F8,	KC_F9,	 KC_F10,   KC_F11,	 KC_F12,   _______,          _______,
        _______, _______,  _______,  _______, _______, _______, _______,  _______, _______, _______, _______,  _______,  _______,  _______,          _______,
        _______, _______,  _______,  _______, _______, _______,           _______, _______, _______, _______,  _______,  _______,  _______,          _______,
        _______, _______,  _______,  _______, _______, BAT_LVL, BAT_LVL,  _______, _______, _______, _______,  _______,  _______,           _______,
        _______, _______,  _______,           _______,          _______,  _______,          _______,           _______,            _______, _______, _______)
};

const uint16_t PROGMEM combo_shifts_caps_word[] = { KC_LSFT, KC_RSFT, COMBO_END };
enum combos{
    BOTH_SHIFT
};
// Note this crap has to exist here with normal static linkage or whatever and is accessed by name via INCLUDING this file. Sketchy defeaults IMO, not C-like
combo_t key_combos[] = {
    //TODO: maybe implement as a manual process_record_user, since I think combo is suppressing key down events making things like shift click worse
    //Perhaps possible by making the shift keys send two keys (custom) SHIFT+TCAPS_LEFT and SHIFT+TCAPS_RIGHT and make the combo defined on TCAPS and not shift? (perhaps qmk will not swallow)
    [BOTH_SHIFT] = COMBO(combo_shifts_caps_word, T_CAPS_WORD),
};
// Per-combo timeouts
uint16_t get_combo_term(uint16_t combo_index, combo_t *combo) {
    switch (combo_index) {
        case BOTH_SHIFT:
            return UINT16_MAX;
    }
    return COMBO_TERM;
}

static bool enable_shift_space_underscore = false;
const key_override_t shift_space_override = {
    .trigger_mods                           = MOD_MASK_SHIFT,
    .layers                                 = ~0,
    .suppressed_mods                        = MOD_MASK_SHIFT,
    .options                                = ko_options_default,
    .negative_mod_mask                      = 0,
    .custom_action                          = NULL,
    .context                                = NULL,
    .trigger                                = KC_SPACE,
    .replacement                            = KC_UNDERSCORE,
    .enabled                                = &enable_shift_space_underscore
};

const key_override_t* my_key_overrides[] = { &shift_space_override, NULL };
const key_override_t** key_overrides = my_key_overrides;

// Override: just adding space support (will do shift-space without cancelling, then will add custom override for shift-space for caps-word mode)
//TODO: debug why shift key is registering as stuck down even though it should be using weak mods
bool caps_word_press_user(uint16_t keycode) {
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
void caps_word_set_user(bool active) {
    enable_shift_space_underscore = active;
}

void keyboard_post_init_user() {
#ifdef DEBUG
    debug_enable = true;
#endif
}

#if defined(ENCODER_MAP_ENABLE)
	const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][2] = {
		[MAC_BASE] = {ENCODER_CCW_CW(KC_VOLD, KC_VOLU)},
		[WIN_BASE] = {ENCODER_CCW_CW(KC_VOLD, KC_VOLU)},
		[MAC_FN1]  = {ENCODER_CCW_CW(UG_VALD, UG_VALU)},
		[WIN_FN1]  = {ENCODER_CCW_CW(UG_VALD, UG_VALU)},
		[FN2]	   = {ENCODER_CCW_CW(_______, _______)},
	};
#endif // ENCODER_MAP_ENABLE

int test_fn(void);
// clang-format on
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (test_fn()) {
        return false;
    }
    if (!process_record_keychron_common(keycode, record)) {
        return false;
    }
    return true;
}
