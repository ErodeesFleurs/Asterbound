module;
#include "GL/glew.h"
import std;
import star.format;
import star.json;
import star.string;
import star.list;
import star.set;

import star.rect;
import star.image;
import star.casting;
import star.texture_atlas;
import star.bi_map;
import star.ref_ptr;
import star.renderer;
import star.type_identity;
import star.logging;


import star.vector;
import star.matrix3;
namespace Star {

inline constexpr std::size_t FrameBufferCount = 1;

// TODO: Bott - This isn't really rendering-specific. Don't really like it either. It's nicer than having redundant bool options, though.
enum class BoolSettingMode {
  Enabled,
  FromSetting,
  Disabled
};
inline constexpr auto BoolSettingModeNames = sortedEnumNames(std::to_array<std::pair<BoolSettingMode, std::string_view>>({
  {BoolSettingMode::Enabled, "Enabled"},
  {BoolSettingMode::FromSetting, "FromSetting"},
  {BoolSettingMode::Disabled, "Disabled"}
}));

bool settingModeValue(BoolSettingMode const& mode, bool const& setting);

// OpenGL 2.0 implementation of Renderer.  OpenGL context must be created and
// active during construction, destruction, and all method calls.
class OpenGlRenderer : public Renderer {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<OpenGlRenderer>, StarException> create(Logger& logger);
  ~OpenGlRenderer() override;

  String rendererId() const override;
  Vec2U screenSize() const override;

  [[nodiscard]] std::expected<void, StarException> loadConfig(Json const& config) override;
  [[nodiscard]] std::expected<void, StarException> loadEffectConfig(String const& name, Json const& effectConfig, StringMap<String> const& shaders) override;

  [[nodiscard]] std::expected<void, StarException> setEffectParameter(String const& parameterName, RenderEffectParameter const& parameter) override;
  [[nodiscard]] std::expected<void, StarException> setEffectScriptableParameter(String const& effectName, String const& parameterName, RenderEffectParameter const& parameter) override;
  std::optional<RenderEffectParameter> getEffectScriptableParameter(String const& effectName, String const& parameterName) override;
  [[nodiscard]] std::expected<void, StarException> setEffectTexture(String const& textureName, ImageView const& image) override;

  [[nodiscard]] std::expected<void, StarException> setScissorRect(std::optional<RectI> const& scissorRect) override;
  std::optional<RectI> scissorRect() const override;

  [[nodiscard]] std::expected<bool, StarException> switchEffectConfig(String const& name) override;

  [[nodiscard]] std::expected<TexturePtr, StarException> createTexture(Image const& texture, TextureAddressing addressing, TextureFiltering filtering) override;
  void setSizeLimitEnabled(bool enabled) override;
  void setMultiTexturingEnabled(bool enabled) override;
  [[nodiscard]] std::expected<void, StarException> setMultiSampling(unsigned multiSampling) override;
  [[nodiscard]] std::expected<void, StarException> setMainHDR(bool enabled) override;
  [[nodiscard]] std::expected<std::shared_ptr<TextureGroup>, StarException> createTextureGroup(TextureGroupSize size, TextureFiltering filtering) override;
  [[nodiscard]] std::expected<std::shared_ptr<RenderBuffer>, StarException> createRenderBuffer() override;

  List<RenderPrimitive>& immediatePrimitives() override;
  void render(RenderPrimitive primitive) override;
  // Detached views are consumed synchronously into main-owned GPU-format bytes.
  [[nodiscard]] std::expected<void, StarException> appendImmediate(DetachedRenderGeometry const& geometry);
  [[nodiscard]] std::expected<void, StarException> setRenderBuffer(std::shared_ptr<RenderBuffer> const& renderBuffer, DetachedRenderGeometry const& geometry);
  [[nodiscard]] std::expected<void, StarException> renderBuffer(std::shared_ptr<RenderBuffer> const& renderBuffer, Mat3F const& transformation) override;

  [[nodiscard]] std::expected<void, StarException> flush(Mat3F const& transformation) override;

  [[nodiscard]] std::expected<void, StarException> setScreenSize(Vec2U screenSize);

  [[nodiscard]] std::expected<void, StarException> startFrame();
  [[nodiscard]] std::expected<void, StarException> finishFrame();

private:
  explicit OpenGlRenderer(Logger& logger);
  [[nodiscard]] std::expected<void, StarException> initialize();
  struct GlTextureAtlasSet : public TextureAtlasSet<GLuint> {
  public:
    GlTextureAtlasSet(unsigned atlasNumCells);

    [[nodiscard]] std::expected<GLuint, StarException> createAtlasTexture(Vec2U const& size, PixelFormat pixelFormat) override;
    void destroyAtlasTexture(GLuint const& glTexture) override;
    [[nodiscard]] std::expected<void, StarException> copyAtlasPixels(GLuint const& glTexture, Vec2U const& bottomLeft, Image const& image) override;

    TextureFiltering textureFiltering;
  };

  struct GlTextureGroup : std::enable_shared_from_this<GlTextureGroup>, public TextureGroup {
    GlTextureGroup(unsigned atlasNumCells);
    ~GlTextureGroup() override;

    TextureFiltering filtering() const override;
    [[nodiscard]] std::expected<TexturePtr, StarException> create(Image const& texture) override;

    GlTextureAtlasSet textureAtlasSet;
  };

  struct GlTexture : public Texture {
    void const* queryType(TypeKey key) const noexcept override {
      return queryTypeOf<GlTexture, Texture>(this, key);
    }
    std::string_view dynamicTypeName() const noexcept override {
      return Star::typeName<GlTexture>();
    }

    virtual GLuint glTextureId() const = 0;
    virtual Vec2U glTextureSize() const = 0;
    virtual Vec2U glTextureCoordinateOffset() const = 0;
  };

  struct GlGroupedTexture : public GlTexture {
    void const* queryType(TypeKey key) const noexcept override {
      return queryTypeOf<GlGroupedTexture, GlTexture>(this, key);
    }
    std::string_view dynamicTypeName() const noexcept override {
      return Star::typeName<GlGroupedTexture>();
    }

    ~GlGroupedTexture() override;

    Vec2U size() const override;
    TextureFiltering filtering() const override;
    TextureAddressing addressing() const override;

    GLuint glTextureId() const override;
    Vec2U glTextureSize() const override;
    Vec2U glTextureCoordinateOffset() const override;

    void incrementBufferUseCount();
    void decrementBufferUseCount();

    unsigned bufferUseCount = 0;
    std::shared_ptr<GlTextureGroup> parentGroup;
    GlTextureAtlasSet::TextureHandle parentAtlasTexture = nullptr;
  };

  struct GlLoneTexture : public GlTexture {
    void const* queryType(TypeKey key) const noexcept override {
      return queryTypeOf<GlLoneTexture, GlTexture>(this, key);
    }
    std::string_view dynamicTypeName() const noexcept override {
      return Star::typeName<GlLoneTexture>();
    }

    ~GlLoneTexture() override;

    Vec2U size() const override;
    TextureFiltering filtering() const override;
    TextureAddressing addressing() const override;

    GLuint glTextureId() const override;
    Vec2U glTextureSize() const override;
    Vec2U glTextureCoordinateOffset() const override;

    GLuint textureId = 0;
    Vec2U textureSize;
    TextureAddressing textureAddressing = TextureAddressing::Clamp;
    TextureFiltering textureFiltering = TextureFiltering::Nearest;
  };

  struct GlPackedVertexData {
    std::uint32_t textureIndex : 2;
    std::uint32_t fullbright : 1;
    std::uint32_t rX : 1;
    std::uint32_t rY : 1;
    std::uint32_t unused : 27;
  };

  struct GlRenderVertex {
    Vec2F pos;
    Vec2F uv;
    Vec4B color;
    union Packed {
      std::uint32_t packed;
      GlPackedVertexData vars;
    } pack;
  };

  struct GlRenderBuffer : public RenderBuffer {
    void const* queryType(TypeKey key) const noexcept override {
      return queryTypeOf<GlRenderBuffer, RenderBuffer>(this, key);
    }
    std::string_view dynamicTypeName() const noexcept override {
      return Star::typeName<GlRenderBuffer>();
    }

    struct GlVertexBufferTexture {
      GLuint texture;
      Vec2U size;
    };

    struct GlVertexBuffer {
      std::array<GlVertexBufferTexture, 4> textures;
      std::size_t textureCount = 0;
      GLuint vertexBuffer = 0;
      std::size_t vertexCount = 0;
      std::size_t vertexCapacity = 0;
    };

    GlRenderBuffer() = default;
    ~GlRenderBuffer() override;

    [[nodiscard]] std::expected<void, StarException> set(List<RenderPrimitive>& primitives) override;
    [[nodiscard]] std::expected<void, StarException> set(DetachedRenderGeometry const& geometry);
    [[nodiscard]] std::expected<void, StarException> append(List<RenderPrimitive> const& primitives);
    [[nodiscard]] std::expected<void, StarException> append(DetachedRenderGeometry const& geometry);
    [[nodiscard]] std::expected<void, StarException> upload();
    void reset();

    template <typename VertexAt>
    [[nodiscard]] std::expected<void, StarException> appendPrimitive(std::uint8_t kind, std::size_t count, TexturePtr const& texture, VertexAt const& vertexAt);

    struct PackedBatch {
      std::array<GlVertexBufferTexture, 4> textures;
      std::size_t textureCount = 0;
      std::size_t firstVertex = 0;
      std::size_t vertexCount = 0;
    };

    TexturePtr whiteTexture;
    List<GlRenderVertex> accumulationBuffer;
    List<PackedBatch> packedBatches;

    HashSet<TexturePtr> usedTextures;
    List<GlVertexBuffer> vertexBuffers;
    GLuint vertexArray = 0;

    bool useMultiTexturing{true};
  };

  struct EffectParameter {
    GLint parameterUniform = -1;
    std::uint8_t parameterType = 0;
    std::optional<RenderEffectParameter> parameterValue;
  };

  struct EffectTexture {
    GLint textureUniform = -1;
    unsigned textureUnit = 0;
    TextureAddressing textureAddressing = TextureAddressing::Clamp;
    TextureFiltering textureFiltering = TextureFiltering::Linear;
    GLint textureSizeUniform = -1;
    RefPtr<GlLoneTexture> textureValue;
  };
  
  struct GlFrameBuffer : RefCounter {
    GLuint id = 0;
    RefPtr<GlLoneTexture> texture;
    
    bool hasAlt = false;
    GLuint altId = 0;
    RefPtr<GlLoneTexture> altTexture;

    Json config;
    std::optional<Vec2U> overrideSize;
    BoolSettingMode hdrMode = BoolSettingMode::Disabled;
    bool alpha = false;
    bool clear = true;
    unsigned multisample = 0;
    unsigned sizeDiv = 1;
    
    bool blitted = false;
    bool justSwapped = false;
    
    [[nodiscard]] std::expected<void, StarException> makeAlt(Vec2U const& screenSize = Vec2U(256, 256));
    [[nodiscard]] std::expected<void, StarException> swap();

    [[nodiscard]] static std::expected<RefPtr<GlFrameBuffer>, StarException> create(Json const& config);
    GlFrameBuffer() = default;
    ~GlFrameBuffer() override;
  };

  class Effect {
  public:
    GLuint program = 0;
    Json config;
    StringMap<EffectParameter> parameters;
    StringMap<EffectParameter> scriptables; // scriptable parameters which can be changed when the effect is not loaded
    StringMap<EffectTexture> textures;

    StringMap<GLuint> attributes;
    StringMap<GLuint> uniforms;

    GLuint getAttribute(String const& name);
    GLuint getUniform(String const& name);
    bool includeVBTextures;
    bool doubleBuffered = false;
  };

  static bool logGlErrorSummary(String prefix);
  [[nodiscard]] static std::expected<void, StarException> checkGlError(std::string_view operation);
  [[nodiscard]] static std::expected<void, StarException> uploadTextureImage(PixelFormat pixelFormat, Vec2U size, std::uint8_t const* data);

  
  [[nodiscard]] static std::expected<RefPtr<GlLoneTexture>, StarException> createGlTexture(ImageView const& image, TextureAddressing addressing, TextureFiltering filtering);

  [[nodiscard]] std::expected<std::shared_ptr<GlRenderBuffer>, StarException> createGlRenderBuffer();

  [[nodiscard]] std::expected<void, StarException> flushImmediatePrimitives(Mat3F const& transformation = Mat3F::identity());

  [[nodiscard]] std::expected<void, StarException> renderGlBuffer(GlRenderBuffer const& renderBuffer, Mat3F const& transformation);

  void setupGlUniforms(Effect& effect, Vec2U screenSize);

  [[nodiscard]] std::expected<RefPtr<OpenGlRenderer::GlFrameBuffer>, StarException> getGlFrameBuffer(String const& id);
  [[nodiscard]] std::expected<void, StarException> blitGlFrameBuffer(RefPtr<OpenGlRenderer::GlFrameBuffer> const& frameBuffer, bool const& useAlt = false);
  [[nodiscard]] std::expected<void, StarException> switchGlFrameBuffer(RefPtr<OpenGlRenderer::GlFrameBuffer> const& frameBuffer);

  Logger& m_logger;
  Vec2U m_screenSize;

  GLuint m_program = 0;

  GLint m_positionAttribute = -1;
  GLint m_colorAttribute = -1;
  GLint m_texCoordAttribute = -1;
  GLint m_dataAttribute = -1;
  List<GLint> m_textureUniforms = {};
  List<GLint> m_textureSizeUniforms = {};
  GLint m_screenSizeUniform = -1;
  GLint m_vertexTransformUniform = -1;

  Json m_config;

  StringMap<Effect> m_effects;
  std::optional<std::reference_wrapper<Effect>> m_currentEffect;

  StringMap<RefPtr<GlFrameBuffer>> m_frameBuffers;
  RefPtr<GlFrameBuffer> m_currentFrameBuffer;

  RefPtr<GlTexture> m_whiteTexture;

  std::optional<RectI> m_scissorRect;

  bool m_limitTextureGroupSize;
  bool m_useMultiTexturing;
  unsigned m_multiSampling; // if non-zero, is enabled and acts as sample count
  bool m_hdrSetting;
  List<std::shared_ptr<GlTextureGroup>> m_liveTextureGroups;

  List<RenderPrimitive> m_immediatePrimitives;
  std::shared_ptr<GlRenderBuffer> m_immediateRenderBuffer;
};

}

export module star.renderer_opengl;

export namespace Star {
  using ::Star::OpenGlRenderer;
  using ::Star::FrameBufferCount;
  using ::Star::BoolSettingMode;
  using ::Star::BoolSettingModeNames;
  using ::Star::settingModeValue;
}
