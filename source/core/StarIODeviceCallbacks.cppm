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
// consume takeError() after each codec call before interpreting codec statuses.
// Borrows its device; device and adapter addresses stay fixed until codec teardown.
class IODeviceCallbacks {
public:
  [[nodiscard]] static std::expected<IODeviceCallbacks, StarException> create(IODevice& device);
  IODeviceCallbacks(IODeviceCallbacks const&) = delete;
  IODeviceCallbacks& operator=(IODeviceCallbacks const&) = delete;
  IODeviceCallbacks(IODeviceCallbacks&&) = default;
  IODeviceCallbacks& operator=(IODeviceCallbacks&&) = default;

  [[nodiscard]] std::optional<StarException> takeError();

  static std::size_t readFunc(void* ptr, std::size_t size, std::size_t nmemb, void* datasource);
  static int seekFunc(void* datasource, ogg_int64_t offset, int whence);
  static long int tellFunc(void* datasource);
  void setupOggCallbacks(ov_callbacks& callbacks);

private:
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

std::optional<StarException> IODeviceCallbacks::takeError() {
  auto error = std::move(m_error);
  m_error.reset();
  return error;
}

std::size_t IODeviceCallbacks::readFunc(void* ptr, std::size_t size, std::size_t nmemb, void* datasource) {
  auto& state = *static_cast<IODeviceCallbacks*>(datasource);
  if (state.m_error || !size || !nmemb) return 0;
  if (nmemb > std::numeric_limits<std::size_t>::max() / size) {
    state.m_error = IOException("Vorbis read request exceeds addressable size");
    return 0;
  }
  auto count = state.m_device.get().read({static_cast<char*>(ptr), size * nmemb});
  if (!count) {
    state.m_error = std::move(count.error());
    return 0;
  }
  return *count / size;
}

int IODeviceCallbacks::seekFunc(void* datasource, ogg_int64_t offset, int whence) {
  auto& state = *static_cast<IODeviceCallbacks*>(datasource);
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

long int IODeviceCallbacks::tellFunc(void* datasource) {
  auto& state = *static_cast<IODeviceCallbacks*>(datasource);
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

void IODeviceCallbacks::setupOggCallbacks(ov_callbacks& callbacks) {
  callbacks.read_func = readFunc;
  callbacks.seek_func = seekFunc;
  callbacks.tell_func = tellFunc;
  callbacks.close_func = nullptr;
}

}
