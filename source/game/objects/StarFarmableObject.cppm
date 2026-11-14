module;
import star.format;
import std;
import star.type_identity;
import star.json;
import star.game_types;
import star.id_map;
import star.random;
import star.periodic;
import star.periodic_function;
import star.casting;
import star.strong_typedef;
import star.set;
import star.color;
import star.lua;
import star.vector;
import star.data_stream;
import star.audio;
import star.thread;
import star.list;
import star.map;
import star.bi_map;
import star.string;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ordered_map;


import star.lua_components;
import star.lua_animation_component;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.entity;
import star.tile_damage;
import star.interaction_types;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.status_effect_entity;
import star.scripted_entity;
import star.chat_action;
import star.chatty_entity;
import star.wiring;
import star.wire_entity;
import star.inspectable_entity;
import star.animated_part_set;
import star.drawable;
import star.animation;
import star.particle;
import star.mixer;
import star.light_source;
import star.networked_animator;
import star.damage_types;
import star.entity_rendering_types;
import star.entity_rendering;
import star.object;

import star.game_timers;

namespace Star {
class Root;

class FarmableObject : public Object {
public:
  void const* query_type(TypeKey key) const noexcept override {
    return query_type_of<FarmableObject, Object>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<FarmableObject>(); }

  [[nodiscard]] static std::expected<std::shared_ptr<FarmableObject>, StarException> create(Root& rootContext, std::shared_ptr<ObjectConfig const> config, Json const& parameters);

  [[nodiscard]] std::expected<void, StarException> update(float dt, std::uint64_t currentStep) override;

  [[nodiscard]] std::expected<bool, StarException> damageTiles(List<Vec2I> const& position, Vec2F const& sourcePosition, TileDamage const& tileDamage) override;
  [[nodiscard]] std::expected<InteractAction, StarException> interact(InteractRequest const& request) override;

  [[nodiscard]] std::expected<bool, StarException> harvest();
  int stage() const;

protected:
  [[nodiscard]] std::expected<void, StarException> readStoredData(Json const& diskStore) override;
  [[nodiscard]] std::expected<Json, StarException> writeStoredData() const override;

private:
  explicit FarmableObject(Root& rootContext);
  [[nodiscard]] std::expected<void, StarException> enterStage(int newStage);

  int m_stage;
  int m_stageAlt;
  double m_stageEnterTime;
  double m_nextStageTime;

  SlidingWindow m_immersion;
  float m_minImmersion;
  float m_maxImmersion;

  bool m_consumeSoilMoisture;

  JsonArray m_stages;
  bool m_finalStage;
};

}

export module star.farmable_object;

export namespace Star {
  using ::Star::FarmableObject;
}
