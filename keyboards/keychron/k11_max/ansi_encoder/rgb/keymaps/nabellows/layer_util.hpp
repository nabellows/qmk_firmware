#pragma once

#include "compat.hpp"

#include <string_view>

#include "key_util.hpp"
#include "layers.hpp"
#include "util.hpp"

template<Layer layer>
struct LayerDef;
template<Layer layer>
constexpr LayerDef<layer> kLayerDef;

#define DEFINE_LAYER(layer, ...) template<> struct LayerDef<Layer::layer> : LayerDefBase<> { constexpr static bool exists() { return true; } consteval LayerDef() __VA_ARGS__ }; static_assert(kLayerDef<Layer::layer>.exists());
// #define DEFINE_LAYER(layer) template<> struct LayerDef<Layer::layer> : LayerDefBase<> { LAYER_INIT
// #define LAYER_INIT(...) consteval LayerDef() { __VA_ARGS__ } };

template<auto layer_base = Layer::LAYOUT_BASE> // Avoid any direct instantiations of the base-def
struct LayerDefBase {
    Key matrix[MATRIX_ROWS][MATRIX_COLS];
#ifdef ENCODER_MAP_ENABLE
	Key encoder_map[NUM_ENCODERS][NUM_DIRECTIONS];
#else
    std::array<std::array<Key, 0>, 0> encoder_map{};
#endif
protected:
    constexpr void trans() {
        ce_fill(KC_TRNS, matrix);
        ce_fill(KC_TRNS, encoder_map);
    }
    constexpr void set(LayerDefBase const& rhs) {
        ce_copy(rhs.matrix, matrix);
        ce_copy(rhs.encoder_map, encoder_map);
    }
    template<Layer layer = layer_base>
    constexpr void clone() { set(kLayerDef<layer>); }
    //TODO: map() (not base)?
    template<bool strict = true>
    constexpr void map_base_pair(Key from, Key to) {
        bool found = false;
        ce_for_each_val<&LayerDefBase::matrix, &LayerDefBase::encoder_map>([&]<auto member>{
            md_index<2> index{};
            while (index_of(kLayerDef<layer_base>.*member, from, index)) {
                if constexpr (strict) {
                    if (found) throw "Ambiguous mapping";
                }
                auto [row, col] = index;
                (this->*member)[row][col] = to;
                found = true;
                ++index.back();
            }
        });
    }

    // Guaranteed to point into self. Was considering using std::span as base of operations,
    // but technically UB/not constexpr to do pointer range checks to see for example,
    // what member it is from (or if its an arbitrary collection of Keys)
    struct KeySpan {
        Key* data;
        sz raw_size;
        sz num_keys;

        constexpr sz size() const { return num_keys; }

        constexpr KeySpan(LayerDefBase& self, auto member, sz from_row, sz from_col, sz to_row, sz to_col)
        : data{ &(self.*member)[from_row][from_col] }, raw_size{ &(self.*member)[to_row][to_col] - data + sz(1) } {
            if (from_row > to_row || from_col > to_col) throw "Invalid Key Span (negative direction)";
            num_keys = raw_size - std::count(data, data+raw_size, Key(KC_NO));
        }

        //TODO: this is a mess, this should be put in base Mapper type. Span is only special in the index "FROM" mode, not the to
        //TODO: size enforcement is not good when gen is string, keyRange, keyList, etc
        template<bool strict = true>
        constexpr KeySpan& to(auto const& gen) {
            sz i = 0;
            auto in_gen_range = [&](bool or_else){
                if constexpr (strict && requires { std::string_view(gen); }) {
                    return (i < std::string_view(gen).size());
                } else if constexpr (strict && requires { std::size(gen); }) {
                    return (i < std::size(gen));
                }
                return or_else;
            };
            for (Key& key : std::span(data, raw_size)) {
                //TODO: is this right?
                if (key.to_qmk() == KC_NO) continue;
                if (!in_gen_range(true)) throw "Mismatch mapping size: base span length is GREATER than fixed-length generator (strict=true)";
                if constexpr (requires (Key k){ gen(i, k); }) {
                    key = Key(gen(i, key));
                } else if constexpr (requires (Key k){ gen(k); }) {
                    key = Key(gen(i));
                } else if constexpr (requires { gen[i]; }){
                    key = Key(gen[i]);
                } else {
                    key = Key(gen);
                }
                ++i;
            }
            if (in_gen_range(false)) throw "Mismatch mapping size: base span length is LESS than fixed-length generator (strict=true)";
            return *this;
        }

        template<bool strict = true, class...Keys>
        requires (std::convertible_to<Keys, Key> && ...)
        constexpr KeySpan& to(Key first, Keys...rest) {
            return to<strict>(KeyList{ first, Key(rest)... });
        }

        template<bool strict = true, class End = Void>
        constexpr KeySpan& to_range(Key range_begin, End range_end = {}) {
            auto out_of_range = [&](Key k){
                if constexpr (requires { k > range_end; }) {
                    return (k > range_end);
                }
                return true;
            };
            if (out_of_range(range_begin)) throw "Invalid range (begin > end)";
            int map_end = 0;
            to([&](int i){
                Key res = range_begin + i;
                if (out_of_range(res)) throw "Range overflow";
                map_end = res;
                return res;
            });
            if constexpr (strict) {
                if (!out_of_range(map_end + 1)) {
                    throw "Ranges do not exactly match in size (strict=true)";
                }
            }
            return *this;
        }
    };

    constexpr KeySpan map_base_span(Key from, Key to) {
        std::optional<KeySpan> res;
        ce_for_each_val<&LayerDefBase::matrix, &LayerDefBase::encoder_map>([&]<auto member>{
            auto& ref = kLayerDef<layer_base>.*member;
            auto from_index = index_of(ref, from);
            auto to_index = index_of(ref, to);
            if (from_index.has_value() != to_index.has_value()) throw "Bad span, base matrix/map contains one key and not the other";
            if (from_index) {
                auto [from_row, from_col] = *from_index;
                auto [to_row, to_col] = *to_index;
                // if (from_row != to_row) throw "Bad span: Span must lie in a single row"; // Unsure if this is preferred
                if (res) throw "Ambiguous span";
                res = KeySpan(*this, member, from_row, from_col, to_row, to_col);
            }
        });
        if (!res) throw "Could not find matching span";
        return *res;
    }

    constexpr KeySpan map_base_span(KeyList<2> pair) {
        return map_base_span(pair.front(), pair.back());
    }

    constexpr KeySpan map_base_row(sz row) {
        return KeySpan(*this, &LayerDefBase::matrix, row, 0, row, MATRIX_COLS-1);
    }

    template<bool strict = true, class From, class To>
    constexpr void map_base_pair(From const& from, To const& to) requires (kKeyListish<From> && kKeyListish<To>)  {
        KeyList from_list = from;
        KeyList to_list = to;
        //TODO: these should both be generators/dynamic ranges and have a different compatiblility check. Then the above KeySpan can spawn specialized mapper and share
        //tbh.... the only advantage to KeySpan existing over key_span() -> KeyList is to avoid N^2 lookup but tbh, its probably trivial for this use case
        //TODO: inconsistent with span approach, tho i guess it makes sense since those cant static_assert (static_assert gives better errors tbh)
        static_assert(from_list.size() == to_list.size(), "Invalid mapping between lists of different sizes");
        for (sz i = 0; i < from_list.size(); ++i) {
            map_base_pair<strict>(from_list[i], to_list[i]);
        }
    }

    template<sz N, bool strict>
    class KeyListMapper {
        LayerDefBase& def;
        KeyList<N> from_list;
        bool mapped = false;
    public:
        constexpr KeyListMapper(LayerDefBase& def, KeyList<N> const& from_list)
        : def{ def }, from_list{ from_list } {}

        constexpr ~KeyListMapper() {
            if (!mapped) throw "Forgot to map me!";
        }

        template<class...Args>
        constexpr auto to(Args&&... args) requires requires { KeyList{ FWD(args)... }; } {
            def.map_base_pair<strict>(from_list, KeyList{ FWD(args)... });
            mapped = true;
        }

        constexpr auto to_single(Key key) {
            def.map_base_pair<strict>(from_list, KeyList<N>{ [=](sz i){ return key; } });
            mapped = true;
        }
    };

    template<bool strict = true, class...Args>
    [[nodiscard]]
    constexpr auto map_base(Args&&... args) requires requires { KeyList{ FWD(args)... }; } {
        KeyList list{ FWD(args)... };
        return KeyListMapper<list.size(), strict>{ *this, list };
    }

    template<bool strict = true, class...Args>
    constexpr auto use_base(Args&&... args) {
        KeyList list { FWD(args)... };
        return map_base_pair<strict>(list, list);
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
