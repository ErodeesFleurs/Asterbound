module;
import std;
import star.format;
import star.type_identity;
import star.string;
import star.list;
import star.json;
import star.id_map;
import star.game_types;
import star.casting;
import star.vector;
import star.data_stream;
import star.strong_typedef;
import star.bi_map;

import star.interaction_types;

import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;

import star.light_source;

import star.entity;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;

import star.rect;
namespace Star {

class InteractiveEntity : public virtual Entity {
public:
  void const* query_type(TypeKey key) const noexcept override {
    return query_type_of<InteractiveEntity, Entity>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<InteractiveEntity>(); }

  // Interaction always takes place on the *server*, whether the interactive
  // entity is master or slave there.
  [[nodiscard]] virtual std::expected<InteractAction, StarException> interact(InteractRequest const& request) = 0;

  // Defaults to metaBoundBox
  virtual RectF interactiveBoundBox() const;

  // Defaults to true
  virtual bool isInteractive() const;

  // Defaults to empty
  [[nodiscard]] virtual std::expected<List<QuestArcDescriptor>, StarException> offeredQuests() const;
  [[nodiscard]] virtual std::expected<StringSet, StarException> turnInQuests() const;

  // Defaults to position()
  [[nodiscard]] virtual std::expected<Vec2F, StarException> questIndicatorPosition() const;
};

}

export module star.interactive_entity;

export namespace Star {
  using ::Star::InteractiveEntity;
}
