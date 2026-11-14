module;
import std;
import star.format;
import star.type_identity;
import star.string;
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

class InspectableEntity : public virtual Entity {
public:
  void const* query_type(TypeKey key) const noexcept override {
    return query_type_of<InspectableEntity, Entity>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<InspectableEntity>(); }

  // Default implementation returns true
  virtual bool inspectable() const;

  // If this entity can be entered into the player log, will return the log
  // identifier.
  [[nodiscard]] virtual std::expected<std::optional<String>, StarException> inspectionLogName() const;

  // Long description to display when inspected, if any
  [[nodiscard]] virtual std::expected<std::optional<String>, StarException> inspectionDescription(String const& species) const;
};

inline bool InspectableEntity::inspectable() const {
  return true;
}

[[nodiscard]] inline std::expected<std::optional<String>, StarException> InspectableEntity::inspectionLogName() const {
  return {};
}

[[nodiscard]] inline std::expected<std::optional<String>, StarException> InspectableEntity::inspectionDescription(String const&) const {
  return {};
}

}

export module star.inspectable_entity;

export namespace Star {
  using ::Star::InspectableEntity;
}
