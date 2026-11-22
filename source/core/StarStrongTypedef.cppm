module;

import std;

namespace Star {

template <typename T>
concept strong_scalar = std::same_as<T, std::remove_cv_t<T>> && (std::integral<T>
    || ((std::same_as<T, float> || std::same_as<T, double>) && std::numeric_limits<T>::is_iec559));

template <strong_scalar Scalar, typename Tag>
struct StrongTypedef {
  Scalar value{};

  constexpr StrongTypedef() noexcept = default;
  explicit constexpr StrongTypedef(Scalar initial) noexcept : value(initial) {}

  constexpr auto operator<=>(StrongTypedef const& rhs) const noexcept {
    if constexpr (std::integral<Scalar>) {
      return value <=> rhs.value;
    } else {
      // Compare IEEE representations so fast-math cannot assume away NaNs or signed zero.
      using Bits = std::conditional_t<sizeof(Scalar) == 4, std::uint32_t, std::uint64_t>;
      constexpr Bits sign = Bits{1} << (std::numeric_limits<Bits>::digits - 1);
      constexpr Bits infinity = std::bit_cast<Bits>(std::numeric_limits<Scalar>::infinity());
      auto key = [=](Bits bits) { return bits & sign ? ~bits + Bits{1} : bits | sign; };
      auto a = std::bit_cast<Bits>(value);
      auto b = std::bit_cast<Bits>(rhs.value);
      if ((a & ~sign) > infinity || (b & ~sign) > infinity)
        return std::partial_ordering::unordered;
      return std::partial_ordering(key(a) <=> key(b));
    }
  }

  constexpr bool operator==(StrongTypedef const& rhs) const noexcept { return (*this <=> rhs) == 0; }
};

}

export module star.strong_typedef;

export namespace Star {
  using ::Star::strong_scalar;
  using ::Star::StrongTypedef;
}
