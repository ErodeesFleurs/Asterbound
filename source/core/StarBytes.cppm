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
inline void reverse_bytes_copy_fixed(void* dest, void const* src) noexcept {
  T value;
  std::memcpy(&value, src, sizeof(value));
  value = std::byteswap(value);
  auto bytes = std::bit_cast<std::array<unsigned char, sizeof(T)>>(value);
  std::ranges::copy(bytes, static_cast<unsigned char*>(dest));
}
}// namespace Detail

inline void reverse_bytes(void* ptr, std::size_t len) noexcept {
  switch (len) {
  case 0:
  case 1: return;
  case 2: return Detail::reverse_bytes_copy_fixed<std::uint16_t>(ptr, ptr);
  case 4: return Detail::reverse_bytes_copy_fixed<std::uint32_t>(ptr, ptr);
  case 8: return Detail::reverse_bytes_copy_fixed<std::uint64_t>(ptr, ptr);
  default: std::ranges::reverse(std::span{static_cast<unsigned char*>(ptr), len});
  }
}

// Source and destination must be disjoint or identical. Both may be unaligned.
inline void reverse_bytes_copy(void* dest, void const* src, std::size_t len) noexcept {
  switch (len) {
  case 0: return;
  case 2: return Detail::reverse_bytes_copy_fixed<std::uint16_t>(dest, src);
  case 4: return Detail::reverse_bytes_copy_fixed<std::uint32_t>(dest, src);
  case 8: return Detail::reverse_bytes_copy_fixed<std::uint64_t>(dest, src);
  default:
    if (dest == src) {
      std::ranges::reverse(std::span{static_cast<unsigned char*>(dest), len});
      return;
    }
    auto const* source = static_cast<unsigned char const*>(src);
    auto* destination = static_cast<unsigned char*>(dest);
    while (len >= sizeof(std::uint64_t)) {
      len -= sizeof(std::uint64_t);
      Detail::reverse_bytes_copy_fixed<std::uint64_t>(destination, source + len);
      destination += sizeof(std::uint64_t);
    }
    for (std::size_t i = 0; i < len; ++i)
      destination[len - 1 - i] = source[i];
  }
}

inline void to_byte_order(ByteOrder order, void* ptr, std::size_t len) noexcept {
  if (order != ByteOrder::NoConversion && native_byte_order() != order)
    reverse_bytes(ptr, len);
}

// As with reverse_bytes_copy, only disjoint or identical ranges are supported.
inline void to_byte_order(ByteOrder order, void* dest, void const* src, std::size_t len) noexcept {
  if (len == 0)
    return;
  if (order != ByteOrder::NoConversion && native_byte_order() != order)
    reverse_bytes_copy(dest, src, len);
  else if (dest != src)
    std::memcpy(dest, src, len);
}

inline void from_byte_order(ByteOrder order, void* ptr, std::size_t len) noexcept { to_byte_order(order, ptr, len); }

inline void from_byte_order(ByteOrder order, void* dest, void const* src, std::size_t len) noexcept {
  to_byte_order(order, dest, src, len);
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
