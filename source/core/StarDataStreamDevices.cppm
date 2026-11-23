module;
import std;
import star.io_device;
import star.string;
import star.buffer;
import star.data_stream;

import star.byte_array;
import star.type_identity;
import star.format;
namespace Star {

// Implements DataStream using function objects as implementations of read/write.
class DataStreamFunctions : public DataStream {
public:
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<DataStreamFunctions, DataStream>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return type_name<DataStreamFunctions>(); }
  // Either callback can be unset; invoking that direction returns an error.
  DataStreamFunctions(std::function<std::expected<std::size_t, StarException>(std::span<char>)> reader,
      std::function<std::expected<std::size_t, StarException>(std::span<char const>)> writer);

  [[nodiscard]] std::expected<void, StarException> read_data(std::span<char> data) override;
  [[nodiscard]] std::expected<void, StarException> write_data(std::span<char const> data) override;

private:
  std::function<std::expected<std::size_t, StarException>(std::span<char>)> m_reader;
  std::function<std::expected<std::size_t, StarException>(std::span<char const>)> m_writer;
};

class DataStreamIODevice : public DataStream {
public:
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<DataStreamIODevice, DataStream>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return type_name<DataStreamIODevice>(); }
  // Borrows the device for the entire adapter lifetime.
  explicit DataStreamIODevice(IODevice& device);

  [[nodiscard]] std::expected<void, StarException> seek(std::size_t pos, IOSeek seek = IOSeek::Absolute);
  [[nodiscard]] std::expected<bool, StarException> at_end() override;
  [[nodiscard]] std::expected<StreamOffset, StarException> pos();

  [[nodiscard]] std::expected<void, StarException> read_data(std::span<char> data) override;
  [[nodiscard]] std::expected<void, StarException> write_data(std::span<char const> data) override;

private:
  IODevice& m_device;
};

class DataStreamBuffer : public DataStream {
public:
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<DataStreamBuffer, DataStream>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return type_name<DataStreamBuffer>(); }
  // Serialization owns its output; synchronous deserialization borrows its input.
  // Deserialization callbacks read only and must keep the input storage valid.

  template <typename T>
    requires(requires(DataStream& stream, T const& value) { stream.write(value); })
  [[nodiscard]] static std::expected<ByteArray, StarException> serialize(T const& t);

  template <typename T>
    requires(requires(DataStream& stream, T const& value) { stream.write_container(value); })
  [[nodiscard]] static std::expected<ByteArray, StarException> serialize_container(T const& t);

  template <typename T, typename WriteFunction>
    requires(requires(DataStream& stream, T const& value, WriteFunction& function) {
      stream.write_container(value, function);
    })
  [[nodiscard]] static std::expected<ByteArray, StarException> serialize_container(T const& t,
                                                                                   WriteFunction write_function);

  template <typename T>
    requires(requires(DataStream& stream, T const& value) { stream.write_map_container(value); })
  [[nodiscard]] static std::expected<ByteArray, StarException> serialize_map_container(T const& t);

  template <typename T, typename WriteFunction>
    requires(requires(DataStream& stream, T const& value, WriteFunction& function) {
      stream.write_map_container(value, function);
    })
  [[nodiscard]] static std::expected<ByteArray, StarException> serialize_map_container(T const& t,
                                                                                       WriteFunction write_function);

  template <typename T>
    requires(requires(DataStream& stream, T& value) { stream.read(value); })
  [[nodiscard]] static std::expected<void, StarException> deserialize(T& t, std::span<char const> data);

  template <typename T>
    requires(requires(DataStream& stream, T& value) { stream.read_container(value); })
  [[nodiscard]] static std::expected<void, StarException> deserialize_container(T& t, std::span<char const> data);

  template <typename T, typename ReadFunction>
    requires(requires(DataStream& stream, T& value, ReadFunction& function) { stream.read_container(value, function); })
  [[nodiscard]] static std::expected<void, StarException> deserialize_container(T& t, std::span<char const> data,
                                                                                ReadFunction read_function);

  template <typename T>
    requires(requires(DataStream& stream, T& value) { stream.read_map_container(value); })
  [[nodiscard]] static std::expected<void, StarException> deserialize_map_container(T& t, std::span<char const> data);

  template <typename T, typename ReadFunction>
    requires(requires(DataStream& stream, T& value, ReadFunction& function) {
      stream.read_map_container(value, function);
    })
  [[nodiscard]] static std::expected<void, StarException> deserialize_map_container(T& t, std::span<char const> data,
                                                                                    ReadFunction read_function);

  template <typename T>
    requires(std::default_initializable<T> && requires(DataStream& stream, T& value) { stream.read(value); })
  [[nodiscard]] static std::expected<T, StarException> deserialize(std::span<char const> data);

  template <typename T>
    requires(std::default_initializable<T> && requires(DataStream& stream, T& value) { stream.read_container(value); })
  [[nodiscard]] static std::expected<T, StarException> deserialize_container(std::span<char const> data);

  template <typename T, typename ReadFunction>
    requires(std::default_initializable<T>
             && requires(DataStream& stream, T& value,
                         ReadFunction& function) { stream.read_container(value, function); })
  [[nodiscard]] static std::expected<T, StarException> deserialize_container(std::span<char const> data,
                                                                             ReadFunction read_function);

  template <typename T>
    requires(std::default_initializable<T>
             && requires(DataStream& stream, T& value) { stream.read_map_container(value); })
  [[nodiscard]] static std::expected<T, StarException> deserialize_map_container(std::span<char const> data);

  template <typename T, typename ReadFunction>
    requires(std::default_initializable<T>
             && requires(DataStream& stream, T& value,
                         ReadFunction& function) { stream.read_map_container(value, function); })
  [[nodiscard]] static std::expected<T, StarException> deserialize_map_container(std::span<char const> data,
                                                                                 ReadFunction read_function);

  DataStreamBuffer();
  DataStreamBuffer(std::size_t initial_size);
  DataStreamBuffer(ByteArray b);

  // A stream owns its bytes and cursor; transfer explicitly rather than aliasing them.
  DataStreamBuffer(DataStreamBuffer const&) = delete;
  DataStreamBuffer& operator=(DataStreamBuffer const&) = delete;
  DataStreamBuffer(DataStreamBuffer&&) = default;
  DataStreamBuffer& operator=(DataStreamBuffer&&) = default;

  // Resize existing buffer to new size.
  [[nodiscard]] std::expected<void, StarException> resize(std::size_t size);
  void reserve(std::size_t size);
  void clear();

  ByteArray& data();
  ByteArray const& data() const;
  ByteArray take_data();

  std::span<char> span();
  std::span<char const> span() const;

  std::size_t size() const;
  bool empty() const;

  [[nodiscard]] std::expected<void, StarException> seek(std::size_t pos, IOSeek seek = IOSeek::Absolute);
  [[nodiscard]] std::expected<bool, StarException> at_end() override;
  [[nodiscard]] std::expected<std::size_t, StarException> pos();

  // Set new buffer.
  void reset(std::size_t new_size);
  void reset(ByteArray b);

  [[nodiscard]] std::expected<void, StarException> read_data(std::span<char> data) override;
  [[nodiscard]] std::expected<void, StarException> write_data(std::span<char const> data) override;

private:
  Buffer m_buffer;
};

class DataStreamExternalBuffer : public DataStream {
public:
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<DataStreamExternalBuffer, DataStream>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return type_name<DataStreamExternalBuffer>(); }
  DataStreamExternalBuffer();
  // Borrows read-only storage; it must remain valid and unmoved until reset or destruction.
  explicit DataStreamExternalBuffer(std::span<char const> external_data);

  DataStreamExternalBuffer(DataStreamExternalBuffer const& buffer) = default;

  std::span<char const> span() const;

  std::size_t size() const;
  bool empty() const;

  [[nodiscard]] std::expected<void, StarException> seek(std::size_t pos, IOSeek mode = IOSeek::Absolute);
  [[nodiscard]] std::expected<bool, StarException> at_end() override;
  [[nodiscard]] std::expected<std::size_t, StarException> pos();
  [[nodiscard]] std::expected<std::size_t, StarException> remaining();

  void reset(std::span<char const> external_data);

  [[nodiscard]] std::expected<void, StarException> read_data(std::span<char> data) override;
  [[nodiscard]] std::expected<void, StarException> write_data(std::span<char const> data) override;

private:
  friend class DataStreamReader;
  ExternalBuffer m_buffer;
};

// A final read-only adapter can decode contiguous bytes without bypassing
// read_data overrides supplied by extensible buffer adapters.
class DataStreamReader final : public DataStreamExternalBuffer {
public:
  using DataStreamExternalBuffer::DataStreamExternalBuffer;
  using DataStreamExternalBuffer::read_vlq_u;

  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<DataStreamReader, DataStreamExternalBuffer>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return type_name<DataStreamReader>(); }

  [[nodiscard]] std::expected<std::size_t, StarException> read_vlq_u(std::uint64_t& value) override;
};

template <typename T>
  requires(requires(DataStream& stream, T const& value) { stream.write(value); })
[[nodiscard]] std::expected<ByteArray, StarException> DataStreamBuffer::serialize(T const& t) {
  DataStreamBuffer ds;
  auto result = ds.write(t);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return ds.take_data();
}

template <typename T>
  requires(requires(DataStream& stream, T const& value) { stream.write_container(value); })
[[nodiscard]] std::expected<ByteArray, StarException> DataStreamBuffer::serialize_container(T const& t) {
  DataStreamBuffer ds;
  auto result = ds.write_container(t);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return ds.take_data();
}

template <typename T, typename WriteFunction>
  requires(requires(DataStream& stream, T const& value, WriteFunction& function) {
    stream.write_container(value, function);
  })
[[nodiscard]] std::expected<ByteArray, StarException>
DataStreamBuffer::serialize_container(T const& t, WriteFunction write_function) {
  DataStreamBuffer ds;
  auto result = ds.write_container(t, write_function);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return ds.take_data();
}

template <typename T>
  requires(requires(DataStream& stream, T const& value) { stream.write_map_container(value); })
[[nodiscard]] std::expected<ByteArray, StarException> DataStreamBuffer::serialize_map_container(T const& t) {
  DataStreamBuffer ds;
  auto result = ds.write_map_container(t);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return ds.take_data();
}

template <typename T, typename WriteFunction>
  requires(requires(DataStream& stream, T const& value, WriteFunction& function) {
    stream.write_map_container(value, function);
  })
[[nodiscard]] std::expected<ByteArray, StarException>
DataStreamBuffer::serialize_map_container(T const& t, WriteFunction write_function) {
  DataStreamBuffer ds;
  auto result = ds.write_map_container(t, write_function);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return ds.take_data();
}

template <typename T>
  requires(requires(DataStream& stream, T& value) { stream.read(value); })
[[nodiscard]] std::expected<void, StarException> DataStreamBuffer::deserialize(T& t, std::span<char const> data) {
  DataStreamReader ds(data);
  return ds.read(t);
}

template <typename T>
  requires(requires(DataStream& stream, T& value) { stream.read_container(value); })
[[nodiscard]] std::expected<void, StarException> DataStreamBuffer::deserialize_container(T& t,
                                                                                         std::span<char const> data) {
  DataStreamReader ds(data);
  return ds.read_container(t);
}

template <typename T, typename ReadFunction>
  requires(requires(DataStream& stream, T& value, ReadFunction& function) { stream.read_container(value, function); })
[[nodiscard]] std::expected<void, StarException>
DataStreamBuffer::deserialize_container(T& t, std::span<char const> data, ReadFunction read_function) {
  DataStreamReader ds(data);
  return ds.read_container(t, read_function);
}

template <typename T>
  requires(requires(DataStream& stream, T& value) { stream.read_map_container(value); })
[[nodiscard]] std::expected<void, StarException>
DataStreamBuffer::deserialize_map_container(T& t, std::span<char const> data) {
  DataStreamReader ds(data);
  return ds.read_map_container(t);
}

template <typename T, typename ReadFunction>
  requires(requires(DataStream& stream, T& value, ReadFunction& function) {
    stream.read_map_container(value, function);
  })
[[nodiscard]] std::expected<void, StarException>
DataStreamBuffer::deserialize_map_container(T& t, std::span<char const> data, ReadFunction read_function) {
  DataStreamReader ds(data);
  return ds.read_map_container(t, read_function);
}

template <typename T>
  requires(std::default_initializable<T> && requires(DataStream& stream, T& value) { stream.read(value); })
[[nodiscard]] std::expected<T, StarException> DataStreamBuffer::deserialize(std::span<char const> data) {
  T t;
  auto result = deserialize(t, data);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return t;
}

template <typename T>
  requires(std::default_initializable<T> && requires(DataStream& stream, T& value) { stream.read_container(value); })
[[nodiscard]] std::expected<T, StarException> DataStreamBuffer::deserialize_container(std::span<char const> data) {
  T t;
  auto result = deserialize_container(t, data);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return t;
}

template <typename T, typename ReadFunction>
  requires(std::default_initializable<T>
           && requires(DataStream& stream, T& value,
                       ReadFunction& function) { stream.read_container(value, function); })
[[nodiscard]] std::expected<T, StarException> DataStreamBuffer::deserialize_container(std::span<char const> data,
                                                                                      ReadFunction read_function) {
  T t;
  auto result = deserialize_container(t, data, read_function);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return t;
}

template <typename T>
  requires(std::default_initializable<T>
           && requires(DataStream& stream, T& value) { stream.read_map_container(value); })
[[nodiscard]] std::expected<T, StarException> DataStreamBuffer::deserialize_map_container(std::span<char const> data) {
  T t;
  auto result = deserialize_map_container(t, data);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return t;
}

template <typename T, typename ReadFunction>
  requires(std::default_initializable<T>
           && requires(DataStream& stream, T& value,
                       ReadFunction& function) { stream.read_map_container(value, function); })
[[nodiscard]] std::expected<T, StarException> DataStreamBuffer::deserialize_map_container(std::span<char const> data,
                                                                                          ReadFunction read_function) {
  T t;
  auto result = deserialize_map_container(t, data, read_function);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return t;
}
}

export module star.data_stream_devices;

export namespace Star {
using ::Star::DataStreamBuffer;
using ::Star::DataStreamExternalBuffer;
using ::Star::DataStreamFunctions;
using ::Star::DataStreamIODevice;
using ::Star::DataStreamReader;
}
