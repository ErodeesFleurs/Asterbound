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
import star.version;

import star.drawable;


import star.image_metadata_database;

import star.matrix3;
import star.line;
namespace Star {

std::expected<std::reference_wrapper<Drawable::ImagePart>, StarException> Drawable::ImagePart::addDirectives(ImageMetadataDatabase const& imageMetadata, Directives const& directives, bool keepImageCenterPosition) {
  if (!directives)
    return std::ref(*this);
  std::optional<Vec2F> oldSize;
  if (keepImageCenterPosition) {
    auto size = imageMetadata.imageSize(image);
    if (!size)
      return std::unexpected(std::move(size.error()));
    oldSize = Vec2F(*size);
  }
  image.directives += directives;
  if (oldSize) {
    auto size = imageMetadata.imageSize(image);
    if (!size)
      return std::unexpected(std::move(size.error()));
    transformation *= Mat3F::translation((*oldSize - Vec2F(*size)) / 2);
  }
  return std::ref(*this);
}

std::expected<std::reference_wrapper<Drawable::ImagePart>, StarException> Drawable::ImagePart::addDirectivesGroup(ImageMetadataDatabase const& imageMetadata, DirectivesGroup const& directivesGroup, bool keepImageCenterPosition) {
  if (directivesGroup.empty())
    return std::ref(*this);
  std::optional<Vec2F> oldSize;
  if (keepImageCenterPosition) {
    auto size = imageMetadata.imageSize(image);
    if (!size)
      return std::unexpected(std::move(size.error()));
    oldSize = Vec2F(*size);
  }
  for (Directives const& directives : directivesGroup.list())
    image.directives += directives;
  if (oldSize) {
    auto size = imageMetadata.imageSize(image);
    if (!size)
      return std::unexpected(std::move(size.error()));
    transformation *= Mat3F::translation((*oldSize - Vec2F(*size)) / 2);
  }
  return std::ref(*this);
}

std::expected<std::reference_wrapper<Drawable::ImagePart>, StarException> Drawable::ImagePart::removeDirectives(ImageMetadataDatabase const& imageMetadata, bool keepImageCenterPosition) {
  std::optional<Vec2F> oldSize;
  if (keepImageCenterPosition) {
    auto size = imageMetadata.imageSize(image);
    if (!size)
      return std::unexpected(std::move(size.error()));
    oldSize = Vec2F(*size);
  }
  image.directives.clear();
  if (oldSize) {
    auto size = imageMetadata.imageSize(image);
    if (!size)
      return std::unexpected(std::move(size.error()));
    transformation *= Mat3F::translation((*oldSize - Vec2F(*size)) / 2);
  }
  return std::ref(*this);
}

Drawable Drawable::makeLine(Line2F const& line, float lineWidth, Color const& color, Vec2F const& position) {
  Drawable drawable;
  drawable.part = LinePart{std::move(line), lineWidth, {}};
  drawable.color = color;
  drawable.position = position;

  return drawable;
}

Drawable Drawable::makePoly(PolyF poly, Color const& color, Vec2F const& position) {
  Drawable drawable;
  drawable.part = PolyPart{std::move(poly)};
  drawable.color = color;
  drawable.position = position;

  return drawable;
}

std::expected<Drawable, StarException> Drawable::makeImage(ImageMetadataDatabase const& imageMetadata, AssetPath image, float pixelSize, bool centered, Vec2F const& position, Color const& color) {
  Drawable drawable;
  Mat3F transformation = Mat3F::identity();
  if (centered) {
    auto size = imageMetadata.imageSize(image);
    if (!size)
      return std::unexpected(std::move(size.error()));
    transformation.translate(-Vec2F(*size) / 2);
  }

  if (pixelSize != 1.0f)
    transformation.scale(pixelSize);

  drawable.part = ImagePart{std::move(image), std::move(transformation)};
  drawable.position = position;
  drawable.color = color;

  return drawable;
}

Drawable::Drawable()
  : color(Color::White), fullbright(false) {}

std::expected<Drawable, StarException> Drawable::create(ImageMetadataDatabase const& imageMetadata, Json const& json) {
  Drawable drawable;
  auto line = json.opt("line");
  if (!line)
    return std::unexpected(std::move(line.error()));
  if (*line) {
    auto geometry = jsonToLine2F(**line);
    if (!geometry)
      return std::unexpected(std::move(geometry.error()));
    auto width = json.getFloat("width");
    if (!width)
      return std::unexpected(std::move(width.error()));
    drawable.part = LinePart{*geometry, *width, {}};
  } else {
    auto poly = json.opt("poly");
    if (!poly)
      return std::unexpected(std::move(poly.error()));
    if (*poly) {
      auto geometry = jsonToPolyF(**poly);
      if (!geometry)
        return std::unexpected(std::move(geometry.error()));
      drawable.part = PolyPart{std::move(*geometry)};
    } else {
      auto image = json.opt("image");
      if (!image)
        return std::unexpected(std::move(image.error()));
      if (*image) {
        auto imageString = (**image).toString();
        if (!imageString)
          return std::unexpected(std::move(imageString.error()));
        AssetPath imagePath(std::move(*imageString));
        Mat3F transformation = Mat3F::identity();
        auto transformationConfig = json.opt("transformation");
        if (!transformationConfig)
          return std::unexpected(std::move(transformationConfig.error()));
        if (*transformationConfig) {
          auto matrix = jsonToMat3F(**transformationConfig);
          if (!matrix)
            return std::unexpected(std::move(matrix.error()));
          transformation = *matrix;
        } else {
          auto centered = json.getBool("centered", true);
          if (!centered)
            return std::unexpected(std::move(centered.error()));
          if (*centered) {
            auto size = imageMetadata.imageSize(imagePath);
            if (!size)
              return std::unexpected(std::move(size.error()));
            transformation.translate(-Vec2F(*size) / 2);
          }
          auto rotation = json.optFloat("rotation");
          if (!rotation)
            return std::unexpected(std::move(rotation.error()));
          if (*rotation)
            transformation.rotate(**rotation);
          auto mirrored = json.getBool("mirrored", false);
          if (!mirrored)
            return std::unexpected(std::move(mirrored.error()));
          if (*mirrored)
            transformation.scale(Vec2F(-1, 1));
          auto scale = json.optFloat("scale");
          if (!scale)
            return std::unexpected(std::move(scale.error()));
          if (*scale)
            transformation.scale(**scale);
        }
        drawable.part = ImagePart{std::move(imagePath), std::move(transformation)};
      }
    }
  }
  auto position = json.opt("position");
  if (!position)
    return std::unexpected(std::move(position.error()));
  if (*position) {
    auto value = jsonToVec2F(**position);
    if (!value)
      return std::unexpected(std::move(value.error()));
    drawable.position = *value;
  }
  auto color = json.opt("color");
  if (!color)
    return std::unexpected(std::move(color.error()));
  if (*color) {
    auto value = jsonToColor(**color);
    if (!value)
      return std::unexpected(std::move(value.error()));
    drawable.color = *value;
  }
  auto fullbright = json.getBool("fullbright", false);
  if (!fullbright)
    return std::unexpected(std::move(fullbright.error()));
  drawable.fullbright = *fullbright;
  return drawable;
}

std::expected<Json, StarException> Drawable::toJson() const {
  JsonObject json;
  if (std::holds_alternative<LinePart>(part)) {
    auto const& line = std::get<LinePart>(part);
    json.set("line", jsonFromLine2F(line.line));
    json.set("width", line.width);
  } else if (std::holds_alternative<PolyPart>(part)) {
    json.set("poly", jsonFromPolyF(std::get<PolyPart>(part).poly));
  } else if (std::holds_alternative<ImagePart>(part)) {
    auto const& image = std::get<ImagePart>(part);
    json.set("image", AssetPath::join(image.image));
    json.set("transformation", jsonFromMat3F(image.transformation));
  }
  json.set("position", jsonFromVec2F(position));
  json.set("color", jsonFromColor(color));
  json.set("fullbright", fullbright);
  return json;
}

void Drawable::translate(Vec2F const& translation) {
  position += translation;
}

void Drawable::rotate(float rotation, Vec2F const& rotationCenter) {
  if (std::holds_alternative<LinePart>(part))
    std::get<LinePart>(part).line.rotate(rotation);
  else if (std::holds_alternative<PolyPart>(part))
    std::get<PolyPart>(part).poly.rotate(rotation);
  else if (std::holds_alternative<ImagePart>(part))
    std::get<ImagePart>(part).transformation.rotate(rotation);

  position = (position - rotationCenter).rotate(rotation) + rotationCenter;
}

void Drawable::scale(float scaling, Vec2F const& scaleCenter) {
  scale(Vec2F::filled(scaling), scaleCenter);
}

void Drawable::scale(Vec2F const& scaling, Vec2F const& scaleCenter) {
  if (std::holds_alternative<LinePart>(part))
    std::get<LinePart>(part).line.scale(scaling);
  else if (std::holds_alternative<PolyPart>(part))
    std::get<PolyPart>(part).poly.scale(scaling);
  else if (std::holds_alternative<ImagePart>(part))
    std::get<ImagePart>(part).transformation.scale(scaling);

  position = (position - scaleCenter).piecewise_multiply(scaling) + scaleCenter;
}

void Drawable::transform(Mat3F const& transformation) {
  Vec2F localTranslation = transformation.transformVec2(Vec2F());
  Mat3F localTransform = Mat3F::translation(-localTranslation) * transformation;

  if (std::holds_alternative<LinePart>(part))
    std::get<LinePart>(part).line.transform(localTransform);
  else if (std::holds_alternative<PolyPart>(part))
    std::get<PolyPart>(part).poly.transform(localTransform);
  else if (std::holds_alternative<ImagePart>(part)) {
    auto& image = std::get<ImagePart>(part);
    image.transformation = localTransform * image.transformation;
  }

  position = transformation.transformVec2(position);
}

void Drawable::rebase(Vec2F const& newBase) {
  if (std::holds_alternative<LinePart>(part))
    std::get<LinePart>(part).line.translate(position - newBase);
  else if (std::holds_alternative<PolyPart>(part))
    std::get<PolyPart>(part).poly.translate(position - newBase);
  else if (std::holds_alternative<ImagePart>(part))
    std::get<ImagePart>(part).transformation.translate(position - newBase);

  position = newBase;
}

std::expected<RectF, StarException> Drawable::boundBox(ImageMetadataDatabase const& imageMetadata, bool cropImages) const {
  RectF boundBox = RectF::null();
  if (std::holds_alternative<LinePart>(part)) {
    auto const& line = std::get<LinePart>(part).line;
    boundBox.combine(line.min());
    boundBox.combine(line.max());

  } else if (std::holds_alternative<PolyPart>(part)) {
    boundBox.combine(std::get<PolyPart>(part).poly.boundBox());

  } else if (std::holds_alternative<ImagePart>(part)) {
    auto const& image = std::get<ImagePart>(part);
    RectF imageRegion = RectF::null();
    if (cropImages) {
      auto nonEmptyRegion = imageMetadata.nonEmptyRegion(image.image);
      if (!nonEmptyRegion)
        return std::unexpected(std::move(nonEmptyRegion.error()));
      if (!nonEmptyRegion->isNull())
        imageRegion = RectF(*nonEmptyRegion);
    } else {
      auto size = imageMetadata.imageSize(image.image);
      if (!size)
        return std::unexpected(std::move(size.error()));
      imageRegion = RectF::withSize(Vec2F(), Vec2F(*size));
    }

    if (!imageRegion.isNull()) {
      boundBox.combine(image.transformation.transformVec2(Vec2F(imageRegion.xMin(), imageRegion.yMin())));
      boundBox.combine(image.transformation.transformVec2(Vec2F(imageRegion.xMax(), imageRegion.yMin())));
      boundBox.combine(image.transformation.transformVec2(Vec2F(imageRegion.xMin(), imageRegion.yMax())));
      boundBox.combine(image.transformation.transformVec2(Vec2F(imageRegion.xMax(), imageRegion.yMax())));
    }
  }

  if (!boundBox.isNull())
    boundBox.translate(position);

  return boundBox;
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
