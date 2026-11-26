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
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<InteractiveEntity, Entity>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<InteractiveEntity>(); }

  // Interaction always takes place on the *server*, whether the interactive
  // entity is master or slave there.
  [[nodiscard]] virtual std::expected<InteractAction, StarException> interact(InteractRequest const& request) = 0;

  // Defaults to meta_bound_box
  virtual RectF interactive_bound_box() const;

  // Defaults to true
  virtual bool is_interactive() const;

  // Defaults to empty
  [[nodiscard]] virtual std::expected<List<QuestArcDescriptor>, StarException> offered_quests() const;
  [[nodiscard]] virtual std::expected<StringSet, StarException> turn_in_quests() const;

  // Defaults to position()
  [[nodiscard]] virtual std::expected<Vec2F, StarException> quest_indicator_position() const;
};

}

export module star.interactive_entity;

export namespace Star {
  using ::Star::InteractiveEntity;
}
