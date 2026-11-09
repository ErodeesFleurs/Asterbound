module;
import std;
import star.byte_array;
import star.io_device;
import star.string;
import star.format;

namespace Star {

// Wraps a ByteArray to an IODevice
class Buffer : public IODevice {
public:
  // Constructs buffer open ReadWrite
  Buffer();
  Buffer(std::size_t initialSize);
  Buffer(ByteArray b);
  Buffer(Buffer const& buffer);
  Buffer(Buffer&& buffer);

  [[nodiscard]] std::expected<StreamOffset, StarException> pos() override;
  [[nodiscard]] std::expected<void, StarException> seek(StreamOffset pos, IOSeek mode = IOSeek::Absolute) override;
  [[nodiscard]] std::expected<void, StarException> resize(StreamOffset size) override;
  [[nodiscard]] std::expected<bool, StarException> at_end() override;

  [[nodiscard]] std::expected<std::size_t, StarException> read(std::span<char> data) override;
  [[nodiscard]] std::expected<std::size_t, StarException> write(std::span<char const> data) override;

  [[nodiscard]] std::expected<std::size_t, StarException> read_absolute(StreamOffset readPosition,
                                                                        std::span<char> data) override;
  [[nodiscard]] std::expected<std::size_t, StarException> write_absolute(StreamOffset writePosition,
                                                                         std::span<char const> data) override;

  [[nodiscard]] std::expected<void, StarException> open(IOMode mode) override;

  [[nodiscard]] std::expected<String, StarException> device_name() const override;

  [[nodiscard]] std::expected<StreamOffset, StarException> size() override;

  [[nodiscard]] std::expected<std::shared_ptr<IODevice>, StarException> clone() override;

  ByteArray& data();
  ByteArray const& data() const;

  // Moves the owned data into the returned array, leaving this Buffer empty
  // with its position reset to zero.
  ByteArray take_data();

  // Returns a pointer to the beginning of the Buffer.
  char* ptr();
  char const* ptr() const;
  std::span<char> span() { return m_bytes.span(); }
  std::span<char const> span() const { return m_bytes.span(); }

  // Same thing as size(), just size_t type (since this is in-memory)
  std::size_t data_size() const;
  void reserve(std::size_t size);

  // Clears buffer, moves position to 0.
  void clear();
  bool empty() const;

  // Reset buffer with new contents, moves position to 0.
  void reset(std::size_t newSize);
  void reset(ByteArray b);

  Buffer& operator=(Buffer const& buffer);
  Buffer& operator=(Buffer&& buffer);

private:
  [[nodiscard]] std::expected<std::size_t, StarException> doRead(std::size_t pos, std::span<char> data);
  [[nodiscard]] std::expected<std::size_t, StarException> doWrite(std::size_t pos, std::span<char const> data);

  std::size_t m_pos;
  ByteArray m_bytes;
};

// Wraps an externally held sequence of bytes to a read-only IODevice
class ExternalBuffer : public IODevice {
public:
  // Constructs an empty ReadOnly ExternalBuffer.
  ExternalBuffer();
  // Constructs a ReadOnly ExternalBuffer pointing to the given external data, which
  // must be valid for the lifetime of the ExternalBuffer.
  ExternalBuffer(std::span<char const> externalData);

  ExternalBuffer(ExternalBuffer const& buffer) = default;
  ExternalBuffer& operator=(ExternalBuffer const& buffer) = default;

  [[nodiscard]] std::expected<StreamOffset, StarException> pos() override;
  [[nodiscard]] std::expected<void, StarException> seek(StreamOffset pos, IOSeek mode = IOSeek::Absolute) override;
  [[nodiscard]] std::expected<bool, StarException> at_end() override;

  [[nodiscard]] std::expected<std::size_t, StarException> read(std::span<char> data) override;
  [[nodiscard]] std::expected<std::size_t, StarException> write(std::span<char const> data) override;

  [[nodiscard]] std::expected<std::size_t, StarException> read_absolute(StreamOffset readPosition,
                                                                        std::span<char> data) override;
  [[nodiscard]] std::expected<std::size_t, StarException> write_absolute(StreamOffset writePosition,
                                                                         std::span<char const> data) override;

  [[nodiscard]] std::expected<String, StarException> device_name() const override;

  [[nodiscard]] std::expected<StreamOffset, StarException> size() override;

  [[nodiscard]] std::expected<std::shared_ptr<IODevice>, StarException> clone() override;

  // Returns a pointer to the beginning of the Buffer.
  char const* ptr() const;
  std::span<char const> span() const { return m_bytes; }

  // Same thing as size(), just size_t type (since this is in-memory)
  std::size_t data_size() const;

  // Clears buffer, moves position to 0.
  bool empty() const;

  operator bool() const;

  // Reset buffer with new contents, moves position to 0.
  void reset(std::span<char const> externalData);

private:
  [[nodiscard]] std::expected<std::size_t, StarException> doRead(std::size_t pos, std::span<char> data);

  std::size_t m_pos;
  std::span<char const> m_bytes;
};

}

export module star.buffer;

export namespace Star {
  using ::Star::Buffer;
  using ::Star::ExternalBuffer;
}
