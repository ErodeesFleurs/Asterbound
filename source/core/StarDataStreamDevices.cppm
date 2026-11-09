module;
import std;
import star.io_device;
import star.string;
import star.buffer;
import star.data_stream;

import star.either;
import star.byte_array;
import star.type_identity;
import star.format;
namespace Star {

// Implements DataStream using function objects as implementations of read/write.
class DataStreamFunctions : public DataStream {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<DataStreamFunctions, DataStream>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return typeName<DataStreamFunctions>();
  }
  // Either callback can be unset; invoking that direction returns an error.
  DataStreamFunctions(std::function<std::expected<std::size_t, StarException>(std::span<char>)> reader,
      std::function<std::expected<std::size_t, StarException>(std::span<char const>)> writer);

  [[nodiscard]] std::expected<void, StarException> readData(std::span<char> data) override;
  [[nodiscard]] std::expected<void, StarException> writeData(std::span<char const> data) override;

private:
  std::function<std::expected<std::size_t, StarException>(std::span<char>)> m_reader;
  std::function<std::expected<std::size_t, StarException>(std::span<char const>)> m_writer;
};

class DataStreamIODevice : public DataStream {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<DataStreamIODevice, DataStream>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return typeName<DataStreamIODevice>();
  }
  // Borrows the device for the entire adapter lifetime.
  explicit DataStreamIODevice(IODevice& device);

  [[nodiscard]] std::expected<void, StarException> seek(std::size_t pos, IOSeek seek = IOSeek::Absolute);
  [[nodiscard]] std::expected<bool, StarException> at_end() override;
  [[nodiscard]] std::expected<StreamOffset, StarException> pos();

  [[nodiscard]] std::expected<void, StarException> readData(std::span<char> data) override;
  [[nodiscard]] std::expected<void, StarException> writeData(std::span<char const> data) override;

private:
  IODevice& m_device;
};

class DataStreamBuffer : public DataStream {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<DataStreamBuffer, DataStream>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return typeName<DataStreamBuffer>();
  }
  // Convenience methods to serialize to / from ByteArray directly without
  // having to construct a temporary DataStreamBuffer to do it

  template <typename T>
  [[nodiscard]] static std::expected<ByteArray, StarException> serialize(T const& t);

  template <typename T>
  [[nodiscard]] static std::expected<ByteArray, StarException> serializeContainer(T const& t);

  template <typename T, typename WriteFunction>
  [[nodiscard]] static std::expected<ByteArray, StarException> serializeContainer(T const& t, WriteFunction writeFunction);

  template <typename T>
  [[nodiscard]] static std::expected<ByteArray, StarException> serializeMapContainer(T const& t);

  template <typename T, typename WriteFunction>
  [[nodiscard]] static std::expected<ByteArray, StarException> serializeMapContainer(T const& t, WriteFunction writeFunction);

  template <typename T>
  [[nodiscard]] static std::expected<void, StarException> deserialize(T& t, ByteArray data);

  template <typename T>
  [[nodiscard]] static std::expected<void, StarException> deserializeContainer(T& t, ByteArray data);

  template <typename T, typename ReadFunction>
  [[nodiscard]] static std::expected<void, StarException> deserializeContainer(T& t, ByteArray data, ReadFunction readFunction);

  template <typename T>
  [[nodiscard]] static std::expected<void, StarException> deserializeMapContainer(T& t, ByteArray data);

  template <typename T, typename ReadFunction>
  [[nodiscard]] static std::expected<void, StarException> deserializeMapContainer(T& t, ByteArray data, ReadFunction readFunction);

  template <typename T>
  [[nodiscard]] static std::expected<T, StarException> deserialize(ByteArray data);

  template <typename T>
  [[nodiscard]] static std::expected<T, StarException> deserializeContainer(ByteArray data);

  template <typename T, typename ReadFunction>
  [[nodiscard]] static std::expected<T, StarException> deserializeContainer(ByteArray data, ReadFunction readFunction);

  template <typename T>
  [[nodiscard]] static std::expected<T, StarException> deserializeMapContainer(ByteArray data);

  template <typename T, typename ReadFunction>
  [[nodiscard]] static std::expected<T, StarException> deserializeMapContainer(ByteArray data, ReadFunction readFunction);

  DataStreamBuffer();
  DataStreamBuffer(std::size_t initialSize);
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
  ByteArray takeData();

  // Raw storage access is reserved for native adapters; business ranges use span().
  char* ptr();
  char const* ptr() const;
  std::span<char> span();
  std::span<char const> span() const;

  std::size_t size() const;
  bool empty() const;

  [[nodiscard]] std::expected<void, StarException> seek(std::size_t pos, IOSeek seek = IOSeek::Absolute);
  [[nodiscard]] std::expected<bool, StarException> at_end() override;
  [[nodiscard]] std::expected<std::size_t, StarException> pos();

  // Set new buffer.
  void reset(std::size_t newSize);
  void reset(ByteArray b);

  [[nodiscard]] std::expected<void, StarException> readData(std::span<char> data) override;
  [[nodiscard]] std::expected<void, StarException> writeData(std::span<char const> data) override;

private:
  Buffer m_buffer;
};

class DataStreamExternalBuffer : public DataStream {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<DataStreamExternalBuffer, DataStream>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return typeName<DataStreamExternalBuffer>();
  }
  DataStreamExternalBuffer();
  // Borrows read-only storage; it must remain valid and unmoved until reset or destruction.
  explicit DataStreamExternalBuffer(std::span<char const> externalData);

  DataStreamExternalBuffer(DataStreamExternalBuffer const& buffer) = default;

  // Raw storage access is reserved for native adapters; business ranges use span().
  char const* ptr() const;
  std::span<char const> span() const;

  std::size_t size() const;
  bool empty() const;

  [[nodiscard]] std::expected<void, StarException> seek(std::size_t pos, IOSeek mode = IOSeek::Absolute);
  [[nodiscard]] std::expected<bool, StarException> at_end() override;
  [[nodiscard]] std::expected<std::size_t, StarException> pos();
  [[nodiscard]] std::expected<std::size_t, StarException> remaining();

  void reset(std::span<char const> externalData);

  [[nodiscard]] std::expected<void, StarException> readData(std::span<char> data) override;
  [[nodiscard]] std::expected<void, StarException> writeData(std::span<char const> data) override;

private:
  ExternalBuffer m_buffer;
};

template <typename T>
[[nodiscard]] std::expected<ByteArray, StarException> DataStreamBuffer::serialize(T const& t) {
  DataStreamBuffer ds;
  auto result = ds.write(t);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return ds.takeData();
}

template <typename T>
[[nodiscard]] std::expected<ByteArray, StarException> DataStreamBuffer::serializeContainer(T const& t) {
  DataStreamBuffer ds;
  auto result = ds.writeContainer(t);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return ds.takeData();
}

template <typename T, typename WriteFunction>
[[nodiscard]] std::expected<ByteArray, StarException> DataStreamBuffer::serializeContainer(T const& t, WriteFunction writeFunction) {
  DataStreamBuffer ds;
  auto result = ds.writeContainer(t, writeFunction);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return ds.takeData();
}

template <typename T>
[[nodiscard]] std::expected<ByteArray, StarException> DataStreamBuffer::serializeMapContainer(T const& t) {
  DataStreamBuffer ds;
  auto result = ds.writeMapContainer(t);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return ds.takeData();
}

template <typename T, typename WriteFunction>
[[nodiscard]] std::expected<ByteArray, StarException> DataStreamBuffer::serializeMapContainer(T const& t, WriteFunction writeFunction) {
  DataStreamBuffer ds;
  auto result = ds.writeMapContainer(t, writeFunction);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return ds.takeData();
}

template <typename T>
[[nodiscard]] std::expected<void, StarException> DataStreamBuffer::deserialize(T& t, ByteArray data) {
  DataStreamBuffer ds(std::move(data));
  return ds.read(t);
}

template <typename T>
[[nodiscard]] std::expected<void, StarException> DataStreamBuffer::deserializeContainer(T& t, ByteArray data) {
  DataStreamBuffer ds(std::move(data));
  return ds.readContainer(t);
}

template <typename T, typename ReadFunction>
[[nodiscard]] std::expected<void, StarException> DataStreamBuffer::deserializeContainer(T& t, ByteArray data, ReadFunction readFunction) {
  DataStreamBuffer ds(std::move(data));
  return ds.readContainer(t, readFunction);
}

template <typename T>
[[nodiscard]] std::expected<void, StarException> DataStreamBuffer::deserializeMapContainer(T& t, ByteArray data) {
  DataStreamBuffer ds(std::move(data));
  return ds.readMapContainer(t);
}

template <typename T, typename ReadFunction>
[[nodiscard]] std::expected<void, StarException> DataStreamBuffer::deserializeMapContainer(T& t, ByteArray data, ReadFunction readFunction) {
  DataStreamBuffer ds(std::move(data));
  return ds.readMapContainer(t, readFunction);
}

template <typename T>
[[nodiscard]] std::expected<T, StarException> DataStreamBuffer::deserialize(ByteArray data) {
  T t;
  auto result = deserialize(t, std::move(data));
  if (!result)
    return std::unexpected(std::move(result.error()));
  return t;
}

template <typename T>
[[nodiscard]] std::expected<T, StarException> DataStreamBuffer::deserializeContainer(ByteArray data) {
  T t;
  auto result = deserializeContainer(t, std::move(data));
  if (!result)
    return std::unexpected(std::move(result.error()));
  return t;
}

template <typename T, typename ReadFunction>
[[nodiscard]] std::expected<T, StarException> DataStreamBuffer::deserializeContainer(ByteArray data, ReadFunction readFunction) {
  T t;
  auto result = deserializeContainer(t, std::move(data), readFunction);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return t;
}

template <typename T>
[[nodiscard]] std::expected<T, StarException> DataStreamBuffer::deserializeMapContainer(ByteArray data) {
  T t;
  auto result = deserializeMapContainer(t, std::move(data));
  if (!result)
    return std::unexpected(std::move(result.error()));
  return t;
}

template <typename T, typename ReadFunction>
[[nodiscard]] std::expected<T, StarException> DataStreamBuffer::deserializeMapContainer(ByteArray data, ReadFunction readFunction) {
  T t;
  auto result = deserializeMapContainer(t, std::move(data), readFunction);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return t;
}

}

export module star.data_stream_devices;

export namespace Star {
  using ::Star::DataStreamFunctions;
  using ::Star::DataStreamIODevice;
  using ::Star::DataStreamBuffer;
  using ::Star::DataStreamExternalBuffer;
}
