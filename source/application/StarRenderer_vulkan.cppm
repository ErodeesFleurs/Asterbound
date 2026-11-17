module;
struct SDL_Window;
struct ImDrawData;
import std;
import star.format;
import star.renderer;
import star.logging;
import star.json;
import star.string;
import star.list;
import star.image;
import star.rect;
import star.vector;
import star.matrix3;

namespace Star {
// Main-thread native backend. Detached geometry is packed before returning;
// CPU command retirement never implies GPU resource retirement.
class VulkanRenderer final : public NativeRenderer {
public:
  static std::expected<std::shared_ptr<VulkanRenderer>, StarException> create(Logger&, SDL_Window&, bool vsync);
  ~VulkanRenderer() override;
  std::expected<void, StarException> initializeGui();
  void newGuiFrame();
  std::expected<bool, StarException> present(ImDrawData&);
  std::expected<void, StarException> setVSyncEnabled(bool);
  void shutdownGui();

  String rendererId() const override;
  Vec2U screenSize() const override;
  std::expected<void, StarException> loadConfig(Json const&) override;
  std::expected<void, StarException> loadEffectConfig(String const&, Json const&, StringMap<String> const&) override;
  std::expected<void, StarException> setEffectParameter(String const&, RenderEffectParameter const&) override;
  std::expected<void, StarException> setEffectScriptableParameter(String const&, String const&, RenderEffectParameter const&) override;
  std::optional<RenderEffectParameter> getEffectScriptableParameter(String const&, String const&) override;
  std::expected<void, StarException> setEffectTexture(String const&, ImageView const&) override;
  std::expected<bool, StarException> switchEffectConfig(String const&) override;
  std::expected<void, StarException> setScissorRect(std::optional<RectI> const&) override;
  std::optional<RectI> scissorRect() const override;
  std::expected<TexturePtr, StarException> createTexture(Image const&, TextureAddressing, TextureFiltering) override;
  void setSizeLimitEnabled(bool) override;
  void setMultiTexturingEnabled(bool) override;
  std::expected<void, StarException> setMultiSampling(unsigned) override;
  std::expected<void, StarException> setMainHDR(bool) override;
  std::expected<std::shared_ptr<TextureGroup>, StarException> createTextureGroup(TextureGroupSize, TextureFiltering) override;
  std::expected<std::shared_ptr<RenderBuffer>, StarException> createRenderBuffer() override;
  List<RenderPrimitive>& immediatePrimitives() override;
  void render(RenderPrimitive) override;
  std::expected<void, StarException> appendImmediate(DetachedRenderGeometry const&) override;
  std::expected<void, StarException> setRenderBuffer(std::shared_ptr<RenderBuffer> const&, DetachedRenderGeometry const&) override;
  std::expected<void, StarException> renderBuffer(std::shared_ptr<RenderBuffer> const&, Mat3F const&) override;
  std::expected<void, StarException> flush(Mat3F const& = Mat3F::identity()) override;
  std::expected<void, StarException> setScreenSize(Vec2U) override;
  std::expected<void, StarException> startFrame() override;
  std::expected<void, StarException> finishFrame() override;
private:
  struct State;
  explicit VulkanRenderer(std::unique_ptr<State>);
  std::unique_ptr<State> m_state;
};
}

export module star.renderer_vulkan;
export namespace Star {
  using ::Star::VulkanRenderer;
}
