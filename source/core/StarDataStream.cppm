module;

import std;
import star.string;
import star.format;
import star.net_compatibility;
import star.byte_array;
import star.bytes;
import star.math_common;
import star.type_identity;
namespace Star {

struct DataStreamExceptionTag {
  static constexpr std::string_view name() { return "DataStreamException"; }
};
using DataStreamException = StarError<DataStreamExceptionTag, IOException>;
extern unsigned const CurrentStreamVersion;

// Portable serialization. Every transfer and complete operator chain has an
// explicit error result; unsuccessful reads never produce a default value.
class DataStream {
public:
  DataStream();
  virtual ~DataStream() = default;
  virtual void const* queryType(TypeKey key) const noexcept {
    return queryTypeOf<DataStream>(this, key);
  }
  virtual std::string_view dynamicTypeName() const noexcept { return typeName<DataStream>(); }

  ByteOrder byteOrder() const;
  void setByteOrder(ByteOrder byteOrder);
  bool nullTerminatedStrings() const;
  void setNullTerminatedStrings(bool nullTerminatedStrings);
  unsigned streamCompatibilityVersion() const;
  void setStreamCompatibilityVersion(unsigned streamCompatibilityVersion);
  void setStreamCompatibilityVersion(NetCompatibilityRules const& rules);

  [[nodiscard]] virtual std::expected<void, StarException> readData(std::span<char> data) = 0;
  [[nodiscard]] virtual std::expected<void, StarException> writeData(std::span<char const> data) = 0;
  [[nodiscard]] virtual std::expected<bool, StarException> at_end() { return false; }
  [[nodiscard]] std::expected<ByteArray, StarException> read_bytes(std::size_t len);
  [[nodiscard]] std::expected<void, StarException> write_bytes(ByteArray const& ba);

  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(bool d);
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(char d);
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(std::int8_t d);
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(std::uint8_t d);
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(std::int16_t d);
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(std::uint16_t d);
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(std::int32_t d);
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(std::uint32_t d);
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(std::int64_t d);
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(std::uint64_t d);
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(float d);
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(double d);
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(bool& d);
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(char& d);
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(std::int8_t& d);
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(std::uint8_t& d);
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(std::int16_t& d);
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(std::uint16_t& d);
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(std::int32_t& d);
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(std::uint32_t& d);
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(std::int64_t& d);
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(std::uint64_t& d);
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(float& d);
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(double& d);

  // VLQ integers retain their on-wire 1..10-byte representation. S maps NPos to zero.
  [[nodiscard]] std::expected<std::size_t, StarException> writeVlqU(std::uint64_t i);
  [[nodiscard]] std::expected<std::size_t, StarException> writeVlqI(std::int64_t i);
  [[nodiscard]] std::expected<std::size_t, StarException> writeVlqS(std::size_t i);
  [[nodiscard]] std::expected<std::size_t, StarException> readVlqU(std::uint64_t& i);
  [[nodiscard]] std::expected<std::size_t, StarException> readVlqI(std::int64_t& i);
  [[nodiscard]] std::expected<std::size_t, StarException> readVlqS(std::size_t& i);
  [[nodiscard]] std::expected<std::uint64_t, StarException> readVlqU();
  [[nodiscard]] std::expected<std::int64_t, StarException> readVlqI();
  [[nodiscard]] std::expected<std::size_t, StarException> readVlqS();

  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(std::string_view s);
  template <std::size_t Size>
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(char const (&text)[Size]) {
    return *this << std::string_view(text, Size - 1);
  }
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(std::string const& s);
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(ByteArray const& d);
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(String const& s);
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(std::string& s);
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(ByteArray& d);
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(String& s);
  template <typename EnumType> requires std::is_enum_v<EnumType>
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(EnumType const& e);
  template <typename EnumType> requires std::is_enum_v<EnumType>
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(EnumType& e);

  template <typename T> [[nodiscard]] std::expected<T, StarException> read();
  template <typename Data> [[nodiscard]] std::expected<void, StarException> read(Data& data);
  template <typename Data> [[nodiscard]] std::expected<void, StarException> write(Data const& data);
  template <typename ReadType, typename Data> [[nodiscard]] std::expected<void, StarException> cread(Data& data);
  template <typename WriteType, typename Data> [[nodiscard]] std::expected<void, StarException> cwrite(Data const& data);
  template <typename IntegralType> [[nodiscard]] std::expected<void, StarException> vuread(IntegralType& data);
  template <typename IntegralType> [[nodiscard]] std::expected<void, StarException> viread(IntegralType& data);
  template <typename IntegralType> [[nodiscard]] std::expected<void, StarException> vsread(IntegralType& data);
  template <typename IntegralType> [[nodiscard]] std::expected<void, StarException> vuwrite(IntegralType const& data);
  template <typename IntegralType> [[nodiscard]] std::expected<void, StarException> viwrite(IntegralType const& data);
  template <typename IntegralType> [[nodiscard]] std::expected<void, StarException> vswrite(IntegralType const& data);
  template <typename FloatType> [[nodiscard]] std::expected<void, StarException> vfread(FloatType& data, FloatType base);
  template <typename FloatType> [[nodiscard]] std::expected<void, StarException> vfwrite(FloatType const& data, FloatType base);
  template <typename PointerType, typename ReadFunction> [[nodiscard]] std::expected<void, StarException> pread(PointerType& pointer, ReadFunction function);
  template <typename PointerType, typename WriteFunction> [[nodiscard]] std::expected<void, StarException> pwrite(PointerType const& pointer, WriteFunction function);
  template <typename PointerType> [[nodiscard]] std::expected<void, StarException> pread(PointerType& pointer);
  template <typename PointerType> [[nodiscard]] std::expected<void, StarException> pwrite(PointerType const& pointer);

  // Callbacks return expected<void, StarException>. Decoding commits only fully
  // decoded elements; pointer decoding commits only after its complete payload.
  template <typename Container, typename WriteFunction> [[nodiscard]] std::expected<void, StarException> writeContainer(Container const& container, WriteFunction function);
  template <typename Container, typename ReadFunction> [[nodiscard]] std::expected<void, StarException> readContainer(Container& container, ReadFunction function);
  template <typename Container, typename WriteFunction> [[nodiscard]] std::expected<void, StarException> writeMapContainer(Container const& container, WriteFunction function);
  template <typename Container, typename ReadFunction> [[nodiscard]] std::expected<void, StarException> readMapContainer(Container& container, ReadFunction function);
  template <typename Container> [[nodiscard]] std::expected<void, StarException> writeContainer(Container const& container);
  template <typename Container> [[nodiscard]] std::expected<void, StarException> readContainer(Container& container);
  template <typename Container> [[nodiscard]] std::expected<void, StarException> writeMapContainer(Container const& container);
  template <typename Container> [[nodiscard]] std::expected<void, StarException> readMapContainer(Container& container);

private:
  [[nodiscard]] std::expected<void, StarException> writeStringData(std::span<char const> data);
  ByteOrder m_byteOrder;
  bool m_nullTerminatedStrings;
  unsigned m_streamCompatibilityVersion;
};

// Failure short-circuits the remainder of a chain. Inspect its final expected.
template <typename Stream, typename T>
  requires std::same_as<Stream, std::expected<std::reference_wrapper<DataStream>, StarException>>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(Stream stream, T const& value) {
  if (!stream)
    return stream;
  return stream->get() << value;
}
template <typename Stream, typename T>
  requires std::same_as<Stream, std::expected<std::reference_wrapper<DataStream>, StarException>>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(Stream stream, T& value) {
  if (!stream)
    return stream;
  return stream->get() >> value;
}

template <typename EnumType> requires std::is_enum_v<EnumType>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator<<(EnumType const& e) {
  return *this << std::to_underlying(e);
}
template <typename EnumType> requires std::is_enum_v<EnumType>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator>>(EnumType& e) {
  auto value = read<std::underlying_type_t<EnumType>>();
  if (!value) return std::unexpected(std::move(value.error()));
  e = static_cast<EnumType>(*value);
  return std::ref(*this);
}
template <typename T>
[[nodiscard]] std::expected<T, StarException> DataStream::read() {
  T value;
  auto result = *this >> value;
  if (!result) return std::unexpected(std::move(result.error()));
  return value;
}
template <typename Data>
[[nodiscard]] std::expected<void, StarException> DataStream::read(Data& data) {
  return (*this >> data).transform([](auto) {});
}
template <typename Data>
[[nodiscard]] std::expected<void, StarException> DataStream::write(Data const& data) {
  return (*this << data).transform([](auto) {});
}
template <typename ReadType, typename Data>
[[nodiscard]] std::expected<void, StarException> DataStream::cread(Data& data) {
  auto value = read<ReadType>();
  if (!value) return std::unexpected(std::move(value.error()));
  data = (Data)*value;
  return {};
}
template <typename WriteType, typename Data>
[[nodiscard]] std::expected<void, StarException> DataStream::cwrite(Data const& data) { return write((WriteType)data); }
template <typename IntegralType>
[[nodiscard]] std::expected<void, StarException> DataStream::vuread(IntegralType& data) {
  auto value = readVlqU();
  if (!value) return std::unexpected(std::move(value.error()));
  data = (IntegralType)*value;
  return {};
}
template <typename IntegralType>
[[nodiscard]] std::expected<void, StarException> DataStream::viread(IntegralType& data) {
  auto value = readVlqI();
  if (!value) return std::unexpected(std::move(value.error()));
  data = (IntegralType)*value;
  return {};
}
template <typename IntegralType>
[[nodiscard]] std::expected<void, StarException> DataStream::vsread(IntegralType& data) {
  auto value = readVlqS();
  if (!value) return std::unexpected(std::move(value.error()));
  data = (IntegralType)*value;
  return {};
}
template <typename IntegralType>
[[nodiscard]] std::expected<void, StarException> DataStream::vuwrite(IntegralType const& data) {
  auto result = writeVlqU(static_cast<std::uint64_t>(data));
  if (!result) return std::unexpected(std::move(result.error()));
  return {};
}
template <typename IntegralType>
[[nodiscard]] std::expected<void, StarException> DataStream::viwrite(IntegralType const& data) {
  auto result = writeVlqI(static_cast<std::int64_t>(data));
  if (!result) return std::unexpected(std::move(result.error()));
  return {};
}
template <typename IntegralType>
[[nodiscard]] std::expected<void, StarException> DataStream::vswrite(IntegralType const& data) {
  auto result = writeVlqS(static_cast<std::size_t>(data));
  if (!result) return std::unexpected(std::move(result.error()));
  return {};
}
template <typename FloatType>
[[nodiscard]] std::expected<void, StarException> DataStream::vfread(FloatType& data, FloatType base) {
  auto value = readVlqI();
  if (!value) return std::unexpected(std::move(value.error()));
  data = (FloatType)*value * base;
  return {};
}
template <typename FloatType>
[[nodiscard]] std::expected<void, StarException> DataStream::vfwrite(FloatType const& data, FloatType base) {
  auto rounded = std::round(data / base);
  if (!std::isfinite(rounded) || rounded < -0x1p63 || rounded >= 0x1p63)
    return std::unexpected(DataStreamException("Fixed-point stream value is outside signed 64-bit range"));
  return viwrite(static_cast<std::int64_t>(rounded));
}
template <typename PointerType, typename ReadFunction>
[[nodiscard]] std::expected<void, StarException> DataStream::pread(PointerType& pointer, ReadFunction function) {
  auto initialized = read<bool>();
  if (!initialized) return std::unexpected(std::move(initialized.error()));
  if (*initialized) {
    if constexpr (requires { static_cast<bool>(function); }) {
      if (!function) return std::unexpected(DataStreamException("No pointer read callback supplied"));
    }
    auto element = std::make_unique<std::remove_cv_t<typename PointerType::element_type>>();
    auto result = function(*this, *element);
    if (!result) return std::unexpected(std::move(result.error()));
    pointer.reset(element.release());
  } else {
    pointer.reset();
  }
  return {};
}
template <typename PointerType, typename WriteFunction>
[[nodiscard]] std::expected<void, StarException> DataStream::pwrite(PointerType const& pointer, WriteFunction function) {
  auto result = write((bool)pointer);
  if (!result) return std::unexpected(std::move(result.error()));
  if (pointer) {
    if constexpr (requires { static_cast<bool>(function); }) {
      if (!function) return std::unexpected(DataStreamException("No pointer write callback supplied"));
    }
    auto payload = function(*this, *pointer);
    if (!payload) return std::unexpected(std::move(payload.error()));
  }
  return {};
}
template <typename PointerType>
[[nodiscard]] std::expected<void, StarException> DataStream::pread(PointerType& pointer) {
  using Element = std::remove_cv_t<typename PointerType::element_type>;
  if constexpr (std::is_default_constructible_v<Element>) {
    return pread(pointer, [](DataStream& ds, Element& value) { return ds.read(value); });
  } else {
    auto initialized = read<bool>();
    if (!initialized) return std::unexpected(std::move(initialized.error()));
    if (*initialized) {
      auto value = read<Element>();
      if (!value) return std::unexpected(std::move(value.error()));
      auto element = std::make_unique<Element>(std::move(*value));
      pointer.reset(element.release());
    } else {
      pointer.reset();
    }
    return {};
  }
}
template <typename PointerType>
[[nodiscard]] std::expected<void, StarException> DataStream::pwrite(PointerType const& pointer) {
  return pwrite(pointer, [](DataStream& ds, std::remove_cv_t<typename PointerType::element_type> const& value) { return ds.write(value); });
}
template <typename Container, typename WriteFunction>
[[nodiscard]] std::expected<void, StarException> DataStream::writeContainer(Container const& container, WriteFunction function) {
  auto count = writeVlqU(container.size());
  if (!count) return std::unexpected(std::move(count.error()));
  if constexpr (requires { static_cast<bool>(function); }) {
    if (!container.empty() && !function)
      return std::unexpected(DataStreamException("No container write callback supplied"));
  }
  for (auto const& element : container) {
    auto result = function(*this, element);
    if (!result) return std::unexpected(std::move(result.error()));
  }
  return {};
}
template <typename Container, typename ReadFunction>
[[nodiscard]] std::expected<void, StarException> DataStream::readContainer(Container& container, ReadFunction function) {
  container.clear();
  auto count = readVlqU();
  if (!count) return std::unexpected(std::move(count.error()));
  if constexpr (requires { static_cast<bool>(function); }) {
    if (*count && !function)
      return std::unexpected(DataStreamException("No container read callback supplied"));
  }
  for (std::uint64_t i = 0; i < *count; ++i) {
    typename Container::value_type element;
    auto result = function(*this, element);
    if (!result) return std::unexpected(std::move(result.error()));
    if constexpr (requires { container.insert(container.end(), std::move(element)).error(); }) {
      auto inserted = container.insert(container.end(), std::move(element));
      if (!inserted) return std::unexpected(std::move(inserted.error()));
    } else {
      container.insert(container.end(), std::move(element));
    }
  }
  return {};
}
template <typename Container, typename WriteFunction>
[[nodiscard]] std::expected<void, StarException> DataStream::writeMapContainer(Container const& container, WriteFunction function) {
  auto count = writeVlqU(container.size());
  if (!count) return std::unexpected(std::move(count.error()));
  if constexpr (requires { static_cast<bool>(function); }) {
    if (!container.empty() && !function)
      return std::unexpected(DataStreamException("No map write callback supplied"));
  }
  for (auto const& element : container) {
    auto result = function(*this, element.first, element.second);
    if (!result) return std::unexpected(std::move(result.error()));
  }
  return {};
}
template <typename Container, typename ReadFunction>
[[nodiscard]] std::expected<void, StarException> DataStream::readMapContainer(Container& container, ReadFunction function) {
  container.clear();
  auto count = readVlqU();
  if (!count) return std::unexpected(std::move(count.error()));
  if constexpr (requires { static_cast<bool>(function); }) {
    if (*count && !function)
      return std::unexpected(DataStreamException("No map read callback supplied"));
  }
  for (std::uint64_t i = 0; i < *count; ++i) {
    typename Container::key_type key;
    typename Container::mapped_type mapped;
    auto result = function(*this, key, mapped);
    if (!result) return std::unexpected(std::move(result.error()));
    container.insert(std::make_pair(std::move(key), std::move(mapped)));
  }
  return {};
}
template <typename Container>
[[nodiscard]] std::expected<void, StarException> DataStream::writeContainer(Container const& container) {
  return writeContainer(container, [](DataStream& ds, typename Container::value_type const& value) { return ds.write(value); });
}
template <typename Container>
[[nodiscard]] std::expected<void, StarException> DataStream::readContainer(Container& container) {
  container.clear();
  auto count = readVlqU();
  if (!count) return std::unexpected(std::move(count.error()));
  for (std::uint64_t i = 0; i < *count; ++i) {
    auto element = read<typename Container::value_type>();
    if (!element) return std::unexpected(std::move(element.error()));
    if constexpr (requires { container.insert(container.end(), std::move(*element)).error(); }) {
      auto inserted = container.insert(container.end(), std::move(*element));
      if (!inserted) return std::unexpected(std::move(inserted.error()));
    } else {
      container.insert(container.end(), std::move(*element));
    }
  }
  return {};
}
template <typename Container>
[[nodiscard]] std::expected<void, StarException> DataStream::writeMapContainer(Container const& container) {
  return writeMapContainer(container, [](DataStream& ds, typename Container::key_type const& key, typename Container::mapped_type const& mapped) -> std::expected<void, StarException> {
    auto result = ds << key << mapped;
    if (!result) return std::unexpected(std::move(result.error()));
    return {};
  });
}
template <typename Container>
[[nodiscard]] std::expected<void, StarException> DataStream::readMapContainer(Container& container) {
  container.clear();
  auto count = readVlqU();
  if (!count) return std::unexpected(std::move(count.error()));
  for (std::uint64_t i = 0; i < *count; ++i) {
    auto key = read<typename Container::key_type>();
    if (!key) return std::unexpected(std::move(key.error()));
    auto mapped = read<typename Container::mapped_type>();
    if (!mapped) return std::unexpected(std::move(mapped.error()));
    container.insert(std::make_pair(std::move(*key), std::move(*mapped)));
  }
  return {};
}

}

export module star.data_stream;
export namespace Star {
  using ::Star::DataStreamExceptionTag;
  using ::Star::DataStreamException;
  using ::Star::CurrentStreamVersion;
  using ::Star::DataStream;
  using ::Star::operator<<;
  using ::Star::operator>>;
}
