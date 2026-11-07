import std;
import star.io_device;
import star.string;
import star.format;
import star.byte_array;

namespace Star {

IODevice::IODevice(IOMode mode) : m_mode(mode) {}
IODevice::~IODevice() = default;

std::expected<void, StarException> IODevice::resize(StreamOffset) {
  return std::unexpected(IOException("resize not supported"));
}

std::expected<void, StarException> IODevice::read_full(std::span<char> data) {
  auto count = read(data);
  if (!count) return std::unexpected(std::move(count.error()));
  if (*count != data.size()) {
    auto end = at_end();
    if (!end) return std::unexpected(std::move(end.error()));
    if (*end)
      return std::unexpected(EofException("Failed to read full buffer in read_full, eof reached."));
    return std::unexpected(IOException("Failed to read full buffer in read_full"));
  }
  return {};
}

std::expected<void, StarException> IODevice::write_full(std::span<char const> data) {
  auto count = write(data);
  if (!count) return std::unexpected(std::move(count.error()));
  if (*count != data.size()) {
    auto end = at_end();
    if (!end) return std::unexpected(std::move(end.error()));
    if (*end)
      return std::unexpected(EofException("Failed to write full buffer in write_full, eof reached."));
    return std::unexpected(IOException("Failed to write full buffer in write_full"));
  }
  return {};
}

std::expected<void, StarException> IODevice::open(IOMode mode) {
  if (mode != m_mode) {
    auto name = device_name();
    if (!name) return std::unexpected(std::move(name.error()));
    return std::unexpected(IOException::format("Cannot reopen device '{}'", *name));
  }
  return {};
}

std::expected<void, StarException> IODevice::close() {
  m_mode = IOMode::Closed;
  return {};
}

std::expected<void, StarException> IODevice::sync() { return {}; }

std::expected<String, StarException> IODevice::device_name() const { return strf("IODevice <{}>", (void*)this); }

std::expected<bool, StarException> IODevice::at_end() {
  auto position = pos();
  if (!position) return std::unexpected(std::move(position.error()));
  auto length = size();
  if (!length) return std::unexpected(std::move(length.error()));
  return *position >= *length;
}

std::expected<StreamOffset, StarException> IODevice::size() {
  auto stored = pos();
  if (!stored) return std::unexpected(IOException("Cannot call size() on IODevice", stored.error()));
  auto moved = seek(0, IOSeek::End);
  if (!moved) return std::unexpected(IOException("Cannot call size() on IODevice", moved.error()));
  auto length = pos();
  auto restored = seek(*stored);
  if (!length) return std::unexpected(IOException("Cannot call size() on IODevice", length.error()));
  if (!restored) return std::unexpected(IOException("Cannot restore position after size() on IODevice", restored.error()));
  return *length;
}

std::expected<std::size_t, StarException> IODevice::read_absolute(StreamOffset readPosition, std::span<char> data) {
  auto stored = pos();
  if (!stored) return std::unexpected(std::move(stored.error()));
  auto moved = seek(readPosition);
  if (!moved) return std::unexpected(std::move(moved.error()));
  auto count = read(data);
  auto restored = seek(*stored);
  if (!count) return std::unexpected(std::move(count.error()));
  if (!restored) return std::unexpected(std::move(restored.error()));
  return *count;
}

std::expected<std::size_t, StarException> IODevice::write_absolute(StreamOffset writePosition,
                                                                   std::span<char const> data) {
  auto stored = pos();
  if (!stored) return std::unexpected(std::move(stored.error()));
  auto moved = seek(writePosition);
  if (!moved) return std::unexpected(std::move(moved.error()));
  auto count = write(data);
  auto restored = seek(*stored);
  if (!count) return std::unexpected(std::move(count.error()));
  if (!restored) return std::unexpected(std::move(restored.error()));
  return *count;
}

std::expected<void, StarException> IODevice::read_full_absolute(StreamOffset position, std::span<char> data) {
  while (!data.empty()) {
    auto count = read_absolute(position, data);
    if (!count) return std::unexpected(std::move(count.error()));
    if (*count == 0 || *count > data.size())
      return std::unexpected(IOException("Failed to read full buffer in read_full_absolute"));
    position += *count;
    data = data.subspan(*count);
  }
  return {};
}

std::expected<void, StarException> IODevice::write_full_absolute(StreamOffset position, std::span<char const> data) {
  while (!data.empty()) {
    auto count = write_absolute(position, data);
    if (!count) return std::unexpected(std::move(count.error()));
    if (*count == 0 || *count > data.size())
      return std::unexpected(IOException("Failed to write full buffer in write_full_absolute"));
    position += *count;
    data = data.subspan(*count);
  }
  return {};
}

std::expected<ByteArray, StarException> IODevice::read_bytes(std::size_t size) {
  ByteArray bytes;
  bytes.resize(size);
  if (size) {
    auto result = read_full(bytes);
    if (!result) return std::unexpected(std::move(result.error()));
  }
  return bytes;
}

std::expected<void, StarException> IODevice::write_bytes(ByteArray const& bytes) { return write_full(bytes); }

std::expected<ByteArray, StarException> IODevice::read_bytes_absolute(StreamOffset position, std::size_t size) {
  ByteArray bytes;
  bytes.resize(size);
  if (size) {
    auto result = read_full_absolute(position, bytes);
    if (!result) return std::unexpected(std::move(result.error()));
  }
  return bytes;
}

std::expected<void, StarException> IODevice::write_bytes_absolute(StreamOffset position, ByteArray const& bytes) {
  return write_full_absolute(position, bytes);
}

void IODevice::set_mode(IOMode mode) { m_mode = mode; }
IODevice::IODevice(IODevice const& rhs) : m_mode(rhs.mode()) {}
IODevice& IODevice::operator=(IODevice const& rhs) {
  m_mode = rhs.mode();
  return *this;
}

}
