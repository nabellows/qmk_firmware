#pragma once

// Tap-hold config (https://docs.qmk.fm/tap_hold)
// Disables a feature where quickly tapping and then re-holding a mod-tap key will force treat as a tap-tap to support
// default hold-and-repeat behavior that is otherwise made impossible by tap-hold configs. I don't generally want this (yet??).
// If I did, I would need to define it per key as it makes zero sense for ctrl/esc
#define QUICK_TAP_TERM 0
// The tap vs hold behavior that most matches me - I am tolerant of any keypress being considered a mod/hold combo while a mod key being down
#define HOLD_ON_OTHER_KEY_PRESS
#define CAPS_WORD_INVERT_ON_SHIFT // CAPS_WORD by default would disable when shift is pressed, now it will invert shift for _/-, a-z, space
#define CAPS_WORD_IDLE_TIMEOUT 0 // There is a feature to auto timeout, lets not use it for now

#define COMBO_TERM_PER_COMBO
#define KEY_OVERRIDE_INCLUDE_WEAK_MODS

// DEBUGGING ONLY (while info.json is tampered with)
#ifdef DEBUG
#define DEBUG_KEY_OVERRIDE
#undef APDAPTIVE_NKRO_ENABLE
#endif
