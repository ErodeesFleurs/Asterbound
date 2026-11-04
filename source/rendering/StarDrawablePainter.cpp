import std;
import star.format;
import star.optional;
import star.json;
import star.string;
import star.data_stream;
import star.poly;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.drawable;
import star.image;
import star.bi_map;
import star.ref_ptr;
import star.renderer;
import star.listener;
import star.asset_texture_group;
import star.drawable_painter;

import star.vector;
import star.matrix3;
namespace Star {

DrawablePainter::DrawablePainter(std::shared_ptr<Renderer> renderer, std::shared_ptr<AssetTextureGroup> textureGroup) {
  m_renderer = std::move(renderer);
  m_textureGroup = std::move(textureGroup);
}

std::expected<void, StarException> DrawablePainter::drawDrawable(Drawable const& drawable) {
  return drawDrawableProjected(drawable, drawable.position, 1.0f, 1.0f);
}

std::expected<void, StarException> DrawablePainter::drawDrawableProjected(Drawable const& drawable, Vec2F position, float scale, float lineWidthScale) {
  Vec4B color = drawable.color.toRgba();
  auto& primitives = m_renderer->immediatePrimitives();

  if (std::holds_alternative<Drawable::LinePart>(drawable.part)) {
    auto const& linePart = std::get<Drawable::LinePart>(drawable.part);
    auto line = linePart.line;
    line.scale(Vec2F::filled(scale));
    line.translate(position);
    Vec2F left = Vec2F(vnorm(line.diff())).rot90() * linePart.width * lineWidthScale / 2.0f;
    
    float fullbright = drawable.fullbright ? 0.0f : 1.0f;
    auto& primitive = primitives.emplace_back(std::in_place_type_t<RenderQuad>(),
      line.min() + left,
      line.min() - left,
      line.max() - left,
      line.max() + left,
      color, fullbright);
    if (auto endColor = optionalRef(linePart.endColor)) {
      RenderQuad& quad = std::get<RenderQuad>(primitive);
      quad.c.color = quad.d.color = endColor->get().toRgba();
    }
  } else if (std::holds_alternative<Drawable::PolyPart>(drawable.part)) {
    auto const& polyPart = std::get<Drawable::PolyPart>(drawable.part);
    PolyF poly = polyPart.poly;
    poly.scale(Vec2F::filled(scale));
    poly.translate(position);

    primitives.emplace_back(std::in_place_type_t<RenderPoly>(), poly.vertexes(), color, 0.0f);

  } else if (std::holds_alternative<Drawable::ImagePart>(drawable.part)) {
    auto const& imagePart = std::get<Drawable::ImagePart>(drawable.part);
    auto loaded = m_textureGroup->loadTexture(imagePart.image);
    if (!loaded)
      return std::unexpected(std::move(loaded.error()));
    TexturePtr texture = std::move(*loaded);

    Vec2F textureSize(texture->size());
    Mat3F transformation = imagePart.transformation;
    transformation.scale(Vec2F::filled(scale));
    Vec2F lowerLeft  = { transformation[0][2] += position.x(), transformation[1][2] += position.y() };
    Vec2F lowerRight = transformation * Vec2F(textureSize.x(), 0.f);
    Vec2F upperRight = transformation * textureSize;
    Vec2F upperLeft  = transformation * Vec2F(0.f, textureSize.y());

    float param1 = drawable.fullbright ? 0.0f : 1.0f;

    primitives.emplace_back(std::in_place_type_t<RenderQuad>(), std::move(texture),
        lowerLeft,  Vec2F{0, 0},
        lowerRight, Vec2F{textureSize[0], 0},
        upperRight, Vec2F{textureSize[0], textureSize[1]},
        upperLeft,  Vec2F{0, textureSize[1]},
      color, param1);
  }
  return {};
}

void DrawablePainter::cleanup(std::int64_t textureTimeout) {
  m_textureGroup->cleanup(textureTimeout);
}

}
