module;
import std;
import star.format;
import star.type_identity;
import star.json;
import star.id_map;
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
import star.casting;
import star.game_types;
import star.strong_typedef;
import star.bi_map;

import star.drawable;

import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;

import star.light_source;

import star.entity;

namespace Star {

class PortraitEntity : public virtual Entity {
public:
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<PortraitEntity, Entity>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<PortraitEntity>(); }

  [[nodiscard]] virtual std::expected<List<Drawable>, StarException> portrait(PortraitMode mode) const = 0;
};

}

export module star.portrait_entity;

export namespace Star {
  using ::Star::PortraitEntity;
}
