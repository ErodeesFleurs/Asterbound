module;
#define OV_EXCLUDE_STATIC_CALLBACKS
#include "vorbis/codec.h"
#include "vorbis/vorbisfile.h"
import std;
import star.io_device;
import star.format;

export module star.io_device_callbacks;

export namespace Star {

// Native Vorbis callbacks return native sentinels. The owning C++ boundary must
// consume take_error() after each codec call before interpreting codec statuses.
// Borrows its device; device and adapter addresses stay fixed until codec teardown.
class IODeviceCallbacks {
public:
  [[nodiscard]] static std::expected<IODeviceCallbacks, StarException> create(IODevice& device);
  IODeviceCallbacks(IODeviceCallbacks const&) = delete;
  IODeviceCallbacks& operator=(IODeviceCallbacks const&) = delete;
  IODeviceCallbacks(IODeviceCallbacks&&) = default;
  IODeviceCallbacks& operator=(IODeviceCallbacks&&) = default;

  [[nodiscard]] std::optional<StarException> take_error();

  [[nodiscard]] static constexpr ov_callbacks callbacks() noexcept {
    return {.read_func = read, .seek_func = seek, .close_func = nullptr, .tell_func = tell};
  }

private:
  static std::size_t read(void* destination, std::size_t size, std::size_t count, void* source);
  static int seek(void* source, ogg_int64_t offset, int whence);
  static long tell(void* source);

  explicit IODeviceCallbacks(IODevice& device) : m_device(device) {}
  std::reference_wrapper<IODevice> m_device;
  std::optional<StarException> m_error;
};

}

namespace Star {

std::expected<IODeviceCallbacks, StarException> IODeviceCallbacks::create(IODevice& device) {
  if (!device.is_open()) {
    auto opened = device.open(IOMode::Read);
    if (!opened) return std::unexpected(std::move(opened.error()));
  }
  return IODeviceCallbacks(device);
}

std::optional<StarException> IODeviceCallbacks::take_error() {
  return std::exchange(m_error, std::nullopt);
}

std::size_t IODeviceCallbacks::read(void* destination, std::size_t size, std::size_t count, void* source) {
  auto& state = *static_cast<IODeviceCallbacks*>(source);
  if (state.m_error || !size || !count) return 0;
  if (count > std::numeric_limits<std::size_t>::max() / size) {
    state.m_error = IOException("Vorbis read request exceeds addressable size");
    return 0;
  }
  auto read = state.m_device.get().read({static_cast<char*>(destination), size * count});
  if (!read) {
    state.m_error = std::move(read.error());
    return 0;
  }
  return *read / size;
}

int IODeviceCallbacks::seek(void* source, ogg_int64_t offset, int whence) {
  auto& state = *static_cast<IODeviceCallbacks*>(source);
  if (state.m_error) return -1;
  if (whence < IOSeek::Absolute || whence > IOSeek::End) {
    state.m_error = IOException("Invalid Vorbis seek origin");
    return -1;
  }
  auto moved = state.m_device.get().seek(offset, static_cast<IOSeek>(whence));
  if (!moved) {
    state.m_error = std::move(moved.error());
    return -1;
  }
  return 0;
}

long IODeviceCallbacks::tell(void* source) {
  auto& state = *static_cast<IODeviceCallbacks*>(source);
  if (state.m_error) return -1;
  auto position = state.m_device.get().pos();
  if (!position) {
    state.m_error = std::move(position.error());
    return -1;
  }
  if (*position < 0 || *position > std::numeric_limits<long>::max()) {
    state.m_error = IOException("Vorbis stream position exceeds callback range");
    return -1;
  }
  return static_cast<long>(*position);
}

}
