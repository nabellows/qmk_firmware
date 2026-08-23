#pragma once

#include "compat.hpp"
#include "config.h"

#include <bit>
#include <stdint.h>
#include <string.h>

extern "C" {
#include "eeconfig.h"
#include "timer.h"
}

extern "C" uint16_t c_offsets[];

namespace mouse {

enum SpeedIndex : uint8_t {
    NORMAL = 0,
    SLOW   = 1,
    FAST   = 3,
};

struct SpeedConfig {
    uint8_t normal;
    uint8_t slow;
    uint8_t fast;
    uint8_t magic;
};

static_assert(sizeof(SpeedConfig) == sizeof(uint32_t));

inline constexpr uint8_t kConfigMagic = 0xA7;
inline constexpr uint8_t kMinSpeed = 1;
inline constexpr uint8_t kMaxSpeed = 127;
inline constexpr uint16_t kSaveDelayMs = 1000;

inline constexpr auto kDefaultSpeeds = []{
    std::array<uint8_t, 4> res{};
    res[SpeedIndex::NORMAL] = MK_C_OFFSET_UNMOD;
    res[SpeedIndex::SLOW] = MK_C_OFFSET_0;
    res[SpeedIndex::FAST] = MK_C_OFFSET_2;
    return res;
}();

inline uint8_t speed_mods = 0;
inline bool speed_dirty = false;
inline uint16_t speed_save_timer = 0;

inline SpeedIndex selected_speed() {
    return speed_mods
        ? SpeedIndex(std::bit_width(speed_mods) - 1)
        : SpeedIndex::NORMAL;
}

inline void load_speeds() {
    const uint32_t raw = eeconfig_read_user();
    SpeedConfig config;
    memcpy(&config, &raw, sizeof(config));
    if (config.magic != kConfigMagic ||
        config.normal < kMinSpeed || config.normal > kMaxSpeed ||
        config.slow < kMinSpeed || config.slow > kMaxSpeed ||
        config.fast < kMinSpeed || config.fast > kMaxSpeed) return;

    c_offsets[SpeedIndex::NORMAL] = config.normal;
    c_offsets[SpeedIndex::SLOW] = config.slow;
    c_offsets[SpeedIndex::FAST] = config.fast;
}

inline void save_speeds() {
    const SpeedConfig config = {
        .normal = uint8_t(c_offsets[SpeedIndex::NORMAL]),
        .slow = uint8_t(c_offsets[SpeedIndex::SLOW]),
        .fast = uint8_t(c_offsets[SpeedIndex::FAST]),
        .magic = kConfigMagic,
    };
    uint32_t raw;
    memcpy(&raw, &config, sizeof(raw));
    eeconfig_update_user(raw);
    speed_dirty = false;
}

inline void adjust_speed(bool increase) {
    uint16_t &speed = c_offsets[selected_speed()];
    if (increase && speed < kMaxSpeed) ++speed;
    if (!increase && speed > kMinSpeed) --speed;
    speed_dirty = true;
    speed_save_timer = timer_read();
}

inline void reset_speed() {
    const SpeedIndex speed = selected_speed();
    c_offsets[speed] = kDefaultSpeeds[speed];
    speed_dirty = true;
    speed_save_timer = timer_read();
}

inline void set_speed_modifier(SpeedIndex speed, bool pressed) {
    const uint8_t mask = 1 << speed;
    if (pressed) speed_mods |= mask;
    else speed_mods &= ~mask;
}

inline void init_eeprom() {
    speed_dirty = false; // Avoid pending write if reset was called
}

inline void housekeeping_task() {
    if (speed_dirty && timer_elapsed(speed_save_timer) > kSaveDelayMs) {
        save_speeds();
    }
}

} // namespace mouse
