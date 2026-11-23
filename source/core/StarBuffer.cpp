import std;
import star.io_device;
import star.string;
import star.buffer;
import star.format;

import star.byte_array;
namespace Star {
namespace {
std::expected<std::size_t, StarException> buffer_seek_position(std::size_t current, std::size_t size,
                                                               StreamOffset offset, IOSeek mode) {
  StreamOffset base = 0;
  if (mode == IOSeek::Relative)
    base = current;
  else if (mode == IOSeek::End) {
    if (!std::in_range<StreamOffset>(size))
      return std::unexpected(IOException("Error, seek position out of range"));
    base = size;
    if (offset == std::numeric_limits<StreamOffset>::min())
      return std::unexpected(IOException("Error, seek position out of range"));
    offset = -offset;
  }
  if ((offset > 0 && base > std::numeric_limits<StreamOffset>::max() - offset)
      || (offset < 0 && (offset == std::numeric_limits<StreamOffset>::min() || base < -offset)))
    return std::unexpected(IOException("Error, seek position out of range"));
  auto position = base + offset;
  if (position < 0 || !std::in_range<std::size_t>(position))
    return std::unexpected(IOException("Error, seek position out of range"));
  return static_cast<std::size_t>(position);
}
}// namespace

Buffer::Buffer() : m_pos(0) { set_mode(IOMode::ReadWrite); }

Buffer::Buffer(std::size_t initial_size)
  : Buffer() {
  reset(initial_size);
}

Buffer::Buffer(ByteArray b)
  : Buffer() {
  reset(std::move(b));
}

Buffer::Buffer(Buffer const& buffer)
  : Buffer() {
  operator=(buffer);
}

Buffer::Buffer(Buffer&& buffer)
  : Buffer() {
  operator=(std::move(buffer));
}

[[nodiscard]] std::expected<StreamOffset, StarException> Buffer::pos() {
  return m_pos;
}

[[nodiscard]] std::expected<void, StarException> Buffer::seek(StreamOffset pos, IOSeek mode) {
  auto position = buffer_seek_position(m_pos, m_bytes.size(), pos, mode);
  if (!position)
    return std::unexpected(std::move(position.error()));
  m_pos = *position;
  return {};
}

[[nodiscard]] std::expected<void, StarException> Buffer::resize(StreamOffset size) {
  if (size < 0 || !std::in_range<std::size_t>(size))
    return std::unexpected(IOException("Error, buffer size out of range"));
  data().resize(static_cast<std::size_t>(size));
  return {};
}

[[nodiscard]] std::expected<bool, StarException> Buffer::at_end() { return m_pos >= m_bytes.size(); }

[[nodiscard]] std::expected<std::size_t, StarException> Buffer::read(std::span<char> data) {
  auto result = do_read(m_pos, data);
  if (!result)
    return std::unexpected(std::move(result.error()));
  m_pos += *result;
  return *result;
}

[[nodiscard]] std::expected<std::size_t, StarException> Buffer::write(std::span<char const> data) {
  auto result = do_write(m_pos, data);
  if (!result)
    return std::unexpected(std::move(result.error()));
  m_pos += *result;
  return *result;
}

[[nodiscard]] std::expected<std::size_t, StarException> Buffer::read_absolute(StreamOffset read_position,
                                                                              std::span<char> data) {
  if (read_position < 0 || !std::in_range<std::size_t>(read_position))
    return std::unexpected(IOException("Error, readPosition out of range"));
  return do_read(static_cast<std::size_t>(read_position), data);
}

[[nodiscard]] std::expected<std::size_t, StarException> Buffer::write_absolute(StreamOffset write_position,
                                                                               std::span<char const> data) {
  if (write_position < 0 || !std::in_range<std::size_t>(write_position))
    return std::unexpected(IOException("Error, writePosition out of range"));
  return do_write(static_cast<std::size_t>(write_position), data);
}

[[nodiscard]] std::expected<void, StarException> Buffer::open(IOMode mode) {
  set_mode(mode);
  if (mode & IOMode::Write && mode & IOMode::Truncate)
    m_bytes.resize(0);
  if (mode & IOMode::Append)
    m_pos = m_bytes.size();
  return {};
}

[[nodiscard]] std::expected<String, StarException> Buffer::device_name() const {
  return Star::format("Buffer <{:#x}>", reinterpret_cast<std::uintptr_t>(this));
}

[[nodiscard]] std::expected<StreamOffset, StarException> Buffer::size() {
  if (!std::in_range<StreamOffset>(m_bytes.size()))
    return std::unexpected(IOException("Error, buffer size out of range"));
  return static_cast<StreamOffset>(m_bytes.size());
}

ByteArray& Buffer::data() {
  return m_bytes;
}

ByteArray const& Buffer::data() const {
  return m_bytes;
}

ByteArray Buffer::take_data() {
  ByteArray ret = std::move(m_bytes);
  reset(0);
  return ret;
}

std::size_t Buffer::data_size() const { return m_bytes.size(); }

void Buffer::reserve(std::size_t size) {
  data().reserve(size);
}

void Buffer::clear() {
  m_pos = 0;
  m_bytes.clear();
}

bool Buffer::empty() const {
  return m_bytes.empty();
}

void Buffer::reset(std::size_t new_size) {
  m_pos = 0;
  m_bytes.fill(new_size, 0);
}

void Buffer::reset(ByteArray b) {
  m_pos = 0;
  m_bytes = std::move(b);
}

Buffer& Buffer::operator=(Buffer const& buffer) = default;

Buffer& Buffer::operator=(Buffer&& buffer) {
  IODevice::operator=(buffer);
  m_pos = buffer.m_pos;
  m_bytes = std::move(buffer.m_bytes);

  buffer.m_pos = 0;
  buffer.m_bytes = ByteArray();

  return *this;
}

[[nodiscard]] std::expected<std::size_t, StarException> Buffer::do_read(std::size_t pos, std::span<char> data) {
  auto len = data.size();
  if (len == 0)
    return 0;

  if (!is_readable())
    return std::unexpected(IOException("Error, read called on non-readable Buffer"));

  if (pos >= m_bytes.size())
    return 0;

  auto source = m_bytes.span().subspan(pos, std::min(m_bytes.size() - pos, len));
  std::ranges::copy(source, data.begin());
  return source.size();
}

[[nodiscard]] std::expected<std::size_t, StarException> Buffer::do_write(std::size_t pos, std::span<char const> data) {
  auto len = data.size();
  if (len == 0)
    return 0;

  if (!is_writable())
    return std::unexpected(EofException("Error, write called on non-writable Buffer"));

  if (len > static_cast<std::size_t>(std::numeric_limits<StreamOffset>::max()) || pos > static_cast<std::size_t>(std::numeric_limits<StreamOffset>::max()) - len)
    return std::unexpected(IOException("Error, write position out of range"));

  m_bytes.write_from(data, pos);
  return len;
}

ExternalBuffer::ExternalBuffer() : m_pos(0), m_bytes() { set_mode(IOMode::Read); }

ExternalBuffer::ExternalBuffer(std::span<char const> external_data) : ExternalBuffer() {
  reset(external_data);
}

[[nodiscard]] std::expected<StreamOffset, StarException> ExternalBuffer::pos() {
  return m_pos;
}

[[nodiscard]] std::expected<void, StarException> ExternalBuffer::seek(StreamOffset pos, IOSeek mode) {
  auto position = buffer_seek_position(m_pos, m_bytes.size(), pos, mode);
  if (!position)
    return std::unexpected(std::move(position.error()));
  m_pos = *position;
  return {};
}

[[nodiscard]] std::expected<bool, StarException> ExternalBuffer::at_end() { return m_pos >= m_bytes.size(); }

[[nodiscard]] std::expected<std::size_t, StarException> ExternalBuffer::read(std::span<char> data) {
  auto result = do_read(m_pos, data);
  if (!result)
    return std::unexpected(std::move(result.error()));
  m_pos += *result;
  return *result;
}

[[nodiscard]] std::expected<std::size_t, StarException> ExternalBuffer::write(std::span<char const>) {
  return std::unexpected(IOException("Error, ExternalBuffer is not writable"));
}

[[nodiscard]] std::expected<std::size_t, StarException> ExternalBuffer::read_absolute(StreamOffset read_position,
                                                                                      std::span<char> data) {
  if (read_position < 0 || !std::in_range<std::size_t>(read_position))
    return std::unexpected(IOException("Error, readPosition out of range"));
  return do_read(static_cast<std::size_t>(read_position), data);
}

[[nodiscard]] std::expected<std::size_t, StarException> ExternalBuffer::write_absolute(StreamOffset,
                                                                                       std::span<char const>) {
  return std::unexpected(IOException("Error, ExternalBuffer is not writable"));
}

[[nodiscard]] std::expected<String, StarException> ExternalBuffer::device_name() const {
  return Star::format("ExternalBuffer <{:#x}>", reinterpret_cast<std::uintptr_t>(this));
}

[[nodiscard]] std::expected<StreamOffset, StarException> ExternalBuffer::size() {
  if (!std::in_range<StreamOffset>(m_bytes.size()))
    return std::unexpected(IOException("Error, buffer size out of range"));
  return static_cast<StreamOffset>(m_bytes.size());
}

std::size_t ExternalBuffer::data_size() const { return m_bytes.size(); }

bool ExternalBuffer::empty() const {
  return m_bytes.empty();
}

ExternalBuffer::operator bool() const {
  return !m_bytes.empty();
}

void ExternalBuffer::reset(std::span<char const> external_data) {
  m_pos = 0;
  m_bytes = external_data;
}

[[nodiscard]] std::expected<std::shared_ptr<IODevice>, StarException> Buffer::clone() {
  auto cloned = std::make_shared<Buffer>(*this);
  // Reset position to 0 while preserving mode and data.
  cloned->m_pos = 0;
  return cloned;
}

[[nodiscard]] std::expected<std::shared_ptr<IODevice>, StarException> ExternalBuffer::clone() {
  return std::make_shared<ExternalBuffer>(*this);
}

[[nodiscard]] std::expected<std::size_t, StarException> ExternalBuffer::do_read(std::size_t pos, std::span<char> data) {
  auto len = data.size();
  if (!is_readable())
    return std::unexpected(IOException("Error, read called on non-readable Buffer"));
  if (len == 0)
    return 0;

  if (pos >= m_bytes.size())
    return 0;

  auto source = m_bytes.subspan(pos, std::min(m_bytes.size() - pos, len));
  std::ranges::copy(source, data.begin());
  return source.size();
}

}
