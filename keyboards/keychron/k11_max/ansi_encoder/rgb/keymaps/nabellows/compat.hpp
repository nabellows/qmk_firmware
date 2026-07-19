#pragma once

#define STR_IMPL(x) #x
#define STR(x) STR_IMPL(x)

#if __cplusplus

#define restrict
#include <string_view>
#include <type_traits>
#include "info_config.h"

constexpr int count_commas(std::string_view str) {
    int commas = 0;
    for (char c : str) commas += c == ',';
    return commas;
}
constexpr int kNumEncoders = count_commas(STR(ENCODER_A_PINS)) + 1;
#define NUM_ENCODERS kNumEncoders

#define ARRAY_SIZE(...) std::extent_v<decltype(__VA_ARGS__)>

#endif
