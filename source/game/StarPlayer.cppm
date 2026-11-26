module;
import std;
import star.format;
import star.type_identity;
import star.json;
import star.net_compatibility;
import star.byte_array;
import star.id_map;
import star.data_stream;
import star.bi_map;
import star.casting;
import star.rpc_promise;
import star.strong_typedef;
import star.game_types;
import star.message_types;
import star.vector;
import star.rect;
import star.a_star;
import star.ordered_set;
import star.poly;
import star.audio;
import star.thread;
import star.list;
import star.map;
import star.set;
import star.color;
import star.string;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.random;
import star.periodic_function;
import star.ordered_map;

import star.uuid;
import star.item_descriptor;
import star.drawable;
import star.animation;
import star.particle;
import star.animated_part_set;
import star.mixer;
import star.light_source;
import star.networked_animator;
import star.humanoid;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.entity;
import star.interaction_types;
import star.tile_damage;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_modification;
import star.lua_root;
import star.force_regions;
import star.world;
import star.physics_entity;
import star.anchorable_entity;
import star.mobile_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.lounging_entities;
import star.chat_action;
import star.chatty_entity;
import star.emote_entity;
import star.portrait_entity;
import star.damage_bar_entity;
import star.nametag_entity;
import star.inspectable_entity;
import star.inventory_types;
import star.movement_controller;
import star.platformer_astar_types;
import star.game_timers;
import star.dance_database;
import star.actor_movement_controller;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.ai_types;
import star.entity_rendering_types;
import star.entity_rendering;
import star.player_types;
import star.lua_components;
import star.lua_actor_movement_component;
import star.lua_animation_component;
import star.radio_message_database;

import star.net_element_basic_fields;
import star.net_element_float_fields;
import star.net_element_containers;
import star.net_element_dynamic_group;
import star.net_element_system;

namespace Star {

class ArmorItem;
class ArmorWearer;
class ToolUser;
class EffectEmitter;

struct PlayerConfig;
class Songbook;
class WireConnector;
class PlayerInventory;
class PlayerBlueprints;
class PlayerTech;
class PlayerCompanions;
class PlayerDeployment;
class PlayerLog;
class TechController;
class ClientContext;
class Statistics;
class StatusController;
class PlayerCodexes;
class QuestManager;
class InteractiveEntity;
class PlayerUniverseMap;
class UniverseClient;
class Root;
class Input;

class Player :
  public virtual ToolUserEntity,
  public virtual LoungingEntity,
  public virtual ChattyEntity,
  public virtual InspectableEntity,
  public virtual DamageBarEntity,
  public virtual PortraitEntity,
  public virtual NametagEntity,
  public virtual PhysicsEntity,
  public virtual EmoteEntity {

public:
  ~Player() override;
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<Player, ToolUserEntity, LoungingEntity, ChattyEntity, InspectableEntity, DamageBarEntity,
                         NametagEntity, PhysicsEntity, EmoteEntity>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<Player>(); }

  enum class State {
    Idle,
    Walk,
    Run,
    Jump,
    Fall,
    Swim,
    SwimIdle,
    TeleportIn,
    TeleportOut,
    Crouch,
    Lounge
  };
  static std::expected<EnumMap<State>, StarException> const state_names;

  [[nodiscard]] static std::expected<std::shared_ptr<Player>, StarException> create(Root& root, std::shared_ptr<PlayerConfig> config);
  [[nodiscard]] static std::expected<std::shared_ptr<Player>, StarException> create(Root& root, std::shared_ptr<PlayerConfig> config, Uuid uuid);
  [[nodiscard]] static std::expected<std::shared_ptr<Player>, StarException> create(Root& root, std::shared_ptr<PlayerConfig> config, ByteArray const& net_store, NetCompatibilityRules rules = {});
  [[nodiscard]] static std::expected<std::shared_ptr<Player>, StarException> create(Root& root, std::shared_ptr<PlayerConfig> config, Json const& disk_store);

  [[nodiscard]] std::expected<void, StarException> disk_load(Json const& disk_store);

  std::optional<std::reference_wrapper<Input>> input() const override { return m_input; }
  void set_input(std::optional<std::reference_wrapper<Input>> input) { m_input = input; }

  std::shared_ptr<ClientContext> client_context() const;
  void set_client_context(std::shared_ptr<ClientContext> client_context);

  std::shared_ptr<Statistics> statistics() const;
  void set_statistics(std::shared_ptr<Statistics> statistics);

  void set_universe_client(std::optional<std::reference_wrapper<UniverseClient>> universe_client);
  std::optional<std::reference_wrapper<UniverseClient>> universe_client() const;

  std::shared_ptr<QuestManager> quest_manager() const;

  [[nodiscard]] std::expected<Json, StarException> disk_store();
  [[nodiscard]] std::expected<ByteArray, StarException> net_store(NetCompatibilityRules rules = {});

  EntityType entity_type() const override;
  ClientEntityMode client_entity_mode() const override;

  [[nodiscard]] std::expected<void, StarException> init(World& world, EntityId entity_id, EntityMode mode) override;
  [[nodiscard]] std::expected<void, StarException> uninit() override;

  Vec2F position() const override;
  Vec2F velocity() const override;

  [[nodiscard]] std::expected<Vec2F, StarException> mouth_position() const override;
  [[nodiscard]] std::expected<Vec2F, StarException> mouth_position(bool ignore_adjustments) const override;
  [[nodiscard]] std::expected<Vec2F, StarException> mouth_offset(bool ignore_adjustments = true) const;
  [[nodiscard]] std::expected<Vec2F, StarException> feet_offset() const;
  [[nodiscard]] std::expected<Vec2F, StarException> head_armor_offset() const;
  [[nodiscard]] std::expected<Vec2F, StarException> chest_armor_offset() const;
  [[nodiscard]] std::expected<Vec2F, StarException> legs_armor_offset() const;
  [[nodiscard]] std::expected<Vec2F, StarException> back_armor_offset() const;

  // relative to current position
  RectF meta_bound_box() const override;

  // relative to current position
  RectF collision_area() const override;

  [[nodiscard]] std::expected<std::pair<ByteArray, std::uint64_t>, StarException> write_net_state(std::uint64_t from_version = 0, NetCompatibilityRules rules = {}) override;
  [[nodiscard]] std::expected<void, StarException> read_net_state(ByteArray const& data, float interpolation_time = 0.0f, NetCompatibilityRules rules = {}) override;

  [[nodiscard]] std::expected<void, StarException> enable_interpolation(float extrapolation_hint = 0.0f) override;
  [[nodiscard]] std::expected<void, StarException> disable_interpolation() override;

  [[nodiscard]] std::expected<std::optional<HitType>, StarException> query_hit(DamageSource const& source) const override;
  [[nodiscard]] std::expected<std::optional<PolyF>, StarException> hit_poly() const override;

  [[nodiscard]] std::expected<List<DamageNotification>, StarException> apply_damage(DamageRequest const& damage) override;
  [[nodiscard]] std::expected<List<DamageNotification>, StarException> self_damage_notifications() override;

  [[nodiscard]] std::expected<void, StarException> hit_other(EntityId target_entity_id, DamageRequest const& damage_request) override;
  [[nodiscard]] std::expected<void, StarException> damaged_other(DamageNotification const& damage) override;

  [[nodiscard]] std::expected<List<DamageSource>, StarException> damage_sources() const override;

  [[nodiscard]] std::expected<bool, StarException> should_destroy() const override;
  [[nodiscard]] std::expected<void, StarException> destroy(std::optional<std::reference_wrapper<RenderCallback>> render_callback) override;

  std::optional<EntityAnchorState> lounging_in() const override;
  [[nodiscard]] std::expected<bool, StarException> lounge(EntityId loungeable_entity_id, std::size_t anchor_index);
  [[nodiscard]] std::expected<void, StarException> stop_lounging();

  [[nodiscard]] std::expected<void, StarException> revive(Vec2F const& foot_position);

  [[nodiscard]] std::expected<List<Drawable>, StarException> portrait(PortraitMode mode) const override;
  bool underwater() const;

  bool shifting() const;
  void set_shifting(bool shifting);
  [[nodiscard]] std::expected<void, StarException> special(int special_key);

  void set_move_vector(Vec2F const& vec);
  void move_left();
  void move_right();
  void move_up();
  void move_down();
  void jump();

  // Presentation-only idle/walk/run/jump/fall/duck request, refreshed each generic script update.
  [[nodiscard]] std::expected<void, StarException> control_movement_state(std::optional<String> const& state);

  [[nodiscard]] std::expected<void, StarException> drop_item();

  [[nodiscard]] std::expected<float, StarException> tool_radius() const;
  [[nodiscard]] std::expected<float, StarException> interact_radius() const override;
  void set_interact_radius(float interact_radius);
  void queue_interact_action(RpcPromise<InteractAction> action);
  [[nodiscard]] std::expected<List<InteractAction>, StarException> pull_interact_actions();

  std::uint64_t currency(String const& currency_type) const;

  float health() const override;
  [[nodiscard]] std::expected<float, StarException> max_health() const override;
  DamageBarType damage_bar() const override;
  [[nodiscard]] std::expected<float, StarException> health_percentage() const;

  [[nodiscard]] std::expected<float, StarException> energy() const override;
  [[nodiscard]] std::expected<float, StarException> max_energy() const;
  [[nodiscard]] std::expected<float, StarException> energy_percentage() const;

  [[nodiscard]] std::expected<float, StarException> energy_regen_block_percent() const;

  [[nodiscard]] std::expected<bool, StarException> energy_locked() const override;
  [[nodiscard]] std::expected<bool, StarException> full_energy() const override;
  [[nodiscard]] std::expected<bool, StarException> consume_energy(float energy) override;

  [[nodiscard]] std::expected<float, StarException> food_percentage() const;

  float breath() const;
  [[nodiscard]] std::expected<float, StarException> max_breath() const;

  float protection() const;

  bool force_nude() const;

  [[nodiscard]] std::expected<String, StarException> description() const override;
  void set_description(String const& description);

  [[nodiscard]] std::expected<List<LightSource>, StarException> light_sources() const override;

  Direction walking_direction() const override;
  Direction facing_direction() const override;

  [[nodiscard]] MessageResponseResult receive_message(ConnectionId sending_connection, String const& message, JsonArray const& args = {}) override;

  [[nodiscard]] std::expected<void, StarException> update(float dt, std::uint64_t current_step) override;

  std::uint64_t presentation_epoch() const override;

  [[nodiscard]] std::expected<void, StarException> collect_render_events(RenderCallback& render_callback) override;

  [[nodiscard]] std::expected<void, StarException> render(RenderCallback& render_callback) override;

  [[nodiscard]] std::expected<void, StarException> render_light_sources(RenderCallback& render_callback) override;
  
  [[nodiscard]] std::expected<void, StarException> set_render_layer(std::optional<EntityRenderLayer> layer);

  Json get_generic_property(String const& name, Json const& default_value = Json()) const;
  void set_generic_property(String const& name, Json const& value);

  std::shared_ptr<PlayerInventory> inventory() const;
  // Returns the number of items from this stack that could be
  // picked up from the world, using inventory tab filtering
  [[nodiscard]] std::expected<std::uint64_t, StarException> items_can_hold(std::shared_ptr<Item> const& items) const;
  // Adds items to the inventory, returning the overflow.
  // The items parameter is invalid after use.
  [[nodiscard]] std::expected<std::shared_ptr<Item>, StarException> pickup_items(std::shared_ptr<Item> const& items, bool silent = false);
  // Pick up all of the given items as possible, dropping the overflow.
  // The item parameter is invalid after use.
  [[nodiscard]] std::expected<void, StarException> give_item(std::shared_ptr<Item> const& item);

  [[nodiscard]] std::expected<void, StarException> trigger_pickup_events(std::shared_ptr<Item> const& item);

  std::shared_ptr<Item> essential_item(EssentialItem essential_item) const;
  bool has_item(ItemDescriptor const& descriptor, bool exact_match = false) const;
  std::uint64_t has_count_of_item(ItemDescriptor const& descriptor, bool exact_match = false) const;
  // altough multiple entries may match, they might have different
  // serializations
  [[nodiscard]] std::expected<ItemDescriptor, StarException> take_item(ItemDescriptor const& descriptor, bool consume_partial = false, bool exact_match = false);
  [[nodiscard]] std::expected<void, StarException> give_item(ItemDescriptor const& descriptor);

  // Clear the item swap slot.
  [[nodiscard]] std::expected<void, StarException> clear_swap();

  [[nodiscard]] std::expected<void, StarException> refresh_items();
  [[nodiscard]] std::expected<void, StarException> refresh_armor();
  [[nodiscard]] std::expected<void, StarException> refresh_humanoid() const;
  // Refresh worn equipment from the inventory
  [[nodiscard]] std::expected<void, StarException> refresh_equipment();

  std::shared_ptr<PlayerBlueprints> blueprints() const;
  [[nodiscard]] std::expected<bool, StarException> add_blueprint(ItemDescriptor const& descriptor, bool show_failure = false);
  bool blueprint_known(ItemDescriptor const& descriptor) const;

  [[nodiscard]] std::expected<bool, StarException> add_collectable(String const& collection_name, String const& collectable_name);

  std::shared_ptr<PlayerUniverseMap> universe_map() const;

  std::shared_ptr<PlayerCodexes> const& codexes() const&;
  std::shared_ptr<PlayerCodexes> const& codexes() const&& = delete;

  std::shared_ptr<PlayerTech> techs() const;
  [[nodiscard]] std::expected<void, StarException> override_tech(std::optional<StringList> const& tech_modules);
  bool tech_overridden() const;

  std::shared_ptr<PlayerCompanions> companions() const;

  std::shared_ptr<PlayerLog> log() const;

  [[nodiscard]] std::expected<std::shared_ptr<InteractiveEntity>, StarException> best_interaction_entity(bool include_nearby);

  // Aim this player's target at the given world position.
  void aim(Vec2F const& position);
  [[nodiscard]] std::expected<Vec2F, StarException> aim_position() const override;

  [[nodiscard]] std::expected<Vec2F, StarException> arm_position(ToolHand hand, Direction facing_direction, float arm_angle, Vec2F offset = {}) const override;
  [[nodiscard]] std::expected<Vec2F, StarException> hand_offset(ToolHand hand, Direction facing_direction) const override;

  [[nodiscard]] std::expected<Vec2F, StarException> hand_position(ToolHand hand, Vec2F const& hand_offset = {}) const override;
  std::shared_ptr<Item> hand_item(ToolHand hand) const override;

  [[nodiscard]] std::expected<Vec2F, StarException> arm_adjustment() const override;

  void set_camera_focus_entity(std::optional<EntityId> const& camera_focus_entity) override;

  [[nodiscard]] std::expected<void, StarException> play_emote(HumanoidEmote emote) override;

  bool can_use_tool() const;

  // "Fires" whatever is in the primary (left) item slot, or the primary fire
  // of the 2H item, at whatever the current aim position is.  Will auto-repeat
  // depending on the item auto repeat setting.
  void begin_primary_fire();
  // "Fires" whatever is in the alternate (right) item slot, or the alt fire of
  // the 2H item, at whatever the current aim position is.  Will auto-repeat
  // depending on the item auto repeat setting.
  void begin_alt_fire();

  void end_primary_fire();
  void end_alt_fire();

  // Triggered whenever the use key is pressed
  void begin_trigger();
  void end_trigger();

  std::shared_ptr<Item> primary_hand_item() const;
  std::shared_ptr<Item> alt_hand_item() const;

  [[nodiscard]] std::expected<Uuid, StarException> uuid() const;

  PlayerMode mode_type() const;
  [[nodiscard]] std::expected<void, StarException> set_mode_type(PlayerMode mode);
  PlayerModeConfig const& mode_config() const &;
  PlayerModeConfig const& mode_config() const && = delete;

  ShipUpgrades const& ship_upgrades() const &;
  void set_ship_upgrades(ShipUpgrades const& ship_upgrades);
  [[nodiscard]] std::expected<void, StarException> apply_ship_upgrades(Json const& upgrades);
  void set_ship_species(String species);
  String ship_species() const;

  [[nodiscard]] std::expected<String, StarException> name() const override;
  [[nodiscard]] std::expected<void, StarException> set_name(String const& name);

  std::optional<String> status_text() const override;
  bool display_nametag() const override;
  [[nodiscard]] std::expected<Vec3B, StarException> nametag_color() const override;
  [[nodiscard]] std::expected<Vec2F, StarException> nametag_origin() const override;
  [[nodiscard]] std::expected<String, StarException> nametag() const override;
  [[nodiscard]] std::expected<void, StarException> set_nametag(std::optional<String> nametag);

  [[nodiscard]] std::expected<void, StarException> update_identity();

  [[nodiscard]] std::expected<void, StarException> set_humanoid_parameter(String key, std::optional<Json> value);
  std::optional<Json> get_humanoid_parameter(String key);
  [[nodiscard]] std::expected<void, StarException> set_humanoid_parameters(JsonObject parameters);
  JsonObject get_humanoid_parameters();
  [[nodiscard]] std::expected<void, StarException> refresh_humanoid_parameters();

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

  [[nodiscard]] std::expected<void, StarException> set_hair      (String const& group, String const& type, String const& directives);
  [[nodiscard]] std::expected<void, StarException> set_facial_hair(String const& group, String const& type, String const& directives);
  [[nodiscard]] std::expected<void, StarException> set_facial_mask(String const& group, String const& type, String const& directives);

  String species() const override;
  [[nodiscard]] std::expected<void, StarException> set_species(String const& species);
  Gender gender() const;
  [[nodiscard]] std::expected<void, StarException> set_gender(Gender const& gender);
  [[nodiscard]] std::expected<void, StarException> set_personality(Personality const& personality);
  [[nodiscard]] std::expected<void, StarException> set_image_path(std::optional<String> const& image_path);

  std::shared_ptr<Humanoid> humanoid();
  std::shared_ptr<Humanoid> humanoid() const;
  HumanoidIdentity const& identity() const;

  [[nodiscard]] std::expected<void, StarException> set_identity(HumanoidIdentity identity);

  void set_admin(bool is_admin);
  bool is_admin() const override;

  [[nodiscard]] std::expected<bool, StarException> in_tool_range() const override;
  [[nodiscard]] std::expected<bool, StarException> in_tool_range(Vec2F const& aim_pos) const override;
  [[nodiscard]] std::expected<bool, StarException> in_interaction_range() const;
  [[nodiscard]] std::expected<bool, StarException> in_interaction_range(Vec2F aim_pos) const;

  [[nodiscard]] std::expected<void, StarException> add_particles(List<Particle> const& particles) override;
  [[nodiscard]] std::expected<void, StarException> add_sound(String const& sound, float volume = 1.0f, float pitch = 1.0f) override;

  bool wire_tool_in_use() const;
  void set_wire_connector(std::optional<std::reference_wrapper<WireConnector>> wire_connector) const;

  [[nodiscard]] std::expected<void, StarException> add_ephemeral_status_effects(List<EphemeralStatusEffect> const& status_effects) override;
  [[nodiscard]] std::expected<ActiveUniqueStatusEffectSummary, StarException> active_unique_status_effect_summary() const override;

  [[nodiscard]] std::expected<float, StarException> power_multiplier() const override;

  bool is_dead() const;
  [[nodiscard]] std::expected<void, StarException> kill();

  [[nodiscard]] std::expected<void, StarException> set_favorite_color(Color color);
  Color favorite_color() const override;

  // Starts the teleport animation sequence, locking player movement and
  // preventing some update code
  [[nodiscard]] std::expected<void, StarException> teleport_out(String const& animation_type = "default", bool deploy = false);
  [[nodiscard]] std::expected<void, StarException> teleport_in();
  [[nodiscard]] std::expected<void, StarException> teleport_abort();

  bool is_teleporting() const;
  bool is_teleporting_out() const;
  [[nodiscard]] std::expected<bool, StarException> can_deploy();
  void deploy_abort(String const& animation_type = "default");
  bool is_deploying() const;
  bool is_deployed() const;

  [[nodiscard]] std::expected<void, StarException> set_busy_state(PlayerBusyState busy_state);

  // A hard move to a specified location
  [[nodiscard]] std::expected<void, StarException> move_to(Vec2F const& foot_position);

  List<String> pull_queued_messages();
  List<std::shared_ptr<Item>> pull_queued_item_drops();

  [[nodiscard]] std::expected<void, StarException> queue_ui_message(String const& message) override;
  void queue_item_pickup_message(std::shared_ptr<Item> const& item);

  [[nodiscard]] std::expected<void, StarException> add_chat_message(String const& message, Json const& config = {});
  void add_emote(HumanoidEmote const& emote, std::optional<float> emote_cooldown = {});
  [[nodiscard]] std::expected<void, StarException> set_dance(std::optional<String> const& dance_name);
  std::pair<HumanoidEmote, float> current_emote() const;

  State current_state() const;

  List<ChatAction> pull_pending_chat_actions() override;

  [[nodiscard]] std::expected<std::optional<String>, StarException> inspection_log_name() const override;
  [[nodiscard]] std::expected<std::optional<String>, StarException> inspection_description(String const& species) const override;

  [[nodiscard]] std::expected<float, StarException> beam_gun_radius() const override;

  [[nodiscard]] std::expected<bool, StarException> instrument_playing() override;
  [[nodiscard]] std::expected<void, StarException> instrument_equipped(String const& instrument_kind) override;
  [[nodiscard]] std::expected<void, StarException> interact(InteractAction action) override;
  [[nodiscard]] std::expected<void, StarException> add_effect_emitters(StringSet const& emitters) override;
  [[nodiscard]] std::expected<void, StarException> request_emote(String const& emote) override;

  ActorMovementController& movement_controller() override;
  StatusController& status_controller() override;

  [[nodiscard]] std::expected<List<PhysicsForceRegion>, StarException> force_regions() const override;

  std::shared_ptr<StatusController> status_controller_ptr();
  std::shared_ptr<ActorMovementController> movement_controller_ptr();

  std::shared_ptr<PlayerConfig> config();

  std::shared_ptr<Songbook> songbook() const;

  [[nodiscard]] std::expected<void, StarException> finalize_creation();

  float time_since_last_gave_damage() const;
  EntityId last_damaged_target() const;

  bool invisible() const;

  [[nodiscard]] std::expected<void, StarException> animate_portrait(float dt);

  [[nodiscard]] std::expected<bool, StarException> is_outside();

  [[nodiscard]] std::expected<void, StarException> drop_selected_items(std::function<std::expected<bool, StarException>(std::shared_ptr<Item>)> filter);
  [[nodiscard]] std::expected<void, StarException> drop_everything();

  bool is_perma_dead() const;

  bool interrupt_radio_message();
  [[nodiscard]] std::expected<std::optional<RadioMessage>, StarException> pull_pending_radio_message();
  [[nodiscard]] std::expected<void, StarException> queue_radio_message(Json const& message_config, float delay = 0);

  // If a cinematic should play, returns it and clears it.  May stop cinematics
  // by returning a null Json.
  [[nodiscard]] std::expected<std::optional<Json>, StarException> pull_pending_cinematic();
  [[nodiscard]] std::expected<void, StarException> set_pending_cinematic(Json const& cinematic, bool unique = false);

  [[nodiscard]] std::expected<void, StarException> set_in_cinematic(bool in_cinematic);

  enum class PresentationMessage { Cinematic, PlayMusic, StopMusic };
  static std::optional<PresentationMessage> presentation_message(String const& message);
  // Without a recipient the relay checks the same arguments without publishing a request.
  // A returned reason rejects only this message; unexpected denotes a native failure.
  [[nodiscard]] static std::expected<std::optional<String>, StarException> process_presentation_message(
      Root& root, PresentationMessage message, JsonArray const& args, std::optional<std::reference_wrapper<Player>> recipient = {});

  struct AltMusicRequest {
    std::optional<StringList> tracks; // A disengaged list requests a stop.
    float fade_time = 0;
    int loops = -1;
  };
  std::optional<AltMusicRequest> pull_pending_alt_music();

  std::optional<PlayerWarpRequest> pull_pending_warp();
  void set_pending_warp(String const& action, std::optional<String> const& animation = {}, bool deploy = false);

  [[nodiscard]] std::expected<std::optional<std::pair<Json, RpcPromiseKeeper<Json>>>, StarException> pull_pending_confirmation();
  void queue_confirmation(Json const& dialog_config, RpcPromiseKeeper<Json> const& result_promise);

  AiState const& ai_state() const;
  AiState& ai_state();

  // In inspection mode, scannable, scanned, and interesting objects will be
  // rendered with special highlighting.
  bool inspecting() const;

  // Will return the highlight effect to give an inspectable entity when inspecting
  [[nodiscard]] std::expected<EntityHighlightEffect, StarException> inspection_highlight(std::shared_ptr<InspectableEntity> const& inspectable_entity) const;

  [[nodiscard]] std::expected<Vec2F, StarException> camera_position();

  using Entity::setTeam;

  std::shared_ptr<NetworkedAnimator> effects_animator();

  // We need to store ephemeral/large/always-changing networked properties that other clients can read. Candidates:
  // genericProperties:
  //   Non-starter, is not networked.
  // statusProperties:
  //   Nope! Changes to the status properties aren't networked efficiently - one change resends the whole map.
  //   We can't fix that because it would break compatibility with vanilla servers.
  // effectsAnimator's globalTags:
  //   Cursed, but viable.
  //   Efficient networking due to using a NetElementMapWrapper.
  //   Unfortunately values are Strings, so to work with Json we need to serialize/deserialize. Whatever.
  //   Additionally, this is compatible with vanilla networking.
  // I call this a 'secret property'.

  // If the secret property exists as a serialized Json string, returns a view to it without deserializing.
  std::optional<StringView> get_secret_property_view(String const& name) const;
  std::optional<std::reference_wrapper<String const>> get_secret_property_ref(String const& name) const;
  // Gets a secret Json property. It will be de-serialized.
  [[nodiscard]] std::expected<Json, StarException> get_secret_property(String const& name, Json default_value = Json()) const;
  // Sets a secret Json property. It will be serialized.
  [[nodiscard]] std::expected<void, StarException> set_secret_property(String const& name, Json const& value);

  [[nodiscard]] std::expected<void, StarException> set_animation_parameter(String name, Json value);

private:
  explicit Player(Root& root);
  [[nodiscard]] std::expected<std::optional<String>, StarException> process_radio_message(Json const& config, float delay);
  void enqueue_radio_message(RadioMessage&& message);
  [[nodiscard]] std::expected<void, StarException> initialize(std::shared_ptr<PlayerConfig> config, Uuid uuid);
  [[nodiscard]] std::expected<void, StarException> net_load(ByteArray const& store, NetCompatibilityRules rules);
  [[nodiscard]] std::expected<void, StarException> install_humanoid();
  using GenericScriptComponent = LuaActorScript;

  // handle input and other events (master only) that happen BEFORE movement/tech controller updates
  [[nodiscard]] std::expected<void, StarException> process_controls();
  [[nodiscard]] std::expected<void, StarException> dispatch_use_interaction(std::shared_ptr<InteractiveEntity> const& entity);

  // state changes and effect animations (master and slave) that happen AFTER movement/tech controller updates
  [[nodiscard]] std::expected<void, StarException> process_state_changes(float dt);
  void update_movement_animation_state();

  [[nodiscard]] std::expected<void, StarException> get_net_states(bool initial);
  [[nodiscard]] std::expected<void, StarException> set_net_states();
  [[nodiscard]] std::expected<void, StarException> apply_net_armor_secrets();
  [[nodiscard]] std::expected<void, StarException> set_net_armor_secret(EquipmentSlot slot, std::shared_ptr<ArmorItem> const& armor, bool visible = true);
  [[nodiscard]] std::expected<void, StarException> set_net_armor_secrets(bool include_empty = false);

  [[nodiscard]] std::expected<List<Drawable>, StarException> drawables(float interpolation = 1.0f) const;
  [[nodiscard]] std::expected<List<LightSource>, StarException> presentation_light_sources(float interpolation) const;
  [[nodiscard]] std::expected<void, StarException> update_humanoid_directives();
  [[nodiscard]] std::expected<List<Particle>, StarException> particles();
  [[nodiscard]] std::expected<String, StarException> get_footstep_sound(Vec2I const& sensor) const;


  NetElementDynamicGroup<NetHumanoid> m_net_humanoid;
  std::shared_ptr<NetHumanoid> m_current_net_humanoid;
  NetElementData<std::optional<String>> m_death_particle_burst;
  LuaAnimationScript m_scripted_animator;
  NetElementHashMap<String, Json> m_scripted_animation_parameters;
  NetworkedAnimator::DynamicTarget m_humanoid_dynamic_target;

  std::optional<std::reference_wrapper<Input>> m_input;
  std::shared_ptr<PlayerConfig> m_config;

  NetElementTopGroup m_net_group;

  std::shared_ptr<ClientContext> m_client_context;
  std::shared_ptr<Statistics> m_statistics;
  std::shared_ptr<QuestManager> m_quest_manager;

  std::shared_ptr<PlayerInventory> m_inventory;
  std::shared_ptr<PlayerBlueprints> m_blueprints;
  std::shared_ptr<PlayerUniverseMap> m_universe_map;
  std::shared_ptr<PlayerCodexes> m_codexes;
  std::shared_ptr<PlayerTech> m_techs;
  std::shared_ptr<PlayerCompanions> m_companions;
  std::shared_ptr<PlayerDeployment> m_deployment;
  std::shared_ptr<PlayerLog> m_log;

  std::optional<std::reference_wrapper<UniverseClient>> m_client; // required for celestial callbacks in scripts
  StringMap<std::shared_ptr<Player::GenericScriptComponent>> m_generic_script_contexts;
  JsonObject m_generic_properties;

  State m_state;
  HumanoidEmote m_emote_state;
  std::optional<Humanoid::State> m_control_movement_state;
  std::optional<Humanoid::State> m_movement_animation_state;

  TimedDance m_dance;

  float m_footstep_timer;
  float m_teleport_timer;
  GameTimer m_emote_cooldown_timer;
  GameTimer m_blink_cooldown_timer;
  float m_last_damaged_other_timer;
  EntityId m_last_damaged_target;
  GameTimer m_age_items_timer;

  float m_footstep_volume_variance;
  float m_landing_volume;
  bool m_landing_noise_pending;
  bool m_footstep_pending;

  String m_teleport_animation_type;
  std::shared_ptr<NetworkedAnimator> m_effects_animator;
  NetworkedAnimator::DynamicTarget m_effects_animator_dynamic_target;

  float m_emote_cooldown;
  Vec2F m_blink_interval;

  HashSet<MoveControlType> m_pending_moves;
  Vec2F m_move_vector;
  bool m_shifting;
  ActorMovementParameters m_zero_g_movement_parameters;

  List<DamageSource> m_damage_sources;

  String m_description;

  PlayerMode m_mode_type;
  PlayerModeConfig m_mode_config;
  ShipUpgrades m_ship_upgrades;
  String m_ship_species;

  std::shared_ptr<ToolUser> m_tools;
  std::shared_ptr<ArmorWearer> m_armor;
  HashMap<EquipmentSlot, std::uint64_t> m_armor_secret_net_versions;

  bool m_use_down = false;
  bool m_use_edge_pending = false;
  bool m_use_edge_ready = false;

  Vec2F m_aim_position;

  std::optional<EntityId> m_camera_focus_entity;

  std::shared_ptr<ActorMovementController> m_movement_controller;
  std::shared_ptr<TechController> m_tech_controller;
  std::shared_ptr<StatusController> m_status_controller;

  float m_food_low_threshold;
  List<PersistentStatusEffect> m_food_low_status_effects;
  List<PersistentStatusEffect> m_food_empty_status_effects;

  List<PersistentStatusEffect> m_in_cinematic_status_effects;

  HumanoidIdentity m_identity;
  bool m_identity_updated;

  bool m_is_admin;
  float m_interact_radius; // hand interact radius
  Vec2F m_walk_into_interact_bias; // offset on position to find an interactable
  // when not pointing at
  // an interactable with the mouse

  List<RpcPromise<InteractAction>> m_pending_interact_actions;

  List<Particle> m_callback_particles;
  List<std::tuple<String, float, float>> m_callback_sounds;
  List<std::shared_ptr<AudioInstance>> m_hidden_loop_audios;
  List<OverheadBar> m_presentation_bars;
  DirectivesGroup m_presentation_humanoid_directives;

  List<String> m_queued_messages;
  List<std::shared_ptr<Item>> m_queued_item_pickups;

  List<ChatAction> m_pending_chat_actions;

  StringSet m_mission_radio_messages;
  bool m_interrupt_radio_message;
  List<std::pair<GameTimer, RadioMessage>> m_delayed_radio_messages;
  Deque<RadioMessage> m_pending_radio_messages;
  std::optional<Json> m_pending_cinematic;
  std::optional<AltMusicRequest> m_pending_alt_music;
  std::optional<PlayerWarpRequest> m_pending_warp;
  Deque<std::pair<Json, RpcPromiseKeeper<Json>>> m_pending_confirmations;

  AiState m_ai_state;

  String m_chat_message;
  bool m_chat_message_changed;
  bool m_chat_message_updated;

  std::shared_ptr<EffectEmitter> m_effect_emitter;

  std::shared_ptr<Songbook> m_songbook;

  int m_hit_damage_notification_limiter;
  int m_hit_damage_notification_limit;

  StringSet m_interesting_objects;

  NetElementUInt m_state_net_state;
  NetElementBool m_shifting_net_state;
  NetElementFloat m_x_aim_position_net_state;
  NetElementFloat m_y_aim_position_net_state;
  NetElementData<HumanoidIdentity> m_identity_net_state;
  NetElementEvent m_refreshed_humanoid_parameters;
  JsonObject m_humanoid_parameters = JsonObject();
  NetElementData<EntityDamageTeam> m_team_net_state;
  NetElementEvent m_landed_net_state;
  NetElementString m_chat_message_net_state;
  NetElementEvent m_new_chat_message_net_state;
  NetElementString m_emote_net_state;
  NetElementData<std::optional<String>> m_humanoid_dance_net_state;
};

}

export module star.player;

export namespace Star {
  using ::Star::ArmorItem;
  using ::Star::ArmorWearer;
  using ::Star::ToolUser;
  using ::Star::EffectEmitter;
  using ::Star::PlayerConfig;
  using ::Star::Songbook;
  using ::Star::WireConnector;
  using ::Star::PlayerInventory;
  using ::Star::PlayerBlueprints;
  using ::Star::PlayerTech;
  using ::Star::PlayerCompanions;
  using ::Star::PlayerDeployment;
  using ::Star::PlayerLog;
  using ::Star::TechController;
  using ::Star::ClientContext;
  using ::Star::Statistics;
  using ::Star::StatusController;
  using ::Star::PlayerCodexes;
  using ::Star::QuestManager;
  using ::Star::InteractiveEntity;
  using ::Star::PlayerUniverseMap;
  using ::Star::UniverseClient;
  using ::Star::Player;
}
