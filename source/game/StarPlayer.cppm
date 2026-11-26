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
  static std::expected<EnumMap<State>, StarException> const StateNames;

  [[nodiscard]] static std::expected<std::shared_ptr<Player>, StarException> create(Root& root, std::shared_ptr<PlayerConfig> config);
  [[nodiscard]] static std::expected<std::shared_ptr<Player>, StarException> create(Root& root, std::shared_ptr<PlayerConfig> config, Uuid uuid);
  [[nodiscard]] static std::expected<std::shared_ptr<Player>, StarException> create(Root& root, std::shared_ptr<PlayerConfig> config, ByteArray const& net_store, NetCompatibilityRules rules = {});
  [[nodiscard]] static std::expected<std::shared_ptr<Player>, StarException> create(Root& root, std::shared_ptr<PlayerConfig> config, Json const& diskStore);

  [[nodiscard]] std::expected<void, StarException> diskLoad(Json const& diskStore);

  std::optional<std::reference_wrapper<Input>> input() const override { return m_input; }
  void setInput(std::optional<std::reference_wrapper<Input>> input) { m_input = input; }

  std::shared_ptr<ClientContext> client_context() const;
  void set_client_context(std::shared_ptr<ClientContext> client_context);

  std::shared_ptr<Statistics> statistics() const;
  void setStatistics(std::shared_ptr<Statistics> statistics);

  void setUniverseClient(std::optional<std::reference_wrapper<UniverseClient>> universeClient);
  std::optional<std::reference_wrapper<UniverseClient>> universeClient() const;

  std::shared_ptr<QuestManager> questManager() const;

  [[nodiscard]] std::expected<Json, StarException> diskStore();
  [[nodiscard]] std::expected<ByteArray, StarException> net_store(NetCompatibilityRules rules = {});

  EntityType entity_type() const override;
  ClientEntityMode client_entity_mode() const override;

  [[nodiscard]] std::expected<void, StarException> init(World& world, EntityId entityId, EntityMode mode) override;
  [[nodiscard]] std::expected<void, StarException> uninit() override;

  Vec2F position() const override;
  Vec2F velocity() const override;

  [[nodiscard]] std::expected<Vec2F, StarException> mouth_position() const override;
  [[nodiscard]] std::expected<Vec2F, StarException> mouth_position(bool ignore_adjustments) const override;
  [[nodiscard]] std::expected<Vec2F, StarException> mouthOffset(bool ignoreAdjustments = true) const;
  [[nodiscard]] std::expected<Vec2F, StarException> feetOffset() const;
  [[nodiscard]] std::expected<Vec2F, StarException> headArmorOffset() const;
  [[nodiscard]] std::expected<Vec2F, StarException> chestArmorOffset() const;
  [[nodiscard]] std::expected<Vec2F, StarException> legsArmorOffset() const;
  [[nodiscard]] std::expected<Vec2F, StarException> backArmorOffset() const;

  // relative to current position
  RectF meta_bound_box() const override;

  // relative to current position
  RectF collision_area() const override;

  [[nodiscard]] std::expected<std::pair<ByteArray, std::uint64_t>, StarException> write_net_state(std::uint64_t fromVersion = 0, NetCompatibilityRules rules = {}) override;
  [[nodiscard]] std::expected<void, StarException> read_net_state(ByteArray const& data, float interpolation_time = 0.0f, NetCompatibilityRules rules = {}) override;

  [[nodiscard]] std::expected<void, StarException> enable_interpolation(float extrapolation_hint = 0.0f) override;
  [[nodiscard]] std::expected<void, StarException> disable_interpolation() override;

  [[nodiscard]] std::expected<std::optional<HitType>, StarException> query_hit(DamageSource const& source) const override;
  [[nodiscard]] std::expected<std::optional<PolyF>, StarException> hit_poly() const override;

  [[nodiscard]] std::expected<List<DamageNotification>, StarException> apply_damage(DamageRequest const& damage) override;
  [[nodiscard]] std::expected<List<DamageNotification>, StarException> self_damage_notifications() override;

  [[nodiscard]] std::expected<void, StarException> hitOther(EntityId targetEntityId, DamageRequest const& damageRequest) override;
  [[nodiscard]] std::expected<void, StarException> damaged_other(DamageNotification const& damage) override;

  [[nodiscard]] std::expected<List<DamageSource>, StarException> damage_sources() const override;

  [[nodiscard]] std::expected<bool, StarException> should_destroy() const override;
  [[nodiscard]] std::expected<void, StarException> destroy(std::optional<std::reference_wrapper<RenderCallback>> render_callback) override;

  std::optional<EntityAnchorState> lounging_in() const override;
  [[nodiscard]] std::expected<bool, StarException> lounge(EntityId loungeableEntityId, std::size_t anchorIndex);
  [[nodiscard]] std::expected<void, StarException> stopLounging();

  [[nodiscard]] std::expected<void, StarException> revive(Vec2F const& footPosition);

  [[nodiscard]] std::expected<List<Drawable>, StarException> portrait(PortraitMode mode) const override;
  bool underwater() const;

  bool shifting() const;
  void setShifting(bool shifting);
  [[nodiscard]] std::expected<void, StarException> special(int specialKey);

  void setMoveVector(Vec2F const& vec);
  void moveLeft();
  void moveRight();
  void moveUp();
  void moveDown();
  void jump();

  // Presentation-only idle/walk/run/jump/fall/duck request, refreshed each generic script update.
  [[nodiscard]] std::expected<void, StarException> controlMovementState(std::optional<String> const& state);

  [[nodiscard]] std::expected<void, StarException> dropItem();

  [[nodiscard]] std::expected<float, StarException> toolRadius() const;
  [[nodiscard]] std::expected<float, StarException> interact_radius() const override;
  void setInteractRadius(float interactRadius);
  void queue_interact_action(RpcPromise<InteractAction> action);
  [[nodiscard]] std::expected<List<InteractAction>, StarException> pull_interact_actions();

  std::uint64_t currency(String const& currencyType) const;

  float health() const override;
  [[nodiscard]] std::expected<float, StarException> max_health() const override;
  DamageBarType damage_bar() const override;
  [[nodiscard]] std::expected<float, StarException> healthPercentage() const;

  [[nodiscard]] std::expected<float, StarException> energy() const override;
  [[nodiscard]] std::expected<float, StarException> maxEnergy() const;
  [[nodiscard]] std::expected<float, StarException> energyPercentage() const;

  [[nodiscard]] std::expected<float, StarException> energyRegenBlockPercent() const;

  [[nodiscard]] std::expected<bool, StarException> energy_locked() const override;
  [[nodiscard]] std::expected<bool, StarException> full_energy() const override;
  [[nodiscard]] std::expected<bool, StarException> consume_energy(float energy) override;

  [[nodiscard]] std::expected<float, StarException> foodPercentage() const;

  float breath() const;
  [[nodiscard]] std::expected<float, StarException> maxBreath() const;

  float protection() const;

  bool forceNude() const;

  [[nodiscard]] std::expected<String, StarException> description() const override;
  void setDescription(String const& description);

  [[nodiscard]] std::expected<List<LightSource>, StarException> light_sources() const override;

  Direction walking_direction() const override;
  Direction facing_direction() const override;

  [[nodiscard]] MessageResponseResult receive_message(ConnectionId sending_connection, String const& message, JsonArray const& args = {}) override;

  [[nodiscard]] std::expected<void, StarException> update(float dt, std::uint64_t currentStep) override;

  std::uint64_t presentation_epoch() const override;

  [[nodiscard]] std::expected<void, StarException> collect_render_events(RenderCallback& render_callback) override;

  [[nodiscard]] std::expected<void, StarException> render(RenderCallback& render_callback) override;

  [[nodiscard]] std::expected<void, StarException> render_light_sources(RenderCallback& render_callback) override;
  
  [[nodiscard]] std::expected<void, StarException> setRenderLayer(std::optional<EntityRenderLayer> layer);

  Json getGenericProperty(String const& name, Json const& defaultValue = Json()) const;
  void setGenericProperty(String const& name, Json const& value);

  std::shared_ptr<PlayerInventory> inventory() const;
  // Returns the number of items from this stack that could be
  // picked up from the world, using inventory tab filtering
  [[nodiscard]] std::expected<std::uint64_t, StarException> itemsCanHold(std::shared_ptr<Item> const& items) const;
  // Adds items to the inventory, returning the overflow.
  // The items parameter is invalid after use.
  [[nodiscard]] std::expected<std::shared_ptr<Item>, StarException> pickupItems(std::shared_ptr<Item> const& items, bool silent = false);
  // Pick up all of the given items as possible, dropping the overflow.
  // The item parameter is invalid after use.
  [[nodiscard]] std::expected<void, StarException> giveItem(std::shared_ptr<Item> const& item);

  [[nodiscard]] std::expected<void, StarException> triggerPickupEvents(std::shared_ptr<Item> const& item);

  std::shared_ptr<Item> essentialItem(EssentialItem essentialItem) const;
  bool hasItem(ItemDescriptor const& descriptor, bool exactMatch = false) const;
  std::uint64_t hasCountOfItem(ItemDescriptor const& descriptor, bool exactMatch = false) const;
  // altough multiple entries may match, they might have different
  // serializations
  [[nodiscard]] std::expected<ItemDescriptor, StarException> takeItem(ItemDescriptor const& descriptor, bool consumePartial = false, bool exactMatch = false);
  [[nodiscard]] std::expected<void, StarException> giveItem(ItemDescriptor const& descriptor);

  // Clear the item swap slot.
  [[nodiscard]] std::expected<void, StarException> clearSwap();

  [[nodiscard]] std::expected<void, StarException> refreshItems();
  [[nodiscard]] std::expected<void, StarException> refreshArmor();
  [[nodiscard]] std::expected<void, StarException> refresh_humanoid() const;
  // Refresh worn equipment from the inventory
  [[nodiscard]] std::expected<void, StarException> refreshEquipment();

  std::shared_ptr<PlayerBlueprints> blueprints() const;
  [[nodiscard]] std::expected<bool, StarException> addBlueprint(ItemDescriptor const& descriptor, bool showFailure = false);
  bool blueprintKnown(ItemDescriptor const& descriptor) const;

  [[nodiscard]] std::expected<bool, StarException> add_collectable(String const& collection_name, String const& collectable_name);

  std::shared_ptr<PlayerUniverseMap> universeMap() const;

  std::shared_ptr<PlayerCodexes> const& codexes() const&;
  std::shared_ptr<PlayerCodexes> const& codexes() const&& = delete;

  std::shared_ptr<PlayerTech> techs() const;
  [[nodiscard]] std::expected<void, StarException> overrideTech(std::optional<StringList> const& techModules);
  bool techOverridden() const;

  std::shared_ptr<PlayerCompanions> companions() const;

  std::shared_ptr<PlayerLog> log() const;

  [[nodiscard]] std::expected<std::shared_ptr<InteractiveEntity>, StarException> bestInteractionEntity(bool includeNearby);
  [[nodiscard]] std::expected<void, StarException> interact_with_entity(std::shared_ptr<InteractiveEntity> entity);

  // Aim this player's target at the given world position.
  void aim(Vec2F const& position);
  [[nodiscard]] std::expected<Vec2F, StarException> aim_position() const override;

  [[nodiscard]] std::expected<Vec2F, StarException> arm_position(ToolHand hand, Direction facing_direction, float arm_angle, Vec2F offset = {}) const override;
  [[nodiscard]] std::expected<Vec2F, StarException> hand_offset(ToolHand hand, Direction facing_direction) const override;

  [[nodiscard]] std::expected<Vec2F, StarException> hand_position(ToolHand hand, Vec2F const& hand_offset = {}) const override;
  std::shared_ptr<Item> hand_item(ToolHand hand) const override;

  [[nodiscard]] std::expected<Vec2F, StarException> arm_adjustment() const override;

  void set_camera_focus_entity(std::optional<EntityId> const& cameraFocusEntity) override;

  [[nodiscard]] std::expected<void, StarException> play_emote(HumanoidEmote emote) override;

  bool canUseTool() const;

  // "Fires" whatever is in the primary (left) item slot, or the primary fire
  // of the 2H item, at whatever the current aim position is.  Will auto-repeat
  // depending on the item auto repeat setting.
  void beginPrimaryFire();
  // "Fires" whatever is in the alternate (right) item slot, or the alt fire of
  // the 2H item, at whatever the current aim position is.  Will auto-repeat
  // depending on the item auto repeat setting.
  void beginAltFire();

  void endPrimaryFire();
  void endAltFire();

  // Triggered whenever the use key is pressed
  void beginTrigger();
  void endTrigger();

  std::shared_ptr<Item> primaryHandItem() const;
  std::shared_ptr<Item> altHandItem() const;

  [[nodiscard]] std::expected<Uuid, StarException> uuid() const;

  PlayerMode modeType() const;
  [[nodiscard]] std::expected<void, StarException> setModeType(PlayerMode mode);
  PlayerModeConfig const& mode_config() const &;
  PlayerModeConfig const& mode_config() const && = delete;

  ShipUpgrades const& ship_upgrades() const &;
  void set_ship_upgrades(ShipUpgrades const& ship_upgrades);
  [[nodiscard]] std::expected<void, StarException> apply_ship_upgrades(Json const& upgrades);
  void setShipSpecies(String species);
  String shipSpecies() const;

  [[nodiscard]] std::expected<String, StarException> name() const override;
  [[nodiscard]] std::expected<void, StarException> setName(String const& name);

  std::optional<String> status_text() const override;
  bool display_nametag() const override;
  [[nodiscard]] std::expected<Vec3B, StarException> nametag_color() const override;
  [[nodiscard]] std::expected<Vec2F, StarException> nametag_origin() const override;
  [[nodiscard]] std::expected<String, StarException> nametag() const override;
  [[nodiscard]] std::expected<void, StarException> setNametag(std::optional<String> nametag);

  [[nodiscard]] std::expected<void, StarException> updateIdentity();

  [[nodiscard]] std::expected<void, StarException> set_humanoid_parameter(String key, std::optional<Json> value);
  std::optional<Json> get_humanoid_parameter(String key);
  [[nodiscard]] std::expected<void, StarException> set_humanoid_parameters(JsonObject parameters);
  JsonObject get_humanoid_parameters();
  [[nodiscard]] std::expected<void, StarException> refresh_humanoid_parameters();

  [[nodiscard]] std::expected<void, StarException> setBodyDirectives(String const& directives);
  [[nodiscard]] std::expected<void, StarException> setEmoteDirectives(String const& directives);

  [[nodiscard]] std::expected<void, StarException> setHairGroup(String const& group);
  [[nodiscard]] std::expected<void, StarException> setHairType(String const& type);
  [[nodiscard]] std::expected<void, StarException> setHairDirectives(String const& directives);

  [[nodiscard]] std::expected<void, StarException> setFacialHairGroup(String const& group);
  [[nodiscard]] std::expected<void, StarException> setFacialHairType(String const& type);
  [[nodiscard]] std::expected<void, StarException> setFacialHairDirectives(String const& directives);

  [[nodiscard]] std::expected<void, StarException> setFacialMaskGroup(String const& group);
  [[nodiscard]] std::expected<void, StarException> setFacialMaskType(String const& type);
  [[nodiscard]] std::expected<void, StarException> setFacialMaskDirectives(String const& directives);

  [[nodiscard]] std::expected<void, StarException> setHair      (String const& group, String const& type, String const& directives);
  [[nodiscard]] std::expected<void, StarException> setFacialHair(String const& group, String const& type, String const& directives);
  [[nodiscard]] std::expected<void, StarException> setFacialMask(String const& group, String const& type, String const& directives);

  String species() const override;
  [[nodiscard]] std::expected<void, StarException> setSpecies(String const& species);
  Gender gender() const;
  [[nodiscard]] std::expected<void, StarException> setGender(Gender const& gender);
  [[nodiscard]] std::expected<void, StarException> setPersonality(Personality const& personality);
  [[nodiscard]] std::expected<void, StarException> setImagePath(std::optional<String> const& imagePath);

  std::shared_ptr<Humanoid> humanoid();
  std::shared_ptr<Humanoid> humanoid() const;
  HumanoidIdentity const& identity() const;

  [[nodiscard]] std::expected<void, StarException> setIdentity(HumanoidIdentity identity);

  void setAdmin(bool isAdmin);
  bool is_admin() const override;

  [[nodiscard]] std::expected<bool, StarException> in_tool_range() const override;
  [[nodiscard]] std::expected<bool, StarException> in_tool_range(Vec2F const& aimPos) const override;
  [[nodiscard]] std::expected<bool, StarException> inInteractionRange() const;
  [[nodiscard]] std::expected<bool, StarException> inInteractionRange(Vec2F aimPos) const;

  [[nodiscard]] std::expected<void, StarException> add_particles(List<Particle> const& particles) override;
  [[nodiscard]] std::expected<void, StarException> add_sound(String const& sound, float volume = 1.0f, float pitch = 1.0f) override;

  bool wireToolInUse() const;
  void setWireConnector(std::optional<std::reference_wrapper<WireConnector>> wireConnector) const;

  [[nodiscard]] std::expected<void, StarException> add_ephemeral_status_effects(List<EphemeralStatusEffect> const& statusEffects) override;
  [[nodiscard]] std::expected<ActiveUniqueStatusEffectSummary, StarException> active_unique_status_effect_summary() const override;

  [[nodiscard]] std::expected<float, StarException> power_multiplier() const override;

  bool isDead() const;
  [[nodiscard]] std::expected<void, StarException> kill();

  [[nodiscard]] std::expected<void, StarException> setFavoriteColor(Color color);
  Color favorite_color() const override;

  // Starts the teleport animation sequence, locking player movement and
  // preventing some update code
  [[nodiscard]] std::expected<void, StarException> teleportOut(String const& animationType = "default", bool deploy = false);
  [[nodiscard]] std::expected<void, StarException> teleportIn();
  [[nodiscard]] std::expected<void, StarException> teleportAbort();

  bool isTeleporting() const;
  bool isTeleportingOut() const;
  [[nodiscard]] std::expected<bool, StarException> canDeploy();
  void deployAbort(String const& animationType = "default");
  bool isDeploying() const;
  bool isDeployed() const;

  [[nodiscard]] std::expected<void, StarException> setBusyState(PlayerBusyState busyState);

  // A hard move to a specified location
  [[nodiscard]] std::expected<void, StarException> moveTo(Vec2F const& footPosition);

  List<String> pullQueuedMessages();
  List<std::shared_ptr<Item>> pullQueuedItemDrops();

  [[nodiscard]] std::expected<void, StarException> queue_ui_message(String const& message) override;
  void queueItemPickupMessage(std::shared_ptr<Item> const& item);

  [[nodiscard]] std::expected<void, StarException> add_chat_message(String const& message, Json const& config = {});
  void addEmote(HumanoidEmote const& emote, std::optional<float> emote_cooldown = {});
  [[nodiscard]] std::expected<void, StarException> set_dance(std::optional<String> const& dance_name);
  std::pair<HumanoidEmote, float> currentEmote() const;

  State currentState() const;

  List<ChatAction> pull_pending_chat_actions() override;

  [[nodiscard]] std::expected<std::optional<String>, StarException> inspectionLogName() const override;
  [[nodiscard]] std::expected<std::optional<String>, StarException> inspectionDescription(String const& species) const override;

  [[nodiscard]] std::expected<float, StarException> beam_gun_radius() const override;

  [[nodiscard]] std::expected<bool, StarException> instrument_playing() override;
  [[nodiscard]] std::expected<void, StarException> instrument_equipped(String const& instrumentKind) override;
  [[nodiscard]] std::expected<void, StarException> interact(InteractAction action) override;
  [[nodiscard]] std::expected<void, StarException> add_effect_emitters(StringSet const& emitters) override;
  [[nodiscard]] std::expected<void, StarException> request_emote(String const& emote) override;

  ActorMovementController& movement_controller() override;
  StatusController& status_controller() override;

  [[nodiscard]] std::expected<List<PhysicsForceRegion>, StarException> force_regions() const override;

  std::shared_ptr<StatusController> statusControllerPtr();
  std::shared_ptr<ActorMovementController> movementControllerPtr();

  std::shared_ptr<PlayerConfig> config();

  std::shared_ptr<Songbook> songbook() const;

  [[nodiscard]] std::expected<void, StarException> finalizeCreation();

  float timeSinceLastGaveDamage() const;
  EntityId lastDamagedTarget() const;

  bool invisible() const;

  [[nodiscard]] std::expected<void, StarException> animatePortrait(float dt);

  [[nodiscard]] std::expected<bool, StarException> isOutside();

  [[nodiscard]] std::expected<void, StarException> dropSelectedItems(std::function<std::expected<bool, StarException>(std::shared_ptr<Item>)> filter);
  [[nodiscard]] std::expected<void, StarException> dropEverything();

  bool isPermaDead() const;

  bool interruptRadioMessage();
  [[nodiscard]] std::expected<std::optional<RadioMessage>, StarException> pullPendingRadioMessage();
  [[nodiscard]] std::expected<void, StarException> queueRadioMessage(Json const& messageConfig, float delay = 0);
  [[nodiscard]] std::expected<void, StarException> queueRadioMessage(RadioMessage message);

  // If a cinematic should play, returns it and clears it.  May stop cinematics
  // by returning a null Json.
  [[nodiscard]] std::expected<std::optional<Json>, StarException> pullPendingCinematic();
  [[nodiscard]] std::expected<void, StarException> setPendingCinematic(Json const& cinematic, bool unique = false);

  [[nodiscard]] std::expected<void, StarException> setInCinematic(bool inCinematic);

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

  std::optional<PlayerWarpRequest> pullPendingWarp();
  void setPendingWarp(String const& action, std::optional<String> const& animation = {}, bool deploy = false);

  [[nodiscard]] std::expected<std::optional<std::pair<Json, RpcPromiseKeeper<Json>>>, StarException> pullPendingConfirmation();
  void queueConfirmation(Json const& dialogConfig, RpcPromiseKeeper<Json> const& resultPromise);

  AiState const& ai_state() const;
  AiState& ai_state();

  // In inspection mode, scannable, scanned, and interesting objects will be
  // rendered with special highlighting.
  bool inspecting() const;

  // Will return the highlight effect to give an inspectable entity when inspecting
  [[nodiscard]] std::expected<EntityHighlightEffect, StarException> inspectionHighlight(std::shared_ptr<InspectableEntity> const& inspectableEntity) const;

  [[nodiscard]] std::expected<Vec2F, StarException> cameraPosition();

  using Entity::setTeam;

  std::shared_ptr<NetworkedAnimator> effectsAnimator();

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
  std::optional<StringView> getSecretPropertyView(String const& name) const;
  std::optional<std::reference_wrapper<String const>> getSecretPropertyRef(String const& name) const;
  // Gets a secret Json property. It will be de-serialized.
  [[nodiscard]] std::expected<Json, StarException> getSecretProperty(String const& name, Json defaultValue = Json()) const;
  // Sets a secret Json property. It will be serialized.
  [[nodiscard]] std::expected<void, StarException> setSecretProperty(String const& name, Json const& value);

  [[nodiscard]] std::expected<void, StarException> setAnimationParameter(String name, Json value);

private:
  explicit Player(Root& root);
  [[nodiscard]] std::expected<std::optional<String>, StarException> process_radio_message(Json const& config, float delay);
  [[nodiscard]] std::expected<void, StarException> initialize(std::shared_ptr<PlayerConfig> config, Uuid uuid);
  [[nodiscard]] std::expected<void, StarException> net_load(ByteArray const& store, NetCompatibilityRules rules);
  [[nodiscard]] std::expected<void, StarException> install_humanoid();
  using GenericScriptComponent = LuaActorScript;

  // handle input and other events (master only) that happen BEFORE movement/tech controller updates
  [[nodiscard]] std::expected<void, StarException> processControls();

  // state changes and effect animations (master and slave) that happen AFTER movement/tech controller updates
  [[nodiscard]] std::expected<void, StarException> processStateChanges(float dt);
  void updateMovementAnimationState();

  [[nodiscard]] std::expected<void, StarException> getNetStates(bool initial);
  [[nodiscard]] std::expected<void, StarException> setNetStates();
  [[nodiscard]] std::expected<void, StarException> apply_net_armor_secrets();
  [[nodiscard]] std::expected<void, StarException> setNetArmorSecret(EquipmentSlot slot, std::shared_ptr<ArmorItem> const& armor, bool visible = true);
  [[nodiscard]] std::expected<void, StarException> setNetArmorSecrets(bool includeEmpty = false);

  [[nodiscard]] std::expected<List<Drawable>, StarException> drawables(float interpolation = 1.0f) const;
  [[nodiscard]] std::expected<List<LightSource>, StarException> presentation_light_sources(float interpolation) const;
  [[nodiscard]] std::expected<void, StarException> update_humanoid_directives();
  [[nodiscard]] std::expected<List<Particle>, StarException> particles();
  [[nodiscard]] std::expected<String, StarException> getFootstepSound(Vec2I const& sensor) const;


  NetElementDynamicGroup<NetHumanoid> m_net_humanoid;
  std::shared_ptr<NetHumanoid> m_current_net_humanoid;
  NetElementData<std::optional<String>> m_deathParticleBurst;
  LuaAnimationScript m_scriptedAnimator;
  NetElementHashMap<String, Json> m_scriptedAnimationParameters;
  NetworkedAnimator::DynamicTarget m_humanoidDynamicTarget;

  std::optional<std::reference_wrapper<Input>> m_input;
  std::shared_ptr<PlayerConfig> m_config;

  NetElementTopGroup m_netGroup;

  std::shared_ptr<ClientContext> m_client_context;
  std::shared_ptr<Statistics> m_statistics;
  std::shared_ptr<QuestManager> m_questManager;

  std::shared_ptr<PlayerInventory> m_inventory;
  std::shared_ptr<PlayerBlueprints> m_blueprints;
  std::shared_ptr<PlayerUniverseMap> m_universeMap;
  std::shared_ptr<PlayerCodexes> m_codexes;
  std::shared_ptr<PlayerTech> m_techs;
  std::shared_ptr<PlayerCompanions> m_companions;
  std::shared_ptr<PlayerDeployment> m_deployment;
  std::shared_ptr<PlayerLog> m_log;

  std::optional<std::reference_wrapper<UniverseClient>> m_client; // required for celestial callbacks in scripts
  StringMap<std::shared_ptr<Player::GenericScriptComponent>> m_genericScriptContexts;
  JsonObject m_genericProperties;

  State m_state;
  HumanoidEmote m_emoteState;
  std::optional<Humanoid::State> m_controlMovementState;
  std::optional<Humanoid::State> m_movementAnimationState;

  TimedDance m_dance;

  float m_footstepTimer;
  float m_teleportTimer;
  GameTimer m_emote_cooldown_timer;
  GameTimer m_blink_cooldown_timer;
  float m_lastDamagedOtherTimer;
  EntityId m_lastDamagedTarget;
  GameTimer m_age_items_timer;

  float m_footstepVolumeVariance;
  float m_landingVolume;
  bool m_landingNoisePending;
  bool m_footstepPending;

  String m_teleportAnimationType;
  std::shared_ptr<NetworkedAnimator> m_effectsAnimator;
  NetworkedAnimator::DynamicTarget m_effectsAnimatorDynamicTarget;

  float m_emote_cooldown;
  Vec2F m_blink_interval;

  HashSet<MoveControlType> m_pendingMoves;
  Vec2F m_moveVector;
  bool m_shifting;
  ActorMovementParameters m_zeroGMovementParameters;

  List<DamageSource> m_damageSources;

  String m_description;

  PlayerMode m_modeType;
  PlayerModeConfig m_modeConfig;
  ShipUpgrades m_ship_upgrades;
  String m_shipSpecies;

  std::shared_ptr<ToolUser> m_tools;
  std::shared_ptr<ArmorWearer> m_armor;
  HashMap<EquipmentSlot, std::uint64_t> m_armor_secret_net_versions;

  bool m_useDown;
  bool m_edgeTriggeredUse;

  Vec2F m_aimPosition;

  std::optional<EntityId> m_cameraFocusEntity;

  std::shared_ptr<ActorMovementController> m_movement_controller;
  std::shared_ptr<TechController> m_techController;
  std::shared_ptr<StatusController> m_statusController;

  float m_foodLowThreshold;
  List<PersistentStatusEffect> m_foodLowStatusEffects;
  List<PersistentStatusEffect> m_foodEmptyStatusEffects;

  List<PersistentStatusEffect> m_inCinematicStatusEffects;

  HumanoidIdentity m_identity;
  bool m_identityUpdated;

  bool m_isAdmin;
  float m_interactRadius; // hand interact radius
  Vec2F m_walkIntoInteractBias; // offset on position to find an interactable
  // when not pointing at
  // an interactable with the mouse

  List<RpcPromise<InteractAction>> m_pending_interact_actions;

  List<Particle> m_callbackParticles;
  List<std::tuple<String, float, float>> m_callbackSounds;
  List<std::shared_ptr<AudioInstance>> m_hiddenLoopAudios;
  List<OverheadBar> m_presentation_bars;
  DirectivesGroup m_presentationHumanoidDirectives;

  List<String> m_queuedMessages;
  List<std::shared_ptr<Item>> m_queuedItemPickups;

  List<ChatAction> m_pending_chat_actions;

  StringSet m_missionRadioMessages;
  bool m_interruptRadioMessage;
  List<std::pair<GameTimer, RadioMessage>> m_delayed_radio_messages;
  Deque<RadioMessage> m_pendingRadioMessages;
  std::optional<Json> m_pendingCinematic;
  std::optional<AltMusicRequest> m_pending_alt_music;
  std::optional<PlayerWarpRequest> m_pendingWarp;
  Deque<std::pair<Json, RpcPromiseKeeper<Json>>> m_pendingConfirmations;

  AiState m_ai_state;

  String m_chat_message;
  bool m_chat_message_changed;
  bool m_chat_message_updated;

  std::shared_ptr<EffectEmitter> m_effectEmitter;

  std::shared_ptr<Songbook> m_songbook;

  int m_hitDamageNotificationLimiter;
  int m_hitDamageNotificationLimit;

  StringSet m_interestingObjects;

  NetElementUInt m_stateNetState;
  NetElementBool m_shiftingNetState;
  NetElementFloat m_xAimPositionNetState;
  NetElementFloat m_yAimPositionNetState;
  NetElementData<HumanoidIdentity> m_identityNetState;
  NetElementEvent m_refreshed_humanoid_parameters;
  JsonObject m_humanoid_parameters = JsonObject();
  NetElementData<EntityDamageTeam> m_teamNetState;
  NetElementEvent m_landedNetState;
  NetElementString m_chat_message_net_state;
  NetElementEvent m_new_chat_message_net_state;
  NetElementString m_emoteNetState;
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
