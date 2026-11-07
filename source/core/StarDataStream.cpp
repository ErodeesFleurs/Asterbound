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

extern unsigned const CurrentStreamVersion = 18; // update OpenProtocolVersion too!

DataStream::DataStream()
  : m_byteOrder(ByteOrder::BigEndian), m_nullTerminatedStrings(false),
    m_streamCompatibilityVersion(CurrentStreamVersion) {}
ByteOrder DataStream::byteOrder() const { return m_byteOrder; }
void DataStream::setByteOrder(ByteOrder order) { m_byteOrder = order; }
bool DataStream::nullTerminatedStrings() const { return m_nullTerminatedStrings; }
void DataStream::setNullTerminatedStrings(bool enabled) { m_nullTerminatedStrings = enabled; }
unsigned DataStream::streamCompatibilityVersion() const { return m_streamCompatibilityVersion; }
void DataStream::setStreamCompatibilityVersion(unsigned version) { m_streamCompatibilityVersion = version; }
void DataStream::setStreamCompatibilityVersion(NetCompatibilityRules const& rules) { m_streamCompatibilityVersion = rules.version(); }

std::expected<ByteArray, StarException> DataStream::readBytes(std::size_t len) {
  if (len > std::size_t(std::numeric_limits<std::ptrdiff_t>::max()))
    return std::unexpected(DataStreamException("Byte stream request exceeds addressable size"));
  ByteArray bytes;
  bytes.resize(len);
  auto result = readData(bytes.span());
  if (!result) return std::unexpected(std::move(result.error()));
  return bytes;
}
std::expected<void, StarException> DataStream::writeBytes(ByteArray const& bytes) {
  return writeData(bytes.span());
}

namespace {
template <typename T>
std::expected<std::reference_wrapper<DataStream>, StarException> writeNumber(DataStream& stream, T value) {
  if constexpr (sizeof(T) > 1) value = toByteOrder(stream.byteOrder(), value);
  auto result = stream.writeData({reinterpret_cast<char const*>(&value), sizeof(value)});
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(stream);
}
template <typename T>
std::expected<std::reference_wrapper<DataStream>, StarException> readNumber(DataStream& stream, T& value) {
  T decoded;
  auto result = stream.readData({reinterpret_cast<char*>(&decoded), sizeof(decoded)});
  if (!result) return std::unexpected(std::move(result.error()));
  if constexpr (sizeof(T) > 1) decoded = fromByteOrder(stream.byteOrder(), decoded);
  value = decoded;
  return std::ref(stream);
}
}

std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator<<(bool value) {
  return writeNumber(*this, static_cast<std::uint8_t>(value));
}
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator<<(char value) { return writeNumber(*this, value); }
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator<<(std::int8_t value) { return writeNumber(*this, value); }
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator<<(std::uint8_t value) { return writeNumber(*this, value); }
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator<<(std::int16_t value) { return writeNumber(*this, value); }
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator<<(std::uint16_t value) { return writeNumber(*this, value); }
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator<<(std::int32_t value) { return writeNumber(*this, value); }
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator<<(std::uint32_t value) { return writeNumber(*this, value); }
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator<<(std::int64_t value) { return writeNumber(*this, value); }
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator<<(std::uint64_t value) { return writeNumber(*this, value); }
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator<<(float value) { return writeNumber(*this, value); }
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator<<(double value) { return writeNumber(*this, value); }
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator>>(bool& value) {
  std::uint8_t byte;
  auto result = readNumber(*this, byte);
  if (!result) return std::unexpected(std::move(result.error()));
  // Arbitrary input bytes must not create invalid bool representations.
  value = byte != 0;
  return std::ref(*this);
}
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator>>(char& value) { return readNumber(*this, value); }
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator>>(std::int8_t& value) { return readNumber(*this, value); }
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator>>(std::uint8_t& value) { return readNumber(*this, value); }
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator>>(std::int16_t& value) { return readNumber(*this, value); }
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator>>(std::uint16_t& value) { return readNumber(*this, value); }
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator>>(std::int32_t& value) { return readNumber(*this, value); }
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator>>(std::uint32_t& value) { return readNumber(*this, value); }
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator>>(std::int64_t& value) { return readNumber(*this, value); }
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator>>(std::uint64_t& value) { return readNumber(*this, value); }
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator>>(float& value) { return readNumber(*this, value); }
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator>>(double& value) { return readNumber(*this, value); }

std::expected<std::size_t, StarException> DataStream::writeVlqU(std::uint64_t value) {
  std::uint8_t bytes[10];
  auto count = Star::writeVlqU(value, bytes);
  auto result = writeData({reinterpret_cast<char const*>(bytes), count});
  if (!result) return std::unexpected(std::move(result.error()));
  return count;
}
std::expected<std::size_t, StarException> DataStream::writeVlqI(std::int64_t value) {
  auto encoded = value < 0 ? (std::uint64_t(-(value + 1)) << 1) | 1 : std::uint64_t(value) << 1;
  return writeVlqU(encoded);
}
std::expected<std::size_t, StarException> DataStream::writeVlqS(std::size_t value) {
  return writeVlqU(value == NPos ? 0 : std::uint64_t(value) + 1);
}
std::expected<std::size_t, StarException> DataStream::readVlqU(std::uint64_t& value) {
  std::uint64_t decoded = 0;
  for (std::size_t i = 0; i < 10; ++i) {
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
std::expected<std::size_t, StarException> DataStream::readVlqI(std::int64_t& value) {
  std::uint64_t encoded;
  auto count = readVlqU(encoded);
  if (!count) return std::unexpected(std::move(count.error()));
  value = encoded & 1 ? -std::int64_t(encoded >> 1) - 1 : std::int64_t(encoded >> 1);
  return *count;
}
std::expected<std::size_t, StarException> DataStream::readVlqS(std::size_t& value) {
  std::uint64_t encoded;
  auto count = readVlqU(encoded);
  if (!count) return std::unexpected(std::move(count.error()));
  value = encoded ? std::size_t(encoded - 1) : NPos;
  return *count;
}
std::expected<std::uint64_t, StarException> DataStream::readVlqU() {
  std::uint64_t value;
  auto count = readVlqU(value);
  if (!count) return std::unexpected(std::move(count.error()));
  return value;
}
std::expected<std::int64_t, StarException> DataStream::readVlqI() {
  std::int64_t value;
  auto count = readVlqI(value);
  if (!count) return std::unexpected(std::move(count.error()));
  return value;
}
std::expected<std::size_t, StarException> DataStream::readVlqS() {
  std::size_t value;
  auto count = readVlqS(value);
  if (!count) return std::unexpected(std::move(count.error()));
  return value;
}

std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator<<(std::string_view value) {
  auto result = writeStringData({value.data(), value.size()});
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(*this);
}
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator<<(std::string const& value) {
  auto result = writeStringData({value.data(), value.size()});
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(*this);
}
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator<<(ByteArray const& value) {
  auto count = writeVlqU(value.size());
  if (!count) return std::unexpected(std::move(count.error()));
  auto result = writeData(value.span());
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(*this);
}
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator<<(String const& value) {
  auto result = writeStringData(value.utf8());
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(*this);
}
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator>>(std::string& value) {
  std::string decoded;
  if (m_nullTerminatedStrings) {
    for (;;) {
      auto byte = read<char>();
      if (!byte) return std::unexpected(std::move(byte.error()));
      if (*byte == '\0') break;
      decoded.push_back(*byte);
    }
  } else {
    auto length = readVlqU();
    if (!length) return std::unexpected(std::move(length.error()));
    if (*length > decoded.max_size())
      return std::unexpected(DataStreamException("Serialized string exceeds addressable size"));
    decoded.resize(std::size_t(*length));
    auto result = readData({decoded.data(), decoded.size()});
    if (!result) return std::unexpected(std::move(result.error()));
  }
  value = std::move(decoded);
  return std::ref(*this);
}
std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator>>(ByteArray& value) {
  auto length = readVlqU();
  if (!length) return std::unexpected(std::move(length.error()));
  if (*length > std::uint64_t(std::numeric_limits<std::ptrdiff_t>::max()))
    return std::unexpected(DataStreamException("Serialized byte array exceeds addressable size"));
  auto decoded = readBytes(std::size_t(*length));
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
std::expected<void, StarException> DataStream::writeStringData(std::span<char const> data) {
  if (m_nullTerminatedStrings) {
    auto result = writeData(data);
    if (!result) return std::unexpected(std::move(result.error()));
    return write<std::uint8_t>(0);
  }
  auto count = writeVlqU(data.size());
  if (!count) return std::unexpected(std::move(count.error()));
  return writeData(data);
}

}
