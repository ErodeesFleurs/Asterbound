module;

import std;
import star.byte_array;
import star.format;
import star.compression;
typedef struct ZSTD_CCtx_s ZSTD_CCtx;
typedef struct ZSTD_DCtx_s ZSTD_DCtx;

namespace Star {

// Exclusive caller-owned contexts and alternating scratch buffers. Independent
// frames reuse native workspace; failures leave the caller's output unchanged.
// Not thread-safe; one owner per caller. Scratch may retain a prior output's
// capacity until this context is destroyed. Move transfers workspace ownership.
class ZstdContext {
public:
  ZstdContext() = default;
  ZstdContext(ZstdContext&&) noexcept = default;
  ZstdContext& operator=(ZstdContext&&) noexcept = default;
  ZstdContext(ZstdContext const&) = delete;
  ZstdContext& operator=(ZstdContext const&) = delete;

  [[nodiscard]] std::expected<void, StarException> compress(std::span<char const> input, ByteArray& output, int level = 0);
  [[nodiscard]] std::expected<void, StarException> decompress(std::span<char const> input, ByteArray& output,
                                                          std::size_t limit = max_uncompressed_size);

private:
  friend class CompressionStream;
  friend class DecompressionStream;
  struct CompressionDeleter {
    void operator()(ZSTD_CCtx* context) const noexcept;
  };
  struct DecompressionDeleter {
    void operator()(ZSTD_DCtx* context) const noexcept;
  };
  std::unique_ptr<ZSTD_CCtx, CompressionDeleter> m_compressor;
  std::unique_ptr<ZSTD_DCtx, DecompressionDeleter> m_decompressor;
  ByteArray m_scratch;
};

class CompressionStream {
public:
  [[nodiscard]] static std::expected<CompressionStream, StarException> create();
  CompressionStream(CompressionStream&&) noexcept = default;
  CompressionStream& operator=(CompressionStream&&) noexcept = default;
  CompressionStream(CompressionStream const&) = delete;
  CompressionStream& operator=(CompressionStream const&) = delete;

  // Appends a flushed continuation, not a complete independent frame.
  // After a codec failure only reset() permits reuse.
  [[nodiscard]] std::expected<void, StarException> compress(std::span<char const> input, ByteArray& output);
  [[nodiscard]] std::expected<ByteArray, StarException> compress(std::span<char const> input);
  [[nodiscard]] std::expected<void, StarException> reset();

private:
  CompressionStream() = default;
  std::unique_ptr<ZSTD_CCtx, ZstdContext::CompressionDeleter> m_context;
  bool m_failed = false;
};

struct DecompressionResult {
  std::size_t consumed;
  std::size_t produced;
};

class DecompressionStream {
public:
  [[nodiscard]] static std::expected<DecompressionStream, StarException> create();
  DecompressionStream(DecompressionStream&&) noexcept = default;
  DecompressionStream& operator=(DecompressionStream&&) noexcept = default;
  DecompressionStream(DecompressionStream const&) = delete;
  DecompressionStream& operator=(DecompressionStream const&) = delete;

  // Bounded step for backpressure: unconsumed input belongs to the caller.
  // Output must not overlap input. Partial frames are valid across calls.
  [[nodiscard]] std::expected<DecompressionResult, StarException> decompress(std::span<char const> input, std::span<char> output);
  // Appends at most limit bytes per call; errors restore output size and make
  // the decoder terminal until reset(). This does not roll back codec state.
  [[nodiscard]] std::expected<void, StarException> decompress(std::span<char const> input, ByteArray& output,
                                                          std::size_t limit = max_uncompressed_size);
  [[nodiscard]] std::expected<ByteArray, StarException> decompress(std::span<char const> input,
                                                               std::size_t limit = max_uncompressed_size);
  [[nodiscard]] std::expected<void, StarException> reset();

private:
  DecompressionStream() = default;
  std::unique_ptr<ZSTD_DCtx, ZstdContext::DecompressionDeleter> m_context;
  bool m_failed = false;
};

class ZstdCompression {
public:
  // Encodes empty input as a valid empty frame; decode also accepts the legacy
  // empty-buffer sentinel. Unknown-size frames require DecompressionStream.
  [[nodiscard]] static std::expected<void, StarException> compress(std::span<char const> input, ByteArray& output, int level = 0);
  [[nodiscard]] static std::expected<ByteArray, StarException> compress(std::span<char const> input, int level = 0);
  [[nodiscard]] static std::expected<void, StarException> decompress(std::span<char const> input, ByteArray& output,
                                                                 std::size_t limit = max_uncompressed_size);
  [[nodiscard]] static std::expected<ByteArray, StarException> decompress(std::span<char const> input,
                                                                      std::size_t limit = max_uncompressed_size);
};

}

export module star.zstd_compression;

export namespace Star {
  using ::Star::CompressionStream;
  using ::Star::DecompressionResult;
  using ::Star::DecompressionStream;
  using ::Star::ZstdContext;
  using ::Star::ZstdCompression;
}
