module;

import std;

namespace Star {

enum class ByteOrder { BigEndian, LittleEndian, NoConversion };

// A mixed-endian object representation has no whole-value byte-order conversion.
static_assert(std::endian::native == std::endian::little || std::endian::native == std::endian::big);

template <typename T>
concept byte_order_value = (std::integral<T> && std::has_unique_object_representations_v<T>)
  || (std::floating_point<T> && (sizeof(T) == sizeof(std::uint32_t) || sizeof(T) == sizeof(std::uint64_t)));

constexpr ByteOrder native_byte_order() noexcept {
  return std::endian::native == std::endian::little ? ByteOrder::LittleEndian : ByteOrder::BigEndian;
}

template <byte_order_value T> constexpr T to_byte_order(ByteOrder order, T value) noexcept {
  if constexpr (sizeof(T) == 1) {
    return value;
  } else {
    if (order == ByteOrder::NoConversion || native_byte_order() == order)
      return value;
    if constexpr (std::integral<T>)
      return std::byteswap(value);
    else if constexpr (sizeof(T) == sizeof(std::uint32_t))
      return std::bit_cast<T>(std::byteswap(std::bit_cast<std::uint32_t>(value)));
    else
      return std::bit_cast<T>(std::byteswap(std::bit_cast<std::uint64_t>(value)));
  }
}

template <byte_order_value T> constexpr T from_byte_order(ByteOrder order, T value) noexcept {
  return to_byte_order(order, value);
}

template <byte_order_value T> constexpr T to_big_endian(T value) noexcept {
  return to_byte_order(ByteOrder::BigEndian, value);
}

template <byte_order_value T> constexpr T to_little_endian(T value) noexcept {
  return to_byte_order(ByteOrder::LittleEndian, value);
}

namespace Detail {
template <std::unsigned_integral T>
  requires std::has_unique_object_representations_v<T>
inline void reverse_bytes_copy_fixed(std::span<std::byte, sizeof(T)> dest, std::span<std::byte const, sizeof(T)> src) noexcept {
  T value;
  std::memcpy(&value, src.data(), sizeof(value));
  auto bytes = std::bit_cast<std::array<std::byte, sizeof(T)>>(std::byteswap(value));
  std::ranges::copy(bytes, dest.begin());
}
}// namespace Detail

// Copy src.size() bytes. The destination must be large enough; the copied
// ranges may be unaligned and must be either disjoint or identical.
inline void reverse_bytes_copy(std::span<std::byte> dest, std::span<std::byte const> src) noexcept {
  dest = dest.first(src.size());
  switch (src.size()) {
  case 0: return;
  case 2: return Detail::reverse_bytes_copy_fixed<std::uint16_t>(dest.first<2>(), src.first<2>());
  case 4: return Detail::reverse_bytes_copy_fixed<std::uint32_t>(dest.first<4>(), src.first<4>());
  case 8: return Detail::reverse_bytes_copy_fixed<std::uint64_t>(dest.first<8>(), src.first<8>());
  default:
    if (dest.data() == src.data()) {
      std::ranges::reverse(dest);
      return;
    }
    constexpr auto width = sizeof(std::uint64_t);
    while (src.size() >= width) {
      Detail::reverse_bytes_copy_fixed<std::uint64_t>(dest.first<width>(), src.last<width>());
      dest = dest.subspan(width);
      src = src.first(src.size() - width);
    }
    std::ranges::reverse_copy(src, dest.begin());
  }
}

inline void reverse_bytes(std::span<std::byte> bytes) noexcept {
  reverse_bytes_copy(bytes, bytes);
}

inline void to_byte_order(ByteOrder order, std::span<std::byte> bytes) noexcept {
  if (order != ByteOrder::NoConversion && native_byte_order() != order)
    reverse_bytes(bytes);
}

// As with reverse_bytes_copy, only disjoint or identical ranges are supported.
inline void to_byte_order(ByteOrder order, std::span<std::byte> dest, std::span<std::byte const> src) noexcept {
  if (order != ByteOrder::NoConversion && native_byte_order() != order)
    reverse_bytes_copy(dest, src);
  else if (!src.empty() && dest.data() != src.data())
    std::ranges::copy(src, dest.first(src.size()).begin());
}

inline void from_byte_order(ByteOrder order, std::span<std::byte> bytes) noexcept { to_byte_order(order, bytes); }

inline void from_byte_order(ByteOrder order, std::span<std::byte> dest, std::span<std::byte const> src) noexcept {
  to_byte_order(order, dest, src);
}

}// namespace Star

export module star.bytes;

export namespace Star {
using ::Star::byte_order_value;
using ::Star::ByteOrder;
using ::Star::from_byte_order;
using ::Star::native_byte_order;
using ::Star::reverse_bytes;
using ::Star::reverse_bytes_copy;
using ::Star::to_big_endian;
using ::Star::to_byte_order;
using ::Star::to_little_endian;
}// namespace Star
