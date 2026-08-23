#pragma once

#include "compat.hpp"
#include "config.h"
#include "eeconfig/eeconfig.hpp"

#include <bit>
#include <stdint.h>

extern "C" {
#include "timer.h"
}

extern "C" uint16_t c_offsets[];

namespace mouse {

struct Config : eeconfig::ConfigBlockBase<eeconfig::Block::MOUSE> {
    uint8_t normal;
    uint8_t slow;
    uint8_t fast;
};

} // namespace mouse

template<>
struct eeconfig::BlockDef<eeconfig::Block::MOUSE> {
    using type = mouse::Config;
};

namespace mouse {

enum SpeedIndex : uint8_t {
    NORMAL = 0,
    SLOW   = 1,
    FAST   = 3,
};

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
    const Config &config = eeconfig::read<Config>();
    if (
        config.normal < kMinSpeed || config.normal > kMaxSpeed ||
        config.slow < kMinSpeed || config.slow > kMaxSpeed ||
        config.fast < kMinSpeed || config.fast > kMaxSpeed) return;

    c_offsets[SpeedIndex::NORMAL] = config.normal;
    c_offsets[SpeedIndex::SLOW] = config.slow;
    c_offsets[SpeedIndex::FAST] = config.fast;
}

inline void save_speeds() {
    const Config config = {
        .normal = uint8_t(c_offsets[SpeedIndex::NORMAL]),
        .slow = uint8_t(c_offsets[SpeedIndex::SLOW]),
        .fast = uint8_t(c_offsets[SpeedIndex::FAST]),
    };
    eeconfig::write(config);
    speed_dirty = false;
}

inline uint8_t get_speed(SpeedIndex speed) {
    return uint8_t(c_offsets[speed]);
}

inline void mark_dirty() {
    speed_dirty = true;
    speed_save_timer = timer_read();
}

inline void set_speed(SpeedIndex speed, uint8_t value) {
    c_offsets[speed] = value < kMinSpeed ? kMinSpeed
                     : value > kMaxSpeed ? kMaxSpeed
                     : value;
    mark_dirty();
}

inline void adjust_speed(bool increase) {
    uint16_t &speed = c_offsets[selected_speed()];
    if (increase && speed < kMaxSpeed) ++speed;
    if (!increase && speed > kMinSpeed) --speed;
    mark_dirty();
}

inline void reset_speed() {
    const SpeedIndex speed = selected_speed();
    c_offsets[speed] = kDefaultSpeeds[speed];
    mark_dirty();
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
