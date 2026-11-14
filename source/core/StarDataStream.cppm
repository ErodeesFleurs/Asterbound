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

class DataStream;

template <typename T>
concept stream_enum = std::is_enum_v<T>;
template <typename T>
concept stream_integer = std::integral<T> || stream_enum<T>;
template <typename Result>
concept stream_result = requires(Result result) {
  { static_cast<bool>(result) } -> std::same_as<bool>;
  { std::move(result.error()) } -> std::convertible_to<StarException>;
};
template <typename T>
concept stream_readable = requires(DataStream& stream, T& value) {
  { stream >> value } -> stream_result;
};
template <typename T>
concept stream_writable = requires(DataStream& stream, T const& value) {
  { stream << value } -> stream_result;
};
template <typename Function, typename... Args>
concept stream_callback = requires(Function& function, DataStream& stream, Args... args) {
  { function(stream, args...) } -> stream_result;
};
template <typename Pointer>
concept stream_pointer = requires(Pointer& pointer, typename Pointer::element_type* element) {
  typename Pointer::element_type;
  { static_cast<bool>(pointer) } -> std::same_as<bool>;
  *pointer;
  pointer.reset();
  pointer.reset(element);
};
template <typename Container>
concept stream_range = std::ranges::input_range<Container const> && requires(Container const& container) {
  { container.size() } -> std::convertible_to<std::uint64_t>;
  { container.empty() } -> std::convertible_to<bool>;
};
template <typename Container>
concept stream_sequence =
  stream_range<Container> && requires(Container& container, typename Container::value_type&& value) {
    container.clear();
    container.insert(container.end(), std::move(value));
  };
template <typename Container>
concept stream_map = stream_range<Container>
  && requires(Container& container, typename Container::key_type key, typename Container::mapped_type mapped) {
       container.clear();
       container.insert(std::make_pair(std::move(key), std::move(mapped)));
     };

// Portable serialization. Every transfer and complete operator chain has an
// explicit error result; unsuccessful reads never produce a default value.
class DataStream {
public:
  DataStream();
  virtual ~DataStream() = default;
  virtual void const* query_type(TypeKey key) const noexcept { return query_type_of<DataStream>(this, key); }
  virtual std::string_view dynamic_type_name() const noexcept { return type_name<DataStream>(); }

  ByteOrder byte_order() const;
  void set_byte_order(ByteOrder byte_order);
  bool null_terminated_strings() const;
  void set_null_terminated_strings(bool null_terminated_strings);
  unsigned stream_compatibility_version() const;
  void set_stream_compatibility_version(unsigned stream_compatibility_version);
  void set_stream_compatibility_version(NetCompatibilityRules const& rules);

  [[nodiscard]] virtual std::expected<void, StarException> read_data(std::span<char> data) = 0;
  [[nodiscard]] virtual std::expected<void, StarException> write_data(std::span<char const> data) = 0;
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
  [[nodiscard]] std::expected<std::size_t, StarException> write_vlq_u(std::uint64_t i);
  [[nodiscard]] std::expected<std::size_t, StarException> write_vlq_i(std::int64_t i);
  [[nodiscard]] std::expected<std::size_t, StarException> write_vlq_s(std::size_t i);
  [[nodiscard]] virtual std::expected<std::size_t, StarException> read_vlq_u(std::uint64_t& i);
  [[nodiscard]] std::expected<std::size_t, StarException> read_vlq_i(std::int64_t& i);
  [[nodiscard]] std::expected<std::size_t, StarException> read_vlq_s(std::size_t& i);
  [[nodiscard]] std::expected<std::uint64_t, StarException> read_vlq_u();
  [[nodiscard]] std::expected<std::int64_t, StarException> read_vlq_i();
  [[nodiscard]] std::expected<std::size_t, StarException> read_vlq_s();

  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(std::string_view s);
  template <std::size_t Size>
    requires(Size > 0)
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(char const (&text)[Size]) {
    return *this << std::string_view(text, Size - 1);
  }
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(std::string const& s);
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(ByteArray const& d);
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(String const& s);
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(std::string& s);
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(ByteArray& d);
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(String& s);
  template <stream_enum EnumType>
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(EnumType const& e);
  template <stream_enum EnumType>
  [[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(EnumType& e);

  template <typename T>
    requires(stream_readable<T>)
  [[nodiscard]] std::expected<T, StarException> read();
  template <typename Data>
    requires(stream_readable<Data>)
  [[nodiscard]] std::expected<void, StarException> read(Data& data);
  template <typename Data>
    requires(stream_writable<Data>)
  [[nodiscard]] std::expected<void, StarException> write(Data const& data);
  template <typename ReadType, typename Data>
    requires(stream_readable<ReadType> && requires(ReadType value, Data& data) { data = (Data)value; })
  [[nodiscard]] std::expected<void, StarException> cread(Data& data);
  template <typename WriteType, typename Data>
    requires(stream_writable<WriteType> && requires(Data const& data) { (WriteType) data; })
  [[nodiscard]] std::expected<void, StarException> cwrite(Data const& data);
  template <stream_integer IntegralType> [[nodiscard]] std::expected<void, StarException> vuread(IntegralType& data);
  template <stream_integer IntegralType> [[nodiscard]] std::expected<void, StarException> viread(IntegralType& data);
  template <stream_integer IntegralType> [[nodiscard]] std::expected<void, StarException> vsread(IntegralType& data);
  template <stream_integer IntegralType>
  [[nodiscard]] std::expected<void, StarException> vuwrite(IntegralType const& data);
  template <stream_integer IntegralType>
  [[nodiscard]] std::expected<void, StarException> viwrite(IntegralType const& data);
  template <stream_integer IntegralType>
  [[nodiscard]] std::expected<void, StarException> vswrite(IntegralType const& data);
  template <std::floating_point FloatType>
  [[nodiscard]] std::expected<void, StarException> vfread(FloatType& data, FloatType base);
  template <std::floating_point FloatType>
  [[nodiscard]] std::expected<void, StarException> vfwrite(FloatType const& data, FloatType base);
  template <typename PointerType, typename ReadFunction>
    requires(stream_pointer<PointerType>
             && std::default_initializable<std::remove_cv_t<typename PointerType::element_type>>
             && stream_callback<ReadFunction, std::remove_cv_t<typename PointerType::element_type>&>)
  [[nodiscard]] std::expected<void, StarException> pread(PointerType& pointer, ReadFunction function);
  template <typename PointerType, typename WriteFunction>
    requires(stream_pointer<PointerType> && stream_callback<WriteFunction, typename PointerType::element_type&>)
  [[nodiscard]] std::expected<void, StarException> pwrite(PointerType const& pointer, WriteFunction function);
  template <typename PointerType>
    requires(stream_pointer<PointerType> && stream_readable<std::remove_cv_t<typename PointerType::element_type>>)
  [[nodiscard]] std::expected<void, StarException> pread(PointerType& pointer);
  template <typename PointerType>
    requires(stream_pointer<PointerType> && stream_writable<std::remove_cv_t<typename PointerType::element_type>>)
  [[nodiscard]] std::expected<void, StarException> pwrite(PointerType const& pointer);

  // Callbacks return expected results with StarException errors. Decoding commits only fully
  // decoded elements; pointer decoding commits only after its complete payload.
  template <typename Container, typename WriteFunction>
    requires(stream_range<Container> && stream_callback<WriteFunction, typename Container::value_type const&>)
  [[nodiscard]] std::expected<void, StarException> write_container(Container const& container, WriteFunction function);
  template <typename Container, typename ReadFunction>
    requires(stream_sequence<Container> && std::default_initializable<typename Container::value_type>
             && stream_callback<ReadFunction, typename Container::value_type&>)
  [[nodiscard]] std::expected<void, StarException> read_container(Container& container, ReadFunction function);
  template <typename Container, typename WriteFunction> requires (stream_range<Container> && requires(typename Container::value_type const& element) { element.first; element.second; } && stream_callback<WriteFunction, typename Container::key_type const&, typename Container::mapped_type const&>) [[nodiscard]] std::expected<void, StarException> write_map_container(Container const& container, WriteFunction function);
  template <typename Container, typename ReadFunction>
    requires(stream_map<Container> && std::default_initializable<typename Container::key_type>
             && std::default_initializable<typename Container::mapped_type>
             && stream_callback<ReadFunction, typename Container::key_type&, typename Container::mapped_type&>)
  [[nodiscard]] std::expected<void, StarException> read_map_container(Container& container, ReadFunction function);
  template <typename Container>
    requires(stream_range<Container> && stream_writable<typename Container::value_type>)
  [[nodiscard]] std::expected<void, StarException> write_container(Container const& container);
  template <typename Container>
    requires(stream_sequence<Container> && stream_readable<typename Container::value_type>)
  [[nodiscard]] std::expected<void, StarException> read_container(Container& container);
  template <typename Container> requires (stream_range<Container> && requires(typename Container::value_type const& element) { element.first; element.second; } && stream_writable<typename Container::key_type> && stream_writable<typename Container::mapped_type>) [[nodiscard]] std::expected<void, StarException> write_map_container(Container const& container);
  template <typename Container>
    requires(stream_map<Container> && stream_readable<typename Container::key_type>
             && stream_readable<typename Container::mapped_type>)
  [[nodiscard]] std::expected<void, StarException> read_map_container(Container& container);

private:
  [[nodiscard]] std::expected<void, StarException> write_string_data(std::span<char const> data);
  ByteOrder m_byteOrder;
  bool m_nullTerminatedStrings;
  unsigned m_streamCompatibilityVersion;
};

namespace detail {
template <typename Container, typename Decode, typename Commit>
concept stream_element_decoder = requires(Container& container, Decode& decode, Commit& commit) {
  container.clear();
  { decode() } -> stream_result;
  { commit(std::move(*decode())) } -> stream_result;
};
template <typename Container, typename Function, typename Decode, typename Commit>
  requires stream_element_decoder<Container, Decode, Commit>
std::expected<void, StarException> read_stream_elements(DataStream& stream, Container& container,
                                                        Function const& function, char const* missing_callback,
                                                        Decode decode, Commit commit) {
  container.clear();
  auto count = stream.read_vlq_u();
  if (!count)
    return std::unexpected(std::move(count.error()));
  if constexpr (requires { static_cast<bool>(function); }) {
    if (*count && !function)
      return std::unexpected(DataStreamException(missing_callback));
  }
  for (std::uint64_t i = 0; i < *count; ++i) {
    auto element = decode();
    if (!element)
      return std::unexpected(std::move(element.error()));
    auto inserted = commit(std::move(*element));
    if (!inserted)
      return std::unexpected(std::move(inserted.error()));
  }
  return {};
}
template <stream_sequence Container>
std::expected<void, StarException> insert_stream_element(Container& container, typename Container::value_type&& value) {
  if constexpr (requires { container.insert(container.end(), std::move(value)).error(); }) {
    auto inserted = container.insert(container.end(), std::move(value));
    if (!inserted)
      return std::unexpected(std::move(inserted.error()));
  } else {
    container.insert(container.end(), std::move(value));
  }
  return {};
}
}// namespace detail

// Failure short-circuits the remainder of a chain. Inspect its final expected.
template <typename Stream, typename T>
  requires std::same_as<Stream, std::expected<std::reference_wrapper<DataStream>, StarException>> && stream_writable<T>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(Stream stream,
                                                                                          T const& value) {
  if (!stream)
    return stream;
  return stream->get() << value;
}
template <typename Stream, typename T>
  requires std::same_as<Stream, std::expected<std::reference_wrapper<DataStream>, StarException>> && stream_readable<T>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(Stream stream, T& value) {
  if (!stream)
    return stream;
  return stream->get() >> value;
}

template <stream_enum EnumType>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException>
DataStream::operator<<(EnumType const& e) {
  return *this << std::to_underlying(e);
}
template <stream_enum EnumType>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> DataStream::operator>>(EnumType& e) {
  auto value = read<std::underlying_type_t<EnumType>>();
  if (!value) return std::unexpected(std::move(value.error()));
  e = static_cast<EnumType>(*value);
  return std::ref(*this);
}
template <typename T>
  requires(stream_readable<T>)
[[nodiscard]] std::expected<T, StarException> DataStream::read() {
  T value;
  auto result = *this >> value;
  if (!result) return std::unexpected(std::move(result.error()));
  return value;
}
template <typename Data>
  requires(stream_readable<Data>)
[[nodiscard]] std::expected<void, StarException> DataStream::read(Data& data) {
  return (*this >> data).transform([](auto) {});
}
template <typename Data>
  requires(stream_writable<Data>)
[[nodiscard]] std::expected<void, StarException> DataStream::write(Data const& data) {
  return (*this << data).transform([](auto) {});
}
template <typename ReadType, typename Data>
  requires(stream_readable<ReadType> && requires(ReadType value, Data& data) { data = (Data)value; })
[[nodiscard]] std::expected<void, StarException> DataStream::cread(Data& data) {
  auto value = read<ReadType>();
  if (!value) return std::unexpected(std::move(value.error()));
  data = (Data)*value;
  return {};
}
template <typename WriteType, typename Data>
  requires(stream_writable<WriteType> && requires(Data const& data) { (WriteType) data; })
[[nodiscard]] std::expected<void, StarException> DataStream::cwrite(Data const& data) {
  return write((WriteType)data);
}
template <stream_integer IntegralType>
[[nodiscard]] std::expected<void, StarException> DataStream::vuread(IntegralType& data) {
  auto value = read_vlq_u();
  if (!value) return std::unexpected(std::move(value.error()));
  data = (IntegralType)*value;
  return {};
}
template <stream_integer IntegralType>
[[nodiscard]] std::expected<void, StarException> DataStream::viread(IntegralType& data) {
  auto value = read_vlq_i();
  if (!value) return std::unexpected(std::move(value.error()));
  data = (IntegralType)*value;
  return {};
}
template <stream_integer IntegralType>
[[nodiscard]] std::expected<void, StarException> DataStream::vsread(IntegralType& data) {
  auto value = read_vlq_s();
  if (!value) return std::unexpected(std::move(value.error()));
  data = (IntegralType)*value;
  return {};
}
template <stream_integer IntegralType>
[[nodiscard]] std::expected<void, StarException> DataStream::vuwrite(IntegralType const& data) {
  auto result = write_vlq_u(static_cast<std::uint64_t>(data));
  if (!result) return std::unexpected(std::move(result.error()));
  return {};
}
template <stream_integer IntegralType>
[[nodiscard]] std::expected<void, StarException> DataStream::viwrite(IntegralType const& data) {
  auto result = write_vlq_i(static_cast<std::int64_t>(data));
  if (!result) return std::unexpected(std::move(result.error()));
  return {};
}
template <stream_integer IntegralType>
[[nodiscard]] std::expected<void, StarException> DataStream::vswrite(IntegralType const& data) {
  auto result = write_vlq_s(static_cast<std::size_t>(data));
  if (!result) return std::unexpected(std::move(result.error()));
  return {};
}
template <std::floating_point FloatType>
[[nodiscard]] std::expected<void, StarException> DataStream::vfread(FloatType& data, FloatType base) {
  auto value = read_vlq_i();
  if (!value) return std::unexpected(std::move(value.error()));
  data = (FloatType)*value * base;
  return {};
}
template <std::floating_point FloatType>
[[nodiscard]] std::expected<void, StarException> DataStream::vfwrite(FloatType const& data, FloatType base) {
  auto rounded = std::round(data / base);
  if (!std::isfinite(rounded) || rounded < -0x1p63 || rounded >= 0x1p63)
    return std::unexpected(DataStreamException("Fixed-point stream value is outside signed 64-bit range"));
  return viwrite(static_cast<std::int64_t>(rounded));
}
template <typename PointerType, typename ReadFunction>
  requires(stream_pointer<PointerType>
           && std::default_initializable<std::remove_cv_t<typename PointerType::element_type>>
           && stream_callback<ReadFunction, std::remove_cv_t<typename PointerType::element_type>&>)
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
  requires(stream_pointer<PointerType> && stream_callback<WriteFunction, typename PointerType::element_type&>)
[[nodiscard]] std::expected<void, StarException> DataStream::pwrite(PointerType const& pointer,
                                                                    WriteFunction function) {
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
  requires(stream_pointer<PointerType> && stream_readable<std::remove_cv_t<typename PointerType::element_type>>)
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
  requires(stream_pointer<PointerType> && stream_writable<std::remove_cv_t<typename PointerType::element_type>>)
[[nodiscard]] std::expected<void, StarException> DataStream::pwrite(PointerType const& pointer) {
  return pwrite(pointer, [](DataStream& ds, std::remove_cv_t<typename PointerType::element_type> const& value) { return ds.write(value); });
}
template <typename Container, typename WriteFunction>
  requires(stream_range<Container> && stream_callback<WriteFunction, typename Container::value_type const&>)
[[nodiscard]] std::expected<void, StarException> DataStream::write_container(Container const& container,
                                                                             WriteFunction function) {
  auto count = write_vlq_u(container.size());
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
  requires(stream_sequence<Container> && std::default_initializable<typename Container::value_type>
           && stream_callback<ReadFunction, typename Container::value_type&>)
[[nodiscard]] std::expected<void, StarException> DataStream::read_container(Container& container,
                                                                            ReadFunction function) {
  using Element = typename Container::value_type;
  return detail::read_stream_elements(
    *this, container, function, "No container read callback supplied",
    [&]() -> std::expected<Element, StarException> {
      Element element;
      auto result = function(*this, element);
      if (!result)
        return std::unexpected(std::move(result.error()));
      return element;
    },
    [&container](Element&& element) { return detail::insert_stream_element(container, std::move(element)); });
}
template <typename Container, typename WriteFunction> requires (stream_range<Container> && requires(typename Container::value_type const& element) { element.first; element.second; } && stream_callback<WriteFunction, typename Container::key_type const&, typename Container::mapped_type const&>)
[[nodiscard]] std::expected<void, StarException> DataStream::write_map_container(Container const& container, WriteFunction function) {
  auto count = write_vlq_u(container.size());
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
  requires(stream_map<Container> && std::default_initializable<typename Container::key_type>
           && std::default_initializable<typename Container::mapped_type>
           && stream_callback<ReadFunction, typename Container::key_type&, typename Container::mapped_type&>)
[[nodiscard]] std::expected<void, StarException> DataStream::read_map_container(Container& container,
                                                                                ReadFunction function) {
  using Element = std::pair<typename Container::key_type, typename Container::mapped_type>;
  return detail::read_stream_elements(
    *this, container, function, "No map read callback supplied",
    [&]() -> std::expected<Element, StarException> {
      Element element;
      auto result = function(*this, element.first, element.second);
      if (!result)
        return std::unexpected(std::move(result.error()));
      return element;
    },
    [&container](Element&& element) -> std::expected<void, StarException> {
      container.insert(std::move(element));
      return {};
    });
}
template <typename Container>
  requires(stream_range<Container> && stream_writable<typename Container::value_type>)
[[nodiscard]] std::expected<void, StarException> DataStream::write_container(Container const& container) {
  return write_container(container,
                         [](DataStream& ds, typename Container::value_type const& value) { return ds.write(value); });
}
template <typename Container>
  requires(stream_sequence<Container> && stream_readable<typename Container::value_type>)
[[nodiscard]] std::expected<void, StarException> DataStream::read_container(Container& container) {
  using Element = typename Container::value_type;
  return detail::read_stream_elements(
    *this, container, [] {}, "", [this] { return read<Element>(); },
    [&container](Element&& element) { return detail::insert_stream_element(container, std::move(element)); });
}
template <typename Container> requires (stream_range<Container> && requires(typename Container::value_type const& element) { element.first; element.second; } && stream_writable<typename Container::key_type> && stream_writable<typename Container::mapped_type>)
[[nodiscard]] std::expected<void, StarException> DataStream::write_map_container(Container const& container) {
  return write_map_container(container,
                             [](DataStream& ds, typename Container::key_type const& key,
                                typename Container::mapped_type const& mapped) -> std::expected<void, StarException> {
                               auto result = ds << key << mapped;
                               if (!result)
                                 return std::unexpected(std::move(result.error()));
                               return {};
                             });
}
template <typename Container>
  requires(stream_map<Container> && stream_readable<typename Container::key_type>
           && stream_readable<typename Container::mapped_type>)
[[nodiscard]] std::expected<void, StarException> DataStream::read_map_container(Container& container) {
  using Element = std::pair<typename Container::key_type, typename Container::mapped_type>;
  return detail::read_stream_elements(
    *this, container, [] {}, "",
    [this]() -> std::expected<Element, StarException> {
      auto key = read<typename Container::key_type>();
      if (!key)
        return std::unexpected(std::move(key.error()));
      auto mapped = read<typename Container::mapped_type>();
      if (!mapped)
        return std::unexpected(std::move(mapped.error()));
      return Element(std::move(*key), std::move(*mapped));
    },
    [&container](Element&& element) -> std::expected<void, StarException> {
      container.insert(std::move(element));
      return {};
    });
}
}

export module star.data_stream;
export namespace Star {
using ::Star::CurrentStreamVersion;
using ::Star::DataStream;
using ::Star::DataStreamException;
using ::Star::DataStreamExceptionTag;
using ::Star::stream_callback;
using ::Star::stream_enum;
using ::Star::stream_integer;
using ::Star::stream_map;
using ::Star::stream_pointer;
using ::Star::stream_range;
using ::Star::stream_readable;
using ::Star::stream_result;
using ::Star::stream_sequence;
using ::Star::stream_writable;
using ::Star::operator<<;
using ::Star::operator>>;
}
