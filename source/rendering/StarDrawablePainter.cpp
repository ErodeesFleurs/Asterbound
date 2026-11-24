import std;
import star.format;
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
import star.rect;
import star.matrix3;
namespace Star {

DrawablePainter::DrawablePainter(std::shared_ptr<Renderer> renderer, std::shared_ptr<AssetTextureGroup> texture_group)
  : m_renderer(std::move(renderer)), m_texture_group(std::move(texture_group)) {}

std::expected<void, StarException> DrawablePainter::draw_drawable(Drawable const& drawable) {
  return draw_drawable_projected(drawable, drawable.position, 1.0f, 1.0f);
}

void DrawablePainter::draw_projected_line(Drawable::LinePart const& part, Color const& color, bool fullbright, Vec2F position,
    float scale, float line_width_scale, std::optional<std::reference_wrapper<RectF const>> viewport) {
  Vec2F start = part.line.min() * scale + position;
  Vec2F end = part.line.max() * scale + position;
  Vec2F direction = end - start;
  RectF center_bounds;
  if (viewport) {
    center_bounds = RectF(start.piecewise_min(end), start.piecewise_max(end));
    // Reject distant strokes without a square root. Leave rounding slack for
    // normalization and width scaling; only the exact bound decides at the edge.
    constexpr float half_with_slack = 0.5f + 4.0f * std::numeric_limits<float>::epsilon();
    float radius = std::abs(part.width * line_width_scale) * half_with_slack + 1.0f;
    if (!viewport->get().intersects(center_bounds.padded(radius))) {
      // Relative error bounds on normalization don't apply to subnormal squares.
      float length_squared = direction.magnitude_squared();
      if (length_squared == 0.0f || length_squared >= std::numeric_limits<float>::min())
        return;
    }
  }
  Vec2F left = direction.normalized().rot90() * part.width * line_width_scale / 2.0f;
  if (viewport && !viewport->get().intersects(center_bounds.padded(left.abs())))
    return;

  auto& primitive = m_renderer->immediatePrimitives().emplace_back(std::in_place_type<RenderQuad>,
      start + left, start - left, end - left, end + left, color.to_rgba(), fullbright ? 0.0f : 1.0f);
  if (part.end_color) {
    auto& quad = std::get<RenderQuad>(primitive);
    quad.c.color = quad.d.color = part.end_color->to_rgba();
  }
}

std::expected<void, StarException> DrawablePainter::draw_drawable_projected(Drawable const& drawable, Vec2F position, float scale, float line_width_scale) {
  if (drawable.is_line()) {
    draw_projected_line(drawable.line_part(), drawable.color, drawable.fullbright, position, scale, line_width_scale, std::nullopt);
  } else if (drawable.is_poly()) {
    auto const& vertices = drawable.poly_part().poly.vertices();
    auto& primitive = m_renderer->immediatePrimitives().emplace_back(std::in_place_type<RenderPoly>);
    auto& output = std::get<RenderPoly>(primitive).vertexes;
    Vec4B color = drawable.color.to_rgba();
    output.reserve(vertices.size());
    for (auto const& vertex : vertices)
      output.emplace_back(vertex * scale + position, Vec2F(), color, 0.0f);
  } else if (drawable.is_image()) {
    auto const& image_part = drawable.image_part();
    auto loaded = m_texture_group->loadTexture(image_part.image);
    if (!loaded)
      return std::unexpected(std::move(loaded.error()));
    TexturePtr texture = std::move(*loaded);

    Vec2F texture_size(texture->size());
    Mat3F transformation = image_part.transformation;
    transformation.scale(Vec2F::filled(scale));
    Vec2F lower_left  = { transformation[0][2] += position.x(), transformation[1][2] += position.y() };
    Vec2F lower_right = transformation.transform_point(Vec2F(texture_size.x(), 0.f));
    Vec2F upper_right = transformation.transform_point(texture_size);
    Vec2F upper_left  = transformation.transform_point(Vec2F(0.f, texture_size.y()));

    float param1 = drawable.fullbright ? 0.0f : 1.0f;

    m_renderer->immediatePrimitives().emplace_back(std::in_place_type<RenderQuad>, std::move(texture),
        lower_left,  Vec2F{0, 0},
        lower_right, Vec2F{texture_size[0], 0},
        upper_right, Vec2F{texture_size[0], texture_size[1]},
        upper_left,  Vec2F{0, texture_size[1]},
      drawable.color.to_rgba(), param1);
  }
  return {};
}

void DrawablePainter::cleanup(std::int64_t texture_timeout) {
  m_texture_group->cleanup(texture_timeout);
}

}
