#pragma once
#define restrict

#include <iterator>
#include <ranges>

template<std::ranges::view V1, std::ranges::view V2>
requires std::ranges::input_range<V1> &&
         std::ranges::input_range<V2> &&
         std::common_reference_with<
             std::ranges::range_reference_t<V1>,
             std::ranges::range_reference_t<V2>>
class concat_view
    : public std::ranges::view_interface<concat_view<V1, V2>>
{
    V1 first_;
    V2 second_;

    using I1_ = std::ranges::iterator_t<V1>;
    using S1_ = std::ranges::sentinel_t<V1>;
    using I2_ = std::ranges::iterator_t<V2>;
    using S2_ = std::ranges::sentinel_t<V2>;

    class iterator {
        I1_ first_it_;
        S1_ first_end_;
        I2_ second_it_;
        S2_ second_end_;
        bool in_first_;

        constexpr void skip_empty_first() {
            if (in_first_ && first_it_ == first_end_) {
                in_first_ = false;
            }
        }

    public:
        using value_type = std::common_type_t<
            std::ranges::range_value_t<V1>,
            std::ranges::range_value_t<V2>>;

        using reference = std::common_reference_t<
            std::ranges::range_reference_t<V1>,
            std::ranges::range_reference_t<V2>>;

        using difference_type = std::common_type_t<
            std::ranges::range_difference_t<V1>,
            std::ranges::range_difference_t<V2>>;

        using iterator_concept = std::input_iterator_tag;

        constexpr iterator(
            I1_ first_it,
            S1_ first_end,
            I2_ second_it,
            S2_ second_end
        )
            : first_it_(std::move(first_it)),
              first_end_(std::move(first_end)),
              second_it_(std::move(second_it)),
              second_end_(std::move(second_end)),
              in_first_(true)
        {
            skip_empty_first();
        }

        constexpr reference operator*() const {
            return in_first_ ? *first_it_ : *second_it_;
        }

        constexpr iterator& operator++() {
            if (in_first_) {
                ++first_it_;
                skip_empty_first();
            } else {
                ++second_it_;
            }

            return *this;
        }

        constexpr void operator++(int) {
            ++*this;
        }

        friend constexpr bool operator==(
            iterator const& it,
            std::default_sentinel_t
        ) {
            return !it.in_first_ &&
                   it.second_it_ == it.second_end_;
        }
    };

public:
    concat_view() requires
        std::default_initializable<V1> &&
        std::default_initializable<V2>
    = default;

    constexpr concat_view(V1 first, V2 second)
        : first_(std::move(first)),
          second_(std::move(second)) {}

    constexpr auto begin() {
        return iterator{
            std::ranges::begin(first_),
            std::ranges::end(first_),
            std::ranges::begin(second_),
            std::ranges::end(second_)
        };
    }

    constexpr auto end() const noexcept {
        return std::default_sentinel;
    }
};

template<class R1, class R2>
concat_view(R1&&, R2&&)
    -> concat_view<
        std::views::all_t<R1>,
        std::views::all_t<R2>>;

