module;
import std;
import star.byte_array;
import star.format;

export module star.compression;

export namespace Star {

// Zlib compression level, ranges from 0 to 9.
using CompressionLevel = int;
inline constexpr CompressionLevel low_compression = 2;
inline constexpr CompressionLevel medium_compression = 5;
inline constexpr CompressionLevel high_compression = 9;

// Never trust the decompressed size declared by a mod, asset pack or peer.
inline constexpr std::size_t max_uncompressed_size = 256 * 1024 * 1024;

// Output overloads retain capacity and clear before codec initialization, even
// on failure. Overlapping input is staged before the output is changed.
[[nodiscard]] std::expected<void, StarException> compress(std::span<char const> input, ByteArray& output, CompressionLevel level = medium_compression);
[[nodiscard]] std::expected<ByteArray, StarException> compress(std::span<char const> input, CompressionLevel level = medium_compression);

// A zero limit is unlimited. Decode failures retain only the output published
// by earlier accepted codec calls, never the failing call's writable tail.
[[nodiscard]] std::expected<void, StarException> decompress(std::span<char const> input, ByteArray& output, std::size_t limit = 0);
[[nodiscard]] std::expected<ByteArray, StarException> decompress(std::span<char const> input, std::size_t limit = 0);
[[nodiscard]] std::expected<void, StarException> decompress_gzip(std::span<char const> input, ByteArray& output, std::size_t limit = 0);
[[nodiscard]] std::expected<ByteArray, StarException> decompress_gzip(std::span<char const> input, std::size_t limit = 0);

}
