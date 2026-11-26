module;
import std;
import star.format;
import star.type_identity;
import star.json;
import star.data_stream_extra;
import star.id_map;
import star.ordered_map;
import star.lru_cache;
import star.thread;
import star.time;
import star.random;
import star.ttl_cache;
import star.casting;
import star.strong_typedef;
import star.audio;
import star.list;
import star.map;
import star.set;
import star.vector;
import star.bi_map;
import star.color;
import star.string;
import star.data_stream;
import star.poly;
import star.rect;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.game_types;
import star.message_types;
import star.rpc_promise;
import star.a_star;
import star.lua;
import star.periodic_function;

import star.net_element_basic_fields;
import star.net_element_containers;
import star.net_element_system;

import star.net_compatibility;
import star.byte_array;
namespace Star {
class Root;
class Monster;
}

import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.entity;
import star.mixer;
import star.drawable;
import star.entity_rendering_types;
import star.animation;
import star.particle;
import star.light_source;
import star.entity_rendering;
import star.tile_damage;
import star.interaction_types;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_modification;
import star.lua_root;
import star.force_regions;
import star.world;
import star.physics_entity;
import star.movement_controller;
import star.platformer_astar_types;
import star.anchorable_entity;
import star.game_timers;
import star.actor_movement_controller;
import star.animated_part_set;
import star.networked_animator;
import star.portrait_entity;
import star.damage_bar_entity;
import star.nametag_entity;
import star.aggressive_entity;
import star.scripted_entity;
import star.chat_action;
import star.chatty_entity;
import star.lua_components;
import star.lua_animation_component;
import star.lua_actor_movement_component;
import star.mobile_entity;
import star.actor_entity;
import star.monster_database;
import star.effect_emitter;

namespace Star {

class StatusController;
class BehaviorState;
class FunctionDatabase;

class Monster
  : public virtual DamageBarEntity,
    public virtual AggressiveEntity,
    public virtual ScriptedEntity,
    public virtual PhysicsEntity,
    public virtual NametagEntity,
    public virtual ChattyEntity,
    public virtual InteractiveEntity,
    public virtual ActorEntity {
  struct ConstructionKey { explicit ConstructionKey() = default; };

public:
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<Monster, DamageBarEntity, AggressiveEntity, ScriptedEntity, PhysicsEntity, NametagEntity,
                         ChattyEntity, InteractiveEntity, ActorEntity>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<Monster>(); }

  struct SkillInfo {
    String label;
    String image;
  };

  Monster(ConstructionKey, Root& root_context, MonsterVariant&& variant);

  [[nodiscard]] static std::expected<std::shared_ptr<Monster>, StarException> create(Root& root_context, MonsterVariant&& variant, std::optional<float> level = {});
  [[nodiscard]] static std::expected<std::shared_ptr<Monster>, StarException> create(Root& root_context, Json const& disk_store);

  [[nodiscard]] std::expected<Json, StarException> diskStore() const;
  [[nodiscard]] std::expected<ByteArray, StarException> net_store(NetCompatibilityRules rules = {});

  EntityType entity_type() const override;
  ClientEntityMode client_entity_mode() const override;

  [[nodiscard]] std::expected<void, StarException> init(World& world, EntityId entityId, EntityMode mode) override;
  [[nodiscard]] std::expected<void, StarException> uninit() override;

  Vec2F position() const override;
  RectF meta_bound_box() const override;

  Vec2F velocity() const;

  [[nodiscard]] Vec2F mouth_offset() const;
  [[nodiscard]] Vec2F feet_offset() const;

  RectF collision_area() const override;

  [[nodiscard]] std::expected<std::pair<ByteArray, std::uint64_t>, StarException> write_net_state(std::uint64_t fromVersion = 0, NetCompatibilityRules rules = {}) override;
  [[nodiscard]] std::expected<void, StarException> read_net_state(ByteArray const& data, float interpolation_time = 0.0f, NetCompatibilityRules rules = {}) override;

  [[nodiscard]] std::expected<void, StarException> enable_interpolation(float extrapolation_hint) override;
  [[nodiscard]] std::expected<void, StarException> disable_interpolation() override;

  [[nodiscard]] std::expected<String, StarException> name() const override;
  [[nodiscard]] std::expected<String, StarException> description() const override;

  [[nodiscard]] std::expected<List<LightSource>, StarException> light_sources() const override;

  [[nodiscard]] std::expected<std::optional<HitType>, StarException> query_hit(DamageSource const& source) const override;
  [[nodiscard]] std::expected<std::optional<PolyF>, StarException> hit_poly() const override;

  [[nodiscard]] std::expected<void, StarException> hitOther(EntityId targetEntityId, DamageRequest const& damageRequest) override;
  [[nodiscard]] std::expected<void, StarException> damaged_other(DamageNotification const& damage) override;

  [[nodiscard]] std::expected<List<DamageNotification>, StarException> apply_damage(DamageRequest const& damage) override;
  [[nodiscard]] std::expected<List<DamageNotification>, StarException> self_damage_notifications() override;

  [[nodiscard]] std::expected<List<DamageSource>, StarException> damage_sources() const override;

  [[nodiscard]] std::expected<bool, StarException> should_die();
  [[nodiscard]] std::expected<void, StarException> knockout();

  [[nodiscard]] std::expected<bool, StarException> should_destroy() const override;
  [[nodiscard]] std::expected<void, StarException> destroy(std::optional<std::reference_wrapper<RenderCallback>> render_callback) override;

  [[nodiscard]] std::expected<void, StarException> update(float dt, std::uint64_t currentStep) override;

  std::uint64_t presentation_epoch() const override;

  [[nodiscard]] std::expected<void, StarException> collect_render_events(RenderCallback& render_callback) override;

  [[nodiscard]] std::expected<void, StarException> render(RenderCallback& render_callback) override;

  [[nodiscard]] std::expected<void, StarException> render_light_sources(RenderCallback& render_callback) override;

  void setPosition(Vec2F const& pos);

  [[nodiscard]] MessageResponseResult receive_message(ConnectionId sending_connection, String const& message, JsonArray const& args) override;

  [[nodiscard]] std::expected<float, StarException> max_health() const override;
  float health() const override;
  DamageBarType damage_bar() const override;

  [[nodiscard]] std::expected<float, StarException> monster_level() const;
  [[nodiscard]] std::expected<SkillInfo, StarException> activeSkillInfo() const;

  [[nodiscard]] std::expected<List<Drawable>, StarException> portrait(PortraitMode mode) const override;
  String type_name() const;
  MonsterVariant monster_variant() const;

  std::optional<String> status_text() const override;
  bool display_nametag() const override;
  [[nodiscard]] std::expected<Vec3B, StarException> nametag_color() const override;
  [[nodiscard]] std::expected<Vec2F, StarException> nametag_origin() const override;
  [[nodiscard]] std::expected<String, StarException> nametag() const override;

  bool aggressive() const override;

  [[nodiscard]] std::expected<std::optional<LuaValue>, StarException> call_script(String const& func, LuaVariadic<LuaValue> const& args) override;
  [[nodiscard]] std::expected<std::optional<LuaValue>, StarException> eval_script(String const& code) override;

  [[nodiscard]] std::expected<Vec2F, StarException> mouth_position() const override;
  [[nodiscard]] std::expected<Vec2F, StarException> mouth_position(bool ignore_adjustments) const override;
  List<ChatAction> pull_pending_chat_actions() override;

  [[nodiscard]] std::expected<List<PhysicsForceRegion>, StarException> force_regions() const override;

  [[nodiscard]] std::expected<InteractAction, StarException> interact(InteractRequest const& request) override;
  bool is_interactive() const override;

  [[nodiscard]] std::expected<Vec2F, StarException> quest_indicator_position() const override;

  ActorMovementController& movement_controller() override;
  StatusController& status_controller() override;

  using Entity::setKeepAlive;
  using Entity::set_unique_id;

private:
  [[nodiscard]] std::expected<void, StarException> initialize(std::optional<float> level);
  [[nodiscard]] std::expected<void, StarException> loadState(Json const& diskStore);

  [[nodiscard]] Vec2F absolute_position(Vec2F relative_position) const;

  [[nodiscard]] std::expected<void, StarException> update_status(float dt);
  [[nodiscard]] std::expected<LuaCallbacks, StarException> makeMonsterCallbacks();

  [[nodiscard]] std::expected<void, StarException> add_chat_message(String const& message, String const& portrait = "");

  [[nodiscard]] std::expected<void, StarException> setupNetStates();
  [[nodiscard]] std::expected<void, StarException> getNetStates(bool initial);
  [[nodiscard]] std::expected<void, StarException> setNetStates();

  struct AnimationDamageSourceTemplate {
    struct LineCollision {
      bool enabled;
      std::int64_t bounces;
    };

    String anchor_part;
    DamageSource source;
    std::optional<LineCollision> line_collision;
  };

  struct DamageLevelPower {
    std::weak_ptr<FunctionDatabase const> functions;
    std::uint32_t level_bits = 0;
    float multiplier = 0.0f;
  };

  [[nodiscard]] std::expected<float, StarException> damage_level_power() const;

  mutable DamageLevelPower m_damage_level_power;
  mutable std::optional<DamageSource> m_touch_damage_source_template;
  // Allocated on first use and indexed in immutable variant iteration order.
  mutable List<std::optional<AnimationDamageSourceTemplate>> m_animation_damage_source_templates;

  NetElementTopGroup m_net_group;

  NetElementData<std::optional<String>> m_unique_id_net_state;
  NetElementData<EntityDamageTeam> m_team_net_state;
  MonsterVariant m_monster_variant;
  std::optional<float> m_monster_level;

  std::shared_ptr<NetworkedAnimator> m_networked_animator;
  NetworkedAnimator::DynamicTarget m_networked_animator_dynamic_target;

  std::shared_ptr<ActorMovementController> m_movement_controller;
  std::shared_ptr<StatusController> m_status_controller;

  std::shared_ptr<EffectEmitter> m_effect_emitter;

  // The set of damage source kinds that were used to kill this entity.
  StringSet m_death_damage_source_kinds;

  bool m_damage_on_touch;
  bool m_aggressive;

  bool m_knocked_out;
  double m_knockout_timer;
  String m_death_particle_burst;
  String m_death_sound;

  String m_active_skill_name;
  Json m_drop_pool;

  Vec2F m_quest_indicator_offset;

  List<std::shared_ptr<BehaviorState>> m_behaviors;
  mutable LuaActorScript m_script_component;
  LuaAnimationScript m_scripted_animator;

  NetElementData<List<PhysicsForceRegion>> m_physics_forces;

  NetElementData<std::optional<float>> m_monster_level_net_state;
  NetElementBool m_damage_on_touch_net_state;
  NetElementData<StringSet> m_animation_damage_parts;
  NetElementData<List<DamageSource>> m_damage_sources;
  NetElementData<Json> m_drop_pool_net_state;
  NetElementBool m_aggressive_net_state;
  NetElementBool m_knocked_out_net_state;
  NetElementString m_death_particle_burst_net_state;
  NetElementString m_death_sound_net_state;
  NetElementString m_active_skill_name_net_state;
  NetElementData<std::optional<String>> m_name;
  NetElementBool m_display_nametag;
  NetElementBool m_interactive;

  List<ChatAction> m_pending_chat_actions;
  NetElementEvent m_new_chat_message_event;
  NetElementString m_chat_message;
  NetElementString m_chat_portrait;

  NetElementData<DamageBarType> m_damage_bar;

  NetElementHashMap<String, Json> m_scripted_animation_parameters;
};

}


export module star.monster;

export namespace Star {
  using ::Star::Monster;
  using ::Star::StatusController;
  using ::Star::BehaviorState;
}
