#pragma once

#include <optional>
#include "compat.hpp"
#include "defer.hpp"

extern "C" {
#include "color.h"
#include "rgb_matrix.h"
}

namespace rgb {

struct Snapshot {
    bool enabled;
    uint8_t mode;
    hsv_t hsv;
    uint8_t speed;

    static Snapshot take() {
        return {
            .enabled = (bool) rgb_matrix_is_enabled(),
            .mode    = rgb_matrix_get_mode(),
            .hsv     = rgb_matrix_get_hsv(),
            .speed   = rgb_matrix_get_speed(),
        };
    }

    void restore() const {
        if (!enabled) {
            rgb_matrix_disable_noeeprom();
            return;
        }

        rgb_matrix_enable_noeeprom();
        rgb_matrix_mode_noeeprom(mode);
        rgb_matrix_sethsv_noeeprom(hsv.h, hsv.s, hsv.v);
        rgb_matrix_set_speed_noeeprom(speed);
    }
};

inline std::optional<rgb_t> indicator_overlay;

inline void flash(rgb_t color, uint32_t ms, bool black_pads = true) {
    auto& defer = kDeferredExecutor<DeferredExecutors::RGB_FLASH>;
    static std::optional<Snapshot> snap{};
    bool nested;
    if (!snap) { // if nested, assume old state
        snap = Snapshot::take();
        nested = false;
    } else {
        nested = true;
    }
    constexpr static auto restore = [](){
        if (snap) {
            indicator_overlay = {};
            snap->restore();
            snap = {};
        }
    };
    indicator_overlay = color;
    if (black_pads && rgb_matrix_is_enabled()) {
        static uint32_t black_ms;
        black_ms = ms;
        // Super ugly, debatable if housekeeping timer task is better, but it causes global variable explosion/per-function housekeeping needs
        // TODO: alternative is instead of DeferredExecutors only having one callback, allow them to declare a generic poll body which checks their
        // own timer, then in here we could more sane-ly write this
        rgb_matrix_disable_noeeprom();
        defer = { nested ? 0 : black_ms / 3, [](){
            rgb_matrix_enable_noeeprom();
            defer = { black_ms, []() {
                rgb_matrix_disable_noeeprom();
                defer = { black_ms / 3, restore };
            } };
        } };
    } else {
        rgb_matrix_enable_noeeprom();
        defer = { ms, restore };
    }
}

} // namespace rgb
