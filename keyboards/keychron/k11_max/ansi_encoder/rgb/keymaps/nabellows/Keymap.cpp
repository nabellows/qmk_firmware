#include "common.hpp"
#include "compat.hpp"

#include "combos.hpp"
#include "config.h"
#include "control.hpp"
#include "key_util.hpp"
#include "keys.hpp"
#include "layer_util.hpp"
#include "layers.hpp"
#include "modifiers.h"
#include "report.h"
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

#define LAYOUT(...) { LAYOUT_69_ansi(__VA_ARGS__) } // std::array friendly

using namespace key_defs;

DEFINE_LAYER(LAYOUT_BASE, {
    set({
        .matrix = LAYOUT(
            KC_ESC, /**/ KC_1,  KC_2,  KC_3,  KC_4,  KC_5,  KC_6, /**/   KC_7,   KC_8,   KC_9,   KC_0,   KC_MINS, KC_EQL,       /**/KC_BSPC,   /**/   KNOB_PRESS,
            /*------------------------------------------------------------------------------------------------------------------------------------------------*/
            KC_TAB, /**/   KC_Q,   KC_W,   KC_E,   KC_R,   KC_T,  /**/ KC_Y,  KC_U,   KC_I,   KC_O,   KC_P,    KC_LBRC, KC_RBRC,/**/ KC_BSLS,  /**/   KC_DEL,
            KC_CAPS,/**/   KC_A,   KC_S,   KC_D,   KC_F,   KC_G,  /**/        KC_H,   KC_J,   KC_K,   KC_L,    KC_SEMI, KC_QUOT,/**/ KC_ENT,   /**/   KC_HOME,
            KC_LSFT,/**/   KC_Z,   KC_X,   KC_C,   KC_V,   KC_B, /**/ RIGHT_B, KC_N,  KC_M,  KC_COMM, KC_DOT,  KC_SLSH,    /**/ KC_RSFT,      KC_UP,
            /*------------------------------------------------------------------------------------------------------------------------------------------------*/
            KC_LCTL, KC_LWIN,      KC_LOPT,    LSPACE,      FN1,  /**/   FN2,       RSPACE,         KC_RCMD,               /**/      KC_LEFT, KC_DOWN, KC_RGHT
        ),
        .encoder_map = ENCODER_CCW_CW(KNOB_CCW, KNOB_CW),
    });
})


DEFINE_LAYER(NORMIE, {
    clone_base();

    // TODO: my vision for this was making it basically the keychron defaults, so make fn1 f-keys and fn2 special keys (vol, etc), without much in the alphabet keys
    // But, I dont really want to add two whole layers for it... Perhaps possible with key overrides
    // Even normies need a way to type esc, f-keys, etc
    map_base(KC_ESC).to(KC_GRV);
    map_base(LSPACE, RSPACE)
        .to_single(KC_SPACE);
    map_base(FN1, FN2)
        .to(kMO(Layer::FN1), kMO(Layer::FN2));
    map_base(KNOB_PRESS, KNOB_CCW, KNOB_CW)
        .to(KC_MUTE, KC_VOLD, KC_VOLU);

    map_base(KC_LWIN).to(TD(TD_LGUI)); // Win key screws you in games (smh win+ctrl+d losing me OW game). Use Fn1. Actually, prefer right hand.
    // Or, escape hatch to BASE somehow (double tab fn1?) Still need kinda an ergo left hand way for stuff like win+shift+s
})

DEFINE_LAYER(BASE, {
    clone<Layer::NORMIE>(); // could use trans(), but we are going to make this a base layer rather than toggle on, so NORMIE will be disabled when this is active (could go the toggle route...)
    map_base(FN1, FN2)
        .to(kMO(Layer::FN1), kMO(Layer::FN2));

    map_base(KC_CAPS).to(CTL_ESC);
    map_base(LSPACE).to(GUI_SPC);
})
static_assert(kLayerDef<Layer::BASE>.matrix[2][0] == CTL_ESC);

DEFINE_LAYER(FN1, {
    trans();
    map_base(FN2).to(kOSL(Layer::SELECT)); // This feels a bit sketchy

    // RGB
    map_base(KNOB_PRESS, KNOB_CCW, KNOB_CW)
        .to(UG_TOGG, UG_VALD, UG_VALU);

    use_base(KC_ESC);
    use_base(KC_CAPS);
    // fat fingered fn1+lspace in arena and got molested.
    // Rspace might also be a bit problematic, but right now fn1 key is really only "special functions plus f keys"
    //  a layer like nav layer, fkeys, numpad, that might want to use down arrow
    // Do we ever even really want to layer lock this version of fn1? Perhaps better to make some real, useful layers on 1/2, numkeys,
    //  keys, etc
    map_base(RSPACE, KC_RCMD).to_single(QK_LAYER_LOCK);
    map_base(KC_LWIN).to(OSM_LGUI);

    map_base_span("1=").to(f_keys<1, 12>);
    map_base(KC_DEL, KC_HOME).to(KC_INS, KC_END);

    map_base("wasd").to(arrows_wasd);
    map_base("hjkl").to(arrows_hjkl);
})
static_assert(kLayerDef<Layer::FN1>.matrix[0][3] == KC_F3);


//TODO: perhaps some double tap fn1 fn2 keys to toggle the layer instead of one-shot (with timeout? gets unset if pressed once? )
DEFINE_LAYER(FN2, {
    trans();
    map_base(FN1).to(kOSL(Layer::SELECT)); // This system a bit cooked

    use_base(KC_ESC);
    use_base(KC_CAPS);
    map_base(KC_RCMD, LSPACE).to_single(QK_LAYER_LOCK);

    map_base(KC_LWIN).to(OSM_LGUI);

    // Assorted typical keyboard keys
    map_base("12").to(KC_BRID, KC_BRIU);
    map_base("34").to(KC_MCTRL, KC_LNPAD); // mac keys (but idk if windows uses)
    map_base('b', RIGHT_B).to_single(BAT_LVL);

    // Media
    map_base("789").to(KC_MEDIA_PREV_TRACK, KC_MEDIA_PLAY_PAUSE, KC_MEDIA_NEXT_TRACK);
    map_base("0-=").to(KC_MUTE, KC_VOLD, KC_VOLU);

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
})
static_assert(kLayerDef<Layer::FN2>.matrix[2][0] == KC_CAPS);
static_assert(kLayerDef<Layer::FN2>.matrix[3][6] == BAT_LVL);
static_assert(kLayerDef<Layer::FN2>.matrix[3][7] == BAT_LVL);

DEFINE_LAYER(SELECT, {
    fill(KC_NO);
    map_base(KC_ESC).to(kMO(layer_self));
    map_base(KC_CAPS).to(kMO(layer_self));
    map_base("c").to(kTG(Layer::CONTROL));
    map_base('b', RIGHT_B).to_single(kTG(Layer::BASE));
    map_base("m").to(kTG(Layer::MOUSE));
})

DEFINE_LAYER(MOUSE, {
    fill(KC_NO);
    using key_defs::mouse_buttons;

    map_base_span("18").to(mouse_buttons<>);
    map_base(LSPACE, RSPACE, KC_ENTER).to_single(MS_BTN1);
    map_base(KC_LOPT, FN1, FN2, KC_RCMD, KC_SEMI).to_single(MS_BTN2);

    map_base("qe").to(mouse_buttons<1, 2>);
    map_base("ui").to(mouse_buttons<1, 2>); // right handed

    map_base("hjkl").to(mouse_hjkl);
    map_base("wasd").to(mouse_wasd);

    map_base(KC_LSFT, KC_RSFT).to_single(MS_ACL0);
    map_base(KC_LCTL).to(MS_ACL2);

    // Speed adjustment
    map_base(KNOB_PRESS, KNOB_CCW, KNOB_CW)
        .to(VAR_RESET, VAR_MINUS, VAR_PLUS);

    map_base(KC_CAPS).to(kTG(layer_self));
})

// Select the variable adjusted by the encoder: Effect, Hue, Speed, sAturation, Brightness, sNap-click
constexpr inline auto kControlKeys = ce_map<Key, control::Var>(control::Var::NONE, {
    { 'e', control::Var::EFFECT },
    { 'h', control::Var::HUE },
    { 's', control::Var::SPEED },
    { 'a', control::Var::SATURATION },
    { 'b', control::Var::BRIGHTNESS },
    { 'n', control::Var::SNAP_CLICK },
});

[[gnu::optimize("O3")]] // Fully folds the small-map pack expansion into constant comparisons/returns.
control::Var control::key_to_var(qmk_key_t base_key) {
    return kControlKeys[base_key];
}

DEFINE_LAYER(CONTROL, {
    fill(KC_NO);
    map_base(KC_ESC).to(kMO(layer_self));
    map_base(KC_CAPS).to(kMO(layer_self));

    //TODO: move to a better layer IMO ('wireless' or other). I like how control does nothing if not using knob
    map_base_span("qr").to(BT_HST1, BT_HST2, BT_HST3, P2P4G); // Keeping this because its printed on the keys

    for (auto& [key, var] : kControlKeys) {
        map_base(key).to(CONTROL_VAR);
    }
    map_base(KNOB_PRESS, KNOB_CCW, KNOB_CW)
        .to(VAR_RESET, VAR_MINUS, VAR_PLUS);
})

extern "C" {

namespace {
constexpr PROGMEM KeymapDef<> KEYMAP;
}
const auto& keymaps = KEYMAP.keymap;

#ifdef ENCODER_MAP_ENABLE
const auto& encoder_map = KEYMAP.encoder_map;
#endif
} // extern "C"

// QMK needs mutable storage
auto key_combos = kQmkComboArray;

extern "C" {

#include "key_overrides.hpp"
#include "keymap_introspection.c"

static_assert(NUM_KEYMAP_LAYERS_RAW == kNumLayers);
static_assert(ARRAY_SIZE(key_combos) == kNumEnabledCombos);
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
