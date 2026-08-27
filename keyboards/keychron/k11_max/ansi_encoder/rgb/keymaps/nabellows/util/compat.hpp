#pragma once

// quantum/util.h stole 'STR', and too hard to get rid of the re-definition warnings
#define TO_STR_IMPL(x) #x
#define TO_STR(x) TO_STR_IMPL(x)

#if __cplusplus

#define restrict
#include <array>
#include <string_view>
#include <type_traits>
#include "info_config.h"

template<class T>
constexpr size_t kArraySize = std::extent_v<T>;

template<class T, size_t N>
constexpr size_t kArraySize<std::array<T, N>> = N;

#define ARRAY_SIZE(...) (kArraySize<std::remove_cvref_t<decltype(__VA_ARGS__)>>)

//horrible, horrible hack. pragma-once should also make this stick even if double included
#ifdef ENCODER_MAP_ENABLE
#   undef ENCODER_MAP_ENABLE
    extern "C" {
#   include "encoder.h"
    }
#   define NUM_DIRECTIONS 2
#   define ENCODER_CCW_CW(ccw, cw) { (cw), (ccw) }
#   define ENCODER_MAP_ENABLE

    constexpr int count_commas(std::string_view str) {
        int commas = 0;
        for (char c : str) commas += c == ',';
        return commas;
    }
    constexpr int kNumEncoders = count_commas(TO_STR(ENCODER_A_PINS)) + 1;
#   undef NUM_ENCODERS
#   define NUM_ENCODERS kNumEncoders
#endif

// Careful not to export something which is not actually used...
#define QMK_INLINE extern "C" __attribute__((used, externally_visible)) inline

#endif

