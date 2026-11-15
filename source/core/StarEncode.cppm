module;
import std;
import star.string;
import star.byte_array;

namespace Star {

// Span operations return the number of bytes written and leave the remaining
// output untouched. Encoders require nonoverlapping input and output; hex emits
// only complete pairs, while Base64 may emit a partial four-character group.
std::size_t hex_encode(std::span<char const> data, std::span<char> output);
std::size_t base64_encode(std::span<char const> data, std::span<char> output);

// Decoders permit disjoint buffers or exactly the same starting address (each
// input group is read before its shorter output is written). Other overlap is
// unsupported. Hex maps invalid nibbles to zero and ignores an odd final nibble.
// Base64 accepts unpadded tails and stops at the first nonalphabet byte, including
// '=', whitespace and NUL; its alphabet is ASCII and does not include '-' or '_'.
[[nodiscard]] std::size_t hex_decode(std::span<char const> data, std::span<char> output);
[[nodiscard]] std::size_t base64_decode(std::span<char const> data, std::span<char> output);

String hex_encode(std::span<char const> data);
String base64_encode(std::span<char const> data);
String hex_encode(ByteArray const& data);
String base64_encode(ByteArray const& data);
[[nodiscard]] ByteArray hex_decode(std::string_view data);
[[nodiscard]] ByteArray base64_decode(std::string_view data);

}

export module star.encode;

export namespace Star {
  using ::Star::hex_encode;
  using ::Star::hex_decode;
  using ::Star::base64_encode;
  using ::Star::base64_decode;
}
