module;
import std;
import star.format;
import star.byte_array;

import star.string;
import star.input_event;
import star.application;
import star.map;
import star.rect;

import star.vector;
namespace Star {

class StatisticsService;
class P2PNetworkingService;
class UserGeneratedContentService;
class DesktopService;
class Image;

// Audio format is always 16 bit signed integer samples
struct AudioFormat {
  unsigned sampleRate;
  unsigned channels;
};

// Window size defaults to 800x600, fixed updates to 60hz, rendering to uncapped,
// maximized and fullscreen are false, vsync is on, the cursor is visible, and
// audio and text input are disabled.
class ApplicationController {
public:
  virtual ~ApplicationController() = default;

  // Fixed simulation frequency; changing it discards old timing debt.
  // Invalid (non-finite or non-positive) rates are ignored.
  virtual void setTargetUpdateRate(float targetUpdateRate) = 0;
  // Independent render cap, preserving the logic deadline when changed.
  // Zero is unlimited; VSync may still block present.
  // Invalid (non-finite or negative) rates are ignored.
  virtual void setTargetRenderRate(float rate) = 0;
  // Previous-to-current fixed-step presentation fraction, stable for a render.
  // Starts at 1 after reset, until the first scheduled simulation update.
  virtual float renderInterpolation() const = 0;
  // Changes whenever timing resynchronizes or overdue logic debt is discarded.
  virtual std::uint64_t renderEpoch() const = 0;
  // Maximum extra updates in one outer-loop batch. Excess overdue whole ticks
  // are discarded instead of allowing unbounded catch-up to starve rendering.
  virtual void setMaxFrameSkip(unsigned maxFrameSkip) = 0;

  virtual void setApplicationTitle(String title) = 0;
  [[nodiscard]] virtual std::expected<void, StarException> setFullscreenWindow(Vec2U fullScreenResolution) = 0;
  virtual void setNormalWindow(Vec2U windowSize) = 0;
  virtual void setMaximizedWindow() = 0;
  [[nodiscard]] virtual std::expected<void, StarException> setBorderlessWindow() = 0;
  #ifdef STAR_SYSTEM_WINDOWS
  virtual void setBorderlessWorkaround(bool active) = 0;
  #endif
  [[nodiscard]] virtual std::expected<void, StarException> setVSyncEnabled(bool vSync) = 0;
  virtual void setCursorVisible(bool cursorVisible) = 0;
  virtual void setCursorPosition(Vec2I cursorPosition) = 0;
  virtual void setCursorHardware(bool cursorHardware) = 0;
  [[nodiscard]] virtual std::expected<bool, StarException> setCursorImage(const String& id, const std::shared_ptr<Image const>& image, unsigned scale, const Vec2I& offset) = 0;
  virtual void setAcceptingTextInput(bool acceptingTextInput) = 0;
  virtual void setTextArea(std::optional<std::pair<RectI, int>> area = {}) = 0;


  virtual AudioFormat enableAudio() = 0;
  virtual void disableAudio() = 0;

  typedef std::function<void(std::span<std::uint8_t>)> AudioCallback;
  [[nodiscard]] virtual std::expected<bool, StarException> openAudioInputDevice(std::uint32_t deviceId, int freq, int channels, AudioCallback callback) = 0;
  virtual bool closeAudioInputDevice() = 0;

  virtual bool hasClipboard() = 0;
  virtual bool setClipboard(String text) = 0;
  virtual bool setClipboardData(StringMap<ByteArray>) = 0;
  [[nodiscard]] virtual std::expected<bool, StarException> setClipboardImage(Image const& image, std::optional<std::reference_wrapper<ByteArray>> png = std::nullopt, std::optional<std::reference_wrapper<String const>> path = std::nullopt) = 0;
  [[nodiscard]] virtual std::expected<bool, StarException> setClipboardFile(String const& path) = 0;
  virtual std::optional<String> getClipboard() = 0;

  virtual bool isFocused() const = 0;

  // Returns the latest actual measured update and render rate, which may be
  // different than the target update rate.
  virtual float updateRate() const = 0;
  virtual float renderFps() const = 0;

  virtual float getDisplayScale() const = 0;

  virtual std::shared_ptr<StatisticsService> statisticsService() const = 0;
  virtual std::shared_ptr<P2PNetworkingService> p2pNetworkingService() const = 0;
  virtual std::shared_ptr<UserGeneratedContentService> userGeneratedContentService() const = 0;
  virtual std::shared_ptr<DesktopService> desktopService() const = 0;
  virtual bool steamIsFlatpak() const = 0;

  // Signals the application to quit
  virtual void quit() = 0;
};

}

export module star.application_controller;

export namespace Star {
  using ::Star::ApplicationController;
  using ::Star::AudioFormat;
}
