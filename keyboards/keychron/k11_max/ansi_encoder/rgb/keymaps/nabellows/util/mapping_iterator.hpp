#pragma once
#define restrict

#include <compare>
#include <concepts>
#include <iterator>
#include <type_traits>
#include <utility>

#include "common.hpp"

template<class T, class It>
class MappingIterator {
    It it_;

    using source_ref  = std::iter_reference_t<It>;
    using source_type = std::remove_reference_t<source_ref>;

    static constexpr bool can_reference =
        std::is_lvalue_reference_v<source_ref> &&
        std::is_convertible_v<source_type*, T const*>;

    template<class U>
    static constexpr decltype(auto) map(U&& value) {
        if constexpr (can_reference) {
            return static_cast<T const&>(value);
        } else {
            return T(std::forward<U>(value));
        }
    }

public:
    using value_type      = T;
    using difference_type = std::iter_difference_t<It>;

    // A mapping iterator can never honestly be contiguous.
    using iterator_concept =
        std::conditional_t<
            std::random_access_iterator<It>,
            std::random_access_iterator_tag,
            std::conditional_t<
                std::bidirectional_iterator<It>,
                std::bidirectional_iterator_tag,
                std::conditional_t<
                    std::forward_iterator<It>,
                    std::forward_iterator_tag,
                    std::input_iterator_tag
                >
            >
        >;

    using iterator_category = iterator_concept;

    constexpr MappingIterator() requires std::default_initializable<It> = default;

    constexpr explicit MappingIterator(auto&&...args): it_{ FWD(args)... } {}

    template<class Other>
        requires std::convertible_to<Other const&, It>
    constexpr MappingIterator(
        MappingIterator<T, Other> const& other
    ) : it_(other.base())
    {}

    constexpr It const& base() const& noexcept {
        return it_;
    }

    constexpr It base() && {
        return std::move(it_);
    }

    constexpr decltype(auto) operator*() const {
        return map(*it_);
    }

    constexpr MappingIterator& operator++() {
        ++it_;
        return *this;
    }

    constexpr MappingIterator operator++(int)
        requires std::forward_iterator<It>
    {
        auto old = *this;
        ++*this;
        return old;
    }

    constexpr void operator++(int)
        requires (!std::forward_iterator<It>)
    {
        ++*this;
    }

    constexpr MappingIterator& operator--()
        requires std::bidirectional_iterator<It>
    {
        --it_;
        return *this;
    }

    constexpr MappingIterator operator--(int)
        requires std::bidirectional_iterator<It>
    {
        auto old = *this;
        --*this;
        return old;
    }

    constexpr MappingIterator& operator+=(difference_type n)
        requires std::random_access_iterator<It>
    {
        it_ += n;
        return *this;
    }

    constexpr MappingIterator& operator-=(difference_type n)
        requires std::random_access_iterator<It>
    {
        it_ -= n;
        return *this;
    }

    constexpr decltype(auto) operator[](difference_type n) const
        requires std::random_access_iterator<It>
    {
        return map(it_[n]);
    }

    friend constexpr MappingIterator operator+(
        MappingIterator it,
        difference_type n
    )
        requires std::random_access_iterator<It>
    {
        return it += n;
    }

    friend constexpr MappingIterator operator+(
        difference_type n,
        MappingIterator it
    )
        requires std::random_access_iterator<It>
    {
        return it += n;
    }

    friend constexpr MappingIterator operator-(
        MappingIterator it,
        difference_type n
    )
        requires std::random_access_iterator<It>
    {
        return it -= n;
    }

    friend constexpr difference_type operator-(
        MappingIterator const& lhs,
        MappingIterator const& rhs
    )
        requires std::sized_sentinel_for<It, It>
    {
        return lhs.it_ - rhs.it_;
    }

    friend constexpr bool operator==(
        MappingIterator const& lhs,
        MappingIterator const& rhs
    )
        requires std::equality_comparable<It>
    {
        return lhs.it_ == rhs.it_;
    }

    friend constexpr auto operator<=>(
        MappingIterator const& lhs,
        MappingIterator const& rhs
    )
        requires std::random_access_iterator<It> &&
                 std::three_way_comparable<It>
    {
        return lhs.it_ <=> rhs.it_;
    }

    friend constexpr bool operator<(
        MappingIterator const& lhs,
        MappingIterator const& rhs
    )
        requires std::random_access_iterator<It> &&
                 (!std::three_way_comparable<It>)
    {
        return lhs.it_ < rhs.it_;
    }

    friend constexpr bool operator>(
        MappingIterator const& lhs,
        MappingIterator const& rhs
    )
        requires std::random_access_iterator<It> &&
                 (!std::three_way_comparable<It>)
    {
        return rhs < lhs;
    }

    friend constexpr bool operator<=(
        MappingIterator const& lhs,
        MappingIterator const& rhs
    )
        requires std::random_access_iterator<It> &&
                 (!std::three_way_comparable<It>)
    {
        return !(rhs < lhs);
    }

    friend constexpr bool operator>=(
        MappingIterator const& lhs,
        MappingIterator const& rhs
    )
        requires std::random_access_iterator<It> &&
                 (!std::three_way_comparable<It>)
    {
        return !(lhs < rhs);
    }
};
