#pragma once

#include <type_traits>

#define ARRAY_SIZE(...) std::extent_v<decltype(__VA_ARGS__)>
