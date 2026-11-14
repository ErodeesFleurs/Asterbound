module;
import std;
import star.format;
import star.type_identity;
import star.string;
import star.json;
import star.id_map;
import star.casting;
import star.vector;
import star.data_stream;
import star.game_types;
import star.strong_typedef;
import star.bi_map;

import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;

import star.light_source;

import star.entity;

namespace Star {

class NametagEntity : public virtual Entity {
public:
  void const* query_type(TypeKey key) const noexcept override {
    return query_type_of<NametagEntity, Entity>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<NametagEntity>(); }

  [[nodiscard]] virtual std::expected<String, StarException> nametag() const = 0;
  virtual std::optional<String> statusText() const = 0;
  virtual bool displayNametag() const = 0;
  [[nodiscard]] virtual std::expected<Vec3B, StarException> nametagColor() const = 0;
  [[nodiscard]] virtual std::expected<Vec2F, StarException> nametagOrigin() const = 0;
};

}

export module star.nametag_entity;

export namespace Star {
  using ::Star::NametagEntity;
}
