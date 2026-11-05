module;
import std;
import star.format;
import star.json;
import star.string;
import star.list;
import star.image;
import star.renderer;
import star.renderer_opengl;
import star.main_thread_dispatcher;
import star.vector;
import star.matrix3;
import star.rect;

namespace Star {
struct ThreadedRendererState;

// Worker-owned Renderer frontend. Only consumeFrame/releaseResources run on the
// context-owning main thread. The caller acknowledges swap before retire and
// must not start another preparation before that acknowledgment.
//
// Resource creation, native draw parameters and drawing report acceptance into
// the ordered stream. Their first backend failure is retained as an owned
// StarException and returned by consumeFrame, retire, and subsequent fallible
// frontend calls; no commands except retirement execute after that failure.
// Config/shader loading, scriptable setters, MSAA and HDR are synchronous
// barriers: their own recoverable backend errors return directly to the caller.
// Shader getters perform an ordered barrier and return the actual backend
// result, retaining any preceding asynchronous error for the fallible channel.
class ThreadedRenderer final : public Renderer {
public:
  struct FrameTicket {
    std::uint64_t generation = 0;
    unsigned slot = 0;
    bool operator==(FrameTicket const&) const = default;
  };

  ThreadedRenderer(std::shared_ptr<OpenGlRenderer> backend, MainThreadDispatcher& dispatcher);
  ~ThreadedRenderer();
  ThreadedRenderer(ThreadedRenderer const&) = delete;
  ThreadedRenderer& operator=(ThreadedRenderer const&) = delete;

  [[nodiscard]] std::expected<void, StarException> startFrame();
  [[nodiscard]] std::expected<void, StarException> finishFrame();
  FrameTicket frameTicket() const;
  [[nodiscard]] std::expected<void, StarException> consumeFrame(FrameTicket ticket);
  [[nodiscard]] std::expected<void, StarException> retire(FrameTicket ticket);
  // Ordered synchronous barrier; main must service it after any pending swap.
  // Also called by the worker on every exit path, before joining.
  [[nodiscard]] std::expected<void, StarException> drain();
  // Main only, worker paused after final drain, before Application/Root dies.
  // Clears every GL owner/backend reference even after a device error.
  [[nodiscard]] std::expected<void, StarException> releaseResources();
  [[nodiscard]] std::expected<void, StarException> setScreenSize(Vec2U size);

  String rendererId() const override;
  Vec2U screenSize() const override;
  [[nodiscard]] std::expected<void, StarException> loadConfig(Json const& config) override;
  [[nodiscard]] std::expected<void, StarException> loadEffectConfig(String const& name, Json const& config, StringMap<String> const& shaders) override;
  [[nodiscard]] std::expected<void, StarException> setEffectParameter(String const& name, RenderEffectParameter const& value) override;
  [[nodiscard]] std::expected<void, StarException> setEffectScriptableParameter(String const& effect, String const& name, RenderEffectParameter const& value) override;
  std::optional<RenderEffectParameter> getEffectScriptableParameter(String const& effect, String const& name) override;
  std::optional<std::uint8_t> getEffectScriptableParameterType(String const& effect, String const& name) override;
  [[nodiscard]] std::expected<void, StarException> setEffectTexture(String const& name, ImageView const& image) override;
  [[nodiscard]] std::expected<bool, StarException> switchEffectConfig(String const& name) override;
  [[nodiscard]] std::expected<void, StarException> setScissorRect(std::optional<RectI> const& rect) override;
  std::optional<RectI> scissorRect() const override;
  [[nodiscard]] std::expected<TexturePtr, StarException> createTexture(Image const& image, TextureAddressing addressing = TextureAddressing::Clamp, TextureFiltering filtering = TextureFiltering::Nearest) override;
  void setSizeLimitEnabled(bool enabled) override;
  void setMultiTexturingEnabled(bool enabled) override;
  [[nodiscard]] std::expected<void, StarException> setMultiSampling(unsigned samples) override;
  [[nodiscard]] std::expected<void, StarException> setMainHDR(bool enabled) override;
  [[nodiscard]] std::expected<std::shared_ptr<TextureGroup>, StarException> createTextureGroup(TextureGroupSize size = TextureGroupSize::Medium, TextureFiltering filtering = TextureFiltering::Nearest) override;
  [[nodiscard]] std::expected<std::shared_ptr<RenderBuffer>, StarException> createRenderBuffer() override;
  List<RenderPrimitive>& immediatePrimitives() override;
  void render(RenderPrimitive primitive) override;
  [[nodiscard]] std::expected<void, StarException> renderBuffer(std::shared_ptr<RenderBuffer> const& buffer, Mat3F const& transform = Mat3F::identity()) override;
  [[nodiscard]] std::expected<void, StarException> flush(Mat3F const& transform = Mat3F::identity()) override;

private:
  std::shared_ptr<ThreadedRendererState> m_state;
};
}

export module star.threaded_renderer;
export namespace Star {
  using ::Star::ThreadedRenderer;
}
