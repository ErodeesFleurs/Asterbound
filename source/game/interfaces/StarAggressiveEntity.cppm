module;
import std;
import star.type_identity;
import star.json;
import star.id_map;
import star.casting;
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

class AggressiveEntity : public virtual Entity {
public:
  void const* query_type(TypeKey key) const noexcept override {
    return query_type_of<AggressiveEntity, Entity>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<AggressiveEntity>(); }

  virtual bool aggressive() const = 0;
};

}

export module star.aggressive_entity;

export namespace Star {
  using ::Star::AggressiveEntity;
}
