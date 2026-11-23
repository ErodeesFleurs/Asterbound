#include <string.h>
import std;
import star.memory;
import star.data_stream;
import star.string;
import star.format;
import star.bytes;
import star.vlq;
import star.net_compatibility;
import star.byte_array;
namespace Star {

DataStream::DataStream()
  : m_byte_order(ByteOrder::BigEndian), m_null_terminated_strings(false),
    m_stream_compatibility_version(open_protocol_version) {}
ByteOrder DataStream::byte_order() const { return m_byte_order; }
void DataStream::set_byte_order(ByteOrder order) { m_byte_order = order; }
bool DataStream::null_terminated_strings() const { return m_null_terminated_strings; }
void DataStream::set_null_terminated_strings(bool enabled) { m_null_terminated_strings = enabled; }
unsigned DataStream::stream_compatibility_version() const { return m_stream_compatibility_version; }
void DataStream::set_stream_compatibility_version(unsigned version) { m_stream_compatibility_version = version; }
void DataStream::set_stream_compatibility_version(NetCompatibilityRules const& rules) {
  m_stream_compatibility_version = rules.version();
}

std::expected<ByteArray, StarException> DataStream::read_bytes(std::size_t len) {
  if (len > std::size_t(std::numeric_limits<std::ptrdiff_t>::max()))
    return std::unexpected(DataStreamException("Byte stream request exceeds addressable size"));
  ByteArray bytes;
  bytes.resize(len);
  auto result = read_data(bytes.span());
  if (!result) return std::unexpected(std::move(result.error()));
  return bytes;
}
std::expected<void, StarException> DataStream::write_bytes(ByteArray const& bytes) { return write_data(bytes.span()); }

namespace {
template <byte_order_value T>
std::expected<std::reference_wrapper<DataStream>, StarException> write_number(DataStream& stream, T value) {
  if constexpr (sizeof(T) > 1)
    value = to_byte_order(stream.byte_order(), value);
  auto result = stream.write_data({reinterpret_cast<char const*>(&value), sizeof(value)});
  if (!result)
    return std::unexpected(std::move(result.error()));
  return std::ref(stream);
}
template <byte_order_value T>
std::expected<std::reference_wrapper<DataStream>, StarException> read_number(DataStream& stream, T& value) {
  T decoded;
  auto result = stream.read_data({reinterpret_cast<char*>(&decoded), sizeof(decoded)});
  if (!result)
    return std::unexpected(std::move(result.error()));
  if constexpr (sizeof(T) > 1)
    decoded = from_byte_order(stream.byte_order(), decoded);
  value = decoded;
  return std::ref(stream);
}
}// namespace

std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator<<(bool value) {
  return write_number(*this, static_cast<std::uint8_t>(value));
}
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator<<(char value) {
  return write_number(*this, value);
}
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator<<(std::int8_t value) {
  return write_number(*this, value);
}
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator<<(std::uint8_t value) {
  return write_number(*this, value);
}
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator<<(std::int16_t value) {
  return write_number(*this, value);
}
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator<<(std::uint16_t value) {
  return write_number(*this, value);
}
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator<<(std::int32_t value) {
  return write_number(*this, value);
}
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator<<(std::uint32_t value) {
  return write_number(*this, value);
}
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator<<(std::int64_t value) {
  return write_number(*this, value);
}
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator<<(std::uint64_t value) {
  return write_number(*this, value);
}
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator<<(float value) {
  return write_number(*this, value);
}
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator<<(double value) {
  return write_number(*this, value);
}
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator>>(bool& value) {
  std::uint8_t byte;
  auto result = read_number(*this, byte);
  if (!result)
    return std::unexpected(std::move(result.error()));
  // Arbitrary input bytes must not create invalid bool representations.
  value = byte != 0;
  return std::ref(*this);
}
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator>>(char& value) {
  return read_number(*this, value);
}
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator>>(std::int8_t& value) {
  return read_number(*this, value);
}
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator>>(std::uint8_t& value) {
  return read_number(*this, value);
}
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator>>(std::int16_t& value) {
  return read_number(*this, value);
}
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator>>(std::uint16_t& value) {
  return read_number(*this, value);
}
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator>>(std::int32_t& value) {
  return read_number(*this, value);
}
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator>>(std::uint32_t& value) {
  return read_number(*this, value);
}
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator>>(std::int64_t& value) {
  return read_number(*this, value);
}
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator>>(std::uint64_t& value) {
  return read_number(*this, value);
}
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator>>(float& value) {
  return read_number(*this, value);
}
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator>>(double& value) {
  return read_number(*this, value);
}

std::expected<std::size_t, StarException> DataStream::write_vlq_u(std::uint64_t value) {
  std::array<char, max_vlq_bytes> bytes;
  auto count = Star::write_vlq_u(value, std::span(bytes));
  auto result = write_data(std::span(bytes).first(count));
  if (!result) return std::unexpected(std::move(result.error()));
  return count;
}
std::expected<std::size_t, StarException> DataStream::write_vlq_i(std::int64_t value) {
  return write_vlq_u(zigzag_encode(value));
}
std::expected<std::size_t, StarException> DataStream::write_vlq_s(std::size_t value) {
  return write_vlq_u(value == npos ? 0 : std::uint64_t(value) + 1);
}
std::expected<std::size_t, StarException> DataStream::read_vlq_u(std::uint64_t& value) {
  std::uint64_t decoded = 0;
  for (std::size_t i = 0; i < max_vlq_bytes; ++i) {
    auto byte = read<std::uint8_t>();
    if (!byte) return std::unexpected(std::move(byte.error()));
    decoded = (decoded << 7) | std::uint64_t(*byte & 127);
    if (!(*byte & 128)) {
      value = decoded;
      return i + 1;
    }
  }
  return std::unexpected(DataStreamException("Error reading VLQ encoded integer!"));
}
std::expected<std::size_t, StarException> DataStream::read_vlq_i(std::int64_t& value) {
  std::uint64_t encoded;
  auto count = read_vlq_u(encoded);
  if (!count) return std::unexpected(std::move(count.error()));
  value = zigzag_decode(encoded);
  return *count;
}
std::expected<std::size_t, StarException> DataStream::read_vlq_s(std::size_t& value) {
  std::uint64_t encoded;
  auto count = read_vlq_u(encoded);
  if (!count) return std::unexpected(std::move(count.error()));
  value = encoded ? std::size_t(encoded - 1) : npos;
  return *count;
}
std::expected<std::uint64_t, StarException> DataStream::read_vlq_u() {
  std::uint64_t value;
  auto count = read_vlq_u(value);
  if (!count) return std::unexpected(std::move(count.error()));
  return value;
}
std::expected<std::int64_t, StarException> DataStream::read_vlq_i() {
  std::int64_t value;
  auto count = read_vlq_i(value);
  if (!count) return std::unexpected(std::move(count.error()));
  return value;
}
std::expected<std::size_t, StarException> DataStream::read_vlq_s() {
  std::size_t value;
  auto count = read_vlq_s(value);
  if (!count) return std::unexpected(std::move(count.error()));
  return value;
}

std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator<<(std::string_view value) {
  auto result = write_string_data({value.data(), value.size()});
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(*this);
}
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator<<(std::string const& value) {
  auto result = write_string_data({value.data(), value.size()});
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(*this);
}
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator<<(ByteArray const& value) {
  auto count = write_vlq_u(value.size());
  if (!count) return std::unexpected(std::move(count.error()));
  auto result = write_data(value.span());
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(*this);
}
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator<<(String const& value) {
  auto result = write_string_data(value.utf8());
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(*this);
}
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator>>(std::string& value) {
  std::string decoded;
  if (m_null_terminated_strings) {
    for (;;) {
      auto byte = read<char>();
      if (!byte) return std::unexpected(std::move(byte.error()));
      if (*byte == '\0') break;
      decoded.push_back(*byte);
    }
  } else {
    auto length = read_vlq_u();
    if (!length) return std::unexpected(std::move(length.error()));
    if (*length > decoded.max_size())
      return std::unexpected(DataStreamException("Serialized string exceeds addressable size"));
    decoded.resize(std::size_t(*length));
    auto result = read_data({decoded.data(), decoded.size()});
    if (!result) return std::unexpected(std::move(result.error()));
  }
  value = std::move(decoded);
  return std::ref(*this);
}
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator>>(ByteArray& value) {
  auto length = read_vlq_u();
  if (!length) return std::unexpected(std::move(length.error()));
  if (*length > std::uint64_t(std::numeric_limits<std::ptrdiff_t>::max()))
    return std::unexpected(DataStreamException("Serialized byte array exceeds addressable size"));
  auto decoded = read_bytes(std::size_t(*length));
  if (!decoded) return std::unexpected(std::move(decoded.error()));
  value = std::move(*decoded);
  return std::ref(*this);
}
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator>>(String& value) {
  auto decoded = read<std::string>();
  if (!decoded) return std::unexpected(std::move(decoded.error()));
  value = std::move(*decoded);
  return std::ref(*this);
}
std::expected<void, StarException> DataStream::write_string_data(std::span<char const> data) {
  if (m_null_terminated_strings) {
    auto result = write_data(data);
    if (!result) return std::unexpected(std::move(result.error()));
    return write<std::uint8_t>(0);
  }
  auto count = write_vlq_u(data.size());
  if (!count) return std::unexpected(std::move(count.error()));
  return write_data(data);
}
}
