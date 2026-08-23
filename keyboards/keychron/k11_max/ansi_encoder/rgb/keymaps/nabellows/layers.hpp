#pragma once

#include "compat.hpp"
#include "util.hpp"

extern "C" {
#include "action_layer.h"
#include "quantum_keycodes.h"
}

constexpr layer_state_t kAllLayers = ~0;

enum class Layer : layer_state_t {
    NORMIE, WIN_BASE = NORMIE,
    BASE, MAC_BASE = BASE,
    FN1,
    FN2,
    SELECT,
    CONTROL,
    MOUSE,

    LAYER_ENUM_END,
    LAYOUT_BASE, // Hack to set non-qmk codes - doesnt get translated into a real layer (at least not by default, can clone)
};

constexpr static int kNumLayers = int(Layer::LAYER_ENUM_END);

#ifdef DYNAMIC_KEYMAP_ENABLE
static_assert(kNumLayers <= MAX_LAYER, "VIA layers cannot fit default layers");
#endif

constexpr decltype(auto) for_each_layer(auto F) {
    return invoke_with_index_seq<kNumLayers>([&]<sz...is>{
        (F.template operator()<Layer(is)>(), ...);
    });
}

constexpr auto kMO(Layer layer) { return MO(layer_state_t(layer)); }
constexpr auto kTO(Layer layer) { return TO(layer_state_t(layer)); }
constexpr auto kTG(Layer layer) { return TG(layer_state_t(layer)); }
constexpr auto kOSL(Layer layer) { return OSL(layer_state_t(layer)); }

inline auto default_layer_set(Layer layer) {
    return default_layer_set(1 << layer_state_t(layer));
}

