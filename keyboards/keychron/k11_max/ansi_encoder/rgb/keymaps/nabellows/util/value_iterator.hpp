#pragma once

#define restrict
#include <compare>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <type_traits>
#include <utility>

template<class T, class D = std::ptrdiff_t>
concept value_bidirectional =
    requires(T value) {
        { --value } -> std::same_as<T&>;
    };

template<class T, class D = std::ptrdiff_t>
concept value_random_access =
    value_bidirectional<T, D> &&
    std::totally_ordered<T> &&
    requires(T value, const T cvalue, D offset) {
        { value += offset } -> std::same_as<T&>;
        { value -= offset } -> std::same_as<T&>;

        { cvalue + offset } -> std::convertible_to<T>;
        { cvalue - offset } -> std::convertible_to<T>;
        { cvalue - cvalue } -> std::convertible_to<D>;
    };

template<class T>
struct ValueIterator {
    using value_type      = T;
    using difference_type = std::ptrdiff_t;

    using iterator_concept =
        std::conditional_t<
            value_random_access<T, difference_type>,
            std::random_access_iterator_tag,
            std::conditional_t<
                value_bidirectional<T, difference_type>,
                std::bidirectional_iterator_tag,
                std::forward_iterator_tag
            >
        >;

    // For legacy iterator machinery.
    using iterator_category = iterator_concept;

    T value;

    constexpr T const& operator*() const {
        return value;
    }

    constexpr ValueIterator& operator++() {
        ++value;
        return *this;
    }

    constexpr ValueIterator operator++(int) {
        auto old = *this;
        ++*this;
        return old;
    }

    constexpr ValueIterator& operator--()
        requires value_bidirectional<T, difference_type>
    {
        --value;
        return *this;
    }

    constexpr ValueIterator operator--(int)
        requires value_bidirectional<T, difference_type>
    {
        auto old = *this;
        --*this;
        return old;
    }

    constexpr ValueIterator& operator+=(difference_type n)
        requires value_random_access<T, difference_type>
    {
        value += n;
        return *this;
    }

    constexpr ValueIterator& operator-=(difference_type n)
        requires value_random_access<T, difference_type>
    {
        value -= n;
        return *this;
    }

    constexpr T const& operator[](difference_type n) const
        requires value_random_access<T, difference_type>
    {
        return value + n;
    }

    friend constexpr ValueIterator operator+(
        ValueIterator it,
        difference_type n
    )
        requires value_random_access<T, difference_type>
    {
        return it += n;
    }

    friend constexpr ValueIterator operator+(
        difference_type n,
        ValueIterator it
    )
        requires value_random_access<T, difference_type>
    {
        return it += n;
    }

    friend constexpr ValueIterator operator-(
        ValueIterator it,
        difference_type n
    )
        requires value_random_access<T, difference_type>
    {
        return it -= n;
    }

    friend constexpr difference_type operator-(
        const ValueIterator& lhs,
        const ValueIterator& rhs
    )
        requires value_random_access<T, difference_type>
    {
        return static_cast<difference_type>(lhs.value - rhs.value);
    }

    friend constexpr bool operator==(
        const ValueIterator&,
        const ValueIterator&
    ) = default;

    friend constexpr auto operator<=>(
        const ValueIterator&,
        const ValueIterator&
    )
        requires std::three_way_comparable<T>
    = default;
};

template<class T>
ValueIterator(T&&) -> ValueIterator<std::remove_cvref_t<T>>;
