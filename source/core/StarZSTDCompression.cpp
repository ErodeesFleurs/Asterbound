import std;
import star.format;
import star.byte_array;

typedef struct ZSTD_CCtx_s ZSTD_CCtx;
typedef struct ZSTD_DCtx_s ZSTD_DCtx;
typedef ZSTD_DCtx ZSTD_DStream;
typedef ZSTD_CCtx ZSTD_CStream;
import star.zstd_compression;
#include <zstd.h>
#include <stdio.h>

import star.compression;

namespace Star {
namespace {

IOException zstdError(std::string_view operation, std::size_t status) {
  return IOException::format("ZSTD {} error ({}): {}", operation, status, ZSTD_getErrorName(status));
}

}

void CompressionStream::StreamDeleter::operator()(ZSTD_CStream* stream) const noexcept {
  auto status = ZSTD_freeCStream(stream);
  if (ZSTD_isError(status)) {
    if (auto written = Star::print(stderr, "Failed to free ZSTD compression stream ({}): {}\n", status, ZSTD_getErrorName(status)); !written)
      std::_Exit(1);
  }
}

std::expected<std::unique_ptr<CompressionStream>, StarException> CompressionStream::create() {
  auto stream = std::unique_ptr<CompressionStream>(new CompressionStream());
  stream->m_cStream.reset(ZSTD_createCStream());
  if (!stream->m_cStream)
    return std::unexpected(IOException("Cannot create ZSTD compression stream"));
  auto status = ZSTD_CCtx_setParameter(stream->m_cStream.get(), ZSTD_c_enableLongDistanceMatching, 1);
  if (ZSTD_isError(status))
    return std::unexpected(zstdError("set long-distance matching", status));
  status = ZSTD_CCtx_setParameter(stream->m_cStream.get(), ZSTD_c_windowLog, 24);
  if (ZSTD_isError(status))
    return std::unexpected(zstdError("set compression window", status));
  status = ZSTD_initCStream(stream->m_cStream.get(), 2);
  if (ZSTD_isError(status))
    return std::unexpected(zstdError("initialise compression stream", status));
  return stream;
}

CompressionStream::~CompressionStream() = default;

std::expected<void, StarException> CompressionStream::compress(std::span<char const> in, ByteArray& out) {
  auto const chunkSize = ZSTD_CStreamOutSize();
  ZSTD_inBuffer input = {in.data(), in.size(), 0};
  auto const originalSize = out.size();
  std::size_t written = originalSize;
  for (;;) {
    if (chunkSize > std::numeric_limits<std::size_t>::max() - written) {
      out.resize(originalSize);
      return std::unexpected(IOException("ZSTD compressed output size overflow"));
    }
    out.resize(written + chunkSize);
    ZSTD_outBuffer output = {out.ptr() + written, chunkSize, 0};
    auto status = ZSTD_compressStream2(m_cStream.get(), &output, &input, ZSTD_e_flush);
    if (ZSTD_isError(status)) {
      out.resize(originalSize);
      return std::unexpected(zstdError("compression", status));
    }
    written += output.pos;
    if (!status && input.pos == input.size)
      break;
  }
  out.resize(written);
  return {};
}

std::expected<void, StarException> CompressionStream::compress(ByteArray const& in, ByteArray& out) {
  return compress(in.span(), out);
}

std::expected<ByteArray, StarException> CompressionStream::compress(std::span<char const> in) {
  ByteArray out;
  auto result = compress(in, out);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return out;
}

std::expected<ByteArray, StarException> CompressionStream::compress(ByteArray const& in) {
  return compress(in.span());
}

void DecompressionStream::StreamDeleter::operator()(ZSTD_DStream* stream) const noexcept {
  auto status = ZSTD_freeDStream(stream);
  if (ZSTD_isError(status)) {
    if (auto written = Star::print(stderr, "Failed to free ZSTD decompression stream ({}): {}\n", status, ZSTD_getErrorName(status)); !written)
      std::_Exit(1);
  }
}

std::expected<std::unique_ptr<DecompressionStream>, StarException> DecompressionStream::create() {
  auto stream = std::unique_ptr<DecompressionStream>(new DecompressionStream());
  stream->m_dStream.reset(ZSTD_createDStream());
  if (!stream->m_dStream)
    return std::unexpected(IOException("Cannot create ZSTD decompression stream"));
  auto status = ZSTD_DCtx_setParameter(stream->m_dStream.get(), ZSTD_d_windowLogMax, 25);
  if (ZSTD_isError(status))
    return std::unexpected(zstdError("set decompression window", status));
  status = ZSTD_initDStream(stream->m_dStream.get());
  if (ZSTD_isError(status))
    return std::unexpected(zstdError("initialise decompression stream", status));
  return stream;
}

DecompressionStream::~DecompressionStream() = default;

std::expected<void, StarException> DecompressionStream::decompress(std::span<char const> in, ByteArray& out) {
  auto const chunkSize = ZSTD_DStreamOutSize();
  ZSTD_inBuffer input = {in.data(), in.size(), 0};
  auto const originalSize = out.size();
  std::size_t written = originalSize;
  for (;;) {
    if (chunkSize > std::numeric_limits<std::size_t>::max() - written) {
      out.resize(originalSize);
      return std::unexpected(IOException("ZSTD decompressed output size overflow"));
    }
    out.resize(written + chunkSize);
    ZSTD_outBuffer output = {out.ptr() + written, chunkSize, 0};
    auto previousInput = input.pos;
    auto status = ZSTD_decompressStream(m_dStream.get(), &output, &input);
    if (ZSTD_isError(status)) {
      out.resize(originalSize);
      return std::unexpected(zstdError("decompression", status));
    }
    written += output.pos;
    if (input.pos == input.size && output.pos < output.size)
      break;
    if (previousInput == input.pos && !output.pos) {
      out.resize(originalSize);
      return std::unexpected(IOException("ZSTD decompression made no progress"));
    }
  }
  out.resize(written);
  return {};
}

std::expected<void, StarException> DecompressionStream::decompress(ByteArray const& in, ByteArray& out) {
  return decompress(in.span(), out);
}

std::expected<ByteArray, StarException> DecompressionStream::decompress(std::span<char const> in) {
  ByteArray out;
  auto result = decompress(in, out);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return out;
}

std::expected<ByteArray, StarException> DecompressionStream::decompress(ByteArray const& in) {
  return decompress(in.span());
}

std::expected<ByteArray, StarException> ZstdCompression::compress(std::span<char const> in, int compressionLevel) {
  auto inLen = in.size();
  if (!inLen)
    return ByteArray();
  auto maxOutSize = ZSTD_compressBound(inLen);
  if (ZSTD_isError(maxOutSize))
    return std::unexpected(zstdError("compression bound", maxOutSize));
  ByteArray out(maxOutSize, 0);
  auto compressedSize = ZSTD_compress(out.ptr(), maxOutSize, in.data(), inLen, compressionLevel);
  if (ZSTD_isError(compressedSize))
    return std::unexpected(zstdError("compression", compressedSize));
  out.resize(compressedSize);
  return out;
}

std::expected<void, StarException> ZstdCompression::compress(std::span<char const> in, ByteArray& out, int compressionLevel) {
  auto result = compress(in, compressionLevel);
  if (!result)
    return std::unexpected(std::move(result.error()));
  out = std::move(*result);
  return {};
}

std::expected<ByteArray, StarException> ZstdCompression::compress(ByteArray const& in, int compressionLevel) {
  return compress(in.span(), compressionLevel);
}

std::expected<void, StarException> ZstdCompression::compress(ByteArray const& in, ByteArray& out, int compressionLevel) {
  return compress(in.span(), out, compressionLevel);
}

std::expected<ByteArray, StarException> ZstdCompression::decompress(std::span<char const> in) {
  auto frameContentSize = ZSTD_getFrameContentSize(in.data(), in.size());
  if (frameContentSize == ZSTD_CONTENTSIZE_ERROR)
    return std::unexpected(IOException("Invalid ZSTD frame header"));
  if (frameContentSize == ZSTD_CONTENTSIZE_UNKNOWN)
    return std::unexpected(IOException("Cannot determine ZSTD decompressed size"));
  if (frameContentSize > MaxUncompressedSize)
    return std::unexpected(IOException::format("ZSTD frame declares {} bytes, above the {} byte limit", frameContentSize, MaxUncompressedSize));
  ByteArray out(frameContentSize, 0);
  auto result = ZSTD_decompress(out.ptr(), frameContentSize, in.data(), in.size());
  if (ZSTD_isError(result))
    return std::unexpected(zstdError("decompression", result));
  out.resize(result);
  return out;
}

std::expected<void, StarException> ZstdCompression::decompress(std::span<char const> in, ByteArray& out) {
  auto result = decompress(in);
  if (!result)
    return std::unexpected(std::move(result.error()));
  out = std::move(*result);
  return {};
}

std::expected<ByteArray, StarException> ZstdCompression::decompress(ByteArray const& in) {
  return decompress(in.span());
}

std::expected<void, StarException> ZstdCompression::decompress(ByteArray const& in, ByteArray& out) {
  return decompress(in.span(), out);
}

}
