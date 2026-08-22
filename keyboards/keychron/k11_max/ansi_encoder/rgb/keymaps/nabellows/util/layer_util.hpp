#pragma once

#include "compat.hpp"

#include <concepts>
#include <iterator>
#include <ranges>
#include <string_view>
#include <type_traits>
#include <utility>

#include "key_util.hpp"
#include "keycodes.h"
#include "layers.hpp"
#include "util.hpp"

template<Layer layer>
struct LayerDef;
template<Layer layer>
constexpr LayerDef<layer> kLayerDef;

#define DEFINE_LAYER(layer, ...) template<> struct LayerDef<Layer::layer> : LayerDefBase<> { constexpr static bool exists() { return true; } consteval LayerDef() __VA_ARGS__ }; static_assert(kLayerDef<Layer::layer>.exists());

template<auto layer_base = Layer::LAYOUT_BASE> // Avoid any direct instantiations of the base-def
struct LayerDefBase {
    Key matrix[MATRIX_ROWS][MATRIX_COLS];
#ifdef ENCODER_MAP_ENABLE
	Key encoder_map[NUM_ENCODERS][NUM_DIRECTIONS];
#else
    std::array<std::array<Key, 0>, 0> encoder_map{};
#endif
protected:
    // One warning is this replaces KC_NO, but it should be undisturbed in base...
    constexpr void fill(Key k) {
        ce_fill(k, matrix);
        ce_fill(k, encoder_map);
    }
    constexpr void trans() { fill(KC_TRNS); }
    constexpr void set(LayerDefBase const& rhs) {
        ce_copy(rhs.matrix, matrix);
        ce_copy(rhs.encoder_map, encoder_map);
    }

    template<Layer layer>
    constexpr void clone() { set(kLayerDef<layer>); }
    constexpr void clone_base() { clone<layer_base>(); }

private:
    template<bool strict, bool single = strict>
    constexpr KeyOutputRange auto lookup_base_key_to_dst_range(Key key) {
        using Index = md_index<2>;
        if constexpr (single) {
            Key* res = nullptr;
            ce_for_each_val<&LayerDefBase::matrix, &LayerDefBase::encoder_map>([&]<auto member>{
                Index index{};
                while (index_of(kLayerDef<layer_base>.*member, key, index)) {
                    if (res) constexpr_fail("Ambiguous mapping");
                    auto [row, col] = index;
                    ++index.back();
                    res = &(this->*member)[row][col];
                }
            });
            if (strict && !res) constexpr_fail("Mapping not found in base! (required by strict=true)");
            return std::span(res, res ? 1 : 0);
        } else { // This version inevitably a bit worse on performance
            bool found = false;
            std::vector<Key*> key_targets;
            ce_for_each_val<&LayerDefBase::matrix, &LayerDefBase::encoder_map>([&]<auto member>{
                auto& maps = kLayerDef<layer_base>.*member;
                Index index{};
                while (index_of(maps, key, index)) {
                    auto [row, col] = index;
                    key_targets.push_back(&(this->*member)[row][col]);
                    ++index.back();
                    found = true;
                }
            });
            if (strict && !found) constexpr_fail("Mapping not found in base! (required by strict=true)");
            return key_targets;
        }
    }

    template<bool strict, KeyOutputRange Dst>
    constexpr void map_range_impl(KeyInputRange auto&& src_r, Dst&& dst_r) {
        auto src = std::ranges::begin(src_r);
        auto src_end = std::ranges::end(src_r);
        auto dst = std::ranges::begin(dst_r);
        auto dst_end = std::ranges::end(dst_r);
        constexpr bool infinite_src = std::is_same_v<std::unreachable_sentinel_t, decltype(src_end)>;
        constexpr bool infinite_dst = std::is_same_v<std::unreachable_sentinel_t, decltype(dst_end)>;
        if (infinite_src && infinite_dst) constexpr_fail("Error: Both mapping source/destination ranges are infinite");

        while (src != src_end && dst != dst_end) {
            if constexpr (KeyRefOutputRange<Dst>) {
                *dst = static_cast<Key>(*src);
            } else {
                static_assert(KeyPtrOutputRange<Dst>, "Unexpected output range type");
                **dst = static_cast<Key>(*src);
            }
            ++src;
            ++dst;
        }
        const bool src_finished = infinite_src || src == src_end;
        const bool dst_finished = infinite_dst || dst == dst_end;

        if (strict && !dst_finished) constexpr_fail("Did not map all keys in destination (source/new-keys range is shorter than destination/base-keys)");
        if (strict && !src_finished) constexpr_fail("Not all source keys were mapped (source/new-keys range is longer than destination/base-keys)");
    }

    constexpr static auto&& common_cast(KeyInputRange auto&& x) { return FWD(x); }
    template<sz N>
    constexpr static auto common_cast(const char (&str)[N]) { return KeyList{str}; }
    template<KeyListish...A>
    requires (!kIsKeyList<A> && ...) // prefer the above for perfect forward
    constexpr static auto common_cast(A&&...args) { return KeyList{ FWD(args)... }; }

    constexpr static Key* key_ptr(Key& k) { return &k; }
    constexpr static Key* key_ptr(Key* p) { return p; }

    constexpr static KeyInputRange decltype(auto) to_src_range(auto&&...args) { return common_cast(FWD(args)...); }

    template<bool strict, KeyInputRange R>
    requires (not InfiniteRange<R>)
    constexpr KeyOutputRange auto to_dst_range_from_base_impl_(R&& r)
    {
        std::vector<Key*> key_targets;
        if constexpr (std::ranges::sized_range<R>) {
            key_targets.reserve(std::ranges::size(FWD(r)));
        }
        for (Key& base_key : FWD(r)) {
            auto dst_range = lookup_base_key_to_dst_range<strict>(base_key);
            for (auto&& dst_key : dst_range) {
                key_targets.push_back(key_ptr(dst_key));
            }
        }
        return key_targets;
    }

    template<bool strict, InfiniteKeyInputRange R>
    constexpr InfiniteKeyOutputRange auto to_dst_range_from_base_impl_(R&& r) {
        return infinite_range(std::ranges::owning_view(FWD(r)) | std::views::transform([this](Key k) {
            return lookup_base_key_to_dst_range<strict>(k);
        }) | std::views::join);
    }

    template<bool strict>
    constexpr KeyOutputRange decltype(auto) to_dst_range_from_base(auto&&...args) { return to_dst_range_from_base_impl_<strict>(common_cast(FWD(args)...)); }

protected:

    template<bool strict = true>
    constexpr void map_base_pairs(auto&& from, auto&& to) {
        map_range_impl<strict>(to_src_range(FWD(to)), to_dst_range_from_base<strict>(FWD(from)));
    }

    // Guaranteed to point into self. Was considering using std::span as base of operations,
    // but technically UB/not constexpr to do pointer range checks to see for example,
    // what member it is from (or if its an arbitrary collection of Keys)
    class KeySpan {
        const Key* base_data;
        Key* data;
        sz raw_size;
    public:
        constexpr KeySpan(LayerDefBase& self, auto member, sz from_row, sz from_col, sz to_row, sz to_col) {
            if (from_row > to_row || from_col > to_col) constexpr_fail("Invalid Key Span (negative direction)");
            base_data = &(kLayerDef<layer_base>.*member)[from_row][from_col];
            data = &(self.*member)[from_row][from_col];
            raw_size = &(self.*member)[to_row][to_col] - data + sz(1);
        }

        constexpr auto to_dst_range() {
            std::vector<Key*> real_keys;
            real_keys.reserve(raw_size);
            for (int i = 0; i < raw_size; ++i) {
                if (base_data[i] != KC_NO) {
                    real_keys.push_back(&data[i]);
                }
            }
            return real_keys;
        }
    };

    constexpr static KeyInputRange auto common_cast(KeySpan span) {
        return span.to_dst_range();
    }

    constexpr KeySpan base_span(Key from, Key to) {
        std::optional<KeySpan> res;
        ce_for_each_val<&LayerDefBase::matrix, &LayerDefBase::encoder_map>([&]<auto member>{
            auto& base_ref = kLayerDef<layer_base>.*member;
            auto from_index = index_of(base_ref, from);
            auto to_index = index_of(base_ref, to);
            if (from_index.has_value() != to_index.has_value()) constexpr_fail("Bad span, base matrix/map contains one key and not the other");
            if (from_index) {
                auto [from_row, from_col] = *from_index;
                auto [to_row, to_col] = *to_index;
                if (from_row != to_row) constexpr_fail("Bad span: Span must lie in a single row"); // Unsure if this is preferred
                if (res) constexpr_fail("Ambiguous span");
                res = KeySpan(*this, member, from_row, from_col, to_row, to_col);
            }
        });
        if (!res) constexpr_fail("Could not find matching span");
        return *res;
    }

    constexpr KeySpan base_span(KeyList<2> pair) {
        return base_span(pair.front(), pair.back());
    }

    constexpr KeySpan map_row(sz row) {
        return KeySpan(*this, &LayerDefBase::matrix, row, 0, row, MATRIX_COLS-1);
    }

    class MappingTracker : CrtpMoveWrapper<MappingTracker> {
        bool needs_map = true;
    public:
        constexpr MappingTracker() = default;
        constexpr MappingTracker(MappingTracker&&) = default;
        constexpr ~MappingTracker() {
            if (needs_map) constexpr_fail("Forgot to map me!");
        }
        constexpr bool mapped(bool val = true) { return needs_map = !val; }
        constexpr void move_out() { needs_map = false; }
    };

    template<bool strict, KeyOutputRange Dst>
    class Mapper {
        LayerDefBase& def;
        Dst dst;
        MappingTracker tracker;
    public:
        constexpr Mapper(LayerDefBase& def, Dst dst)
        : def{ def }, dst{ std::move(dst) } {}

        template<class...Args>
        constexpr void to(Args&&... args) {
            def.map_range_impl<strict>(to_src_range(FWD(args)...), dst);
            tracker.mapped();
        }

        constexpr void to_single(Key key) {
            static_assert(InfiniteRange<decltype(repeat_view(key))>);
            def.map_range_impl<strict>(repeat_view(key), dst);
            tracker.mapped();
        }

        template<sz N>
        constexpr void to_range(KeyRange<N> range) {
            def.map_range_impl<strict>(range, dst);
            tracker.mapped();
        }
    };
private:
    template<bool strict, class Dst>
    constexpr auto mapper(LayerDefBase& def, Dst dst) { return Mapper<strict, Dst>(def, std::move(dst)); }
protected:

    template<bool strict = true, class...Args>
    [[nodiscard]]
    constexpr auto map_base(Args&&... args) {
        return mapper<strict>(*this, to_dst_range_from_base<strict>(FWD(args)...));
    }

    template<bool strict = true>
    constexpr auto map_base_span(Key from, Key to) {
        return mapper<strict>(*this, base_span(from, to).to_dst_range());
    }

    template<bool strict = true>
    constexpr auto map_base_span(KeyList<2> pair) {
        return map_base_span<strict>(pair.front(), pair.back());
    }

    template<bool strict = true, class...Args>
    constexpr auto use_base(Args&&... args) {
        return map_range_impl<strict>(to_src_range(FWD(args)...), to_dst_range_from_base<strict>(FWD(args)...));
    }
};

template<Layer layer>
struct LayerDef : LayerDefBase<> {
    static_assert(VFalse<layer>, "Layer definition not found!");
    constexpr static bool exists() { return false; }
};

template<class = void>
struct KeymapDef {
#define LAYOUT(layer) [layer_state_t(layer)] = LAYOUT_69_ansi
// Just self documenting
#define MAP(KEY, NEW_MAP) NEW_MAP

    using enum Layer;
    Key keymap[kNumLayers][MATRIX_ROWS][MATRIX_COLS]{};
#ifdef ENCODER_MAP_ENABLE
    Key encoder_map[kNumLayers][NUM_ENCODERS][NUM_DIRECTIONS]{};
#else
    std::array<std::array<Key, 0>, 0> encoder_map[kNumLayers];
#endif

    consteval KeymapDef() {
        for_each_layer([this]<Layer layer>{
            const auto i = layer_state_t(layer);
            ce_copy(kLayerDef<layer>.matrix, keymap[i]);
            ce_copy(kLayerDef<layer>.encoder_map, encoder_map[i]);
        });
    }
};
