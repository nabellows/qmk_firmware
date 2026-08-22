#pragma once

#include "compat.hpp"
#include "keycodes.h"
#include "quantum_keycodes.h"
#include "util.hpp"
#include "value_iterator.hpp"

#include <array>
#include <concepts>
#include <iterator>
#include <ranges>
#include <span>
#include <string_view>
#include <utility>

#define KC_SEMI KC_SEMICOLON

using qmk_key_t = uint16_t;

namespace detail {
constexpr std::string_view unshifted_char_keys =
    "1234567890-="
    "[]\\"
    ";'"
    ",./";
constexpr std::string_view shifted_char_keys =
    "!@#$%^&*()_+"
    "{}|"
    ":\""
    "<>?";
constexpr qmk_key_t keys[] = {
    KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,
    KC_7,    KC_8,    KC_9,    KC_0,    KC_MINS, KC_EQL,
    KC_LBRC, KC_RBRC, KC_BSLS,
    KC_SEMI, KC_QUOT,
    KC_COMM, KC_DOT,  KC_SLSH,
};
static_assert(unshifted_char_keys.size() == shifted_char_keys.size());
static_assert(unshifted_char_keys.size() == std::size(keys));
} // detail

constexpr qmk_key_t char_to_key(char c) {
    using namespace detail;
    if (c >= 'a' && c <= 'z') return KC_A + c - 'a';
    if (c >= 'A' && c <= 'Z') return S(KC_A + c - 'A');
    if (c == '0') return KC_0;
    if (c >= '1' && c <= '9') return KC_1 + (c - '1');

    sz i;
    i = unshifted_char_keys.find(c);
    if (i != std::string_view::npos) return keys[i];
    i = shifted_char_keys.find(c);
    if (i != std::string_view::npos) return S(keys[i]);

    constexpr_fail("char not supported for char_to_key");
}

constexpr char key_to_char(qmk_key_t k) {
    using namespace detail;
    if (k >= KC_A && k <= KC_Z) return 'a' + (k - KC_A);
    if (k >= S(KC_A) && k <= S(KC_Z)) return 'A' + (k - S(KC_A));
    if (k == KC_0) return '0';
    if (k >= KC_1 && k <= KC_9) return '1' + (k - KC_1);

    for (sz i = 0; i < std::size(keys); ++i) {
        if (k == keys[i]) return unshifted_char_keys[i];
        if (k == S(keys[i])) return shifted_char_keys[i];
    }
    constexpr_fail("key not supported by key_to_char");
}

struct Key {
    qmk_key_t val; // Purposely do not init, constexpr should fail in places they forget to init
    constexpr Key() = default;
    constexpr Key(qmk_key_t val) : val{ val }{}
    constexpr Key(auto val) requires requires { qmk_key_t(val); } : val{ qmk_key_t(val) }{}
    constexpr Key(char c) : val{ char_to_key(c) }{}

    constexpr char to_char() const { return key_to_char(val); }
    constexpr qmk_key_t to_qmk() const { return val; }
    constexpr operator qmk_key_t() const { return val; }

    constexpr Key& operator++() { ++val; return *this; }
    constexpr Key operator++(int) { return std::exchange(*this, val + 1); }
};

template<sz N>
constexpr auto to_keys(const char (&str)[N]) {
    std::array<Key, N-1> res;
    for (sz i = 0; i < N-1; ++i) {
        res[i] = char_to_key(str[i]);
    }
    return res;
}

static_assert(char_to_key('a') == KC_A);
static_assert(char_to_key('L') == (KC_L | QK_LSFT));
static_assert(char_to_key(',') == KC_COMM);
static_assert(char_to_key('>') == KC_GT);
static_assert(char_to_key('$') == KC_DOLLAR);
static_assert(Key('0').to_char() == '0');
static_assert(Key('0') == KC_0);

template<class R>
concept KeyInputRange =
    std::ranges::input_range<R> &&
    std::constructible_from<
        Key,
        std::ranges::range_reference_t<R>
    >;

template<class R>
concept KeyOutputRange =
    std::ranges::range<R> &&
    requires(std::ranges::iterator_t<R> it, Key key) {
        *it = key;
    };

template<sz N>
struct KeyList : std::array<Key, N> {
    using Arr = std::array<Key, N>;
    constexpr KeyList(Arr const& arr): Arr{ arr } {}
    constexpr KeyList(const char (&str)[N+1]) : KeyList { to_keys(str) } {}
    template<class...Keys>
    requires (sizeof...(Keys) == N && (std::convertible_to<Keys, Key> && ...))
    constexpr KeyList(Keys...keys) : KeyList { Arr{ keys... } } {}
    constexpr KeyList(auto gen) requires requires { gen(sz(0)); } {
        for (sz i = 0; i < N; ++i) (*this)[i] = Key(gen(i));
    }

    constexpr static sz size() { return N; }
};
static_assert(KeyInputRange<KeyList<10>>);
static_assert(KeyOutputRange<KeyList<3>>);
static_assert(!KeyOutputRange<const KeyList<9>>);
static_assert(KeyInputRange<const KeyList<9>>);

template<sz N>
KeyList(const char (&str)[N]) -> KeyList<N-1>;
template<class...Keys>
requires (std::convertible_to<Keys, Key> && ...)
KeyList(Keys...) -> KeyList<sizeof...(Keys)>;

template<class T>
inline constexpr bool kIsKeyList = false;
template<sz N>
inline constexpr bool kIsKeyList<KeyList<N>> = true;

template<class...Ts>
concept KeyListish = requires { KeyList{ std::declval<Ts>()... }; };
static_assert(KeyListish<qk_keycode_defines>);

struct InfiniteKeyRange {
    constexpr InfiniteKeyRange(Key first): first{ first } {}
    constexpr Key operator[](sz i) const { return first + i; }

    constexpr auto begin() const { return map_val_iterator<Key>(first); }
    constexpr auto end() const { return std::unreachable_sentinel; }
protected:
    qmk_key_t first; // use raw key since it is handled by iterator better
};
static_assert(std::ranges::random_access_range<InfiniteKeyRange>);

template<sz N = std::dynamic_extent>
class KeyRange : public InfiniteKeyRange {
    sz len;
    constexpr void validate_size() const {
        if constexpr (is_fixed_size) {
            if (size() != N)
                constexpr_fail("Invalid fixed key range (size mismatch)");
        }
    }
public:
    constexpr KeyRange(Key first, sz len) : InfiniteKeyRange{ first }, len{ len } { validate_size(); }
    constexpr KeyRange(Key first, Key last) : KeyRange(first, last-first+1) { if (last < first) constexpr_fail("Invalid KeyRange"); }
    constexpr KeyRange(const KeyRange& other) : InfiniteKeyRange{ other }, len{ other.len } { validate_size(); }

    constexpr KeyRange& operator=(const KeyRange& other) {
        first = other.first;
        len = other.len;
        validate_size();
        return *this;
    }

    constexpr static bool is_fixed_size = N != std::dynamic_extent;

    constexpr sz size() const { return len; }

    constexpr auto end() const { return map_val_iterator<Key>(qmk_key_t(first + len)); }

    constexpr KeyList<N> to_list() const requires (is_fixed_size) {
        return [&](sz i){ return first + i; };
    }
    constexpr operator KeyList<N>() const requires(is_fixed_size) { return to_list(); }

    template<sz M>
    constexpr KeyList<M> to_list() const {
        if (size() < M) constexpr_fail("Invalid fixed key range (size mismatch)");
        return [&](sz i){ return first + i; };
    }
    template<sz M>
    constexpr operator KeyList<M>() const { return to_list<M>(); }
};
static_assert(std::ranges::random_access_range<KeyRange<std::dynamic_extent>>);

template<Key first, Key last>
constexpr KeyRange<last.to_qmk() - first.to_qmk() + 1> kKeyRange = { first, last };


