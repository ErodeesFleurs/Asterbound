module;
import std;
import star.format;
import star.json;
import star.string;
import star.list;

import star.image;
import star.poly;
import star.bi_map;
import star.ref_ptr;
import star.type_identity;

import star.vector;
import star.matrix3;
import star.rect;
namespace Star {

struct RendererExceptionTag {
  static constexpr std::string_view name() { return "RendererException"; }
};
using RendererException = StarError<RendererExceptionTag, StarException>;

class Texture;
using TexturePtr = RefPtr<Texture>;

enum class TextureAddressing {
  Clamp,
  Wrap
};
inline constexpr auto TextureAddressingNames = sortedEnumNames(std::to_array<std::pair<TextureAddressing, std::string_view>>({
  {TextureAddressing::Clamp, "Clamp"},
  {TextureAddressing::Wrap, "Wrap"}
}));

enum class TextureFiltering {
  Nearest,
  Linear
};
inline constexpr auto TextureFilteringNames = sortedEnumNames(std::to_array<std::pair<TextureFiltering, std::string_view>>({
  {TextureFiltering::Nearest, "Nearest"},
  {TextureFiltering::Linear, "Linear"}
}));

// Medium is the maximum guaranteed texture group size
// Where a Medium sized texture group is expected to fill a single page Large can be used,
// but is not guaranteed to be supported by all systems.
// Where Large sized textures are not supported, a Medium one is used
enum class TextureGroupSize {
  Small,
  Medium,
  Large
};

// Both screen coordinates and texture coordinates are in pixels from the
// bottom left to top right.
struct RenderVertex {
  Vec2F screenCoordinate;
  Vec2F textureCoordinate;
  Vec4B color;
  float param1;
};

class RenderTriangle {
public:
  RenderTriangle() = default;
  RenderTriangle(Vec2F posA, Vec2F posB, Vec2F posC, Vec4B color = Vec4B::filled(255), float param1 = 0.0f);
  RenderTriangle(TexturePtr tex, Vec2F posA, Vec2F uvA, Vec2F posB, Vec2F uvB, Vec2F posC, Vec2F uvC, Vec4B color = Vec4B::filled(255), float param1 = 0.0f);

  TexturePtr texture;
  RenderVertex a, b, c;
};

class RenderQuad {
public:
  RenderQuad() = default;
  RenderQuad(Vec2F posA, Vec2F posB, Vec2F posC, Vec2F posD, Vec4B color = Vec4B::filled(255), float param1 = 0.0f);
  RenderQuad(TexturePtr tex, Vec2F minScreen, float textureScale = 1.0f, Vec4B color = Vec4B::filled(255), float param1 = 0.0f);
  RenderQuad(TexturePtr tex, RectF const& screenCoords, Vec4B color = Vec4B::filled(255), float param1 = 0.0f);
  RenderQuad(TexturePtr tex, Vec2F posA, Vec2F uvA, Vec2F posB, Vec2F uvB, Vec2F posC, Vec2F uvC, Vec2F posD, Vec2F uvD, Vec4B color = Vec4B::filled(255), float param1 = 0.0f);
  RenderQuad(TexturePtr tex, RenderVertex vA, RenderVertex vB, RenderVertex vC, RenderVertex vD);
  RenderQuad(RectF const& rect, Vec4B color = Vec4B::filled(255), float param1 = 0.0f);

  TexturePtr texture;
  RenderVertex a, b, c, d;
};

class RenderPoly {
public:
  RenderPoly() = default;
  RenderPoly(List<Vec2F> const& verts, Vec4B color, float param1 = 0.0f);

  TexturePtr texture;
  List<RenderVertex> vertexes;
};

RenderQuad renderTexturedRect(TexturePtr texture, Vec2F minScreen, float textureScale = 1.0f, Vec4B color = Vec4B::filled(255), float param1 = 0.0f);
RenderQuad renderTexturedRect(TexturePtr texture, RectF const& screenCoords, Vec4B color = Vec4B::filled(255), float param1 = 0.0f);
RenderQuad renderFlatRect(RectF const& rect, Vec4B color, float param1 = 0.0f);
RenderPoly renderFlatPoly(PolyF const& poly, Vec4B color, float param1 = 0.0f);

using RenderPrimitive = std::variant<RenderTriangle, RenderQuad, RenderPoly>;

// Thread transport owns the indexed vertices and numeric texture handles.
// The backend borrows these spans and its main-owned texture table only during
// synchronous packing; none of these references may outlive that call.
struct DetachedRenderPrimitive {
  std::uint64_t texture = 0;
  std::size_t first = 0;
  std::size_t count = 0;
  std::uint8_t kind = 0; // triangle, quad, polygon
};
struct DetachedRenderGeometry {
  std::span<DetachedRenderPrimitive const> primitives;
  std::span<RenderVertex const> vertices;
  std::unordered_map<std::uint64_t, TexturePtr> const& textures;
};

class Texture : public RefCounter {
public:
  ~Texture() override = default;

  void const* query_type(TypeKey key) const noexcept override { return query_type_of<Texture, RefCounter>(this, key); }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<Texture>(); }

  virtual Vec2U size() const = 0;
  virtual TextureFiltering filtering() const = 0;
  virtual TextureAddressing addressing() const = 0;
};

// Textures may be created individually, or in a texture group.  Textures in
// a texture group will be faster to render when rendered together, and will
// use less texture memory when many small textures are in a common group.
// Texture groups must all have the same texture parameters, and will always
// use clamped texture addressing.
class TextureGroup {
public:
  virtual ~TextureGroup() = default;

  virtual TextureFiltering filtering() const = 0;
  [[nodiscard]] virtual std::expected<TexturePtr, StarException> create(Image const& texture) = 0;
};

class RenderBuffer {
public:
  virtual ~RenderBuffer() = default;

  virtual void const* query_type(TypeKey key) const noexcept { return query_type_of<RenderBuffer>(this, key); }
  virtual std::string_view dynamic_type_name() const noexcept { return Star::type_name<RenderBuffer>(); }

  // Transforms the given primitives into a form suitable for the underlying
  // graphics system and stores it for fast replaying.
  [[nodiscard]] virtual std::expected<void, StarException> set(List<RenderPrimitive>& primitives) = 0;
};

using RenderEffectParameter = std::variant<float, int, Vec4F, Vec3F, Vec2F, bool>;

class Renderer {
public:
  virtual ~Renderer() = default;

  virtual String rendererId() const = 0;
  virtual Vec2U screenSize() const = 0;

  [[nodiscard]] virtual std::expected<void, StarException> loadConfig(Json const& config) = 0;

  // The actual shaders used by this renderer will be in a default no effects
  // state when constructed, but can be overridden here.  This config will be
  // specific to each type of renderer, so it will be necessary to key the
  // configuration off of the renderId string.  This should not be called every
  // frame, because it will result in a recompile of the underlying shader set.
  [[nodiscard]] virtual std::expected<void, StarException> loadEffectConfig(String const& name, Json const& effectConfig, StringMap<String> const& shaders) = 0;

  // The effect config will specify named parameters and textures which can be
  // set here.
  [[nodiscard]] virtual std::expected<void, StarException> setEffectParameter(String const& parameterName, RenderEffectParameter const& parameter) = 0;
  [[nodiscard]] virtual std::expected<void, StarException> setEffectScriptableParameter(String const& effectName, String const& parameterName, RenderEffectParameter const& parameter) = 0;
  virtual std::optional<RenderEffectParameter> getEffectScriptableParameter(String const& effectName, String const& parameterName) = 0;
  [[nodiscard]] virtual std::expected<void, StarException> setEffectTexture(String const& textureName, ImageView const& image) = 0;
  [[nodiscard]] virtual std::expected<bool, StarException> switchEffectConfig(String const& name) = 0;

  // Any further rendering will be scissored based on this rect, specified in
  // pixels
  [[nodiscard]] virtual std::expected<void, StarException> setScissorRect(std::optional<RectI> const& scissorRect) = 0;
  virtual std::optional<RectI> scissorRect() const = 0;

  [[nodiscard]] virtual std::expected<TexturePtr, StarException> createTexture(Image const& texture,
      TextureAddressing addressing = TextureAddressing::Clamp,
      TextureFiltering filtering = TextureFiltering::Nearest) = 0;
  virtual void setSizeLimitEnabled(bool enabled) = 0;
  virtual void setMultiTexturingEnabled(bool enabled) = 0;
  [[nodiscard]] virtual std::expected<void, StarException> setMultiSampling(unsigned multiSampling) = 0;
  [[nodiscard]] virtual std::expected<void, StarException> setMainHDR(bool enabled) = 0;
  [[nodiscard]] virtual std::expected<std::shared_ptr<TextureGroup>, StarException> createTextureGroup(TextureGroupSize size = TextureGroupSize::Medium, TextureFiltering filtering = TextureFiltering::Nearest) = 0;
  [[nodiscard]] virtual std::expected<std::shared_ptr<RenderBuffer>, StarException> createRenderBuffer() = 0;

  virtual List<RenderPrimitive>& immediatePrimitives() = 0;
  virtual void render(RenderPrimitive primitive) = 0;
  [[nodiscard]] virtual std::expected<void, StarException> renderBuffer(std::shared_ptr<RenderBuffer> const& renderBuffer, Mat3F const& transformation = Mat3F::identity()) = 0;

  [[nodiscard]] virtual std::expected<void, StarException> flush(Mat3F const& transformation = Mat3F::identity()) = 0;
};

}

export module star.renderer;

export namespace Star {
  using ::Star::RendererExceptionTag;
  using ::Star::RendererException;
  using ::Star::Texture;
  using ::Star::TexturePtr;
  using ::Star::TextureGroup;
  using ::Star::RenderBuffer;
  using ::Star::Renderer;
  using ::Star::TextureAddressing;
  using ::Star::TextureAddressingNames;
  using ::Star::TextureFiltering;
  using ::Star::TextureFilteringNames;
  using ::Star::TextureGroupSize;
  using ::Star::RenderVertex;
  using ::Star::RenderTriangle;
  using ::Star::RenderQuad;
  using ::Star::RenderPoly;
  using ::Star::renderTexturedRect;
  using ::Star::renderFlatRect;
  using ::Star::renderFlatPoly;
  using ::Star::RenderPrimitive;
  using ::Star::DetachedRenderPrimitive;
  using ::Star::DetachedRenderGeometry;
  using ::Star::RenderEffectParameter;
}
