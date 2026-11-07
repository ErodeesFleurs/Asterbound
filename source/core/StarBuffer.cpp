import std;
import star.io_device;
import star.string;
import star.buffer;
import star.format;

import star.byte_array;
namespace Star {

Buffer::Buffer()
  : m_pos(0) {
  setMode(IOMode::ReadWrite);
}

Buffer::Buffer(std::size_t initialSize)
  : Buffer() {
  reset(initialSize);
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
  StreamOffset base = 0;
  if (mode == IOSeek::Relative)
    base = m_pos;
  else if (mode == IOSeek::End) {
    if (!std::in_range<StreamOffset>(m_bytes.size()))
      return std::unexpected(IOException("Error, seek position out of range"));
    base = m_bytes.size();
    if (pos == std::numeric_limits<StreamOffset>::min())
      return std::unexpected(IOException("Error, seek position out of range"));
    pos = -pos;
  }
  if ((pos > 0 && base > std::numeric_limits<StreamOffset>::max() - pos)
      || (pos < 0 && (pos == std::numeric_limits<StreamOffset>::min() || base < -pos)))
    return std::unexpected(IOException("Error, seek position out of range"));
  auto newPos = base + pos;
  if (newPos < 0 || !std::in_range<std::size_t>(newPos))
    return std::unexpected(IOException("Error, seek position out of range"));
  m_pos = static_cast<std::size_t>(newPos);
  return {};
}

[[nodiscard]] std::expected<void, StarException> Buffer::resize(StreamOffset size) {
  if (size < 0 || !std::in_range<std::size_t>(size))
    return std::unexpected(IOException("Error, buffer size out of range"));
  data().resize(static_cast<std::size_t>(size));
  return {};
}

[[nodiscard]] std::expected<bool, StarException> Buffer::atEnd() {
  return m_pos >= m_bytes.size();
}

[[nodiscard]] std::expected<std::size_t, StarException> Buffer::read(std::span<char> data) {
  auto result = doRead(m_pos, data);
  if (!result)
    return std::unexpected(std::move(result.error()));
  m_pos += *result;
  return *result;
}

[[nodiscard]] std::expected<std::size_t, StarException> Buffer::write(std::span<char const> data) {
  auto result = doWrite(m_pos, data);
  if (!result)
    return std::unexpected(std::move(result.error()));
  m_pos += *result;
  return *result;
}

[[nodiscard]] std::expected<std::size_t, StarException> Buffer::readAbsolute(StreamOffset readPosition, std::span<char> data) {
  if (readPosition < 0 || !std::in_range<std::size_t>(readPosition))
    return std::unexpected(IOException("Error, readPosition out of range"));
  return doRead(static_cast<std::size_t>(readPosition), data);
}

[[nodiscard]] std::expected<std::size_t, StarException> Buffer::writeAbsolute(StreamOffset writePosition, std::span<char const> data) {
  if (writePosition < 0 || !std::in_range<std::size_t>(writePosition))
    return std::unexpected(IOException("Error, writePosition out of range"));
  return doWrite(static_cast<std::size_t>(writePosition), data);
}

[[nodiscard]] std::expected<void, StarException> Buffer::open(IOMode mode) {
  setMode(mode);
  if (mode & IOMode::Write && mode & IOMode::Truncate)
    m_bytes.resize(0);
  if (mode & IOMode::Append)
    m_pos = m_bytes.size();
  return {};
}

[[nodiscard]] std::expected<String, StarException> Buffer::deviceName() const {
  return strf("Buffer <{}>", (void*)this);
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

ByteArray Buffer::takeData() {
  ByteArray ret = std::move(m_bytes);
  reset(0);
  return ret;
}

char* Buffer::ptr() {
  return data().ptr();
}

char const* Buffer::ptr() const {
  return m_bytes.ptr();
}

std::size_t Buffer::dataSize() const {
  return m_bytes.size();
}

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

void Buffer::reset(std::size_t newSize) {
  m_pos = 0;
  m_bytes.fill(newSize, 0);
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

[[nodiscard]] std::expected<std::size_t, StarException> Buffer::doRead(std::size_t pos, std::span<char> data) {
  auto len = data.size();
  if (len == 0)
    return 0;

  if (!isReadable())
    return std::unexpected(IOException("Error, read called on non-readable Buffer"));

  if (pos >= m_bytes.size())
    return 0;

  std::size_t l = std::min(m_bytes.size() - pos, len);
  std::memcpy(data.data(), m_bytes.ptr() + pos, l);
  return l;
}

[[nodiscard]] std::expected<std::size_t, StarException> Buffer::doWrite(std::size_t pos, std::span<char const> data) {
  auto len = data.size();
  if (len == 0)
    return 0;

  if (!isWritable())
    return std::unexpected(EofException("Error, write called on non-writable Buffer"));

  if (len > static_cast<std::size_t>(std::numeric_limits<StreamOffset>::max()) || pos > static_cast<std::size_t>(std::numeric_limits<StreamOffset>::max()) - len)
    return std::unexpected(IOException("Error, write position out of range"));

  if (pos + len > m_bytes.size())
    m_bytes.resize(pos + len);

  std::memcpy(m_bytes.ptr() + pos, data.data(), len);
  return len;
}

ExternalBuffer::ExternalBuffer()
  : m_pos(0), m_bytes() {
  setMode(IOMode::Read);
}

ExternalBuffer::ExternalBuffer(std::span<char const> externalData) : ExternalBuffer() {
  reset(externalData);
}

[[nodiscard]] std::expected<StreamOffset, StarException> ExternalBuffer::pos() {
  return m_pos;
}

[[nodiscard]] std::expected<void, StarException> ExternalBuffer::seek(StreamOffset pos, IOSeek mode) {
  StreamOffset base = 0;
  if (mode == IOSeek::Relative)
    base = m_pos;
  else if (mode == IOSeek::End) {
    if (!std::in_range<StreamOffset>(m_bytes.size()))
      return std::unexpected(IOException("Error, seek position out of range"));
    base = m_bytes.size();
    if (pos == std::numeric_limits<StreamOffset>::min())
      return std::unexpected(IOException("Error, seek position out of range"));
    pos = -pos;
  }
  if ((pos > 0 && base > std::numeric_limits<StreamOffset>::max() - pos)
      || (pos < 0 && (pos == std::numeric_limits<StreamOffset>::min() || base < -pos)))
    return std::unexpected(IOException("Error, seek position out of range"));
  auto newPos = base + pos;
  if (newPos < 0 || !std::in_range<std::size_t>(newPos))
    return std::unexpected(IOException("Error, seek position out of range"));
  m_pos = static_cast<std::size_t>(newPos);
  return {};
}

[[nodiscard]] std::expected<bool, StarException> ExternalBuffer::atEnd() {
  return m_pos >= m_bytes.size();
}

[[nodiscard]] std::expected<std::size_t, StarException> ExternalBuffer::read(std::span<char> data) {
  auto result = doRead(m_pos, data);
  if (!result)
    return std::unexpected(std::move(result.error()));
  m_pos += *result;
  return *result;
}

[[nodiscard]] std::expected<std::size_t, StarException> ExternalBuffer::write(std::span<char const>) {
  return std::unexpected(IOException("Error, ExternalBuffer is not writable"));
}

[[nodiscard]] std::expected<std::size_t, StarException> ExternalBuffer::readAbsolute(StreamOffset readPosition, std::span<char> data) {
  if (readPosition < 0 || !std::in_range<std::size_t>(readPosition))
    return std::unexpected(IOException("Error, readPosition out of range"));
  return doRead(static_cast<std::size_t>(readPosition), data);
}

[[nodiscard]] std::expected<std::size_t, StarException> ExternalBuffer::writeAbsolute(StreamOffset, std::span<char const>) {
  return std::unexpected(IOException("Error, ExternalBuffer is not writable"));
}

[[nodiscard]] std::expected<String, StarException> ExternalBuffer::deviceName() const {
  return strf("ExternalBuffer <{}>", (void*)this);
}

[[nodiscard]] std::expected<StreamOffset, StarException> ExternalBuffer::size() {
  if (!std::in_range<StreamOffset>(m_bytes.size()))
    return std::unexpected(IOException("Error, buffer size out of range"));
  return static_cast<StreamOffset>(m_bytes.size());
}

char const* ExternalBuffer::ptr() const {
  return m_bytes.data();
}

std::size_t ExternalBuffer::dataSize() const {
  return m_bytes.size();
}

bool ExternalBuffer::empty() const {
  return m_bytes.empty();
}

ExternalBuffer::operator bool() const {
  return !m_bytes.empty();
}

void ExternalBuffer::reset(std::span<char const> externalData) {
  m_pos = 0;
  m_bytes = externalData;
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

[[nodiscard]] std::expected<std::size_t, StarException> ExternalBuffer::doRead(std::size_t pos, std::span<char> data) {
  auto len = data.size();
  if (!isReadable())
    return std::unexpected(IOException("Error, read called on non-readable Buffer"));
  if (len == 0)
    return 0;

  if (pos >= m_bytes.size())
    return 0;

  std::size_t l = std::min(m_bytes.size() - pos, len);
  std::memcpy(data.data(), m_bytes.data() + pos, l);
  return l;
}

}
