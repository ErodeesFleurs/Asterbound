module;
import std;
import star.format;
import star.type_identity;
import star.list;
import star.json;
import star.id_map;
import star.casting;
import star.data_stream;
import star.game_types;
import star.poly;
import star.strong_typedef;
import star.bi_map;

import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;

import star.light_source;

import star.entity;

namespace Star {

class StatusEffectEntity : public virtual Entity {
public:
  void const* query_type(TypeKey key) const noexcept override {
    return query_type_of<StatusEffectEntity, Entity>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<StatusEffectEntity>(); }

  [[nodiscard]] virtual std::expected<List<PersistentStatusEffect>, StarException> statusEffects() const = 0;
  virtual PolyF statusEffectArea() const = 0;
};

}

export module star.status_effect_entity;

export namespace Star {
  using ::Star::StatusEffectEntity;
}
