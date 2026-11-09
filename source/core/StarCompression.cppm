module;
#include <stdio.h>
#include <zlib-ng.h>
#include <errno.h>
#include <string.h>
import std;
import star.byte_array;
import star.io_device;
import star.string;
import star.format;

export module star.compression;

export namespace Star {

// Zlib compression level, ranges from 0 to 9
using CompressionLevel = int;
CompressionLevel const LowCompression = 2;
CompressionLevel const MediumCompression = 5;
CompressionLevel const HighCompression = 9;

// Never trust the decompressed size declared by a mod, asset pack or peer.
constexpr std::size_t MaxUncompressedSize = 256 * 1024 * 1024;

[[nodiscard]] std::expected<void, StarException> compressData(ByteArray const& in, ByteArray& out, CompressionLevel compression = MediumCompression);
[[nodiscard]] std::expected<ByteArray, StarException> compressData(ByteArray const& in, CompressionLevel compression = MediumCompression);
[[nodiscard]] std::expected<void, StarException> uncompressData(std::span<char const> in, ByteArray& out, std::size_t limit = 0);
[[nodiscard]] std::expected<ByteArray, StarException> uncompressData(std::span<char const> in, std::size_t limit = 0);
[[nodiscard]] std::expected<void, StarException> uncompressData(ByteArray const& in, ByteArray& out, std::size_t limit = 0);
[[nodiscard]] std::expected<ByteArray, StarException> uncompressData(ByteArray const& in, std::size_t limit = 0);
[[nodiscard]] std::expected<void, StarException> uncompressDataGzip(std::span<char const> in, ByteArray& out, std::size_t limit = 0);
[[nodiscard]] std::expected<ByteArray, StarException> uncompressDataGzip(std::span<char const> in, std::size_t limit = 0);
[[nodiscard]] std::expected<void, StarException> uncompressDataGzip(ByteArray const& in, ByteArray& out, std::size_t limit = 0);
[[nodiscard]] std::expected<ByteArray, StarException> uncompressDataGzip(ByteArray const& in, std::size_t limit = 0);

// Random access to a (potentially) compressed file.
class CompressedFile : public IODevice {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<CompressedFile>, StarException> open(String const& filename, IOMode mode, CompressionLevel comp = MediumCompression);
  CompressedFile();
  CompressedFile(String filename);
  ~CompressedFile() override;

  [[nodiscard]] std::expected<void, StarException> set_filename(String filename);
  [[nodiscard]] std::expected<void, StarException> setCompression(CompressionLevel compression);
  [[nodiscard]] std::expected<StreamOffset, StarException> pos() override;
  // Only forward seeks are supported on writes; reads emulate seeks slowly.
  [[nodiscard]] std::expected<void, StarException> seek(StreamOffset pos, IOSeek seek = IOSeek::Absolute) override;
  [[nodiscard]] std::expected<bool, StarException> at_end() override;
  [[nodiscard]] std::expected<std::size_t, StarException> read(std::span<char> data) override;
  [[nodiscard]] std::expected<std::size_t, StarException> write(std::span<char const> data) override;
  [[nodiscard]] std::expected<void, StarException> open(IOMode mode) override;
  // Compression is ignored on read. Always truncates on write.
  [[nodiscard]] std::expected<void, StarException> open(IOMode mode, CompressionLevel compression);
  [[nodiscard]] std::expected<void, StarException> sync() override;
  [[nodiscard]] std::expected<void, StarException> close() override;
  [[nodiscard]] std::expected<std::shared_ptr<IODevice>, StarException> clone() override;

private:
  String m_filename;
  void* m_file;
  CompressionLevel m_compression;
};

}

namespace Star {
namespace {

std::expected<void, StarException> inflateData(std::span<char const> in, ByteArray& out, std::size_t limit, int windowBits) {
  auto inLen = in.size();
  out.clear();
  if (!inLen)
    return {};
  constexpr std::size_t BufferSize = 32 * 1024;
  zng_stream stream{};
  int status = zng_inflateInit2(&stream, windowBits);
  if (status != Z_OK)
    return std::unexpected(IOException::format("Failed to initialise inflate ({})", status));
  auto cleanup = [](zng_stream* stream) noexcept {
    int status = zng_inflateEnd(stream);
    if (status != Z_OK) {
      if (!Star::print(stderr, "Failed to end inflate stream ({})\n", status))
        std::_Exit(1);
    }
  };
  std::unique_ptr<zng_stream, decltype(cleanup)> owner(&stream, cleanup);
  std::size_t consumed = 0;
  do {
    if (!stream.avail_in && consumed < inLen) {
      auto chunk = std::min(inLen - consumed, std::size_t(std::numeric_limits<decltype(stream.avail_in)>::max()));
      stream.next_in = reinterpret_cast<std::uint8_t const*>(in.data() + consumed);
      stream.avail_in = chunk;
      consumed += chunk;
    }
    auto committed = out.size();
    if (BufferSize > std::numeric_limits<std::size_t>::max() - committed)
      return std::unexpected(IOException("Decompressed output size overflow"));
    out.reserve(committed + BufferSize);
    stream.next_out = reinterpret_cast<std::uint8_t*>(out.data() + committed);
    stream.avail_out = BufferSize;
    auto previousInput = stream.avail_in;
    status = zng_inflate(&stream, Z_NO_FLUSH);
    if (status != Z_OK && status != Z_STREAM_END && status != Z_BUF_ERROR)
      return std::unexpected(IOException::format("Decompression error ({}): {}", status, stream.msg ? stream.msg : "no detail"));
    auto produced = BufferSize - stream.avail_out;
    // Neither codec failures nor limit failures publish this call's writable tail.
    if (limit && produced > limit - out.size())
      return std::unexpected(IOException::format("Decompression exceeds limit of {} bytes", limit));
    out.resize(committed + produced);
    if (status != Z_STREAM_END && !produced && previousInput == stream.avail_in)
      return std::unexpected(IOException::format("Incomplete compressed data ({})", status));
  } while (status != Z_STREAM_END);
  owner.release();
  status = zng_inflateEnd(&stream);
  if (status != Z_OK)
    return std::unexpected(IOException::format("Failed to end inflate stream ({})", status));
  return {};
}

IOException compressedFileError(std::string_view operation, gzFile file) {
  int status = Z_OK;
  char const* detail = zng_gzerror(file, &status);
  return IOException::format("Compressed file {} error ({}): {}", operation, status, detail ? detail : "no detail");
}

}

std::expected<void, StarException> compressData(ByteArray const& in, ByteArray& out, CompressionLevel compression) {
  out.clear();
  if (in.empty())
    return {};
  constexpr std::size_t BufferSize = 32 * 1024;
  zng_stream stream{};
  int status = zng_deflateInit(&stream, compression);
  if (status != Z_OK)
    return std::unexpected(IOException::format("Failed to initialise deflate ({})", status));
  auto cleanup = [](zng_stream* stream) noexcept {
    int status = zng_deflateEnd(stream);
    if (status != Z_OK) {
      if (auto written = Star::print(stderr, "Failed to end deflate stream ({})\n", status); !written)
        std::_Exit(1);
    }
  };
  std::unique_ptr<zng_stream, decltype(cleanup)> owner(&stream, cleanup);
  std::size_t consumed = 0;
  do {
    if (!stream.avail_in && consumed < in.size()) {
      auto chunk = std::min(in.size() - consumed, std::size_t(std::numeric_limits<decltype(stream.avail_in)>::max()));
      stream.next_in = reinterpret_cast<std::uint8_t const*>(in.data() + consumed);
      stream.avail_in = chunk;
      consumed += chunk;
    }
    auto committed = out.size();
    if (BufferSize > std::numeric_limits<std::size_t>::max() - committed)
      return std::unexpected(IOException("Compressed output size overflow"));
    out.reserve(committed + BufferSize);
    stream.next_out = reinterpret_cast<std::uint8_t*>(out.data() + committed);
    stream.avail_out = BufferSize;
    status = zng_deflate(&stream, consumed == in.size() ? Z_FINISH : Z_NO_FLUSH);
    if (status != Z_OK && status != Z_STREAM_END)
      return std::unexpected(IOException::format("Compression error ({}): {}", status, stream.msg ? stream.msg : "no detail"));
    out.resize(committed + BufferSize - stream.avail_out);
  } while (status != Z_STREAM_END);
  owner.release();
  status = zng_deflateEnd(&stream);
  if (status != Z_OK)
    return std::unexpected(IOException::format("Failed to end deflate stream ({})", status));
  return {};
}

std::expected<ByteArray, StarException> compressData(ByteArray const& in, CompressionLevel compression) {
  ByteArray out = ByteArray::with_reserve(in.size());
  auto result = compressData(in, out, compression);
  if (!result)
    return std::unexpected(std::move(result.error()));
  // Judge scratch slack relative to both input reservation and actual output.
  // Reusable caller-supplied outputs are never compacted.
  auto retained = std::max(out.size(), in.size());
  if (out.capacity() - retained > retained)
    out.shrink_to_fit();
  return out;
}

std::expected<void, StarException> uncompressData(std::span<char const> in, ByteArray& out, std::size_t limit) {
  return inflateData(in, out, limit, 15);
}

std::expected<ByteArray, StarException> uncompressData(std::span<char const> in, std::size_t limit) {
  ByteArray out;
  auto result = uncompressData(in, out, limit);
  if (!result)
    return std::unexpected(std::move(result.error()));
  if (out.capacity() - out.size() > out.size())
    out.shrink_to_fit();
  return out;
}

std::expected<void, StarException> uncompressData(ByteArray const& in, ByteArray& out, std::size_t limit) {
  return uncompressData(in.span(), out, limit);
}

std::expected<ByteArray, StarException> uncompressData(ByteArray const& in, std::size_t limit) {
  return uncompressData(in.span(), limit);
}

std::expected<void, StarException> uncompressDataGzip(std::span<char const> in, ByteArray& out, std::size_t limit) {
  return inflateData(in, out, limit, 31);
}

std::expected<ByteArray, StarException> uncompressDataGzip(std::span<char const> in, std::size_t limit) {
  ByteArray out;
  auto result = uncompressDataGzip(in, out, limit);
  if (!result)
    return std::unexpected(std::move(result.error()));
  if (out.capacity() - out.size() > out.size())
    out.shrink_to_fit();
  return out;
}

std::expected<void, StarException> uncompressDataGzip(ByteArray const& in, ByteArray& out, std::size_t limit) {
  return uncompressDataGzip(in.span(), out, limit);
}

std::expected<ByteArray, StarException> uncompressDataGzip(ByteArray const& in, std::size_t limit) {
  return uncompressDataGzip(in.span(), limit);
}

std::expected<std::shared_ptr<CompressedFile>, StarException> CompressedFile::open(String const& filename, IOMode mode, CompressionLevel comp) {
  auto file = std::make_shared<CompressedFile>(filename);
  auto result = file->open(mode, comp);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return file;
}

CompressedFile::CompressedFile()
  : IODevice(IOMode::Closed), m_file(nullptr), m_compression(MediumCompression) {}

CompressedFile::CompressedFile(String filename)
  : IODevice(IOMode::Closed), m_filename(std::move(filename)), m_file(nullptr), m_compression(MediumCompression) {}

CompressedFile::~CompressedFile() {
  auto result = close();
  if (!result) {
    if (auto written = Star::print(stderr, "Failed to close compressed file: {}\n", result.error().what()); !written)
      std::_Exit(1);
  }
}

std::expected<StreamOffset, StarException> CompressedFile::pos() {
  if (!m_file)
    return std::unexpected(IOException("Cannot query position of closed compressed file"));
  auto position = zng_gztell(static_cast<gzFile>(m_file));
  if (position < 0)
    return std::unexpected(compressedFileError("tell", static_cast<gzFile>(m_file)));
  return position;
}

std::expected<void, StarException> CompressedFile::seek(StreamOffset offset, IOSeek seekMode) {
  auto beginning = pos();
  if (!beginning)
    return std::unexpected(std::move(beginning.error()));
  int whence;
  if (seekMode == IOSeek::Relative)
    whence = SEEK_CUR;
  else if (seekMode == IOSeek::Absolute)
    whence = SEEK_SET;
  else
    return std::unexpected(IOException("Cannot seek with SeekEnd in compressed file"));
  if (!std::in_range<z_off64_t>(offset))
    return std::unexpected(IOException("Compressed file seek offset is out of range"));
  if (seekMode == IOSeek::Relative && ((offset > 0 && *beginning > std::numeric_limits<StreamOffset>::max() - offset)
      || (offset < 0 && *beginning < std::numeric_limits<StreamOffset>::min() - offset)))
    return std::unexpected(IOException("Compressed file seek position is out of range"));
  auto result = zng_gzseek(static_cast<gzFile>(m_file), static_cast<z_off64_t>(offset), whence);
  if (result < 0)
    return std::unexpected(compressedFileError("seek", static_cast<gzFile>(m_file)));
  auto expectedPosition = seekMode == IOSeek::Relative ? *beginning + offset : offset;
  if (result != expectedPosition)
    return std::unexpected(EofException("Error, unexpected end of file found"));
  return {};
}

std::expected<bool, StarException> CompressedFile::at_end() {
  if (!m_file)
    return std::unexpected(IOException("Cannot query end of closed compressed file"));
  int status;
  zng_gzerror(static_cast<gzFile>(m_file), &status);
  if (status != Z_OK && status != Z_STREAM_END)
    return std::unexpected(compressedFileError("end query", static_cast<gzFile>(m_file)));
  return zng_gzeof(static_cast<gzFile>(m_file)) != 0;
}

std::expected<std::size_t, StarException> CompressedFile::read(std::span<char> data) {
  if (data.empty())
    return 0;
  if (!m_file || !is_readable())
    return std::unexpected(IOException("Compressed file is not open for reading"));
  int result = zng_gzread(static_cast<gzFile>(m_file), data.data(), std::min(data.size(), std::size_t(std::numeric_limits<int>::max())));
  int status;
  zng_gzerror(static_cast<gzFile>(m_file), &status);
  if (result < 0 || (status != Z_OK && status != Z_STREAM_END))
    return std::unexpected(compressedFileError("read", static_cast<gzFile>(m_file)));
  if (!result)
    return std::unexpected(EofException("Error, unexpected end of file found"));
  return static_cast<std::size_t>(result);
}

std::expected<std::size_t, StarException> CompressedFile::write(std::span<char const> data) {
  if (data.empty())
    return 0;
  if (!m_file || !is_writable())
    return std::unexpected(IOException("Compressed file is not open for writing"));
  int result = zng_gzwrite(static_cast<gzFile>(m_file), data.data(), std::min(data.size(), std::size_t(std::numeric_limits<int>::max())));
  if (!result)
    return std::unexpected(compressedFileError("write", static_cast<gzFile>(m_file)));
  return static_cast<std::size_t>(result);
}

std::expected<void, StarException> CompressedFile::set_filename(String filename) {
  if (is_open())
    return std::unexpected(IOException("Cannot call setFilename while CompressedFile is open"));
  m_filename = std::move(filename);
  return {};
}

std::expected<void, StarException> CompressedFile::setCompression(CompressionLevel compression) {
  if (is_open())
    return std::unexpected(IOException("Cannot call setCompression while CompressedFile is open"));
  if (compression < 0 || compression > 9)
    return std::unexpected(IOException::format("Invalid compression level {}", compression));
  m_compression = compression;
  return {};
}

std::expected<void, StarException> CompressedFile::open(IOMode mode, CompressionLevel compression) {
  auto closed = close();
  if (!closed)
    return std::unexpected(std::move(closed.error()));
  auto configured = setCompression(compression);
  if (!configured)
    return std::unexpected(std::move(configured.error()));
  return open(mode);
}

std::expected<void, StarException> CompressedFile::sync() {
  if (!m_file)
    return std::unexpected(IOException("Cannot sync closed compressed file"));
  int status = zng_gzflush(static_cast<gzFile>(m_file), Z_FULL_FLUSH);
  if (status != Z_OK)
    return std::unexpected(IOException::format("Compressed file flush error ({}): {}", status, zng_gzerror(static_cast<gzFile>(m_file), nullptr)));
  return {};
}

std::expected<void, StarException> CompressedFile::open(IOMode mode) {
  if (is_open())
    return std::unexpected(IOException("CompressedFile is already open"));
  String modeString;
  if (mode & IOMode::Append)
    return std::unexpected(IOException("CompressedFile not compatible with Append mode"));
  if ((mode & IOMode::Read) && (mode & IOMode::Write))
    return std::unexpected(IOException("CompressedFile not compatible with ReadWrite mode"));
  if (mode & IOMode::Write)
    modeString = "wb";
  else if (mode & IOMode::Read)
    modeString = "rb";
  else
    return std::unexpected(IOException("CompressedFile requires Read or Write mode"));
  modeString += String(std::to_string(m_compression));
  m_file = zng_gzopen(m_filename.utf8_data(), modeString.utf8_data());
  if (!m_file) {
    int status = errno;
    return std::unexpected(IOException::format("Cannot open filename '{}' (errno {}): {}", m_filename, status, strerror(status)));
  }
  set_mode(mode);
  return {};
}

std::expected<void, StarException> CompressedFile::close() {
  auto file = static_cast<gzFile>(std::exchange(m_file, nullptr));
  set_mode(IOMode::Closed);
  if (!file)
    return {};
  int status = zng_gzclose(file);
  int systemStatus = errno;
  if (status != Z_OK)
    return std::unexpected(IOException::format("Compressed file close error ({}; errno {}): {}", status, systemStatus, status == Z_ERRNO ? strerror(systemStatus) : zng_zError(status)));
  return {};
}

std::expected<std::shared_ptr<IODevice>, StarException> CompressedFile::clone() {
  auto cloned = std::make_shared<CompressedFile>(m_filename);
  auto configured = cloned->setCompression(m_compression);
  if (!configured)
    return std::unexpected(std::move(configured.error()));
  if (is_open()) {
    auto opened = cloned->open(mode());
    if (!opened)
      return std::unexpected(std::move(opened.error()));
    auto position = pos();
    if (!position)
      return std::unexpected(std::move(position.error()));
    auto sought = cloned->seek(*position);
    if (!sought)
      return std::unexpected(std::move(sought.error()));
  }
  return cloned;
}

}
