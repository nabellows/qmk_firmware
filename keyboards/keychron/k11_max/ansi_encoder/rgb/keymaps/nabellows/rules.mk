VIA_ENABLE = yes
CAPS_WORD_ENABLE = yes
COMBO_ENABLE = yes
COMMAND_ENABLE = no
KEY_OVERRIDE_ENABLE = yes
LAYER_LOCK_ENABLE = yes
TAP_DANCE_ENABLE = yes

OPT_DEFS += -DWIRELESS_RAW_ENABLE

KEYMAP_INTROSPECTION = no

CXXFLAGS += -fconcepts-diagnostics-depth=3 -fno-exceptions
CXXFLAGS += -Wswitch -Wswitch -Werror=switch
CXXFLAGS += -Wswitch-enum -Wswitch -Werror=switch-enum
CXXFLAGS += -Wno-error=undef -Wno-undef

EXTRAINCDIRS += $(KEYMAP_PATH)/util

# THIS FILE CANNOT BE NAMED keymap.cpp NOR main.cpp BECAUSE QMK BUILD SYSTEM IGNORES IT
SRC += Keymap.cpp
SRC += Overrides.cpp

# ALLOW_WARNINGS=yes
