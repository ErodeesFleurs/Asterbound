import star.format;
import star.json;
import star.json_extra;
import star.list;
import star.rect;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.string;
import star.poly;
import star.color;
import star.data_stream_extra;
import star.ordered_map;
import star.bi_map;
import star.io_device;
import star.ref_ptr;
import star.vector;
import star.game_types;
import star.logging;
import star.listener;
import std;

import star.drawable;


import star.image_metadata_database;

import star.matrix3;
import star.line;
namespace Star {

static std::expected<std::reference_wrapper<Drawable::ImagePart>, StarException> commit_centered_directives(
    Drawable::ImagePart& part, ImageMetadataDatabase const& image_metadata, AssetPath&& candidate) {
  auto old_size = image_metadata.image_size(part.image);
  if (!old_size)
    return std::unexpected(std::move(old_size.error()));
  auto new_size = image_metadata.image_size(candidate);
  if (!new_size)
    return std::unexpected(std::move(new_size.error()));
  if (*old_size != *new_size)
    part.transformation *= Mat3F::translation((Vec2F(*old_size) - Vec2F(*new_size)) / 2);
  part.image.directives = std::move(candidate.directives);
  return std::ref(part);
}

template <std::invocable<DirectivesGroup&> Append>
static std::expected<std::reference_wrapper<Drawable::ImagePart>, StarException> append_image_directives(
    Drawable::ImagePart& part, ImageMetadataDatabase const& image_metadata, bool keep_center, Append append) {
  if (!keep_center) {
    append(part.image.directives);
    return std::ref(part);
  }
  auto candidate = part.image;
  append(candidate.directives);
  return commit_centered_directives(part, image_metadata, std::move(candidate));
}

std::expected<std::reference_wrapper<Drawable::ImagePart>, StarException> Drawable::ImagePart::add_directives(
    ImageMetadataDatabase const& image_metadata, Directives const& directives, bool keep_center) {
  if (directives.empty())
    return std::ref(*this);
  return append_image_directives(*this, image_metadata, keep_center,
      [&](DirectivesGroup& target) { target += directives; });
}

std::expected<std::reference_wrapper<Drawable::ImagePart>, StarException> Drawable::ImagePart::add_directives_group(
    ImageMetadataDatabase const& image_metadata, DirectivesGroup const& directives, bool keep_center) {
  if (directives.empty())
    return std::ref(*this);
  return append_image_directives(*this, image_metadata, keep_center, [&](DirectivesGroup& target) {
    // An uncentered self-append may reallocate the source list.
    auto const& source = directives.list();
    for (std::size_t i = 0, count = source.size(); i < count; ++i)
      target += source[i];
  });
}

std::expected<std::reference_wrapper<Drawable::ImagePart>, StarException> Drawable::ImagePart::remove_directives(
    ImageMetadataDatabase const& image_metadata, bool keep_center) {
  if (image.directives.empty())
    return std::ref(*this);
  if (!keep_center) {
    image.directives.clear();
    return std::ref(*this);
  }
  return commit_centered_directives(*this, image_metadata, {image.base_path, image.sub_path, {}});
}

Drawable Drawable::make_line(Line2F const& line, float line_width, Color const& color, Vec2F const& position) {
  Drawable drawable;
  drawable.part = LinePart{line, line_width, {}};
  drawable.color = color;
  drawable.position = position;

  return drawable;
}

Drawable Drawable::make_poly(PolyF poly, Color const& color, Vec2F const& position) {
  Drawable drawable;
  drawable.part = PolyPart{std::move(poly)};
  drawable.color = color;
  drawable.position = position;

  return drawable;
}

static std::expected<Drawable, StarException> make_image_drawable(ImageMetadataDatabase const& image_metadata,
    AssetPath&& image, float pixel_size, bool centered, Vec2F const& position, std::span<Directives const> processing, Color const& color) {
  Mat3F transformation = Mat3F::identity();
  Vec2F original_size;
  if (centered || !processing.empty()) {
    auto size = image_metadata.image_size(image);
    if (!size)
      return std::unexpected(std::move(size.error()));
    original_size = Vec2F(*size);
    if (centered)
      transformation.translate(-original_size / 2);
  }

  if (pixel_size != 1.0f)
    transformation.scale(pixel_size);

  if (!processing.empty()) {
    for (auto const& directives : processing)
      image.directives += directives;
    auto processed_size = image_metadata.image_size(image);
    if (!processed_size)
      return std::unexpected(std::move(processed_size.error()));
    transformation *= Mat3F::translation((original_size - Vec2F(*processed_size)) / 2);
  }

  Drawable drawable;
  drawable.part = Drawable::ImagePart{std::move(image), std::move(transformation)};
  drawable.position = position;
  drawable.color = color;

  return drawable;
}

std::expected<Drawable, StarException> Drawable::make_image(ImageMetadataDatabase const& image_metadata, AssetPath image, float pixel_size, bool centered, Vec2F const& position, Color const& color) {
  return make_image_drawable(image_metadata, std::move(image), pixel_size, centered, position, {}, color);
}

std::expected<Drawable, StarException> Drawable::make_animation_image(ImageMetadataDatabase const& image_metadata,
    AssetPath image, float pixel_size, bool centered, Vec2F const& position, DirectivesGroup const& processing, Color const& color) {
  return make_image_drawable(image_metadata, std::move(image), pixel_size, centered, position, processing.list(), color);
}

void Drawable::translate_all(std::span<Drawable> drawables, Vec2F const& translation) {
  for (auto& drawable : drawables)
    drawable.translate(translation);
}

void Drawable::rotate_all(std::span<Drawable> drawables, float rotation, Vec2F const& rotation_center) {
  for (auto& drawable : drawables)
    drawable.rotate(rotation, rotation_center);
}

void Drawable::scale_all(std::span<Drawable> drawables, float scaling, Vec2F const& scale_center) {
  scale_all(drawables, Vec2F::filled(scaling), scale_center);
}

void Drawable::scale_all(std::span<Drawable> drawables, Vec2F const& scaling, Vec2F const& scale_center) {
  for (auto& drawable : drawables)
    drawable.scale(scaling, scale_center);
}

void Drawable::transform_all(std::span<Drawable> drawables, Mat3F const& transformation) {
  for (auto& drawable : drawables)
    drawable.transform(transformation);
}

void Drawable::rebase_all(std::span<Drawable> drawables, Vec2F const& new_base) {
  for (auto& drawable : drawables)
    drawable.rebase(new_base);
}

[[nodiscard]] std::expected<RectF, StarException> Drawable::bound_box_all(ImageMetadataDatabase const& image_metadata, std::span<Drawable const> drawables, bool crop_images) {
  RectF result = RectF::null();
  for (auto const& drawable : drawables) {
    auto bounds = drawable.bound_box(image_metadata, crop_images);
    if (!bounds)
      return std::unexpected(std::move(bounds.error()));
    result.combine(*bounds);
  }
  return result;
}

Drawable::Drawable()
  : color(Color::palette_white), fullbright(false) {}

std::expected<Drawable, StarException> Drawable::create(ImageMetadataDatabase const& image_metadata, Json const& json) {
  Drawable drawable;
  auto line = json.opt("line");
  if (!line)
    return std::unexpected(std::move(line.error()));
  if (*line) {
    auto geometry = json_to_line2f(**line);
    if (!geometry)
      return std::unexpected(std::move(geometry.error()));
    auto width = json.get_float("width");
    if (!width)
      return std::unexpected(std::move(width.error()));
    LinePart part{*geometry, *width, {}};
    auto end_color = json.opt("endColor");
    if (!end_color)
      return std::unexpected(std::move(end_color.error()));
    if (*end_color) {
      auto value = json_to_color(**end_color);
      if (!value)
        return std::unexpected(std::move(value.error()));
      part.end_color = *value;
    }
    drawable.part = std::move(part);
  } else {
    auto poly = json.opt("poly");
    if (!poly)
      return std::unexpected(std::move(poly.error()));
    if (*poly) {
      auto geometry = json_to_polyf(**poly);
      if (!geometry)
        return std::unexpected(std::move(geometry.error()));
      drawable.part = PolyPart{std::move(*geometry)};
    } else {
      auto image = json.opt("image");
      if (!image)
        return std::unexpected(std::move(image.error()));
      if (*image) {
        auto image_string = (**image).to_string();
        if (!image_string)
          return std::unexpected(std::move(image_string.error()));
        AssetPath image_path(std::move(*image_string));
        Mat3F transformation = Mat3F::identity();
        auto transformation_config = json.opt("transformation");
        if (!transformation_config)
          return std::unexpected(std::move(transformation_config.error()));
        if (*transformation_config) {
          auto matrix = json_to_mat3f(**transformation_config);
          if (!matrix)
            return std::unexpected(std::move(matrix.error()));
          transformation = *matrix;
        } else {
          auto centered = json.get_bool("centered", true);
          if (!centered)
            return std::unexpected(std::move(centered.error()));
          if (*centered) {
            auto size = image_metadata.image_size(image_path);
            if (!size)
              return std::unexpected(std::move(size.error()));
            transformation.translate(-Vec2F(*size) / 2);
          }
          auto rotation = json.opt_float("rotation");
          if (!rotation)
            return std::unexpected(std::move(rotation.error()));
          if (*rotation)
            transformation.rotate(**rotation);
          auto mirrored = json.get_bool("mirrored", false);
          if (!mirrored)
            return std::unexpected(std::move(mirrored.error()));
          if (*mirrored)
            transformation.scale(Vec2F(-1, 1));
          auto scale = json.opt_float("scale");
          if (!scale)
            return std::unexpected(std::move(scale.error()));
          if (*scale)
            transformation.scale(**scale);
        }
        drawable.part = ImagePart{std::move(image_path), std::move(transformation)};
      }
    }
  }
  auto position = json.opt("position");
  if (!position)
    return std::unexpected(std::move(position.error()));
  if (*position) {
    auto value = json_to_vec2f(**position);
    if (!value)
      return std::unexpected(std::move(value.error()));
    drawable.position = *value;
  }
  auto color = json.opt("color");
  if (!color)
    return std::unexpected(std::move(color.error()));
  if (*color) {
    auto value = json_to_color(**color);
    if (!value)
      return std::unexpected(std::move(value.error()));
    drawable.color = *value;
  }
  auto fullbright = json.get_bool("fullbright", false);
  if (!fullbright)
    return std::unexpected(std::move(fullbright.error()));
  drawable.fullbright = *fullbright;
  return drawable;
}

std::expected<Json, StarException> Drawable::to_json() const {
  JsonObject json;
  if (std::holds_alternative<LinePart>(part)) {
    auto const& line = std::get<LinePart>(part);
    json.set("line", json_from_line2f(line.line));
    json.set("width", line.width);
    if (line.end_color)
      json.set("endColor", json_from_color(*line.end_color));
  } else if (std::holds_alternative<PolyPart>(part)) {
    json.set("poly", json_from_polyf(std::get<PolyPart>(part).poly));
  } else if (std::holds_alternative<ImagePart>(part)) {
    auto const& image = std::get<ImagePart>(part);
    json.set("image", AssetPath::join(image.image));
    json.set("transformation", json_from_mat3f(image.transformation));
  }
  json.set("position", json_from_vec2f(position));
  json.set("color", json_from_color(color));
  json.set("fullbright", fullbright);
  return json;
}

void Drawable::translate(Vec2F const& translation) {
  position += translation;
}

void Drawable::rotate(float rotation, Vec2F const& rotation_center) {
  if (std::holds_alternative<LinePart>(part))
    std::get<LinePart>(part).line.rotate(rotation);
  else if (std::holds_alternative<PolyPart>(part))
    std::get<PolyPart>(part).poly.rotate(rotation);
  else if (std::holds_alternative<ImagePart>(part))
    std::get<ImagePart>(part).transformation.rotate(rotation);

  position = (position - rotation_center).rotate(rotation) + rotation_center;
}

void Drawable::scale(float scaling, Vec2F const& scale_center) {
  scale(Vec2F::filled(scaling), scale_center);
}

void Drawable::scale(Vec2F const& scaling, Vec2F const& scale_center) {
  if (std::holds_alternative<LinePart>(part))
    std::get<LinePart>(part).line.scale(scaling);
  else if (std::holds_alternative<PolyPart>(part))
    std::get<PolyPart>(part).poly.scale(scaling);
  else if (std::holds_alternative<ImagePart>(part))
    std::get<ImagePart>(part).transformation.scale(scaling);

  position = (position - scale_center).piecewise_multiply(scaling) + scale_center;
}

void Drawable::transform(Mat3F const& transformation) {
  Vec2F local_translation = transformation.transform_point(Vec2F());
  Mat3F local_transform = Mat3F::translation(-local_translation) * transformation;

  if (std::holds_alternative<LinePart>(part))
    std::get<LinePart>(part).line.transform(local_transform);
  else if (std::holds_alternative<PolyPart>(part))
    std::get<PolyPart>(part).poly.transform(local_transform);
  else if (std::holds_alternative<ImagePart>(part)) {
    auto& image = std::get<ImagePart>(part);
    image.transformation = local_transform * image.transformation;
  }

  position = transformation.transform_point(position);
}

void Drawable::rebase(Vec2F const& new_base) {
  if (std::holds_alternative<LinePart>(part))
    std::get<LinePart>(part).line.translate(position - new_base);
  else if (std::holds_alternative<PolyPart>(part))
    std::get<PolyPart>(part).poly.translate(position - new_base);
  else if (std::holds_alternative<ImagePart>(part))
    std::get<ImagePart>(part).transformation.translate(position - new_base);

  position = new_base;
}

std::expected<RectF, StarException> Drawable::bound_box(ImageMetadataDatabase const& image_metadata, bool crop_images) const {
  RectF bounds = RectF::null();
  if (std::holds_alternative<LinePart>(part)) {
    auto const& line = std::get<LinePart>(part).line;
    bounds.combine(line.min());
    bounds.combine(line.max());

  } else if (std::holds_alternative<PolyPart>(part)) {
    bounds.combine(std::get<PolyPart>(part).poly.bounding_box());

  } else if (std::holds_alternative<ImagePart>(part)) {
    auto const& image = std::get<ImagePart>(part);
    RectF image_region = RectF::null();
    if (crop_images) {
      auto non_empty_region = image_metadata.non_empty_region(image.image);
      if (!non_empty_region)
        return std::unexpected(std::move(non_empty_region.error()));
      if (!non_empty_region->is_null())
        image_region = RectF(*non_empty_region);
    } else {
      auto size = image_metadata.image_size(image.image);
      if (!size)
        return std::unexpected(std::move(size.error()));
      image_region = RectF::with_size(Vec2F(), Vec2F(*size));
    }

    if (!image_region.is_null()) {
      bounds.combine(image.transformation.transform_point(Vec2F(image_region.x_min(), image_region.y_min())));
      bounds.combine(image.transformation.transform_point(Vec2F(image_region.x_max(), image_region.y_min())));
      bounds.combine(image.transformation.transform_point(Vec2F(image_region.x_min(), image_region.y_max())));
      bounds.combine(image.transformation.transform_point(Vec2F(image_region.x_max(), image_region.y_max())));
    }
  }

  if (!bounds.is_null())
    bounds.translate(position);

  return bounds;
}

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, Drawable::LinePart& line) {
  return ds >> line.line >> line.width;
}

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, Drawable::LinePart const& line) {
  return ds << line.line << line.width;
}

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, Drawable::PolyPart& poly) {
  return ds >> poly.poly;
}

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, Drawable::PolyPart const& poly) {
  return ds << poly.poly;
}

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, Drawable::ImagePart& image) {
  return ds >> image.image >> image.transformation;
}

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, Drawable::ImagePart const& image) {
  return ds << image.image << image.transformation;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, Drawable& drawable) {
  return ds >> drawable.part >> drawable.position >> drawable.color >> drawable.fullbright;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, Drawable const& drawable) {
  return ds << drawable.part << drawable.position << drawable.color << drawable.fullbright;
}

}
