module;
import std;
import star.format;
import star.type_identity;
import star.json;
import star.audio;
import star.thread;
import star.list;
import star.map;
import star.set;
import star.bi_map;
import star.color;
import star.string;
import star.data_stream;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.game_types;

import star.mixer;
import star.drawable;
import star.entity_rendering_types;
import star.animation;
import star.particle;

import star.light_source;
import star.entity_rendering;

namespace Star {

  class RenderableItem {
  public:
    virtual void const* query_type(TypeKey key) const noexcept { return query_type_of<RenderableItem>(this, key); }
    virtual std::string_view dynamic_type_name() const noexcept { return Star::type_name<RenderableItem>(); }

    virtual ~RenderableItem() = default;

    [[nodiscard]]
    virtual std::expected<void, StarException> render(RenderCallback& renderCallback, EntityRenderLayer renderLayer) = 0;
  };

}

export module star.renderable_item;

export namespace Star {
  using ::Star::RenderableItem;
}
