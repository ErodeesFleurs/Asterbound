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
import star.listener;
import star.asset_texture_group;

namespace Star {

class DrawablePainter {
public:
  DrawablePainter(std::shared_ptr<Renderer> renderer, std::shared_ptr<AssetTextureGroup> textureGroup);

  [[nodiscard]] std::expected<void, StarException> drawDrawable(Drawable const& drawable);
  // Project a borrowed drawable without copying its asset or directive payload.
  [[nodiscard]] std::expected<void, StarException> drawDrawableProjected(Drawable const& drawable, Vec2F position, float scale, float lineWidthScale);

  void cleanup(std::int64_t textureTimeout);

private:
  std::shared_ptr<Renderer> m_renderer;
  std::shared_ptr<AssetTextureGroup> m_textureGroup;
};

}

export module star.drawable_painter;

export namespace Star {
  using ::Star::DrawablePainter;
}
