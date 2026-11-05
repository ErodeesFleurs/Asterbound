module;

import std;
import star.format;
import star.string;
import star.vector;
import star.input_event;
import star.logging;

namespace Star {

class ApplicationController;
class Renderer;

struct ApplicationExceptionTag {
  static constexpr std::string_view name() { return "ApplicationException"; }
};
using ApplicationException = StarError<ApplicationExceptionTag, StarException>;

enum class WindowMode {
  Normal,
  Maximized,
  Fullscreen,
  Borderless
};

// Lifecycle, input, simulation and presentation preparation have one application
// owner (ClientLogic), separate from the SDL/OpenGL main thread. Audio callbacks
// are the explicit exception below. Never share live UI/Lua/world state with
// the graphics consumer.
class Application {
public:
  struct FrameTimings {
    std::int64_t events = 0;
    std::int64_t updates = 0;
    std::int64_t render = 0;
    std::int64_t finish = 0;
    std::int64_t gui = 0;
    std::int64_t present = 0;
    std::int64_t throttle = 0;
    std::int64_t total = 0;
    unsigned updateCount = 0;
  };

  virtual ~Application() = default;

  // Called once on application startup, before any other methods.
  [[nodiscard]] virtual std::expected<void, StarException> startup(StringList const& cmdLineArgs);

  // Available after startup and until this application is destroyed.
  virtual Logger& logger() const = 0;

  // Called on application initialization, before rendering initialization.  If
  // overriden, must call base class instance.
  [[nodiscard]] virtual std::expected<void, StarException> applicationInit(std::shared_ptr<ApplicationController> appController);

  // Called immediately after application initialization on startup, and then
  // also whenever the renderer invalidated and recreated.  If overridden, must
  // call base class instance.
  [[nodiscard]] virtual std::expected<void, StarException> renderInit(std::shared_ptr<Renderer> renderer);

  // Called when the window mode or size is changed.
  [[nodiscard]] virtual std::expected<void, StarException> windowChanged(WindowMode windowMode, Vec2U screenSize);

  // Called before update, once for every pending event.
  [[nodiscard]] virtual std::expected<void, StarException> processInput(InputEvent const& event);

  // Fixed simulation updates, zero or more times between renders. No ImGui
  // frame is active here; immediate-mode UI submission belongs in render().
  [[nodiscard]] virtual std::expected<void, StarException> update();
  
  // Returns how many frames have been skipped.
  virtual unsigned framesSkipped() const;

  // Independently paced by the render cap and VSync, with one active ImGui
  // frame per call. May run without a simulation update, or after several.
  [[nodiscard]] virtual std::expected<void, StarException> render();

  // A prepared frame has actually been presented. Runs on the application
  // owner before preparing another frame, never on the graphics consumer.
  [[nodiscard]] virtual std::expected<void, StarException> framePresented();

  // Will be called *from a different thread* to retrieve audio data (if audio
  // is playing). Default implementation simply fills the buffer with silence.
  [[nodiscard]] virtual std::expected<void, StarException> getAudioData(std::span<std::int16_t> sampleData, std::size_t frameCount);

  // Called once after audio callbacks stop, including on failure. Release all
  // retained platform services and sockets here: main joins SDK callbacks and
  // releases graphics resources before Application destruction and logger death.
  [[nodiscard]] virtual std::expected<void, StarException> shutdown();

  std::shared_ptr<ApplicationController> const& appController() const;
  std::shared_ptr<Renderer> const& renderer() const;
  // Previous completed pipeline iteration, in wall-clock microseconds.
  // Accessed on the application owner only. CPU preparation, backend submission
  // and waiting may overlap; these are not GPU execution timers.
  FrameTimings const& frameTimings() const { return m_frameTimings; }
  void recordFrameTimings(FrameTimings timings) { m_frameTimings = timings; }

private:
  std::shared_ptr<ApplicationController> m_appController;
  std::shared_ptr<Renderer> m_renderer;
  FrameTimings m_frameTimings;
};

inline std::shared_ptr<ApplicationController> const& Application::appController() const {
  return m_appController;
}

inline std::shared_ptr<Renderer> const& Application::renderer() const {
  return m_renderer;
}

}

export module star.application;

export namespace Star {
  using ::Star::ApplicationController;
  using ::Star::Renderer;
  using ::Star::Application;
  using ::Star::ApplicationExceptionTag;
  using ::Star::ApplicationException;
  using ::Star::WindowMode;
}

