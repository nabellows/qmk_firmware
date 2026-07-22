#include "compat.hpp"

#include "combos.hpp"
#include "config.h"
#include "key_util.hpp"
#include "keys.hpp"
#include "layer_util.hpp"
#include "layers.hpp"
#include "util.hpp"

extern "C" {

#include QMK_KEYBOARD_H

#include "action.h"
#include "action_layer.h"
#include "action_tapping.h"
#include "action_util.h"
#include "keychron_common.h"
#include "keycodes.h"
#include "process_combo.h"
#include "progmem.h"
#include "quantum_keycodes.h"

} // extern "C"


using namespace key_defs;

DEFINE_LAYER(LAYOUT_BASE, {
    set({
        .matrix = LAYOUT_69_ansi(
            KC_ESC, /**/ KC_1,  KC_2,  KC_3,  KC_4,  KC_5,  KC_6, /**/   KC_7,   KC_8,   KC_9,   KC_0,   KC_MINS, KC_EQL,       /**/KC_BSPC,   /**/   KNOB_PRESS,
            /*------------------------------------------------------------------------------------------------------------------------------------------------*/
            KC_TAB, /**/   KC_Q,   KC_W,   KC_E,   KC_R,   KC_T,  /**/ KC_Y,  KC_U,   KC_I,   KC_O,   KC_P,    KC_LBRC, KC_RBRC,/**/ KC_BSLS,  /**/   KC_DEL,
            KC_CAPS,/**/   KC_A,   KC_S,   KC_D,   KC_F,   KC_G,  /**/        KC_H,   KC_J,   KC_K,   KC_L,    KC_SEMI, KC_QUOT,/**/ KC_ENT,   /**/   KC_HOME,
            KC_LSFT,/**/   KC_Z,   KC_X,   KC_C,   KC_V,   KC_B,  /**/ KC_B,  KC_N,   KC_M,  KC_COMM, KC_DOT,  KC_SLSH,    /**/ KC_RSFT,      KC_UP,
            /*------------------------------------------------------------------------------------------------------------------------------------------------*/
            KC_LCTL, KC_LWIN,      KC_LOPT,    LSPACE,      FN1,  /**/   FN2,       RSPACE,         KC_RCMD,               /**/      KC_LEFT, KC_DOWN, KC_RGHT
        ),
        .encoder_map = ENCODER_CCW_CW(KNOB_CCW, KNOB_CW),
    });
})

DEFINE_LAYER(BASE, {
    clone();
    map_base(FN1, FN2)
        .to(kMO(Layer::FN1), kMO(Layer::FN2));

    map_base(KC_CAPS).to(CTL_ESC);
    map_base(KC_ESC).to(KC_GRV);

    map_base(LSPACE).to(GUI_SPC);
    map_base(RSPACE).to(KC_SPACE);

    map_base(KNOB_PRESS, KNOB_CCW, KNOB_CW)
        .to(KC_MUTE, KC_VOLD, KC_VOLU);
})
static_assert(kLayerDef<Layer::BASE>.matrix[2][0] == CTL_ESC);

DEFINE_LAYER(FN1, {
    set({
        .matrix = LAYOUT_69_ansi(
            _______, _______,  _______,  _______, _______, _______, _______,  _______, _______, _______, _______,  _______,  _______,  _______,          UG_TOGG,
            _______, BT_HST1,  BT_HST2,  BT_HST3, P2P4G,   _______, _______,  _______, _______, _______, _______,  _______,  _______,  _______,          KC_INS,
            UG_TOGG, UG_NEXT,  UG_VALU,  UG_HUEU, UG_SATU, UG_SPDU,           _______, _______, _______, _______,  _______,  _______,  _______,          KC_END,
            _______, UG_PREV,  UG_VALD,  UG_HUED, UG_SATD, UG_SPDD, _______,  _______, _______, _______,  _______, _______,  _______,           KC_PGUP,
            _______, _______,  _______,           _______,          _______,  _______,          _______,           _______,            _______, KC_PGDN, _______
        ),
        .encoder_map = ENCODER_CCW_CW(UG_VALD, UG_VALU),
    });
    map_base(LSPACE, RSPACE).to_single(QK_LAYER_LOCK);

    map_base_span("1=").to(f_keys<1, 12>);
    map_base("hjkl").to(arrows_hjkl);

    use_base(KC_ESC);
})

//TODO: perhaps some double tap fn1 fn2 keys to toggle the layer instead of one-shot (with timeout? gets unset if pressed once? )
DEFINE_LAYER(FN2, {
    trans();
    map_base(LSPACE, RSPACE).to_single(QK_LAYER_LOCK);

    map_base("12").to(KC_BRID, KC_BRIU);
    map_base("34").to(KC_MCTRL, KC_LNPAD); // mac keys (but idk if windows uses)
    map_base("56").to(UG_VALD, UG_VALU);
    map_base("789").to(KC_MEDIA_PREV_TRACK, KC_MEDIA_PLAY_PAUSE, KC_MEDIA_NEXT_TRACK);
    map_base("0-=").to(KC_MUTE, KC_VOLD, KC_VOLU);

    use_base(KC_ESC);
    use_base(KC_CAPS);

    map_base<false>("b").to(BAT_LVL); // b is duplicated so we disable strict mode
    map_base(KC_BACKSPACE).to(NK_TOGG);

    // Numpad
    map_base(
        "uio"
        "jkl"
        "nm,."
    ).to(
        "789"
        "456"
        "0123"
    );

    // TODO: interface like this? I guess you could default-init matrix and then still designate init the encoder
    // encoder_map = ENCODER_CCW_CW(UG_VALD, UG_VALU);
})

extern "C" {

namespace {
constexpr PROGMEM KeymapDef<> KEYMAP;
}
extern const auto& keymaps = KEYMAP.keymap;

#ifdef ENCODER_MAP_ENABLE
extern const auto& encoder_map = KEYMAP.encoder_map;
#endif
} // extern "C"

// (QMK EXPORT) (expects not const for some reason)
auto key_combos = invoke_with_index_seq<kNumCombos>([]<sz...is>{
    return std::array<combo_t, kNumCombos> {{
        COMBO(ComboDef<Combo(is)>::keys, ComboDef<Combo(is)>::action)...
    }};
});

extern "C" {

#include "key_overrides.hpp"
#include "keymap_introspection.c"

static_assert(NUM_KEYMAP_LAYERS_RAW == kNumLayers);
static_assert(ARRAY_SIZE(key_combos) == kNumCombos);
static_assert(ARRAY_SIZE(key_overrides) == kNumKeyOverrides);
}

static_assert(index_of(kLayerDef<Layer::BASE>.matrix, KC_1) == md_index<2>{0, 1});
static_assert(index_of(kLayerDef<Layer::BASE>.matrix, KC_EQL) == md_index<2>{0, 12});

static_assert(kLayerDef<Layer::BASE>.matrix[0][1] == KC_1);
static_assert(kLayerDef<Layer::FN1>.matrix[0][1] == KC_F1);
static_assert(kLayerDef<Layer::BASE>.matrix[0][12] == KC_EQL);
static_assert(kLayerDef<Layer::FN1>.matrix[0][12] == KC_F12);
static_assert(kLayerDef<Layer::FN1>.encoder_map[0][0] == UG_VALU);
static_assert(kLayerDef<Layer::FN1>.encoder_map[0][1] == UG_VALD);

static_assert(kLayerDef<Layer::FN2>.matrix[2][8] == KC_5);
