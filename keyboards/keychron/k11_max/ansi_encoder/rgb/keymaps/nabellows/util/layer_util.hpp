#pragma once

#include "compat.hpp"

#include <concepts>
#include <iterator>
#include <ranges>
#include <string_view>
#include <type_traits>
#include <utility>

#include "concat_view.hpp"
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
    //TODO: private
protected:
    template<sz rows, sz cols>
    constexpr auto positions_to_view(Key(&data)[rows][cols], auto positions) {
        return std::views::iota(sz(0), positions.size()) | std::views::filter([positions](sz i){
            return positions[i];
        }) | std::views::transform([&data](sz i)->Key&{
            return data[i / cols][i % cols];
        });
    }


    template<bool strict, bool single = strict>
    constexpr KeyOutputRange auto lookup_base_key_to_dst_range(Key key) {
        using Index = md_index<2>;
        if constexpr (single) {
            Key* res = nullptr;
            ce_for_each_val<&LayerDefBase::matrix, &LayerDefBase::encoder_map>([&]<auto member>{
                Index index{};
                while (index_of(kLayerDef<layer_base>.*member, key, index)) {
                    if (res) throw "Ambiguous mapping";
                    auto [row, col] = index;
                    ++index.back();
                    res = &(this->*member)[row][col];
                }
            });
            if (strict && !res) throw "Mapping not found in base! (required by strict=true)";
            return std::span(res, res ? 1 : 0);
        } else { // This version inevitably a bit worse on performance
            bool found = false;
            auto [matrix_view, encoder_view] = ce_for_each_val<&LayerDefBase::matrix, &LayerDefBase::encoder_map>([&]<auto member>{
                auto& maps = kLayerDef<layer_base>.*member;
                using Member = std::remove_reference_t<decltype(maps)>;
                constexpr sz N = sizeof(maps) / sizeof(Key);
                constexpr sz rows = std::extent_v<Member, 0>;
                constexpr sz cols = std::extent_v<Member, 1>;
                static_assert(rows*cols == N);
                std::array<bool, N> positions = {};
                Index index{};
                while (index_of(maps, key, index)) {
                    auto [row, col] = index;
                    const sz offset = row*cols + col;
                    positions[offset] = true;
                    ++index.back();
                    found = true;
                }
                return positions_to_view(this->*member, positions);
            });
            if (strict && !found) throw "Mapping not found in base! (required by strict=true)";
            return concat_view{ matrix_view, encoder_view };
        }
    }
protected:
    //TODO: map() (not base)? To do so, perhaps we pass layer, and custom case constexpr-if the layer==current (then dont even use kLayerDef, use *this)
    template<bool strict = true>
    constexpr void map_base_pair(Key from, Key to) {
    }

private:
    template<bool strict>
    constexpr void map_range_impl(KeyInputRange auto&& src_r, KeyOutputRange auto&& dst_r) {
        auto src = std::ranges::begin(src_r);
        auto src_end = std::ranges::end(src_r);
        auto dst = std::ranges::begin(dst_r);
        auto dst_end = std::ranges::end(dst_r);
        constexpr bool infinite_src = std::is_same_v<std::unreachable_sentinel_t, decltype(src_end)>;
        constexpr bool infinite_dst = std::is_same_v<std::unreachable_sentinel_t, decltype(dst_end)>;
        if (infinite_src && infinite_dst) throw "Error: Both mapping source/destination ranges are infinite";

        while (src != src_end && dst != dst_end) {
            *dst = static_cast<Key>(*src);
            ++src;
            ++dst;
        }
        const bool src_finished = infinite_src || src == src_end;
        const bool dst_finished = infinite_dst || dst == dst_end;

        if (strict && !dst_finished) throw "Did not map all keys in destination (source/new-keys range is shorter than destination/base-keys)";
        if (strict && !src_finished) throw "Not all source keys were mapped (source/new-keys range is longer than destination/base-keys)";
    }

    constexpr static auto&& common_cast(KeyInputRange auto&& x) { return FWD(x); }
    template<sz N>
    constexpr static auto common_cast(const char (&str)[N]) { return KeyList{str}; }
    template<KeyListish...A>
    requires (!kIsKeyList<A> && ...) // prefer the above for perfect forward
    constexpr static auto common_cast(A&&...args) { return KeyList{ FWD(args)... }; }

    constexpr static KeyInputRange decltype(auto) to_src_range(auto&&...args) { return common_cast(FWD(args)...); }

    template<bool strict>
    constexpr KeyOutputRange auto to_dst_range_from_base_impl_(KeyInputRange auto&& r)
    {
        return FWD(r) | std::views::transform([this](Key k) {
            return lookup_base_key_to_dst_range<strict>(k);
        }) | std::views::join;
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
        sz num_keys; //TODO: remove? Or still handy?
    public:

        constexpr sz size() const { return num_keys; }

        constexpr KeySpan(LayerDefBase& self, auto member, sz from_row, sz from_col, sz to_row, sz to_col) {
            if (from_row > to_row || from_col > to_col) throw "Invalid Key Span (negative direction)";
            base_data = &(kLayerDef<layer_base>.*member)[from_row][from_col];
            data = &(self.*member)[from_row][from_col];
            raw_size = &(self.*member)[to_row][to_col] - data + sz(1);
            num_keys = raw_size - std::count(data, data+raw_size, Key(KC_NO));
        }

        constexpr auto to_dst_range() {
            return std::views::iota(sz(0), raw_size)
            | std::views::filter([bd=base_data](sz i){ return bd[i] != KC_NO; })
            | std::views::transform([d=data](sz i) -> Key& { return d[i]; });
        }

        // TODO: remove
        // TODO: Should just invoke .to(KeyRange(args...)) and KeyRange should be adapted to have the finite/infinite function
        template<bool strict = true, class End = Void>
        constexpr KeySpan& to_range(Key range_begin, End range_end = {}) {
            constexpr bool finite = requires { range_begin <= range_end; };
            auto in_range = [&](Key k){
                if constexpr (finite) return range_begin <= k && k <= range_end;
                return true;
            };
            if (!in_range(range_begin)) throw "Invalid range (begin > end)";
            Key cur = range_begin;
            to([&](int i){
                if (!in_range(cur)) throw "Range overflow (rhs/'to' side of mapping is too short to map all the keys)";
                return cur++;
            });
            if (strict && finite && in_range(cur)) {
                throw "Ranges do not exactly match in size (strict=true)";
            }
            return *this;
        }
    };

    constexpr static KeyInputRange auto common_cast(KeySpan span) {
        return span.to_dst_range();
    }

    constexpr KeySpan base_span(Key from, Key to) {
        std::optional<KeySpan> res;
        ce_for_each_val<&LayerDefBase::matrix, &LayerDefBase::encoder_map>([&]<auto member>{
            auto& ref = kLayerDef<layer_base>.*member;
            auto from_index = index_of(ref, from);
            auto to_index = index_of(ref, to);
            if (from_index.has_value() != to_index.has_value()) throw "Bad span, base matrix/map contains one key and not the other";
            if (from_index) {
                auto [from_row, from_col] = *from_index;
                auto [to_row, to_col] = *to_index;
                if (from_row != to_row) throw "Bad span: Span must lie in a single row"; // Unsure if this is preferred
                if (res) throw "Ambiguous span";
                res = KeySpan(*this, member, from_row, from_col, to_row, to_col);
            }
        });
        if (!res) throw "Could not find matching span";
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
            if (needs_map) throw "Forgot to map me!";
        }
        constexpr bool mapped(bool val = true) { return needs_map = !val; }
        constexpr void move_out() { needs_map = false; }
    };

    template<bool strict, KeyOutputRange Dst>
    class Mapper {
        LayerDefBase& def;
        Dst dst; // TODO: simplify and call map_base_pairs and instead just store a tuple (not ref..) here?
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
            static_assert(std::same_as<decltype(repeat_view(key).end()), std::unreachable_sentinel_t>);
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
    constexpr auto map_base(Args&&... args) requires requires { KeyList{ FWD(args)... }; } {
        KeyList list{ FWD(args)... };
        return mapper<strict>(*this, to_dst_range_from_base<strict>(FWD(args)...));
    }

    template<bool strict = true>
    constexpr auto map_base_span(Key from, Key to) {
        return mapper<strict>(*this, base_span(from, to).to_dst_range());
    }

    constexpr auto map_base_span(KeyList<2> pair) {
        return map_base_span(pair.front(), pair.back());
    }

    template<bool strict = true, class...Args>
    constexpr auto use_base(Args&&... args) {
        KeyList list { FWD(args)... }; // TODO:
        return map_base_pairs<strict>(list, list);
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
