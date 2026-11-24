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
    // Local/JSON gradient; legacy network packets carry only the start color.
    std::optional<Color> end_color;
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
    [[nodiscard]] std::expected<std::reference_wrapper<ImagePart>, StarException> add_directives(
        ImageMetadataDatabase const& image_metadata, Directives const& directives, bool keep_center = false);
    [[nodiscard]] std::expected<std::reference_wrapper<ImagePart>, StarException> add_directives_group(
        ImageMetadataDatabase const& image_metadata, DirectivesGroup const& directives, bool keep_center = false);

    // Remove directives from this ImagePart, while optionally keeping the
    // transformed center of the image the same if the directives change the
    // image size.
    [[nodiscard]] std::expected<std::reference_wrapper<ImagePart>, StarException> remove_directives(
        ImageMetadataDatabase const& image_metadata, bool keep_center = false);
  };

  static Drawable make_line(Line2F const& line, float line_width, Color const& color, Vec2F const& position = Vec2F());
  static Drawable make_poly(PolyF poly, Color const& color, Vec2F const& position = Vec2F());
  [[nodiscard]] static std::expected<Drawable, StarException> make_image(ImageMetadataDatabase const& image_metadata, AssetPath image, float pixel_size, bool centered, Vec2F const& position, Color const& color = Color::palette_white);
  [[nodiscard]] static std::expected<Drawable, StarException> make_animation_image(ImageMetadataDatabase const& image_metadata,
      AssetPath image, float pixel_size, bool centered, Vec2F const& position, DirectivesGroup const& processing, Color const& color);

  static void translate_all(std::span<Drawable> drawables, Vec2F const& translation);
  static void rotate_all(std::span<Drawable> drawables, float rotation, Vec2F const& rotation_center = Vec2F());
  static void scale_all(std::span<Drawable> drawables, float scaling, Vec2F const& scale_center = Vec2F());
  static void scale_all(std::span<Drawable> drawables, Vec2F const& scaling, Vec2F const& scale_center = Vec2F());
  static void transform_all(std::span<Drawable> drawables, Mat3F const& transformation);
  static void rebase_all(std::span<Drawable> drawables, Vec2F const& new_base = Vec2F());
  [[nodiscard]] static std::expected<RectF, StarException> bound_box_all(ImageMetadataDatabase const& image_metadata, std::span<Drawable const> drawables, bool crop_images);

  Drawable();
  [[nodiscard]] static std::expected<Drawable, StarException> create(ImageMetadataDatabase const& image_metadata, Json const& json);

  [[nodiscard]] std::expected<Json, StarException> to_json() const;

  void translate(Vec2F const& translation);
  void rotate(float rotation, Vec2F const& rotation_center = Vec2F());
  void scale(float scaling, Vec2F const& scale_center = Vec2F());
  void scale(Vec2F const& scaling, Vec2F const& scale_center = Vec2F());
  void transform(Mat3F const& transformation);

  // Change the base position of a drawable without changing the position that
  // the drawable appears, useful to re-base a set of drawables at the same
  // position so that they will be transformed together with minimal drift
  // between them.
  void rebase(Vec2F const& new_base = Vec2F());

  [[nodiscard]] std::expected<RectF, StarException> bound_box(ImageMetadataDatabase const& image_metadata, bool crop_images) const;

  bool is_line() const;
  LinePart& line_part();
  LinePart const& line_part() const;

  bool is_poly() const;
  PolyPart& poly_part();
  PolyPart const& poly_part() const;

  bool is_image() const;
  ImagePart& image_part();
  ImagePart const& image_part() const;

  std::variant<std::monostate, LinePart, PolyPart, ImagePart> part;

  Vec2F position;
  Color color;
  bool fullbright;
  bool operator==(Drawable const&) const = default;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, Drawable& drawable);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, Drawable const& drawable);

inline bool Drawable::is_line() const {
  return std::holds_alternative<LinePart>(part);
}

inline Drawable::LinePart& Drawable::line_part() {
  return std::get<LinePart>(part);
}

inline Drawable::LinePart const& Drawable::line_part() const {
  return std::get<LinePart>(part);
}

inline bool Drawable::is_poly() const {
  return std::holds_alternative<PolyPart>(part);
}

inline Drawable::PolyPart& Drawable::poly_part() {
  return std::get<PolyPart>(part);
}

inline Drawable::PolyPart const& Drawable::poly_part() const {
  return std::get<PolyPart>(part);
}

inline bool Drawable::is_image() const {
  return std::holds_alternative<ImagePart>(part);
}

inline Drawable::ImagePart& Drawable::image_part() {
  return std::get<ImagePart>(part);
}

inline Drawable::ImagePart const& Drawable::image_part() const {
  return std::get<ImagePart>(part);
}

}

export module star.drawable;

export namespace Star {
  using ::Star::Drawable;
}
