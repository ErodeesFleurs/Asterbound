module;
#include <stdio.h>
#include <zlib-ng.h>
import std;
import star.byte_array;
import star.format;

module star.compression;

namespace Star {
namespace {

constexpr std::size_t codec_chunk_size = 32 * 1024;

struct ZlibStream {
  zng_stream state{};
  bool inflating;
  bool active = false;

  explicit ZlibStream(bool inflating) : inflating(inflating) {}
  ZlibStream(ZlibStream const&) = delete;
  ZlibStream& operator=(ZlibStream const&) = delete;

  int end() noexcept {
    active = false;
    return inflating ? zng_inflateEnd(&state) : zng_deflateEnd(&state);
  }

  ~ZlibStream() {
    if (active) {
      auto status = end();
      if (status != Z_OK) {
        if (!Star::print(stderr, "Failed to end {} stream ({})\n", inflating ? "inflate" : "deflate", status))
          std::_Exit(1);
      }
    }
  }
};

bool overlaps_output(std::span<char const> input, ByteArray const& output) {
  if (input.empty() || !output.capacity())
    return false;
  // Integer differences avoid unrelated-pointer comparisons and end-address
  // overflow, and include initialized input borrowed from the reserved tail.
  auto input_address = reinterpret_cast<std::uintptr_t>(input.data());
  auto output_address = reinterpret_cast<std::uintptr_t>(output.data());
  return input_address >= output_address
      ? input_address - output_address < output.capacity()
      : output_address - input_address < input.size();
}

std::expected<void, StarException> inflate_data(std::span<char const> input, ByteArray& output, std::size_t limit, int window_bits) {
  ByteArray staged_input;
  if (overlaps_output(input, output)) {
    staged_input = ByteArray(input);
    input = staged_input.span();
  }
  output.clear();
  if (input.empty())
    return {};
  ZlibStream owner(true);
  auto& stream = owner.state;
  int status = zng_inflateInit2(&stream, window_bits);
  if (status != Z_OK)
    return std::unexpected(IOException::format("Failed to initialise inflate ({})", status));
  owner.active = true;
  std::size_t consumed = 0;
  do {
    if (!stream.avail_in && consumed < input.size()) {
      auto chunk = std::min(input.size() - consumed, std::size_t(std::numeric_limits<decltype(stream.avail_in)>::max()));
      stream.next_in = reinterpret_cast<std::uint8_t const*>(input.data() + consumed);
      stream.avail_in = chunk;
      consumed += chunk;
    }
    auto committed = output.size();
    if (codec_chunk_size > ByteArray::max_size() - committed)
      return std::unexpected(IOException("Decompressed output size overflow"));
    // Keep call boundaries stable: failure publishes earlier accepted calls,
    // so changing the writable chunk would also change the error prefix.
    output.reserve(committed + codec_chunk_size);
    stream.next_out = reinterpret_cast<std::uint8_t*>(output.data() + committed);
    stream.avail_out = codec_chunk_size;
    auto previous_input = stream.avail_in;
    status = zng_inflate(&stream, Z_NO_FLUSH);
    if (status != Z_OK && status != Z_STREAM_END && status != Z_BUF_ERROR)
      return std::unexpected(IOException::format("Decompression error ({}): {}", status, stream.msg ? stream.msg : "no detail"));
    auto produced = codec_chunk_size - stream.avail_out;
    // Neither codec failures nor limit failures publish this call's tail.
    if (limit && produced > limit - committed)
      return std::unexpected(IOException::format("Decompression exceeds limit of {} bytes", limit));
    output.resize(committed + produced);
    if (status != Z_STREAM_END && !produced && previous_input == stream.avail_in)
      return std::unexpected(IOException::format("Incomplete compressed data ({})", status));
  } while (status != Z_STREAM_END);
  status = owner.end();
  if (status != Z_OK)
    return std::unexpected(IOException::format("Failed to end inflate stream ({})", status));
  return {};
}

void compact_result(ByteArray& output) {
  if (output.capacity() - output.size() > output.size())
    output.shrink_to_fit();
}

}

std::expected<void, StarException> compress(std::span<char const> input, ByteArray& output, CompressionLevel level) {
  ByteArray staged_input;
  if (overlaps_output(input, output)) {
    staged_input = ByteArray(input);
    input = staged_input.span();
  }
  output.clear();
  if (input.empty())
    return {};
  ZlibStream owner(false);
  auto& stream = owner.state;
  int status = zng_deflateInit(&stream, level);
  if (status != Z_OK)
    return std::unexpected(IOException::format("Failed to initialise deflate ({})", status));
  owner.active = true;
  // Small inputs fit in their native bound without a 32KiB scratch allocation.
  // Larger inputs grow incrementally, never reserving their uncompressed size.
  auto output_chunk = input.size() <= 4096
      ? std::size_t(zng_deflateBound(&stream, input.size())) : codec_chunk_size;
  std::size_t consumed = 0;
  do {
    if (!stream.avail_in && consumed < input.size()) {
      auto chunk = std::min(input.size() - consumed, std::size_t(std::numeric_limits<decltype(stream.avail_in)>::max()));
      stream.next_in = reinterpret_cast<std::uint8_t const*>(input.data() + consumed);
      stream.avail_in = chunk;
      consumed += chunk;
    }
    auto committed = output.size();
    if (output_chunk > ByteArray::max_size() - committed)
      return std::unexpected(IOException("Compressed output size overflow"));
    output.reserve(committed + output_chunk);
    stream.next_out = reinterpret_cast<std::uint8_t*>(output.data() + committed);
    stream.avail_out = output_chunk;
    status = zng_deflate(&stream, consumed == input.size() ? Z_FINISH : Z_NO_FLUSH);
    if (status != Z_OK && status != Z_STREAM_END)
      return std::unexpected(IOException::format("Compression error ({}): {}", status, stream.msg ? stream.msg : "no detail"));
    output.resize(committed + output_chunk - stream.avail_out);
  } while (status != Z_STREAM_END);
  status = owner.end();
  if (status != Z_OK)
    return std::unexpected(IOException::format("Failed to end deflate stream ({})", status));
  return {};
}

std::expected<ByteArray, StarException> compress(std::span<char const> input, CompressionLevel level) {
  ByteArray output;
  auto result = compress(input, output, level);
  if (!result)
    return std::unexpected(std::move(result.error()));
  compact_result(output);
  return output;
}

std::expected<void, StarException> decompress(std::span<char const> input, ByteArray& output, std::size_t limit) {
  return inflate_data(input, output, limit, 15);
}

std::expected<ByteArray, StarException> decompress(std::span<char const> input, std::size_t limit) {
  ByteArray output;
  auto result = decompress(input, output, limit);
  if (!result)
    return std::unexpected(std::move(result.error()));
  compact_result(output);
  return output;
}

std::expected<void, StarException> decompress_gzip(std::span<char const> input, ByteArray& output, std::size_t limit) {
  return inflate_data(input, output, limit, 31);
}

std::expected<ByteArray, StarException> decompress_gzip(std::span<char const> input, std::size_t limit) {
  ByteArray output;
  auto result = decompress_gzip(input, output, limit);
  if (!result)
    return std::unexpected(std::move(result.error()));
  compact_result(output);
  return output;
}

}
