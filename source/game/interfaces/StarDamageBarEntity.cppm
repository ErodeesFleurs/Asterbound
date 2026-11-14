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

import star.portrait_entity;

namespace Star {

enum class DamageBarType : std::uint8_t {
  Default,
  None,
  Special
};
extern std::expected<EnumMap<DamageBarType>, StarException> const DamageBarTypeNames;

class DamageBarEntity : public virtual PortraitEntity {
public:
  void const* query_type(TypeKey key) const noexcept override {
    return query_type_of<DamageBarEntity, PortraitEntity>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<DamageBarEntity>(); }

  virtual float health() const = 0;
  [[nodiscard]] virtual std::expected<float, StarException> maxHealth() const = 0;
  virtual DamageBarType damageBar() const = 0;
};

}

export module star.damage_bar_entity;

export namespace Star {
  using ::Star::DamageBarEntity;
  using ::Star::DamageBarType;
  using ::Star::DamageBarTypeNames;
}
