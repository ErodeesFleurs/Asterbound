module;
import star.format;
import std;
import star.json;

import star.string;
import star.data_stream;
import star.poly;
import star.color;
import star.list;
import star.rect;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;

import star.vector;
import star.matrix3;
import star.line;
namespace Star {

class ImageMetadataDatabase;

struct Drawable {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, Drawable& drawable);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, Drawable const& drawable);

  struct LinePart {
    Line2F line;
    float width;
    std::optional<Color> endColor;
    bool operator==(LinePart const&) const = default;
  };

  struct PolyPart {
    PolyF poly;
    bool operator==(PolyPart const&) const = default;
  };

  struct ImagePart {
    AssetPath image;
    // Transformation of the image in pixel space (0, 0) - (width, height) to
    // the final drawn space
    Mat3F transformation;
    bool operator==(ImagePart const&) const = default;

    // Add directives to this ImagePart, while optionally keeping the
    // transformed center of the image the same if the directives change the
    // image size.
    [[nodiscard]] std::expected<std::reference_wrapper<ImagePart>, StarException> addDirectives(ImageMetadataDatabase const& imageMetadata, Directives const& directives, bool keepImageCenterPosition = false);
    [[nodiscard]] std::expected<std::reference_wrapper<ImagePart>, StarException> addDirectivesGroup(ImageMetadataDatabase const& imageMetadata, DirectivesGroup const& directivesGroup, bool keepImageCenterPosition = false);

    // Remove directives from this ImagePart, while optionally keeping the
    // transformed center of the image the same if the directives change the
    // image size.
    [[nodiscard]] std::expected<std::reference_wrapper<ImagePart>, StarException> removeDirectives(ImageMetadataDatabase const& imageMetadata, bool keepImageCenterPosition = false);
  };

  static Drawable makeLine(Line2F const& line, float lineWidth, Color const& color, Vec2F const& position = Vec2F());
  static Drawable makePoly(PolyF poly, Color const& color, Vec2F const& position = Vec2F());
  [[nodiscard]] static std::expected<Drawable, StarException> makeImage(ImageMetadataDatabase const& imageMetadata, AssetPath image, float pixelSize, bool centered, Vec2F const& position, Color const& color = Color::palette_white);
  [[nodiscard]] static std::expected<Drawable, StarException> make_animation_image(ImageMetadataDatabase const& image_metadata,
      AssetPath image, float pixel_size, bool centered, Vec2F const& position, DirectivesGroup const& processing, Color const& color);

  template <typename DrawablesContainer>
  static void translateAll(DrawablesContainer& drawables, Vec2F const& translation);

  template <typename DrawablesContainer>
  static void rotateAll(DrawablesContainer& drawables, float rotation, Vec2F const& rotationCenter = Vec2F());

  template <typename DrawablesContainer>
  static void scaleAll(DrawablesContainer& drawables, float scaling, Vec2F const& scaleCenter = Vec2F());

  template <typename DrawablesContainer>
  static void scaleAll(DrawablesContainer& drawables, Vec2F const& scaling, Vec2F const& scaleCenter = Vec2F());

  template <typename DrawablesContainer>
  static void transformAll(DrawablesContainer& drawables, Mat3F const& transformation);

  template <typename DrawablesContainer>
  static void rebaseAll(DrawablesContainer& drawables, Vec2F const& newBase = Vec2F());

  template <typename DrawablesContainer>
  [[nodiscard]] static std::expected<RectF, StarException> boundBoxAll(ImageMetadataDatabase const& imageMetadata, DrawablesContainer const& drawables, bool cropImages);

  Drawable();
  [[nodiscard]] static std::expected<Drawable, StarException> create(ImageMetadataDatabase const& imageMetadata, Json const& json);

  [[nodiscard]] std::expected<Json, StarException> toJson() const;

  void translate(Vec2F const& translation);
  void rotate(float rotation, Vec2F const& rotationCenter = Vec2F());
  void scale(float scaling, Vec2F const& scaleCenter = Vec2F());
  void scale(Vec2F const& scaling, Vec2F const& scaleCenter = Vec2F());
  void transform(Mat3F const& transformation);

  // Change the base position of a drawable without changing the position that
  // the drawable appears, useful to re-base a set of drawables at the same
  // position so that they will be transformed together with minimal drift
  // between them.
  void rebase(Vec2F const& newBase = Vec2F());

  [[nodiscard]] std::expected<RectF, StarException> boundBox(ImageMetadataDatabase const& imageMetadata, bool cropImages) const;

  bool isLine() const;
  LinePart& linePart();
  LinePart const& linePart() const;

  bool isPoly() const;
  PolyPart& polyPart();
  PolyPart const& polyPart() const;

  bool isImage() const;
  ImagePart& imagePart();
  ImagePart const& imagePart() const;

  std::variant<std::monostate, LinePart, PolyPart, ImagePart> part;

  Vec2F position;
  Color color;
  bool fullbright;
  bool operator==(Drawable const&) const = default;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, Drawable& drawable);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, Drawable const& drawable);

template <typename DrawablesContainer>
void Drawable::translateAll(DrawablesContainer& drawables, Vec2F const& translation) {
  for (auto& drawable : drawables)
    drawable.translate(translation);
}

template <typename DrawablesContainer>
void Drawable::rotateAll(DrawablesContainer& drawables, float rotation, Vec2F const& rotationCenter) {
  for (auto& drawable : drawables)
    drawable.rotate(rotation, rotationCenter);
}

template <typename DrawablesContainer>
void Drawable::scaleAll(DrawablesContainer& drawables, float scaling, Vec2F const& scaleCenter) {
  for (auto& drawable : drawables)
    drawable.scale(scaling, scaleCenter);
}

template <typename DrawablesContainer>
void Drawable::scaleAll(DrawablesContainer& drawables, Vec2F const& scaling, Vec2F const& scaleCenter) {
  for (auto& drawable : drawables)
    drawable.scale(scaling, scaleCenter);
}

template <typename DrawablesContainer>
void Drawable::transformAll(DrawablesContainer& drawables, Mat3F const& transformation) {
  for (auto& drawable : drawables)
    drawable.transform(transformation);
}

template <typename DrawablesContainer>
void Drawable::rebaseAll(DrawablesContainer& drawables, Vec2F const& newBase) {
  for (auto& drawable : drawables)
    drawable.rebase(newBase);
}

template <typename DrawablesContainer>
[[nodiscard]] std::expected<RectF, StarException> Drawable::boundBoxAll(ImageMetadataDatabase const& imageMetadata, DrawablesContainer const& drawables, bool cropImages) {
  RectF boundBox = RectF::null();
  for (auto const& drawable : drawables) {
    auto bounds = drawable.boundBox(imageMetadata, cropImages);
    if (!bounds)
      return std::unexpected(std::move(bounds.error()));
    boundBox.combine(*bounds);
  }
  return boundBox;
}

inline bool Drawable::isLine() const {
  return std::holds_alternative<LinePart>(part);
}

inline Drawable::LinePart& Drawable::linePart() {
  return std::get<LinePart>(part);
}

inline Drawable::LinePart const& Drawable::linePart() const {
  return std::get<LinePart>(part);
}

inline bool Drawable::isPoly() const {
  return std::holds_alternative<PolyPart>(part);
}

inline Drawable::PolyPart& Drawable::polyPart() {
  return std::get<PolyPart>(part);
}

inline Drawable::PolyPart const& Drawable::polyPart() const {
  return std::get<PolyPart>(part);
}

inline bool Drawable::isImage() const {
  return std::holds_alternative<ImagePart>(part);
}

inline Drawable::ImagePart& Drawable::imagePart() {
  return std::get<ImagePart>(part);
}

inline Drawable::ImagePart const& Drawable::imagePart() const {
  return std::get<ImagePart>(part);
}

}

export module star.drawable;

export namespace Star {
  using ::Star::Drawable;
}
