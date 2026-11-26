module;
import std;
import star.format;
import star.type_identity;
import star.json;
import star.game_types;
import star.message_types;
import star.list;
import star.vector;
import star.poly;
import star.rect;
import star.string;
import star.color;
import star.directives;
import star.lua;

import star.net_element_basic_fields;
import star.net_element_float_fields;
import star.net_element_containers;
import star.net_element_dynamic_group;
import star.net_element_system;

import star.net_compatibility;
import star.byte_array;
namespace Star {
class Root;
class Npc;
}

import star.damage_types;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.interaction_types;
import star.item_descriptor;
import star.quest_descriptor;
import star.interactive_entity;
import star.world;
import star.physics_entity;
import star.force_regions;
import star.anchorable_entity;
import star.game_timers;
import star.dance_database;
import star.actor_movement_controller;
import star.drawable;
import star.particle;
import star.networked_animator;
import star.humanoid;
import star.portrait_entity;
import star.damage_bar_entity;
import star.nametag_entity;
import star.scripted_entity;
import star.chat_action;
import star.chatty_entity;
import star.emote_entity;
import star.lounging_entities;
import star.tool_user_entity;
import star.lua_actor_movement_component;
import star.lua_animation_component;
import star.npc_database;
import star.effect_emitter;

namespace Star {

class ArmorWearer;
class ToolUser;

class Songbook;
class Item;
class RenderCallback;
class StatusController;
class BehaviorState;

class Npc
  : public virtual DamageBarEntity,
    public virtual PortraitEntity,
    public virtual NametagEntity,
    public virtual ScriptedEntity,
    public virtual ChattyEntity,
    public virtual InteractiveEntity,
    public virtual LoungingEntity,
    public virtual ToolUserEntity,
    public virtual PhysicsEntity,
    public virtual EmoteEntity {
  struct ConstructionKey { explicit ConstructionKey() = default; };

public:
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<Npc, DamageBarEntity, NametagEntity, ScriptedEntity, ChattyEntity, InteractiveEntity,
                         LoungingEntity, ToolUserEntity, PhysicsEntity, EmoteEntity>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<Npc>(); }

  Npc(ConstructionKey, Root& root_context, NpcVariant&& npc_variant);

  [[nodiscard]] static std::expected<std::shared_ptr<Npc>, StarException> create(Root& root_context, ByteArray const& net_store, NetCompatibilityRules rules = {});
  [[nodiscard]] static std::expected<std::shared_ptr<Npc>, StarException> create(Root& root_context, NpcVariant npc_variant);
  [[nodiscard]] static std::expected<std::shared_ptr<Npc>, StarException> create(Root& root_context, NpcVariant npc_variant, Json const& initial_state);

  [[nodiscard]] std::expected<Json, StarException> disk_store() const;
  [[nodiscard]] std::expected<ByteArray, StarException> net_store(NetCompatibilityRules rules = {});

  EntityType entity_type() const override;
  ClientEntityMode client_entity_mode() const override;

  [[nodiscard]] std::expected<void, StarException> init(World& world, EntityId entity_id, EntityMode mode) override;
  [[nodiscard]] std::expected<void, StarException> uninit() override;

  Vec2F position() const override;
  RectF meta_bound_box() const override;

  [[nodiscard]] std::expected<Vec2F, StarException> mouth_offset(bool ignore_adjustments = true) const;
  [[nodiscard]] std::expected<Vec2F, StarException> feet_offset() const;
  [[nodiscard]] std::expected<Vec2F, StarException> head_armor_offset() const;
  [[nodiscard]] std::expected<Vec2F, StarException> chest_armor_offset() const;
  [[nodiscard]] std::expected<Vec2F, StarException> legs_armor_offset() const;
  [[nodiscard]] std::expected<Vec2F, StarException> back_armor_offset() const;

  RectF collision_area() const override;

  [[nodiscard]] std::expected<std::pair<ByteArray, std::uint64_t>, StarException> write_net_state(std::uint64_t from_version = 0, NetCompatibilityRules rules = {}) override;
  [[nodiscard]] std::expected<void, StarException> read_net_state(ByteArray const& data, float interpolation_time = 0.0f, NetCompatibilityRules rules = {}) override;

  [[nodiscard]] std::expected<void, StarException> enable_interpolation(float extrapolation_hint = 0.0f) override;
  [[nodiscard]] std::expected<void, StarException> disable_interpolation() override;

  [[nodiscard]] std::expected<String, StarException> description() const override;
  String species() const override;
  Gender gender() const;
  String npc_type() const;

  [[nodiscard]] std::expected<Json, StarException> script_config_parameter(String const& parameter_name, Json const& default_value = Json()) const;

  [[nodiscard]] std::expected<std::optional<HitType>, StarException> query_hit(DamageSource const& source) const override;
  [[nodiscard]] std::expected<std::optional<PolyF>, StarException> hit_poly() const override;

  [[nodiscard]] std::expected<void, StarException> damaged_other(DamageNotification const& damage) override;

  [[nodiscard]] std::expected<List<DamageNotification>, StarException> apply_damage(DamageRequest const& damage) override;
  [[nodiscard]] std::expected<List<DamageNotification>, StarException> self_damage_notifications() override;

  [[nodiscard]] std::expected<bool, StarException> should_destroy() const override;
  [[nodiscard]] std::expected<void, StarException> destroy(std::optional<std::reference_wrapper<RenderCallback>> render_callback) override;

  [[nodiscard]] std::expected<void, StarException> update(float dt, std::uint64_t current_version) override;

  std::uint64_t presentation_epoch() const override;

  [[nodiscard]] std::expected<void, StarException> collect_render_events(RenderCallback& render_callback) override;

  [[nodiscard]] std::expected<void, StarException> render(RenderCallback& render_callback) override;

  [[nodiscard]] std::expected<void, StarException> render_light_sources(RenderCallback& render_callback) override;

  void set_position(Vec2F const& pos);

  [[nodiscard]] std::expected<float, StarException> max_health() const override;
  float health() const override;
  DamageBarType damage_bar() const override;

  [[nodiscard]] std::expected<List<Drawable>, StarException> portrait(PortraitMode mode) const override;
  [[nodiscard]] std::expected<String, StarException> name() const override;
  std::optional<String> status_text() const override;
  bool display_nametag() const override;
  [[nodiscard]] std::expected<Vec3B, StarException> nametag_color() const override;
  [[nodiscard]] std::expected<Vec2F, StarException> nametag_origin() const override;
  [[nodiscard]] std::expected<String, StarException> nametag() const override;

  bool aggressive() const;

  [[nodiscard]] std::expected<std::optional<LuaValue>, StarException> call_script(String const& func, LuaVariadic<LuaValue> const& args) override;
  [[nodiscard]] std::expected<std::optional<LuaValue>, StarException> eval_script(String const& code) override;

  [[nodiscard]] std::expected<Vec2F, StarException> mouth_position() const override;
  [[nodiscard]] std::expected<Vec2F, StarException> mouth_position(bool ignore_adjustments) const override;
  List<ChatAction> pull_pending_chat_actions() override;

  bool is_interactive() const override;
  [[nodiscard]] std::expected<InteractAction, StarException> interact(InteractRequest const& request) override;
  RectF interactive_bound_box() const override;

  std::optional<EntityAnchorState> lounging_in() const override;

  [[nodiscard]] std::expected<List<QuestArcDescriptor>, StarException> offered_quests() const override;
  [[nodiscard]] std::expected<StringSet, StarException> turn_in_quests() const override;
  [[nodiscard]] std::expected<Vec2F, StarException> quest_indicator_position() const override;

  [[nodiscard]] std::expected<List<LightSource>, StarException> light_sources() const override;

  [[nodiscard]] MessageResponseResult receive_message(ConnectionId sending_connection, String const& message, JsonArray const& args) override;

  [[nodiscard]] std::expected<Vec2F, StarException> arm_position(ToolHand hand, Direction facing_direction, float arm_angle, Vec2F offset = {}) const override;
  [[nodiscard]] std::expected<Vec2F, StarException> hand_offset(ToolHand hand, Direction facing_direction) const override;
  [[nodiscard]] std::expected<Vec2F, StarException> hand_position(ToolHand hand, Vec2F const& hand_offset = {}) const override;
  std::shared_ptr<Item> hand_item(ToolHand hand) const override;
  [[nodiscard]] std::expected<Vec2F, StarException> arm_adjustment() const override;
  Vec2F velocity() const override;
  [[nodiscard]] std::expected<Vec2F, StarException> aim_position() const override;
  [[nodiscard]] std::expected<float, StarException> interact_radius() const override;
  Direction facing_direction() const override;
  Direction walking_direction() const override;
  bool is_admin() const override;
  Color favorite_color() const override;
  [[nodiscard]] std::expected<float, StarException> beam_gun_radius() const override;
  [[nodiscard]] std::expected<void, StarException> add_particles(List<Particle> const& particles) override;
  [[nodiscard]] std::expected<void, StarException> add_sound(String const& sound, float volume = 1.0f, float pitch = 1.0f) override;
  [[nodiscard]] std::expected<bool, StarException> in_tool_range() const override;
  [[nodiscard]] std::expected<bool, StarException> in_tool_range(Vec2F const& position) const override;
  [[nodiscard]] std::expected<void, StarException> add_ephemeral_status_effects(List<EphemeralStatusEffect> const& status_effects) override;
  [[nodiscard]] std::expected<ActiveUniqueStatusEffectSummary, StarException> active_unique_status_effect_summary() const override;
  [[nodiscard]] std::expected<float, StarException> power_multiplier() const override;
  [[nodiscard]] std::expected<bool, StarException> full_energy() const override;
  [[nodiscard]] std::expected<float, StarException> energy() const override;
  [[nodiscard]] std::expected<bool, StarException> energy_locked() const override;
  [[nodiscard]] std::expected<bool, StarException> consume_energy(float energy) override;
  [[nodiscard]] std::expected<void, StarException> queue_ui_message(String const& message) override;
  [[nodiscard]] std::expected<bool, StarException> instrument_playing() override;
  [[nodiscard]] std::expected<void, StarException> instrument_equipped(String const& instrument_kind) override;
  [[nodiscard]] std::expected<void, StarException> interact(InteractAction action) override;
  [[nodiscard]] std::expected<void, StarException> add_effect_emitters(StringSet const& emitters) override;
  [[nodiscard]] std::expected<void, StarException> request_emote(String const& emote) override;
  ActorMovementController& movement_controller() override;
  StatusController& status_controller() override;
  Songbook& songbook();
  void set_camera_focus_entity(std::optional<EntityId> const& camera_focus_entity) override;

  [[nodiscard]] std::expected<void, StarException> play_emote(HumanoidEmote emote) override;

  [[nodiscard]] std::expected<List<DamageSource>, StarException> damage_sources() const override;

  [[nodiscard]] std::expected<List<PhysicsForceRegion>, StarException> force_regions() const override;

  using Entity::set_unique_id;

  HumanoidIdentity const& identity() const;
  [[nodiscard]] std::expected<void, StarException> set_identity(HumanoidIdentity identity);

  [[nodiscard]] std::expected<void, StarException> set_humanoid_parameter(String key, std::optional<Json> value);
  std::optional<Json> get_humanoid_parameter(String key);
  [[nodiscard]] std::expected<void, StarException> set_humanoid_parameters(JsonObject parameters);
  JsonObject get_humanoid_parameters();

  [[nodiscard]] std::expected<void, StarException> set_body_directives(String const& directives);
  [[nodiscard]] std::expected<void, StarException> set_emote_directives(String const& directives);

  [[nodiscard]] std::expected<void, StarException> set_hair_group(String const& group);
  [[nodiscard]] std::expected<void, StarException> set_hair_type(String const& type);
  [[nodiscard]] std::expected<void, StarException> set_hair_directives(String const& directives);

  [[nodiscard]] std::expected<void, StarException> set_facial_hair_group(String const& group);
  [[nodiscard]] std::expected<void, StarException> set_facial_hair_type(String const& type);
  [[nodiscard]] std::expected<void, StarException> set_facial_hair_directives(String const& directives);

  [[nodiscard]] std::expected<void, StarException> set_facial_mask_group(String const& group);
  [[nodiscard]] std::expected<void, StarException> set_facial_mask_type(String const& type);
  [[nodiscard]] std::expected<void, StarException> set_facial_mask_directives(String const& directives);

  [[nodiscard]] std::expected<void, StarException> set_hair(String const& group, String const& type, String const& directives);
  [[nodiscard]] std::expected<void, StarException> set_facial_hair(String const& group, String const& type, String const& directives);
  [[nodiscard]] std::expected<void, StarException> set_facial_mask(String const& group, String const& type, String const& directives);

  [[nodiscard]] std::expected<void, StarException> set_species(String const& species);
  [[nodiscard]] std::expected<void, StarException> set_gender(Gender const& gender);
  [[nodiscard]] std::expected<void, StarException> set_personality(Personality const& personality);
  [[nodiscard]] std::expected<void, StarException> set_image_path(std::optional<String> const& image_path);

  [[nodiscard]] std::expected<void, StarException> set_favorite_color(Color color);
  [[nodiscard]] std::expected<void, StarException> set_name(String const& name);
  void set_description(String const& description);

  [[nodiscard]] std::expected<std::shared_ptr<Humanoid>, StarException> humanoid() const;

  bool force_nude() const;

private:
  [[nodiscard]] std::expected<List<LightSource>, StarException> presentation_light_sources(float interpolation) const;
  [[nodiscard]] std::expected<void, StarException> update_humanoid_directives();
  struct TouchDamageTemplate {
    DamageSource source;
    bool uses_collision_poly;
  };
  mutable std::optional<TouchDamageTemplate> m_touch_damage_template;
  DirectivesGroup m_presentation_humanoid_directives;
  [[nodiscard]] std::expected<void, StarException> initialize();
  [[nodiscard]] std::expected<void, StarException> load_state(Json const& disk_store);

  [[nodiscard]] std::expected<Vec2F, StarException> get_absolute_position(Vec2F relative_position) const;

  [[nodiscard]] std::expected<void, StarException> tick_shared(float dt);
  [[nodiscard]] std::expected<LuaCallbacks, StarException> make_npc_callbacks();

  [[nodiscard]] std::expected<void, StarException> setup_net_states();
  [[nodiscard]] std::expected<void, StarException> get_net_states(bool initial);
  [[nodiscard]] std::expected<void, StarException> set_net_states();

  [[nodiscard]] std::expected<void, StarException> add_chat_message(String const& message, Json const& config, String const& portrait = "");
  [[nodiscard]] std::expected<void, StarException> add_emote(HumanoidEmote emote);
  [[nodiscard]] std::expected<void, StarException> set_dance(std::optional<String> const& dance_name);

  [[nodiscard]] std::expected<bool, StarException> set_item_slot(String const& slot, ItemDescriptor item_descriptor);

  [[nodiscard]] std::expected<bool, StarException> can_use_tool() const;

  void disable_worn_armor(bool disable);

  [[nodiscard]] std::expected<void, StarException> refresh_humanoid_parameters();
  [[nodiscard]] std::expected<void, StarException> replace_humanoid(std::optional<HumanoidIdentity> identity = std::nullopt);
  [[nodiscard]] std::expected<void, StarException> accept_replicated_humanoid(Humanoid& body);
  [[nodiscard]] std::expected<void, StarException> bind_actor_animator();
  [[nodiscard]] std::expected<void, StarException> bind_presentation_animator();
  [[nodiscard]] std::expected<void, StarException> restart_presentation_animator();

  NetElementDynamicGroup<NetHumanoid> m_net_humanoid;
  std::weak_ptr<Humanoid> m_applied_humanoid;
  // The Lua tables borrow the animator until their callback groups retire.
  std::shared_ptr<Humanoid> m_actor_callback_humanoid;
  std::shared_ptr<Humanoid> m_presentation_callback_humanoid;
  LuaAnimationScript m_scripted_animator;
  NetElementHashMap<String, Json> m_scripted_animation_parameters;
  NetworkedAnimator::DynamicTarget m_humanoid_dynamic_target;

  NpcVariant m_npc_variant;
  NetElementTopGroup m_net_group;
  NetElementData<StringList> m_drop_pools;

  NetElementData<std::optional<String>> m_unique_id_net_state;
  NetElementData<EntityDamageTeam> m_team_net_state;

  ClientEntityMode m_client_entity_mode;

  NetElementEnum<Humanoid::State> m_humanoid_state_net_state;
  NetElementEnum<HumanoidEmote> m_humanoid_emote_state_net_state;
  NetElementData<std::optional<String>> m_humanoid_dance_net_state;

  NetElementData<HumanoidIdentity> m_identity_net_state;
  NetElementEvent m_refreshed_humanoid_parameters;
  bool m_identity_updated = false;

  NetElementData<std::optional<String>> m_death_particle_burst;

  std::shared_ptr<ActorMovementController> m_movement_controller;
  std::shared_ptr<StatusController> m_status_controller;
  std::shared_ptr<EffectEmitter> m_effect_emitter;

  NetElementBool m_aggressive;

  List<std::shared_ptr<BehaviorState>> m_behaviors;
  mutable LuaActorScript m_script_component;

  List<ChatAction> m_pending_chat_actions;
  NetElementEvent m_new_chat_message_event;
  NetElementString m_chat_message;
  NetElementString m_chat_portrait;
  NetElementData<Json> m_chat_config;
  bool m_chat_message_updated;

  NetElementData<std::optional<String>> m_status_text;
  NetElementBool m_display_nametag;

  HumanoidEmote m_emote_state;
  GameTimer m_emote_cooldown_timer;
  TimedDance m_dance;
  GameTimer m_blink_cooldown_timer;
  Vec2F m_blink_interval;

  NetElementBool m_is_interactive;

  NetElementData<List<QuestArcDescriptor>> m_offered_quests;
  NetElementData<StringSet> m_turn_in_quests;

  Vec2F m_quest_indicator_offset;

  std::shared_ptr<ArmorWearer> m_armor;
  std::shared_ptr<ToolUser> m_tools;
  std::shared_ptr<Songbook> m_songbook;

  NetElementBool m_disable_worn_armor;

  NetElementFloat m_x_aim_position;
  NetElementFloat m_y_aim_position;

  NetElementBool m_shifting;
  NetElementBool m_damage_on_touch;

  int m_hit_damage_notification_limiter;
  int m_hit_damage_notification_limit;
};

}


export module star.npc;

export namespace Star {
  using ::Star::ArmorWearer;
  using ::Star::ToolUser;
  using ::Star::Songbook;
  using ::Star::Item;
  using ::Star::RenderCallback;
  using ::Star::Npc;
  using ::Star::StatusController;
}
