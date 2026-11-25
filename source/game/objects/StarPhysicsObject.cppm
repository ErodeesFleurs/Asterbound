module;
import star.format;
import std;
import star.type_identity;
import star.json;
import star.json_extra;
import star.id_map;
import star.casting;
import star.data_stream;
import star.game_types;
import star.strong_typedef;
import star.bi_map;
import star.list;
import star.random;
import star.periodic;
import star.periodic_function;
import star.set;
import star.color;
import star.lua;
import star.audio;
import star.thread;
import star.map;
import star.string;
import star.rect;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ordered_map;


import star.lua_root;
import star.lua_components;
import star.lua_animation_component;
import star.tile_damage;
import star.interaction_types;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
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
import star.networked_animator;
import star.entity_rendering_types;
import star.entity_rendering;
import star.object;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.collision_block;
import star.force_regions;
import star.physics_entity;


import star.net_element_basic_fields;
import star.net_element_float_fields;

namespace Star {
class Root;

class PhysicsObject : public Object, public virtual PhysicsEntity {
public:
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<PhysicsObject, Object, PhysicsEntity>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<PhysicsObject>(); }

  [[nodiscard]] static std::expected<std::shared_ptr<PhysicsObject>, StarException> create(Root& rootContext, std::shared_ptr<ObjectConfig const> config, Json const& parameters = Json());

  [[nodiscard]] std::expected<void, StarException> enable_interpolation(float extrapolation_hint = 0.0f) override;
  [[nodiscard]] std::expected<void, StarException> disable_interpolation() override;

  [[nodiscard]] std::expected<void, StarException> init(World& world, EntityId entityId, EntityMode mode) override;
  [[nodiscard]] std::expected<void, StarException> uninit() override;

  [[nodiscard]] std::expected<void, StarException> update(float dt, std::uint64_t currentStep) override;

  RectF meta_bound_box() const override;

  [[nodiscard]] std::expected<List<PhysicsForceRegion>, StarException> force_regions() const override;

  std::size_t movingCollisionCount() const override;
  [[nodiscard]] std::expected<std::optional<PhysicsMovingCollision>, StarException> movingCollision(std::size_t positionIndex) const override;

private:
  explicit PhysicsObject(Root& rootContext);
  struct PhysicsForceConfig {
    PhysicsForceRegion force_region;
    NetElementBool enabled;
  };

  struct PhysicsCollisionConfig {
    PhysicsMovingCollision movingCollision;
    NetElementFloat xPosition;
    NetElementFloat yPosition;
    NetElementBool enabled;
  };

  OrderedHashMap<String, PhysicsForceConfig> m_physics_forces;
  OrderedHashMap<String, PhysicsCollisionConfig> m_physicsCollisions;

  RectF m_meta_bound_box;
};

}

export module star.physics_object;

export namespace Star {
  using ::Star::PhysicsObject;
}
