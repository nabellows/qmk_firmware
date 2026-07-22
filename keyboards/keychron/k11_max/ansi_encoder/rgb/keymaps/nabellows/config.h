#pragma once

// Tap-hold config (https://docs.qmk.fm/tap_hold)
// Disables a feature where quickly tapping and then re-holding a mod-tap key will force treat as a tap-tap to support
// default hold-and-repeat behavior that is otherwise made impossible by tap-hold configs. I don't generally want this (yet??).
// If I did, I would need to define it per key as it makes zero sense for ctrl/esc
#define QUICK_TAP_TERM 0
#define CAPS_WORD_INVERT_ON_SHIFT // CAPS_WORD by default would disable when shift is pressed, now it will invert shift for _/-, a-z, space
#define CAPS_WORD_IDLE_TIMEOUT 5000 // Disable CAPS_WORD if nothing pressed for 5 seconds

#define PERMISSIVE_HOLD_PER_KEY
#define HOLD_ON_OTHER_KEY_PRESS_PER_KEY

#define COMBO_TERM_PER_COMBO
#define KEY_OVERRIDE_INCLUDE_WEAK_MODS
