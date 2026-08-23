#pragma once

#include "compat.hpp"
#include "common.hpp"
#include "config.h"
#include "type_list.hpp"
#include "util.hpp"

#include <array>
#include <string.h>
#include <concepts>
#include <tuple>
#include <type_traits>
#include <utility>

extern "C" {
#include "eeconfig.h"
}

namespace eeconfig {

enum class Block : uint8_t {
    MOUSE,
    ENUM_END,
};
inline constexpr auto kNumConfigBlocks = sz(Block::ENUM_END);

template<Block block>
struct ConfigBlockBase {
    constexpr static Block config_block_name = block;
};

template<Block block>
struct BlockDef;

template<Block block>
using block_t = typename BlockDef<block>::type;

template<Block block>
inline block_t<block> const& read();

template<class BlockT>
requires std::same_as<BlockT, block_t<BlockT::config_block_name>>
inline BlockT const& read() {
    return read<BlockT::config_block_name>();
}

template<Block block>
inline void write(block_t<block> const& value);

template<class BlockT>
requires std::same_as<BlockT, block_t<BlockT::config_block_name>>
inline void write(BlockT const& block) {
    return write<BlockT::config_block_name>(block);
}

inline void invalidate();

} // namespace eeconfig
