import std;
import star.format;
import star.byte_array;

typedef struct ZSTD_CCtx_s ZSTD_CCtx;
typedef struct ZSTD_DCtx_s ZSTD_DCtx;
import star.zstd_compression;
#include <zstd.h>
#include <stdio.h>

namespace Star {
namespace {

IOException zstd_error(std::string_view operation, std::size_t status) {
  return IOException::format("ZSTD {} error ({}): {}", operation, status, ZSTD_getErrorName(status));
}

bool overlaps(std::span<char const> input, std::span<char const> output) {
  if (input.empty() || output.empty())
    return false;
  auto input_address = reinterpret_cast<std::uintptr_t>(input.data());
  auto output_address = reinterpret_cast<std::uintptr_t>(output.data());
  return input_address >= output_address ? input_address - output_address < output.size()
                                        : output_address - input_address < input.size();
}

// Input may borrow any part of the output allocation, including its spare tail.
void stabilize(std::span<char const>& input, ByteArray const& output, ByteArray& snapshot) {
  if (overlaps(input, {output.data(), output.capacity()})) {
    snapshot = ByteArray(input);
    input = snapshot.span();
  }
}

template <class Operation>
concept codec_operation = requires(Operation& operation, ByteArray& output) {
  { std::invoke(operation, output) } -> std::same_as<std::expected<void, StarException>>;
};

template <codec_operation Operation>
std::expected<ByteArray, StarException> owning_result(Operation&& operation) {
  ByteArray output;
  auto result = std::invoke(operation, output);
  if (!result)
    return std::unexpected(std::move(result.error()));
  // Small native bounds are cheap to retain; large high-ratio frames are not.
  if (output.capacity() > 256 && output.capacity() - output.size() > output.size())
    output.shrink_to_fit();
  return output;
}

}

void ZstdContext::CompressionDeleter::operator()(ZSTD_CCtx* context) const noexcept {
  auto status = ZSTD_freeCCtx(context);
  if (ZSTD_isError(status) && !Star::print(stderr, "Failed to free ZSTD compressor: {}\n", ZSTD_getErrorName(status)))
    std::_Exit(1);
}

void ZstdContext::DecompressionDeleter::operator()(ZSTD_DCtx* context) const noexcept {
  auto status = ZSTD_freeDCtx(context);
  if (ZSTD_isError(status) && !Star::print(stderr, "Failed to free ZSTD decompressor: {}\n", ZSTD_getErrorName(status)))
    std::_Exit(1);
}

std::expected<void, StarException> ZstdContext::compress(std::span<char const> input, ByteArray& output, int level) {
  if (!m_compressor)
    m_compressor.reset(ZSTD_createCCtx());
  if (!m_compressor)
    return std::unexpected(IOException("Cannot create ZSTD compressor"));
  auto bound = ZSTD_compressBound(input.size());
  if (ZSTD_isError(bound))
    return std::unexpected(zstd_error("compression bound", bound));
  if (bound > ByteArray::max_size())
    return std::unexpected(IOException("ZSTD compressed output size overflow"));
  m_scratch.resize(bound);
  auto size = ZSTD_compressCCtx(m_compressor.get(), m_scratch.data(), bound, input.data(), input.size(), level);
  if (ZSTD_isError(size))
    return std::unexpected(zstd_error("compression", size));
  m_scratch.resize(size);
  std::swap(m_scratch, output);
  return {};
}

std::expected<void, StarException> ZstdContext::decompress(std::span<char const> input, ByteArray& output, std::size_t limit) {
  // Accept the legacy empty-buffer sentinel as well as real empty Zstd frames.
  if (input.empty()) {
    m_scratch.clear();
    std::swap(m_scratch, output);
    return {};
  }
  auto size = ZSTD_getFrameContentSize(input.data(), input.size());
  if (size == ZSTD_CONTENTSIZE_ERROR)
    return std::unexpected(IOException("Invalid ZSTD frame header"));
  if (size == ZSTD_CONTENTSIZE_UNKNOWN)
    return std::unexpected(IOException("Cannot determine ZSTD decompressed size"));
  if (size > limit || size > ByteArray::max_size())
    return std::unexpected(IOException::format("ZSTD frame declares {} bytes, above the {} byte limit", size, limit));
  if (!m_decompressor)
    m_decompressor.reset(ZSTD_createDCtx());
  if (!m_decompressor)
    return std::unexpected(IOException("Cannot create ZSTD decompressor"));
  m_scratch.resize(size);
  auto decoded = ZSTD_decompressDCtx(m_decompressor.get(), m_scratch.data(), size, input.data(), input.size());
  if (ZSTD_isError(decoded))
    return std::unexpected(zstd_error("decompression", decoded));
  m_scratch.resize(decoded);
  std::swap(m_scratch, output);
  return {};
}

std::expected<CompressionStream, StarException> CompressionStream::create() {
  CompressionStream stream;
  stream.m_context.reset(ZSTD_createCCtx());
  if (!stream.m_context)
    return std::unexpected(IOException("Cannot create ZSTD compression stream"));
  auto status = ZSTD_CCtx_setParameter(stream.m_context.get(), ZSTD_c_enableLongDistanceMatching, 1);
  if (ZSTD_isError(status))
    return std::unexpected(zstd_error("set long-distance matching", status));
  status = ZSTD_CCtx_setParameter(stream.m_context.get(), ZSTD_c_windowLog, 24);
  if (ZSTD_isError(status))
    return std::unexpected(zstd_error("set compression window", status));
  status = ZSTD_CCtx_setParameter(stream.m_context.get(), ZSTD_c_compressionLevel, 2);
  if (ZSTD_isError(status))
    return std::unexpected(zstd_error("set compression level", status));
  return stream;
}

std::expected<void, StarException> CompressionStream::reset() {
  if (!m_context)
    return std::unexpected(IOException("Cannot reset a moved-from ZSTD compression stream"));
  auto status = ZSTD_CCtx_reset(m_context.get(), ZSTD_reset_session_only);
  if (ZSTD_isError(status))
    return std::unexpected(zstd_error("reset compression stream", status));
  m_failed = false;
  return {};
}

std::expected<void, StarException> CompressionStream::compress(std::span<char const> input, ByteArray& output) {
  if (!m_context || m_failed)
    return std::unexpected(IOException("ZSTD compression stream requires reset"));
  ByteArray snapshot;
  stabilize(input, output, snapshot);
  auto bound = ZSTD_compressBound(input.size());
  if (ZSTD_isError(bound))
    return std::unexpected(zstd_error("compression bound", bound));
  auto chunk_size = std::max(std::size_t(64), std::min(bound, ZSTD_CStreamOutSize()));
  ZSTD_inBuffer source{input.data(), input.size(), 0};
  auto written = output.size();
  for (;;) {
    if (chunk_size > ByteArray::max_size() - written) {
      m_failed = true;
      return std::unexpected(IOException("ZSTD compressed output size overflow"));
    }
    output.reserve(written + chunk_size);
    ZSTD_outBuffer destination{output.data() + written, chunk_size, 0};
    auto previous_input = source.pos;
    auto status = ZSTD_compressStream2(m_context.get(), &destination, &source, ZSTD_e_flush);
    if (ZSTD_isError(status)) {
      m_failed = true;
      return std::unexpected(zstd_error("compression", status));
    }
    written += destination.pos;
    if (!status && source.pos == source.size)
      break;
    if (previous_input == source.pos && !destination.pos) {
      m_failed = true;
      return std::unexpected(IOException("ZSTD compression made no progress"));
    }
  }
  output.resize(written);
  return {};
}

std::expected<ByteArray, StarException> CompressionStream::compress(std::span<char const> input) {
  return owning_result([&](ByteArray& output) { return compress(input, output); });
}

std::expected<DecompressionStream, StarException> DecompressionStream::create() {
  DecompressionStream stream;
  stream.m_context.reset(ZSTD_createDCtx());
  if (!stream.m_context)
    return std::unexpected(IOException("Cannot create ZSTD decompression stream"));
  auto status = ZSTD_DCtx_setParameter(stream.m_context.get(), ZSTD_d_windowLogMax, 25);
  if (ZSTD_isError(status))
    return std::unexpected(zstd_error("set decompression window", status));
  return stream;
}

std::expected<void, StarException> DecompressionStream::reset() {
  if (!m_context)
    return std::unexpected(IOException("Cannot reset a moved-from ZSTD decompression stream"));
  auto status = ZSTD_DCtx_reset(m_context.get(), ZSTD_reset_session_only);
  if (ZSTD_isError(status))
    return std::unexpected(zstd_error("reset decompression stream", status));
  m_failed = false;
  return {};
}

std::expected<DecompressionResult, StarException> DecompressionStream::decompress(std::span<char const> input, std::span<char> output) {
  if (!m_context || m_failed)
    return std::unexpected(IOException("ZSTD decompression stream requires reset"));
  if (overlaps(input, output))
    return std::unexpected(IOException("ZSTD decompression spans overlap"));
  if (output.empty())
    return DecompressionResult{0, 0};
  ZSTD_inBuffer source{input.data(), input.size(), 0};
  ZSTD_outBuffer destination{output.data(), output.size(), 0};
  auto status = ZSTD_decompressStream(m_context.get(), &destination, &source);
  if (ZSTD_isError(status)) {
    m_failed = true;
    return std::unexpected(zstd_error("decompression", status));
  }
  return DecompressionResult{source.pos, destination.pos};
}

std::expected<void, StarException> DecompressionStream::decompress(std::span<char const> input, ByteArray& output, std::size_t limit) {
  if (!m_context || m_failed)
    return std::unexpected(IOException("ZSTD decompression stream requires reset"));
  ByteArray snapshot;
  stabilize(input, output, snapshot);
  auto original_size = output.size();
  auto budget = std::min(limit, ByteArray::max_size() - original_size);
  std::size_t produced = 0;
  auto chunk_hint = std::max(std::size_t(256), std::min(input.size(), std::size_t(8192)) * 4);
  std::array<char, 1> probe;
  for (;;) {
    auto chunk_size = std::min(chunk_hint, budget - produced);
    std::span<char> tail = probe;
    if (chunk_size) {
      output.reserve(original_size + produced + chunk_size);
      tail = {output.data() + original_size + produced, chunk_size};
    }
    auto decoded = decompress(input, tail);
    if (!decoded)
      return std::unexpected(std::move(decoded.error()));
    if (!chunk_size && decoded->produced) {
      m_failed = true;
      return std::unexpected(IOException::format("ZSTD decompression exceeds limit of {} bytes", limit));
    }
    input = input.subspan(decoded->consumed);
    produced += decoded->produced;
    if (input.empty() && decoded->produced < tail.size())
      break;
    if (!decoded->consumed && !decoded->produced) {
      m_failed = true;
      return std::unexpected(IOException("ZSTD decompression made no progress"));
    }
    chunk_hint = std::min(chunk_hint * 2, std::size_t(32768));
  }
  output.resize(original_size + produced);
  return {};
}

std::expected<ByteArray, StarException> DecompressionStream::decompress(std::span<char const> input, std::size_t limit) {
  return owning_result([&](ByteArray& output) { return decompress(input, output, limit); });
}

std::expected<void, StarException> ZstdCompression::compress(std::span<char const> input, ByteArray& output, int level) {
  ZstdContext context;
  return context.compress(input, output, level);
}

std::expected<ByteArray, StarException> ZstdCompression::compress(std::span<char const> input, int level) {
  return owning_result([&](ByteArray& output) { return compress(input, output, level); });
}

std::expected<void, StarException> ZstdCompression::decompress(std::span<char const> input, ByteArray& output, std::size_t limit) {
  ZstdContext context;
  return context.decompress(input, output, limit);
}

std::expected<ByteArray, StarException> ZstdCompression::decompress(std::span<char const> input, std::size_t limit) {
  return owning_result([&](ByteArray& output) { return decompress(input, output, limit); });
}

}
