#pragma once

#include "mouse.hpp"

extern "C" {
#include "raw_hid.h"
#include "timer.h"
#include "via.h"
}

namespace via_config {

enum class MouseViaValue : uint8_t {
    NORMAL = 1,
    SLOW,
    FAST,
};

inline constexpr uint8_t kUiSyncRequest = 0x16;
inline constexpr uint8_t kUiSyncVersion = 0x01;
inline constexpr uint8_t kUiSyncChannelValuePairs = 0x01;
inline constexpr uint8_t kReportSize = 32;
inline constexpr uint16_t kUiSyncDelayMs = 50;

inline uint8_t pending_mouse_sync = 0;
inline uint16_t mouse_sync_timer = 0;

inline mouse::SpeedIndex mouse_speed(MouseViaValue value) {
    switch (value) {
        case MouseViaValue::NORMAL: return mouse::SpeedIndex::NORMAL;
        case MouseViaValue::SLOW:   return mouse::SpeedIndex::SLOW;
        case MouseViaValue::FAST:   return mouse::SpeedIndex::FAST;
    }
    __builtin_unreachable();
}

inline MouseViaValue mouse_via_value(mouse::SpeedIndex speed) {
    switch (speed) {
        case mouse::SpeedIndex::NORMAL: return MouseViaValue::NORMAL;
        case mouse::SpeedIndex::SLOW:   return MouseViaValue::SLOW;
        case mouse::SpeedIndex::FAST:   return MouseViaValue::FAST;
        default: __builtin_unreachable();
    }
}

inline void request_mouse_sync(mouse::SpeedIndex speed = mouse::selected_speed()) {
    pending_mouse_sync |= 1 << uint8_t(mouse_via_value(speed));
    mouse_sync_timer = timer_read();
}

inline void housekeeping_task() {
    if (!pending_mouse_sync || timer_elapsed(mouse_sync_timer) <= kUiSyncDelayMs) return;

    uint8_t report[kReportSize] = {
        kUiSyncRequest,
        kUiSyncVersion,
        kUiSyncChannelValuePairs,
        0,
    };
    for (uint8_t value = uint8_t(MouseViaValue::NORMAL); value <= uint8_t(MouseViaValue::FAST); ++value) {
        if (!(pending_mouse_sync & (1 << value))) continue;
        const uint8_t target = 4 + report[3] * 2;
        report[target] = id_custom_channel;
        report[target + 1] = value;
        ++report[3];
    }
    pending_mouse_sync = 0;
    raw_hid_send(report, sizeof(report));
}

inline void custom_value_command(uint8_t *data, uint8_t length) {
    if (length < 4 || data[1] != id_custom_channel) {
        data[0] = id_unhandled;
        return;
    }

    switch (data[0]) {
        case id_custom_get_value:
        case id_custom_set_value: {
            const MouseViaValue value = MouseViaValue(data[2]);
            if (value < MouseViaValue::NORMAL || value > MouseViaValue::FAST) {
                data[0] = id_unhandled;
                return;
            }
            const mouse::SpeedIndex speed = mouse_speed(value);
            if (data[0] == id_custom_get_value) {
                data[3] = mouse::get_speed(speed);
            } else {
                mouse::set_speed(speed, data[3]);
            }
            return;
        }

        case id_custom_save:
            mouse::save_speeds();
            return;

        default:
            data[0] = id_unhandled;
    }
}

} // namespace via_config
