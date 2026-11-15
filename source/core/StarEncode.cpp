import std;
import star.format;
import star.string;
import star.byte_array;
import star.encode;

namespace Star {
namespace {

constexpr std::string_view HexChars = "0123456789abcdef";
constexpr std::string_view Base64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
constexpr std::uint8_t InvalidBase64 = 0xff;
constexpr auto Base64Values = [] {
  std::array<std::uint8_t, 256> values;
  values.fill(InvalidBase64);
  for (std::size_t i = 0; i < Base64Chars.size(); ++i)
    values[static_cast<unsigned char>(Base64Chars[i])] = static_cast<std::uint8_t>(i);
  return values;
}();

constexpr auto HexValues = [] {
  std::array<std::uint8_t, 256> values{};
  for (unsigned i = 0; i < 10; ++i)
    values['0' + i] = static_cast<std::uint8_t>(i);
  for (unsigned i = 0; i < 6; ++i) {
    values['A' + i] = static_cast<std::uint8_t>(10 + i);
    values['a' + i] = static_cast<std::uint8_t>(10 + i);
  }
  return values;
}();

constexpr unsigned hex_nibble(char value) {
  return HexValues[static_cast<unsigned char>(value)];
}

constexpr std::optional<std::size_t> hex_encoded_size(std::size_t size, std::size_t limit) {
  if (size > limit / 2)
    return std::nullopt;
  return size * 2;
}

constexpr std::optional<std::size_t> base64_encoded_size(std::size_t size, std::size_t limit) {
  auto groups = size / 3 + (size % 3 != 0);
  if (groups > limit / 4)
    return std::nullopt;
  return groups * 4;
}

constexpr auto SizeMax = std::numeric_limits<std::size_t>::max();
static_assert(hex_encoded_size(0, 0) == 0);
static_assert(hex_encoded_size(SizeMax / 2, SizeMax) == SizeMax - 1);
static_assert(base64_encoded_size(0, 0) == 0);
static_assert(base64_encoded_size(1, 4) == 4);
static_assert(base64_encoded_size(2, 4) == 4);
static_assert(base64_encoded_size(3, 4) == 4);
static_assert(base64_encoded_size((SizeMax / 4) * 3, SizeMax) == (SizeMax / 4) * 4);
static_assert(!hex_encoded_size(1, 1));
static_assert(!hex_encoded_size(SizeMax / 2 + 1, SizeMax));
static_assert(!base64_encoded_size(1, 3));
static_assert(!base64_encoded_size((SizeMax / 4) * 3 + 1, SizeMax));
static_assert(!base64_encoded_size(SizeMax, SizeMax));

}

std::size_t hex_encode(std::span<char const> data, std::span<char> output) {
  auto count = std::min(data.size(), output.size() / 2);
  for (std::size_t i = 0; i < count; ++i) {
    auto byte = static_cast<unsigned char>(data[i]);
    output[i * 2] = HexChars[byte >> 4];
    output[i * 2 + 1] = HexChars[byte & 0x0f];
  }
  return count * 2;
}

std::size_t hex_decode(std::span<char const> data, std::span<char> output) {
  auto count = std::min(data.size() / 2, output.size());
  for (std::size_t i = 0; i < count; ++i)
    output[i] = static_cast<char>((hex_nibble(data[i * 2]) << 4) | hex_nibble(data[i * 2 + 1]));
  return count;
}

std::size_t base64_encode(std::span<char const> data, std::span<char> output) {
  std::size_t written = 0;
  while (data.size() >= 3 && output.size() >= 4) {
    auto a = static_cast<unsigned char>(data[0]);
    auto b = static_cast<unsigned char>(data[1]);
    auto c = static_cast<unsigned char>(data[2]);
    output[0] = Base64Chars[a >> 2];
    output[1] = Base64Chars[((a & 0x03) << 4) | (b >> 4)];
    output[2] = Base64Chars[((b & 0x0f) << 2) | (c >> 6)];
    output[3] = Base64Chars[c & 0x3f];
    data = data.subspan(3);
    output = output.subspan(4);
    written += 4;
  }
  if (data.empty() || output.empty())
    return written;

  // This is also the final complete input block when output holds fewer than
  // four characters: publish exactly the available encoded prefix, including padding.
  auto count = std::min(output.size(), std::size_t(4));
  auto a = static_cast<unsigned char>(data[0]);
  output[0] = Base64Chars[a >> 2];
  if (count > 1) {
    auto b = data.size() > 1 ? static_cast<unsigned char>(data[1]) : 0u;
    output[1] = Base64Chars[((a & 0x03) << 4) | (b >> 4)];
    if (count > 2) {
      auto c = data.size() > 2 ? static_cast<unsigned char>(data[2]) : 0u;
      output[2] = data.size() > 1 ? Base64Chars[((b & 0x0f) << 2) | (c >> 6)] : '=';
      if (count > 3)
        output[3] = data.size() > 2 ? Base64Chars[c & 0x3f] : '=';
    }
  }
  return written + count;
}

std::size_t base64_decode(std::span<char const> data, std::span<char> output) {
  std::size_t written = 0;
  while (data.size() >= 4 && output.size() >= 3) {
    auto a = Base64Values[static_cast<unsigned char>(data[0])];
    auto b = Base64Values[static_cast<unsigned char>(data[1])];
    auto c = Base64Values[static_cast<unsigned char>(data[2])];
    auto d = Base64Values[static_cast<unsigned char>(data[3])];
    if ((a | b | c | d) == InvalidBase64)
      break;
    // Load the complete group before writing: same-start decoding is safe.
    output[0] = static_cast<char>((a << 2) | (b >> 4));
    output[1] = static_cast<char>((b << 4) | (c >> 2));
    output[2] = static_cast<char>((c << 6) | d);
    data = data.subspan(4);
    output = output.subspan(3);
    written += 3;
  }
  if (output.empty())
    return written;

  std::array<std::uint8_t, 4> tail{};
  std::size_t accepted = 0;
  while (accepted < std::min(data.size(), tail.size())) {
    auto value = Base64Values[static_cast<unsigned char>(data[accepted])];
    if (value == InvalidBase64)
      break;
    tail[accepted++] = value;
  }
  auto count = std::min(output.size(), accepted > 1 ? accepted - 1 : 0);
  if (count > 0)
    output[0] = static_cast<char>((tail[0] << 2) | (tail[1] >> 4));
  if (count > 1)
    output[1] = static_cast<char>((tail[1] << 4) | (tail[2] >> 2));
  if (count > 2)
    output[2] = static_cast<char>((tail[2] << 6) | tail[3]);
  return written + count;
}

String hex_encode(std::span<char const> data) {
  std::string result;
  auto size = hex_encoded_size(data.size(), result.max_size());
  if (!size)
    Star::fatal_error("Hex encoding exceeds String max_size", false);
  result.resize_and_overwrite(*size,
      [data](char* output, std::size_t size) noexcept {
        auto destination = std::span<char>(output, size);
        return hex_encode(data, destination);
      });
  return result;
}

String base64_encode(std::span<char const> data) {
  std::string result;
  auto size = base64_encoded_size(data.size(), result.max_size());
  if (!size)
    Star::fatal_error("Base64 encoding exceeds String max_size", false);
  result.resize_and_overwrite(*size,
      [data](char* output, std::size_t size) noexcept {
        auto destination = std::span<char>(output, size);
        return base64_encode(data, destination);
      });
  return result;
}

String hex_encode(ByteArray const& data) {
  return hex_encode(data.span());
}

ByteArray hex_decode(std::string_view data) {
  ByteArray result;
  result.resize(data.size() / 2);
  (void)hex_decode(std::span(data), result.span());
  return result;
}

String base64_encode(ByteArray const& data) {
  return base64_encode(data.span());
}

ByteArray base64_decode(std::string_view data) {
  ByteArray result;
  auto tail = data.size() % 4;
  result.resize((data.size() / 4) * 3 + (tail > 1 ? tail - 1 : 0));
  result.resize(base64_decode(std::span(data), result.span()));
  return result;
}

}
