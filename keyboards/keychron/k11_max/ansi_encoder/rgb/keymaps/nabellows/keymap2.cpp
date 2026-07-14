#include "compat.hpp"

extern "C" {

#include "action_layer.h"
#include "action_util.h"
#include "keymap_us.h"
#include "modifiers.h"

#include "keycodes.h"
#include QMK_KEYBOARD_H
#include "keychron_common.h"
#include "keycodes.h"
#include "process_key_override.h"
#include "progmem.h"
#include "process_combo.h"
#include "quantum_keycodes.h"
//TODO: shrink the extern C block, can move all the declarations here (or to a keymap.hpp header...). Might also want to
//modernify the keymap_introspectio but technically the community module system thing seems like it wants to pretend to be C...
//unsure.
// Or does the rest of my constepxr stuff just work and forget about extern C

enum layers{
	MAC_BASE,
	WIN_BASE,
	MAC_FN1,
	WIN_FN1,
	FN2,
    //TODO: Add more layers or use for bottom left two pinky keys and right cmd? (or, keep right-cmd for the rare right-only override?)
    // One could be a leader key... Or macro? Might as well still try to use layers though
};

enum custom_kc{
    CTL_ESC = CTL_T(KC_ESC),
};

// clang-format off
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [MAC_BASE] = LAYOUT_69_ansi(
        KC_GRV,  KC_1,	   KC_2,	 KC_3,	  KC_4,    KC_5,	KC_6,	  KC_7,    KC_8,	KC_9,	 KC_0,	   KC_MINS,  KC_EQL,   KC_BSPC,          KC_MUTE,
        KC_TAB,  KC_Q,	   KC_W,	 KC_E,	  KC_R,    KC_T,	KC_Y,	  KC_U,    KC_I,	KC_O,	 KC_P,	   KC_LBRC,  KC_RBRC,  KC_BSLS,          KC_DEL,
        CTL_ESC, KC_A,	   KC_S,	 KC_D,	  KC_F,    KC_G,              KC_H,    KC_J,	KC_K,	 KC_L,	   KC_SCLN,  KC_QUOT,  KC_ENT,           KC_HOME,
        KC_LSFT, KC_Z,	   KC_X,	 KC_C,    KC_V,	   KC_B,	KC_B,     KC_N,	   KC_M,	KC_COMM, KC_DOT,   KC_SLSH,  KC_RSFT,           KC_UP,
        KC_LCTL, KC_LOPTN, KC_LOPTN,   MT(MOD_LGUI, KC_SPC),  MO(MAC_FN1), MO(FN2),       KC_SPC,            KC_RCMMD,           KC_LEFT, KC_DOWN, KC_RGHT),

    //TODO: Refactor to make the win duplicates (a) not exist or (b) reuse mac definition
    [WIN_BASE] = LAYOUT_69_ansi(
        KC_GRV,  KC_1,	   KC_2,	 KC_3,	  KC_4,    KC_5,	KC_6,	  KC_7,    KC_8,	KC_9,	 KC_0,	   KC_MINS,  KC_EQL,   KC_BSPC,          KC_MUTE,
        KC_TAB,  KC_Q,	   KC_W,	 KC_E,	  KC_R,    KC_T,	KC_Y,	  KC_U,    KC_I,	KC_O,	 KC_P,	   KC_LBRC,  KC_RBRC,  KC_BSLS,          KC_DEL,
        CTL_ESC, KC_A,	   KC_S,	 KC_D,	  KC_F,    KC_G,              KC_H,    KC_J,	KC_K,	 KC_L,	   KC_SCLN,  KC_QUOT,  KC_ENT,           KC_HOME,
        KC_LSFT, KC_Z,	   KC_X,	 KC_C,    KC_V,	   KC_B,	KC_B,     KC_N,	   KC_M,	KC_COMM, KC_DOT,   KC_SLSH,  KC_RSFT,           KC_UP,
        KC_LCTL, KC_LWIN,  KC_LALT,           KC_SPC,           MO(WIN_FN1), MO(FN2),       KC_SPC,            KC_RALT,            KC_LEFT, KC_DOWN, KC_RGHT),

    [MAC_FN1] = LAYOUT_69_ansi(
        KC_GRV,  KC_BRID,  KC_BRIU, KC_MCTRL, KC_LNPAD,UG_VALD, UG_VALU,  KC_MPRV, KC_MPLY, KC_MNXT, KC_MUTE,  KC_VOLD,  KC_VOLU,  _______,          UG_TOGG,
        _______, BT_HST1,  BT_HST2,  BT_HST3, P2P4G,   _______, _______,  _______, _______, _______, _______,  _______,  _______,  _______,          KC_INS,
        UG_TOGG, UG_NEXT,  UG_VALU,  UG_HUEU, UG_SATU, UG_SPDU,           KC_LEFT, KC_DOWN, KC_UP,   KC_RIGHT, _______,  _______,  _______,          KC_END,
        _______, UG_PREV,  UG_VALD,  UG_HUED, UG_SATD, UG_SPDD,  _______, NK_TOGG, _______, _______,  _______,  _______,  _______, KC_PGUP,
        _______, _______,  _______,           _______,          _______,  _______,          _______,           _______,            _______, KC_PGDN, _______),

    [WIN_FN1] = LAYOUT_69_ansi(
        KC_GRV,  KC_BRID,  KC_BRIU,  KC_TASK, KC_FILE, UG_VALD, UG_VALU,  KC_MPRV, KC_MPLY, KC_MNXT, KC_MUTE,  KC_VOLD,  KC_VOLU,  _______,          UG_TOGG,
        _______, BT_HST1,  BT_HST2,  BT_HST3, P2P4G,   _______, _______,  _______, _______, _______, _______,  _______,	 _______,  _______,          KC_INS,
        UG_TOGG, UG_NEXT,  UG_VALU,  UG_HUEU, UG_SATU, UG_SPDU,           _______, _______, _______, _______,  _______,  _______,  _______,          KC_END,
        _______, UG_PREV,  UG_VALD,  UG_HUED, UG_SATD, UG_SPDD,  _______, NK_TOGG, _______, _______,  _______,  _______,  _______, KC_PGUP,
        _______, _______,  _______,           _______,          _______,  _______,          _______,           _______,            _______, KC_PGDN, _______),

    //TODO: might be nice to similarly define a macro or const-init functino which lets you map from 'base' layer to new key so quite simply I specify pairs like KC_1->KC_F1
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
combo_t key_combos[] = {
    [BOTH_SHIFT] = COMBO(combo_shifts_caps_word, QK_CAPS_WORD_TOGGLE),
};
uint16_t get_combo_term(uint16_t combo_index, combo_t *combo) {
    switch (combo_index) {
        case BOTH_SHIFT:
            return UINT16_MAX;
    }
    return COMBO_TERM;
}

constexpr layer_state_t kAllLayers = ~0;

static bool enable_shift_space_underscore = false;
const key_override_t shift_space_override = {
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

const key_override_t* key_overrides[] = { &shift_space_override };

// Override: just adding space support (will do shift-space without cancelling, then will add custom override for shift-space for caps-word mode)
//TODO: debug why shift key is registering as stuck down even though it should be using weak mods
bool caps_word_press_user(uint16_t keycode) {
    switch (keycode) {
        // Keycodes that continue Caps Word, with shift applied.
        case KC_A ... KC_Z:
        case KC_MINS:
        case KC_SPACE:
            add_weak_mods(MOD_BIT(KC_LSFT));  // Apply shift to next key.
            return true;

        // Keycodes that continue Caps Word, without shifting.
        case KC_1 ... KC_0:
        case KC_BSPC:
        case KC_DEL:
        case KC_UNDS:
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

// clang-format on
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (!process_record_keychron_common(keycode, record)) {
        return false;
    }
    return true;
}

#include "keymap_introspection.hpp"

}

