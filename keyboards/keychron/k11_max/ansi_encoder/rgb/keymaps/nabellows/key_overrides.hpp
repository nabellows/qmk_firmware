#pragma once

#include "compat.hpp"
#include "caps_word.hpp"


extern "C" {
#include "process_key_override.h"

constexpr const key_override_t* key_overrides[] = {
    &caps_word::shift_space_override
};

constexpr sz kNumKeyOverrides = std::extent_v<decltype(key_overrides)>;

}
