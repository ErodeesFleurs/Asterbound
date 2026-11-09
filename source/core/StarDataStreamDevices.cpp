import star.data_stream_devices;
import std;
import star.io_device;
import star.data_stream;
import star.buffer;

import star.byte_array;
import star.format;
namespace Star {

std::expected<std::size_t, StarException> DataStreamReader::read_vlq_u(std::uint64_t& value) {
  auto position = m_buffer.pos();
  if (!position)
    return std::unexpected(std::move(position.error()));
  auto bytes = m_buffer.span();
  if (!m_buffer.is_readable() || *position < 0 || std::uint64_t(*position) >= bytes.size())
    return DataStream::read_vlq_u(value);
  bytes = bytes.subspan(static_cast<std::size_t>(*position));
  auto available = std::min<std::size_t>(bytes.size(), 10);
  std::uint64_t decoded = 0;
  std::size_t consumed = 0;
  bool complete = false;
  for (; consumed < available;) {
    auto byte = static_cast<std::uint8_t>(bytes[consumed++]);
    decoded = (decoded << 7) | std::uint64_t(byte & 127);
    if (!(byte & 128)) {
      complete = true;
      break;
    }
  }
  auto advanced = m_buffer.seek(static_cast<StreamOffset>(consumed), IOSeek::Relative);
  if (!advanced)
    return std::unexpected(std::move(advanced.error()));
  if (complete) {
    value = decoded;
    return consumed;
  }
  if (consumed == 10)
    return std::unexpected(DataStreamException("Error reading VLQ encoded integer!"));
  // The old byte-wise path attempts the first missing byte and keeps its error.
  char missing;
  auto result = m_buffer.read_full({&missing, 1});
  if (!result)
    return std::unexpected(std::move(result.error()));
  return std::unexpected(DataStreamException("Error reading VLQ encoded integer!"));
}

DataStreamFunctions::DataStreamFunctions(std::function<std::expected<std::size_t, StarException>(std::span<char>)> reader,
    std::function<std::expected<std::size_t, StarException>(std::span<char const>)> writer)
  : m_reader(std::move(reader)), m_writer(std::move(writer)) {}

[[nodiscard]] std::expected<void, StarException> DataStreamFunctions::read_data(std::span<char> data) {
  if (!m_reader)
    return std::unexpected(DataStreamException("DataStreamFunctions no read function given"));
  auto result = m_reader(data);
  if (!result)
    return std::unexpected(std::move(result.error()));
  if (*result != data.size())
    return std::unexpected(EofException("DataStreamFunctions did not read full length"));
  return {};
}

[[nodiscard]] std::expected<void, StarException> DataStreamFunctions::write_data(std::span<char const> data) {
  if (!m_writer)
    return std::unexpected(DataStreamException("DataStreamFunctions no write function given"));
  auto result = m_writer(data);
  if (!result)
    return std::unexpected(std::move(result.error()));
  if (*result != data.size())
    return std::unexpected(EofException("DataStreamFunctions did not write full length"));
  return {};
}

DataStreamIODevice::DataStreamIODevice(IODevice& device)
  : m_device(device) {}

[[nodiscard]] std::expected<void, StarException> DataStreamIODevice::seek(std::size_t pos, IOSeek mode) {
  if (!std::in_range<StreamOffset>(pos))
    return std::unexpected(IOException("Error, seek position out of range"));
  return m_device.seek(static_cast<StreamOffset>(pos), mode);
}

[[nodiscard]] std::expected<bool, StarException> DataStreamIODevice::at_end() { return m_device.at_end(); }

[[nodiscard]] std::expected<StreamOffset, StarException> DataStreamIODevice::pos() {
  return m_device.pos();
}

[[nodiscard]] std::expected<void, StarException> DataStreamIODevice::read_data(std::span<char> data) {
  return m_device.read_full(data);
}

[[nodiscard]] std::expected<void, StarException> DataStreamIODevice::write_data(std::span<char const> data) {
  return m_device.write_full(data);
}

DataStreamBuffer::DataStreamBuffer() = default;

DataStreamBuffer::DataStreamBuffer(std::size_t s)
  : DataStreamBuffer() {
  reset(s);
}

DataStreamBuffer::DataStreamBuffer(ByteArray b)
  : DataStreamBuffer() {
  reset(std::move(b));
}

[[nodiscard]] std::expected<void, StarException> DataStreamBuffer::resize(std::size_t size) {
  if (!std::in_range<StreamOffset>(size))
    return std::unexpected(IOException("Error, buffer size out of range"));
  return m_buffer.resize(static_cast<StreamOffset>(size));
}

void DataStreamBuffer::reserve(std::size_t size) {
  m_buffer.reserve(size);
}

void DataStreamBuffer::clear() {
  m_buffer.clear();
}

ByteArray& DataStreamBuffer::data() {
  return m_buffer.data();
}

ByteArray const& DataStreamBuffer::data() const {
  return m_buffer.data();
}

ByteArray DataStreamBuffer::take_data() { return m_buffer.take_data(); }

char* DataStreamBuffer::ptr() {
  return m_buffer.ptr();
}

const char* DataStreamBuffer::ptr() const {
  return m_buffer.ptr();
}

std::span<char> DataStreamBuffer::span() {
  return m_buffer.span();
}

std::span<char const> DataStreamBuffer::span() const {
  return m_buffer.span();
}

std::size_t DataStreamBuffer::size() const { return m_buffer.data_size(); }

bool DataStreamBuffer::empty() const {
  return m_buffer.empty();
}

[[nodiscard]] std::expected<void, StarException> DataStreamBuffer::seek(std::size_t pos, IOSeek mode) {
  if (!std::in_range<StreamOffset>(pos))
    return std::unexpected(IOException("Error, seek position out of range"));
  return m_buffer.seek(static_cast<StreamOffset>(pos), mode);
}

[[nodiscard]] std::expected<bool, StarException> DataStreamBuffer::at_end() { return m_buffer.at_end(); }

[[nodiscard]] std::expected<std::size_t, StarException> DataStreamBuffer::pos() {
  auto result = m_buffer.pos();
  if (!result)
    return std::unexpected(std::move(result.error()));
  return static_cast<std::size_t>(*result);
}

void DataStreamBuffer::reset(std::size_t newSize) {
  m_buffer.reset(newSize);
}

void DataStreamBuffer::reset(ByteArray b) {
  m_buffer.reset(std::move(b));
}

[[nodiscard]] std::expected<void, StarException> DataStreamBuffer::read_data(std::span<char> data) {
  return m_buffer.read_full(data);
}

[[nodiscard]] std::expected<void, StarException> DataStreamBuffer::write_data(std::span<char const> data) {
  return m_buffer.write_full(data);
}

DataStreamExternalBuffer::DataStreamExternalBuffer() : m_buffer() {}

DataStreamExternalBuffer::DataStreamExternalBuffer(std::span<char const> externalData) : DataStreamExternalBuffer() {
  reset(externalData);
}

char const* DataStreamExternalBuffer::ptr() const {
  return m_buffer.ptr();
}

std::span<char const> DataStreamExternalBuffer::span() const {
  return m_buffer.span();
}

std::size_t DataStreamExternalBuffer::size() const { return m_buffer.data_size(); }

bool DataStreamExternalBuffer::empty() const {
  return m_buffer.empty();
}

[[nodiscard]] std::expected<void, StarException> DataStreamExternalBuffer::seek(std::size_t pos, IOSeek mode) {
  if (!std::in_range<StreamOffset>(pos))
    return std::unexpected(IOException("Error, seek position out of range"));
  return m_buffer.seek(static_cast<StreamOffset>(pos), mode);
}

[[nodiscard]] std::expected<bool, StarException> DataStreamExternalBuffer::at_end() { return m_buffer.at_end(); }

[[nodiscard]] std::expected<std::size_t, StarException> DataStreamExternalBuffer::pos() {
  auto result = m_buffer.pos();
  if (!result)
    return std::unexpected(std::move(result.error()));
  return static_cast<std::size_t>(*result);
}

[[nodiscard]] std::expected<std::size_t, StarException> DataStreamExternalBuffer::remaining() {
  auto result = pos();
  if (!result)
    return std::unexpected(std::move(result.error()));
  if (*result > m_buffer.data_size())
    return std::unexpected(EofException("Error, buffer position is past end"));
  return m_buffer.data_size() - *result;
}

void DataStreamExternalBuffer::reset(std::span<char const> externalData) {
  m_buffer.reset(externalData);
}

[[nodiscard]] std::expected<void, StarException> DataStreamExternalBuffer::read_data(std::span<char> data) {
  return m_buffer.read_full(data);
}
[[nodiscard]] std::expected<void, StarException> DataStreamExternalBuffer::write_data(std::span<char const> data) {
  return m_buffer.write_full(data);
}
}
