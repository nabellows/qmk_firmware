#pragma once

#include "compat.hpp"

#include <cstring>
#include "compat.hpp"
#include "eeconfig.hpp"
#include "type_list.hpp"
#include "util.hpp"


namespace eeconfig {

template<class=void>
struct AutoImpl {
    static constexpr decltype(auto) unpack_blocks(auto f) {
        return unpack_enum<Block>(f);
    }

    static constexpr decltype(auto) unpack_block_defs(auto f) {
        return unpack_blocks([&]<Block...blocks>() -> decltype(auto) {
            return tinvoke_t<BlockDef<blocks>...>(f);
        });
    }

    static constexpr decltype(auto) unpack_block_types(auto f) {
        return unpack_blocks([&]<Block...blocks>() -> decltype(auto) {
            return tinvoke_t<block_t<blocks>...>(f);
        });
    }

    using Unpacked = decltype(unpack_block_types(lMakeTypeList))::template apply_to<std::tuple>;
    inline static constexpr sz kPackedSize = unpack_block_types([]<class...Blocks>(){
        static_assert((std::is_trivially_copyable_v<Blocks> && ...));
        return (sizeof(Blocks) + ... + 0);
    });
    static_assert(kPackedSize <= EECONFIG_USER_DATA_SIZE);
    using Packed = std::array<uint8_t, kPackedSize>;

    inline static Unpacked cache{};
    inline static bool loaded = false;
    inline static bool valid = false;

    inline static bool load() {
        if (loaded) return valid;

        loaded = true;
        valid = eeconfig_is_user_datablock_valid();
        if (!valid) return false;

        Packed packed;
        if (eeconfig_read_user_datablock(packed.data(), 0, packed.size()) != packed.size()) {
            valid = false;
            return false;
        }
        unpack(packed);
        return true;
    }

    template<Block block>
    inline static block_t<block> const& read() {
        load();
        return get<sz(block)>(cache);
    }

    template<Block block>
    inline static void write(block_t<block> const& value) {
        load(); // Ensure we write back ALL blocks correctly if we never loaded before
        get<sz(block)>(cache) = value;

        const auto packed = pack();
        eeconfig_update_user_datablock(packed.data(), 0, packed.size());
        valid = true;
    }

    inline static void invalidate() {
        loaded = false;
        valid = false;
    }

    template<sz I>
    consteval static sz block_offset() {
        return invoke_with_index_seq<I>([]<sz...prior>(){
            return (sizeof(block_t<Block(prior)>) + ... + 0);
        });
    }

    inline static void unpack(Packed const& packed) {
        return invoke_with_index_seq<kNumConfigBlocks>([&]<sz...is>(){
            (memcpy(&get<is>(cache), packed.data() + block_offset<is>(), sizeof(get<is>(cache))), ...);
        });
    }

    inline static Packed pack() {
        Packed packed;
        invoke_with_index_seq<kNumConfigBlocks>([&]<sz...is>(){
            (memcpy(packed.data() + block_offset<is>(), &get<is>(cache), sizeof(get<is>(cache))), ...);
        });
        return packed;
    }
};

// Stupid trick to make clangd shutup
#ifdef EECONFIG_IMPL
template<Block block>
inline block_t<block> const& read() {
    return AutoImpl<>::read<block>();
}

template<Block block>
inline void write(block_t<block> const& value) {
    return AutoImpl<>::write<block>(value);
}

inline void invalidate() {
    return AutoImpl<>::invalidate();
}
#endif

} // namespace eeconfig
