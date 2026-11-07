module;
import std;

import star.byte_array;
import star.string;
import star.format;

namespace Star {
using StreamOffset = std::int64_t;


struct EofExceptionTag {
  static constexpr std::string_view name() { return "EofException"; }
};
using EofException = StarError<EofExceptionTag, IOException>;

enum class IOMode : std::uint8_t {
  Closed = 0x0,
  Read = 0x1,
  Write = 0x2,
  ReadWrite = 0x3,
  Append = 0x4,
  Truncate = 0x8,
};

IOMode operator|(IOMode a, IOMode b);
bool operator&(IOMode a, IOMode b);

// Should match SEEK_SET, SEEK_CUR, AND SEEK_END
enum IOSeek : std::uint8_t {
  Absolute = 0,
  Relative = 1,
  End = 2
};

// Abstract Interface to a random access I/O device.
class IODevice {
public:
  IODevice(IOMode mode = IOMode::Closed);
  virtual ~IODevice();

  // Do a read or write that may result in less data read or written than
  // requested.
  [[nodiscard]] virtual std::expected<std::size_t, StarException> read(std::span<char> data) = 0;
  [[nodiscard]] virtual std::expected<std::size_t, StarException> write(std::span<char const> data) = 0;

  [[nodiscard]] virtual std::expected<StreamOffset, StarException> pos() = 0;
  [[nodiscard]] virtual std::expected<void, StarException> seek(StreamOffset pos, IOSeek mode = IOSeek::Absolute) = 0;

  // Default implementation reports unsupported operation.
  [[nodiscard]] virtual std::expected<void, StarException> resize(StreamOffset size);

  // Read / write from an absolute offset in the file without modifying the
  // current file position.  Default implementation stores the file position,
  // then seeks and calls read/write partial, then restores the file position,
  // and is not thread safe.
  [[nodiscard]] virtual std::expected<std::size_t, StarException> readAbsolute(StreamOffset readPosition, std::span<char> data);
  [[nodiscard]] virtual std::expected<std::size_t, StarException> writeAbsolute(StreamOffset writePosition, std::span<char const> data);

  // Full operations report short transfers, including a distinct EOF error.
  [[nodiscard]] virtual std::expected<void, StarException> readFull(std::span<char> data);
  [[nodiscard]] virtual std::expected<void, StarException> writeFull(std::span<char const> data);
  [[nodiscard]] virtual std::expected<void, StarException> readFullAbsolute(StreamOffset readPosition, std::span<char> data);
  [[nodiscard]] virtual std::expected<void, StarException> writeFullAbsolute(StreamOffset writePosition, std::span<char const> data);

  // Default implementation only accepts the current mode.
  [[nodiscard]] virtual std::expected<void, StarException> open(IOMode mode);

  // Default implementation sets mode equal to Closed.
  [[nodiscard]] virtual std::expected<void, StarException> close();

  // Devices without buffered state have nothing to synchronize.
  [[nodiscard]] virtual std::expected<void, StarException> sync();

  // Returns a clone of this device with the same mode.
  [[nodiscard]] virtual std::expected<std::shared_ptr<IODevice>, StarException> clone() = 0;

  // Default implementation just prints address of generic IODevice
  [[nodiscard]] virtual std::expected<String, StarException> deviceName() const;

  // Is the file position at the end of the file and there is no more to read?
  // This is not the same as feof, which returns true after an unsuccesful read
  // past the end, it should return true after succesfully reading the final
  // byte.  Default implementation returns pos() >= size();
  [[nodiscard]] virtual std::expected<bool, StarException> atEnd();

  // Default is to store position, seek end, then restore position.
  [[nodiscard]] virtual std::expected<StreamOffset, StarException> size();

  IOMode mode() const;
  bool isOpen() const;
  bool isReadable() const;
  bool isWritable() const;

  [[nodiscard]] std::expected<ByteArray, StarException> readBytes(std::size_t size);
  [[nodiscard]] std::expected<void, StarException> writeBytes(ByteArray const& p);

  [[nodiscard]] std::expected<ByteArray, StarException> readBytesAbsolute(StreamOffset readPosition, std::size_t size);
  [[nodiscard]] std::expected<void, StarException> writeBytesAbsolute(StreamOffset writePosition, ByteArray const& p);

protected:
  void setMode(IOMode mode);

  IODevice(IODevice const&);
  IODevice& operator=(IODevice const&);

private:
  std::atomic<IOMode> m_mode;
};

inline IOMode operator|(IOMode a, IOMode b) {
  return static_cast<IOMode>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}

inline bool operator&(IOMode a, IOMode b) { return static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b); }

inline IOMode IODevice::mode() const {
  return m_mode;
}

inline bool IODevice::isOpen() const {
  return m_mode != IOMode::Closed;
}

inline bool IODevice::isReadable() const {
  return m_mode & IOMode::Read;
}

inline bool IODevice::isWritable() const {
  return m_mode & IOMode::Write;
}

}

export module star.io_device;

export namespace Star {
  using ::Star::StreamOffset;
  using ::Star::EofExceptionTag;
  using ::Star::EofException;
  using ::Star::IOMode;
  using ::Star::IOSeek;
  using ::Star::Absolute;
  using ::Star::Relative;
  using ::Star::End;
  using ::Star::IODevice;
  using ::Star::operator|;
  using ::Star::operator&;
}
