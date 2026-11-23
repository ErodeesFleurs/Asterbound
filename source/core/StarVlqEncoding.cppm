module;
import std;
import star.memory;

export module star.vlq;

export namespace Star {

// Each octet carries seven payload bits, most significant group first.
inline constexpr std::size_t max_vlq_bytes = 10;

template <typename T>
concept vlq_byte = std::same_as<T, char> || std::same_as<T, unsigned char>;

[[nodiscard]] constexpr std::size_t vlq_u_size(std::uint64_t value) noexcept {
  return std::max(1, (std::bit_width(value) + 6) / 7);
}

[[nodiscard]] constexpr std::uint64_t zigzag_encode(std::int64_t value) noexcept {
  return (static_cast<std::uint64_t>(value) << 1) ^ (std::uint64_t{0} - (value < 0));
}

[[nodiscard]] constexpr std::int64_t zigzag_decode(std::uint64_t value) noexcept {
  auto magnitude = static_cast<std::int64_t>(value >> 1);
  return value & 1 ? -magnitude - 1 : magnitude;
}

template <vlq_byte Byte>
[[nodiscard]] constexpr std::size_t write_vlq_u(std::uint64_t value, std::span<Byte, max_vlq_bytes> output) noexcept {
  auto count = vlq_u_size(value);
  for (std::size_t i = count; i != 0; --i) {
    output[i - 1] = static_cast<Byte>((value & 0x7f) | (i == count ? 0 : 0x80));
    value >>= 7;
  }
  return count;
}

template <vlq_byte Byte>
[[nodiscard]] constexpr std::size_t write_vlq_i(std::int64_t value, std::span<Byte, max_vlq_bytes> output) noexcept {
  return write_vlq_u(zigzag_encode(value), output);
}

// Failure leaves value unchanged. Noncanonical and wrapping encodings are wire-compatible.
template <typename Byte, std::size_t extent> requires vlq_byte<std::remove_const_t<Byte>>
[[nodiscard]] constexpr std::size_t read_vlq_u(std::uint64_t& value, std::span<Byte, extent> input) noexcept {
  auto limit = std::min(input.size(), max_vlq_bytes);
  std::uint64_t decoded = 0;
  for (std::size_t i = 0; i < limit; ++i) {
    auto byte = static_cast<unsigned char>(input[i]);
    decoded = (decoded << 7) | (byte & 0x7f);
    if (!(byte & 0x80)) {
      value = decoded;
      return i + 1;
    }
  }
  return npos;
}

template <typename Byte, std::size_t extent> requires vlq_byte<std::remove_const_t<Byte>>
[[nodiscard]] constexpr std::size_t read_vlq_i(std::int64_t& value, std::span<Byte, extent> input) noexcept {
  std::uint64_t encoded;
  auto count = read_vlq_u(encoded, input);
  if (count != npos) value = zigzag_decode(encoded);
  return count;
}

}
