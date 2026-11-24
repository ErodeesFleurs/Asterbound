module;
import std;
import star.format;
import star.json;

import star.string;
import star.data_stream;
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
import star.vector;
import star.rect;
import star.listener;
import star.asset_texture_group;

namespace Star {

class DrawablePainter {
public:
  DrawablePainter(std::shared_ptr<Renderer> renderer, std::shared_ptr<AssetTextureGroup> texture_group);

  [[nodiscard]] std::expected<void, StarException> draw_drawable(Drawable const& drawable);
  // Project a borrowed drawable without copying its asset or directive payload.
  [[nodiscard]] std::expected<void, StarException> draw_drawable_projected(Drawable const& drawable, Vec2F position, float scale, float line_width_scale);
  // World lines scale their endpoints in tile units but their strokes in pixels.
  void draw_projected_line(Drawable::LinePart const& part, Color const& color, bool fullbright, Vec2F position,
      float scale, float line_width_scale, std::optional<std::reference_wrapper<RectF const>> viewport);

  void cleanup(std::int64_t texture_timeout);

private:
  std::shared_ptr<Renderer> m_renderer;
  std::shared_ptr<AssetTextureGroup> m_texture_group;
};

}

export module star.drawable_painter;

export namespace Star {
  using ::Star::DrawablePainter;
}
