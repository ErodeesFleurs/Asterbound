module;
import star.type_identity;
import star.xx_hash;
import star.json;
import star.id_map;
import star.game_types;
import star.message_types;
import star.material_types;
import star.image;
import star.list;
import star.bi_map;
import star.vector;
import star.lua;
import star.format;
import star.color;
import star.string;
import star.data_stream;
import star.rect;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.random;
import star.weighted_pool;
import star.casting;
import star.strong_typedef;
import star.audio;
import star.map;
import star.set;
import star.rpc_promise;
import star.ordered_map;
import star.net_compatibility;
import std;
import star.worker_pool;
import star.curve25519;

import star.sector_array_2d;
import star.perlin;
import star.btree;
import star.lru_cache;
import star.data_stream_devices;
import star.btree_database;
import star.ordered_set;

import star.collision_block;
import star.liquid_types;
import star.tile_damage;
import star.tile_sector_array;
import star.animation;
import star.particle;
import star.weather_types;
import star.celestial_coordinate;
import star.sky_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.world_layout;
import star.collision_generator;
import star.world_tiles;
import star.item_descriptor;
import star.celestial_types;
import star.chat_types;
import star.uuid;
import star.tile_modification;
import star.damage_types;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.interaction_types;
import star.warping;
import star.world_geometry;
import star.wiring;
import star.quest_descriptor;
import star.interactive_entity;
import star.tile_entity;
import star.inspectable_entity;
import star.plant;
import star.plant_database;
import star.biome_placement;
import star.lua_root;
import star.versioning_database;
import star.world_storage;
import star.player_types;
import star.sky_parameters;
import star.system_world;
import star.damage_manager;
import star.net_packets;
import star.drawable;
import star.entity_rendering_types;
import star.sky_render_data;
import star.parallax;
import star.cellular_light_array;
import star.cellular_lighting;
import star.world_render_data;
import star.world_structure;
import star.chat_action;
import star.mixer;
import star.entity_rendering;
import star.world;
import star.lua_components;
import star.world_client_state;
import star.interpolation_tracker;
import star.ambient;
import star.weather;
import star.game_timers;

namespace Star {

class Root;

struct Biome;
class WorldTemplate;
class TileDrawer;
class Sky;
class Parallax;
class LuaRoot;
class DamageManager;
class EntityMap;
class ParticleManager;
class UniverseClient;
class Player;
class Item;
class ClientContext;
class PlayerStorage;
struct OverheadBar;

class MaterialDatabase;
class LiquidsDatabase;

// Gather a stable client tile snapshot into the calculator's current region.
// The caller excludes tile mutation until all borrowed parallel work completes.
// Wrapped tile images are retained even when the calculation spans a full world.
[[nodiscard]] std::expected<void, StarException>
gatherClientLightingTiles(CellularLightingCalculator& calculator, ClientTileSectorArray& tiles, WorkerPool& workers,
                          MaterialDatabase const& materials, LiquidsDatabase const& liquids,
                          Vec3F const& environmentLight, float undergroundLevel);

struct WorldClientExceptionTag {
  static constexpr std::string_view name() { return "WorldClientException"; }
};
using WorldClientException = StarError<WorldClientExceptionTag, StarException>;

// Tick-owned integrals, ordered as direction, magnitude and signed wind time.
// Display sampling only reads the two fixed-size endpoints.
struct ParallaxWindTime {
  void beginUpdate(double epoch) {
    previous = current;
    lastEpoch = epoch;
  }

  void update(double epoch, float wind) {
    if (lastEpoch && epoch >= *lastEpoch) {
      double elapsed = epoch - *lastEpoch;
      current += Vec3D(wind > 0.0f ? 1.0 : -1.0, std::abs(wind), wind) * elapsed;
    } else {
      previous = current;
    }
    lastEpoch = epoch;
  }

  Vec3D sample(float alpha) const {
    return previous + std::clamp(static_cast<double>(alpha), 0.0, 1.0) * (current - previous);
  }

  void resetPresentation() { previous = current; }

  Vec3D previous;
  Vec3D current;
  std::optional<double> lastEpoch;
};

// Pending, completed and displayed maps rotate ownership without copying. A
// recycled completed map is not fresh until publish succeeds again.
// WorldClient protects every operation with its map mutex; display borrowing
// must have ended before take exchanges the previous displayed storage.
struct ClientLightingResult {
  void publish(Lightmap& pending, Vec2I position) {
    lightMap.swap(pending);
    minPosition = position;
    ready = true;
  }

  bool take(Lightmap& output, Vec2I& position) {
    if (!ready)
      return false;
    lightMap.swap(output);
    position = minPosition;
    ready = false;
    return true;
  }

  void reset() { ready = false; }

  Lightmap lightMap;
  Vec2I minPosition;
  bool ready = false;
};

class WorldClient : public World {
public:
  void query_type(TypeQuery& query) const noexcept override { query_type_of<WorldClient, World>(*this, query); }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<WorldClient>(); }

  using ScriptComponent = LuaEntityScript;

  [[nodiscard]] static std::expected<std::shared_ptr<WorldClient>, StarException> create(Root& root, std::shared_ptr<Player> mainPlayer, std::shared_ptr<LuaRoot> luaRoot, UniverseClient& universe);
  [[nodiscard]] static std::expected<std::shared_ptr<WorldClient>, StarException> create(Root& root, ClientSubWorldId subWorldId, UniverseClient& universe);
  [[nodiscard]] std::expected<void, StarException> close();
  ~WorldClient() override;

  ConnectionId connection() const override;
  WorldGeometry geometry() const override;
  std::uint64_t currentStep() const override;
  MaterialId material(Vec2I const& position, TileLayer layer) const override;
  MaterialHue materialHueShift(Vec2I const& position, TileLayer layer) const override;
  ModId mod(Vec2I const& position, TileLayer layer) const override;
  MaterialHue modHueShift(Vec2I const& position, TileLayer layer) const override;
  MaterialColorVariant colorVariant(Vec2I const& position, TileLayer layer) const override;
  LiquidLevel liquidLevel(Vec2I const& pos) const override;
  [[nodiscard]] std::expected<LiquidLevel, StarException> liquidLevel(RectF const& region) const override;
  [[nodiscard]] std::expected<TileModificationList, StarException> validTileModifications(TileModificationList const& modificationList, bool allowEntityOverlap) const override;
  [[nodiscard]] std::expected<TileModificationList, StarException> applyTileModifications(TileModificationList const& modificationList, bool allowEntityOverlap) override;
  [[nodiscard]] std::expected<TileModificationList, StarException> replaceTiles(TileModificationList const& modificationList, TileDamage const& tileDamage, bool applyDamage = false) override;
  [[nodiscard]] std::expected<bool, StarException> damageWouldDestroy(Vec2I const& pos, TileLayer layer, TileDamage const& tileDamage) const override;
  std::shared_ptr<Entity> entity(EntityId entityId) const override;
  [[nodiscard]] std::expected<void, StarException> addEntity(std::shared_ptr<Entity> const& entity, EntityId entityId = null_entity_id) override;
  [[nodiscard]] std::expected<std::shared_ptr<Entity>, StarException> closestEntity(Vec2F const& center, float radius, EntityFilter selector = EntityFilter()) const override;
  [[nodiscard]] std::expected<void, StarException> forAllEntities(EntityCallback entityCallback) const override;
  [[nodiscard]] std::expected<void, StarException> forEachEntity(RectF const& boundBox, EntityCallback callback) const override;
  [[nodiscard]] std::expected<void, StarException> forEachEntityLine(Vec2F const& begin, Vec2F const& end, EntityCallback callback) const override;
  [[nodiscard]] std::expected<void, StarException> forEachEntityAtTile(Vec2I const& pos, EntityCallbackOf<TileEntity> entityCallback) const override;
  [[nodiscard]] std::expected<std::shared_ptr<Entity>, StarException> findEntity(RectF const& boundBox, EntityFilter entityFilter) const override;
  [[nodiscard]] std::expected<std::shared_ptr<Entity>, StarException> findEntityLine(Vec2F const& begin, Vec2F const& end, EntityFilter entityFilter) const override;
  [[nodiscard]] std::expected<std::shared_ptr<Entity>, StarException> findEntityAtTile(Vec2I const& pos, EntityFilterOf<TileEntity> entityFilter) const override;
  [[nodiscard]] std::expected<bool, StarException> tileIsOccupied(Vec2I const& pos, TileLayer layer, bool includeEphemeral = false, bool checkCollision = false) const override;
  [[nodiscard]] std::expected<CollisionKind, StarException> tileCollisionKind(Vec2I const& pos) const override;
  [[nodiscard]] std::expected<bool, StarException> collision_blocks_satisfy(RectI const& region, std::function<std::expected<bool, StarException>(CollisionBlock const&)> const& predicate) const override;
  [[nodiscard]] std::expected<bool, StarException> isTileConnectable(Vec2I const& pos, TileLayer layer, bool tilesOnly = false) const override;
  [[nodiscard]] std::expected<bool, StarException> pointTileCollision(Vec2F const& point, CollisionSet const& collisionSet = DefaultCollisionSet) const override;
  [[nodiscard]] std::expected<bool, StarException> lineTileCollision(Vec2F const& begin, Vec2F const& end, CollisionSet const& collisionSet = DefaultCollisionSet) const override;
  [[nodiscard]] std::expected<std::optional<std::pair<Vec2F, Vec2I>>, StarException> lineTileCollisionPoint(Vec2F const& begin, Vec2F const& end, CollisionSet const& collisionSet = DefaultCollisionSet) const override;
  [[nodiscard]] std::expected<List<Vec2I>, StarException> collidingTilesAlongLine(Vec2F const& begin, Vec2F const& end, CollisionSet const& collisionSet = DefaultCollisionSet, int maxSize = -1, bool includeEdges = true) const override;
  [[nodiscard]] std::expected<bool, StarException> rectTileCollision(RectI const& region, CollisionSet const& collisionSet = DefaultCollisionSet) const override;
  [[nodiscard]] std::expected<TileDamageResult, StarException> damageTiles(List<Vec2I> const& pos, TileLayer layer, Vec2F const& sourcePosition, TileDamage const& tileDamage, std::optional<EntityId> sourceEntity = {}) override;
  [[nodiscard]] std::expected<std::shared_ptr<InteractiveEntity>, StarException> getInteractiveInRange(Vec2F const& targetPosition, Vec2F const& sourcePosition, float maxRange) const override;
  [[nodiscard]] std::expected<bool, StarException> canReachEntity(Vec2F const& position, float radius, EntityId targetEntity, bool preferInteractive = true) const override;
  [[nodiscard]] std::expected<RpcPromise<InteractAction>, StarException> interact(InteractRequest const& request) override;
  [[nodiscard]] std::expected<float, StarException> gravity(Vec2F const& pos) const override;
  [[nodiscard]] std::expected<float, StarException> windLevel(Vec2F const& pos) const override;
  [[nodiscard]] std::expected<float, StarException> lightLevel(Vec2F const& pos) const override;
  [[nodiscard]] std::expected<bool, StarException> breathable(Vec2F const& pos) const override;
  [[nodiscard]] std::expected<float, StarException> threatLevel() const override;
  [[nodiscard]] std::expected<StringList, StarException> environmentStatusEffects(Vec2F const& pos) const override;
  [[nodiscard]] std::expected<StringList, StarException> weatherStatusEffects(Vec2F const& pos) const override;
  [[nodiscard]] std::expected<bool, StarException> exposedToWeather(Vec2F const& pos) const override;
  [[nodiscard]] std::expected<std::optional<String>, StarException> activeWeather(Vec2F const& pos) const override;
  [[nodiscard]] std::expected<bool, StarException> isUnderground(Vec2F const& pos) const override;
  [[nodiscard]] std::expected<bool, StarException> disableDeathDrops() const override;
  [[nodiscard]] std::expected<List<PhysicsForceRegion>, StarException> forceRegions() const override;
  [[nodiscard]] std::expected<Json, StarException> getProperty(String const& propertyName, Json const& def = Json()) const override;
  [[nodiscard]] std::expected<void, StarException> setProperty(String const& propertyName, Json const& property) override;
  void timer(float delay, WorldAction worldAction) override;
  double epochTime() const override;
  std::uint32_t day() const override;
  float dayLength() const override;
  float timeOfDay() const override;
  std::shared_ptr<LuaRoot> luaRoot() override;
  [[nodiscard]] std::expected<RpcPromise<Vec2F>, StarException> findUniqueEntity(String const& uniqueId) override;
  [[nodiscard]] std::expected<RpcPromise<Json>, StarException> sendEntityMessage(std::variant<EntityId, String> const& entity, String const& message, JsonArray const& args = {}) override;
  bool isTileProtected(Vec2I const& pos) const override;
  [[nodiscard]] std::expected<StringMap<LuaCallbacks>, StarException> luaThreadCallbacks() const override;

  // Is this WorldClient properly initialized in a world
  bool inWorld() const;
  
  bool isHeadless() const;
  ClientSubWorldId subWorldId() const;

  bool inSpace() const;
  bool flying() const;

  bool mainPlayerDead() const;
  [[nodiscard]] std::expected<void, StarException> reviveMainPlayer();
  bool respawnInWorld() const;
  void setRespawnInWorld(bool respawnInWorld);

  std::int64_t latency() const;

  [[nodiscard]] std::expected<void, StarException> resendEntity(EntityId entityId);
  [[nodiscard]] std::expected<void, StarException> removeEntity(EntityId entityId, bool andDie);

  std::shared_ptr<WorldTemplate const> currentTemplate() const;
  void setTemplate(Json newTemplate);
  std::shared_ptr<Sky const> currentSky() const;

  void dimWorld();
  bool interactiveHighlightMode() const;
  void setInteractiveHighlightMode(bool enabled);
  void setParallax(std::shared_ptr<Parallax> newParallax);
  void overrideGravity(float gravity);
  void resetGravity();

  List<EntityId> entity_ids() const override;

  // Disable normal client-side lighting algorithm, everything full brightness.
  bool fullBright() const;
  void setFullBright(bool fullBright);
  // Disable asynchronous client-side lighting algorithm, run on main thread.
  bool asyncLighting() const;
  void setAsyncLighting(bool asyncLighting);
  // Spatial log generated collision geometry.
  bool collisionDebug() const;
  void setCollisionDebug(bool collisionDebug);

  [[nodiscard]] std::expected<void, StarException> handleIncomingPackets(List<std::shared_ptr<Packet>> const& packets);
  List<std::shared_ptr<Packet>> getOutgoingPackets();

  // Set the rendering window for this client.
  void setClientWindow(RectI window);
  // Sets the client window around the position of the main player.
  void centerClientWindowOnPlayer(Vec2U const& windowSize);
  void centerClientWindowOnPlayer();
  RectI clientWindow() const;
  WorldClientState& clientState();

  [[nodiscard]] MessageResponseResult receiveMessage(ConnectionId fromConnection, String const& message, JsonArray const& args);

  std::shared_ptr<WorldClient::ScriptComponent> scriptContext(String const& contextName);

  [[nodiscard]] std::expected<void, StarException> update(float dt);
  // borderTiles here should extend the client window for border tile
  // calculations.  It is not necessary on the light array.
  [[nodiscard]] std::expected<void, StarException> render(WorldRenderData& renderData, unsigned borderTiles, std::optional<std::reference_wrapper<TileDrawer>> tileDrawer, PresentationContext presentation);
  Vec2F renderPosition(EntityId entityId, PresentationContext presentation) const;
  void resetPresentation();
  List<std::shared_ptr<AudioInstance>> pullPendingAudio();
  List<std::shared_ptr<AudioInstance>> pull_pending_music();

  [[nodiscard]] std::expected<bool, StarException> playerCanReachEntity(EntityId entityId, bool preferInteractive = true) const;

  void disconnectAllWires(Vec2I wireEntityPosition, WireNode const& node);
  void wire(Vec2I const& outputPosition, std::size_t outputIndex, Vec2I const& inputPosition, std::size_t inputIndex);
  void connectWire(WireConnection const& output, WireConnection const& input);

  // Functions for sending broadcast messages to other players that can receive them,
  // on completely vanilla servers by smuggling it through a DamageNotification.
  // It's cursed as fuck, but it works.
  bool sendSecretBroadcast(StringView broadcast, bool raw = false, bool compress = true);
  [[nodiscard]] std::expected<bool, StarException> handleSecretBroadcast(std::shared_ptr<Player> player, StringView broadcast);

  [[nodiscard]] std::expected<List<ChatAction>, StarException> pull_pending_chat_actions();

  WorldStructure const& centralStructure() const;

  DungeonId dungeonId(Vec2I const& pos) const;

  [[nodiscard]] std::expected<void, StarException> collectLiquid(List<Vec2I> const& tilePositions, LiquidId liquidId);

  [[nodiscard]] std::expected<bool, StarException> waitForLighting(std::optional<std::reference_wrapper<WorldRenderData>> renderData = std::nullopt);

  using BroadcastCallback = std::function<std::expected<bool, StarException>(std::shared_ptr<Player>, StringView)>;
  BroadcastCallback& broadcastCallback();
  
  bool shouldExpire();
  
  bool pullRequestedDestroy();
  void requestDestroy();

private:
  explicit WorldClient(Root& root, UniverseClient& universe);
  [[nodiscard]] std::expected<void, StarException> initialize(std::shared_ptr<Player> mainPlayer, std::shared_ptr<LuaRoot> luaRoot, UniverseClient& universe);
  [[nodiscard]] std::expected<void, StarException> initialize(ClientSubWorldId subWorldId, UniverseClient& universe);
  PresentationContext presentationFor(Entity const& entity, PresentationContext presentation) const;
  bool verify_broadcast_signature(std::shared_ptr<Player> const& player, bool legacy,
      std::span<char const> public_key, std::span<char const> signature, std::span<char const> message);

  static const float DropDist;

  struct ClientRenderCallback : RenderCallback {
    explicit ClientRenderCallback(std::optional<std::reference_wrapper<TileDrawer>> drawer, PresentationContext presentation = PresentationContext(), float dt = 0.0f, Vec2F offset = {}) : m_tileDrawer(drawer), m_presentation(presentation), m_timestep(dt), m_offset(offset) {}
    std::optional<std::reference_wrapper<TileDrawer>> tileDrawer() const override { return m_tileDrawer; }
    PresentationContext presentation() const override { return m_presentation; }
    float timestep() const override { return m_timestep; }
    void clear();
    std::optional<std::reference_wrapper<TileDrawer>> m_tileDrawer;
    PresentationContext m_presentation;
    float m_timestep;
    Vec2F m_offset;
    [[nodiscard]] std::expected<void, StarException> addDrawable(Drawable drawable, EntityRenderLayer renderLayer) override;
    [[nodiscard]] std::expected<void, StarException> addLightSource(LightSource lightSource) override;
    [[nodiscard]] std::expected<void, StarException> addParticle(Particle particle) override;
    [[nodiscard]] std::expected<void, StarException> addAudio(std::shared_ptr<AudioInstance> audio) override;
    [[nodiscard]] std::expected<void, StarException> addTilePreview(PreviewTile preview) override;
    [[nodiscard]] std::expected<void, StarException> addOverheadBar(OverheadBar bar) override;

    Map<EntityRenderLayer, List<Drawable>> drawables;
    Map<EntityRenderLayer, List<Drawable>>* drawableTarget = nullptr;
    List<LightSource> lightSources;
    List<Particle> particles;
    List<std::shared_ptr<AudioInstance>> audios;
    List<PreviewTile> previewTiles;
    List<OverheadBar> overheadBars;
  };

  struct EntityPresentation {
    std::weak_ptr<Entity> instance;
    std::uint64_t epoch = 0;
    PresentationPosition position;
    bool continuous = false;
  };

  struct HiddenEntityAudio {
    std::weak_ptr<Entity> instance;
    List<std::shared_ptr<AudioInstance>> loops;
  };

  struct DamageNumber {
    float amount;
    Vec2F position;
    std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double>> timestamp;
  };

  struct DamageNumberKey {
    String damageNumberParticleKind;
    EntityId sourceEntityId;
    EntityId targetEntityId;

    bool operator<(DamageNumberKey const& other) const;
  };

  using ClientTileGetter = std::function<std::expected<std::reference_wrapper<ClientTile const>, StarException>(Vec2I)>;

  [[nodiscard]] std::expected<void, StarException> lightingTileGather();
  [[nodiscard]] std::expected<void, StarException> lightingCalc();
  [[nodiscard]] std::expected<void, StarException> lightingMain(std::stop_token stopToken);

  [[nodiscard]] std::expected<void, StarException> initWorld(WorldStartPacket const& packet);
  [[nodiscard]] std::expected<void, StarException> initializeWorld(WorldStartPacket const& packet);
  [[nodiscard]] std::expected<void, StarException> clearWorld();
  [[nodiscard]] std::expected<void, StarException> tryGiveMainPlayerItem(std::shared_ptr<Item> item, bool silent = false);

  [[nodiscard]] std::expected<void, StarException> notifyEntityCreate(std::shared_ptr<Entity> const& entity);

  // Queues pending (step based) updates to server,
  [[nodiscard]] std::expected<void, StarException> queueUpdatePackets(bool sendEntityUpdates);
  [[nodiscard]] std::expected<void, StarException> handleDamageNotifications();

  [[nodiscard]] std::expected<void, StarException> sparkDamagedBlocks();

  Vec2I environment_biome_track_position() const;
  [[nodiscard]] std::expected<std::optional<String>, StarException> currentWeatherDomain() const;

  void play_alt_music(StringList tracks, float fade_time, int loops);
  void stop_alt_music(float fade_time);

  [[nodiscard]] std::expected<std::shared_ptr<Biome const>, StarException> mainEnvironmentBiome() const;

  // Populates foregroundTransparent / backgroundTransparent flag on ClientTile
  // based on transparency rules.
  [[nodiscard]] std::expected<bool, StarException> read_net_tile(Vec2I const& pos, NetTile const& net_tile, bool invalidate = true);
  CollisionKind effective_collision(Vec2I const& pos, ClientTile const& tile) const;
  void invalidate_prediction_change(Vec2I const& pos, std::optional<CollisionKind> before, std::optional<CollisionKind> after);
  [[nodiscard]] std::expected<void, StarException> renderCollisionDebug();

  [[nodiscard]] std::expected<void, StarException> inform_tile_prediction(Vec2I const& pos, TileModification const& modification);

  void setTileProtection(DungeonId dungeonId, bool isProtected);

  [[nodiscard]] std::expected<void, StarException> setupForceRegions();

  Json m_clientConfig;
  std::shared_ptr<WorldTemplate> m_worldTemplate;
  WorldStructure m_centralStructure;
  Vec2F m_playerStart;
  bool m_respawnInWorld = false;
  JsonObject m_worldProperties;

  std::shared_ptr<EntityMap> m_entityMap;
  Map<EntityId, EntityPresentation> m_entityPresentation;
  Map<EntityId, HiddenEntityAudio> m_hiddenEntityAudio;
  float m_inspectionFlickerMultiplier = 1.0f;
  float m_presentationTimestep = 0.0f;
  ClientRenderCallback m_eventRenderCallback{std::nullopt};
  std::shared_ptr<ClientTileSectorArray> m_tileArray;
  ClientTileGetter m_tileGetterFunction;
  std::shared_ptr<DamageManager> m_damageManager;
  std::shared_ptr<LuaRoot> m_luaRoot;

  WorldGeometry m_geometry;
  std::uint64_t m_currentStep;
  double m_currentTime;
  bool m_fullBright = false;
  bool m_asyncLighting = false;
  CellularLightingCalculator m_lightingCalculator;
  mutable CellularLightIntensityCalculator m_lightIntensityCalculator;
  ThreadFunction<void> m_lightingThread;
  bool m_lightingThreadStarted = false;
  
  std::mutex m_lightingMutex;
  std::condition_variable m_lightingCond;
  std::stop_source m_lightingStopSource{std::nostopstate};

  std::mutex m_lightMapPrepMutex;
  std::mutex m_lightMapMutex;

  Lightmap m_pendingLightMap;
  ClientLightingResult m_lightMap;
  List<LightSource> m_pendingLights;
  List<std::pair<Vec2F, Vec3F>> m_pendingParticleLights;
  RectI m_pendingLightRange;
  std::atomic<bool> m_pendingLightReady;
  List<PreviewTile> m_previewTiles;

  std::shared_ptr<Sky> m_sky;

  CollisionGenerator m_collision_generator;

  std::shared_ptr<WorldClientState> m_clientState;
  std::optional<ConnectionId> m_clientId;

  std::shared_ptr<Player> m_mainPlayer;

  bool m_collisionDebug = false;

  // Client side entity updates are not done until m_inWorld is true, which is
  // set to true after we have entered a world *and* the first batch of updates
  // are received.
  bool m_inWorld = false;
  
  bool m_headless = false;
  
  ClientSubWorldId m_subWorldId;
  bool m_requestedDestroy = false;

  GameTimer m_worldDimTimer;
  float m_worldDimLevel;
  Vec3B m_worldDimColor;

  bool m_interactiveHighlightMode = false;

  GameTimer m_parallaxFadeTimer;
  std::shared_ptr<Parallax> m_currentParallax;
  std::shared_ptr<Parallax> m_nextParallax;
  std::optional<String> m_weatherParallaxAsset;
  std::shared_ptr<Biome const> m_weatherParallaxBiome;
  std::shared_ptr<Parallax> m_weatherParallax;
  std::optional<String> m_weatherDomain;
  ParallaxWindTime m_parallaxWindTime;

  std::optional<float> m_overrideGravity;

  std::shared_ptr<ClientWeather> m_weather;
  std::shared_ptr<ParticleManager> m_particles;

  List<std::shared_ptr<AudioInstance>> m_samples;
  List<std::shared_ptr<AudioInstance>> m_music;

  HashMap<EntityId, std::uint64_t> m_masterEntitiesNetVersion;

  InterpolationTracker m_interpolationTracker;
  GameTimer m_entityUpdateTimer;

  List<std::shared_ptr<Packet>> m_outgoingPackets;
  std::optional<std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds>> m_pingTime;
  std::chrono::milliseconds m_latency;

  Set<EntityId> m_requestedDrops;

  Particle m_blockDamageParticle;
  Particle m_blockDamageParticleVariance;
  float m_blockDamageParticleProbability;

  Particle m_blockDingParticle;
  Particle m_blockDingParticleVariance;
  float m_blockDingParticleProbability;

  HashSet<Vec2I> m_damagedBlocks;

  AmbientManager m_ambient_sounds;
  AmbientManager m_music_track;
  AmbientManager m_alt_music_track;

  List<std::pair<float, WorldAction>> m_timers;

  Map<DamageNumberKey, DamageNumber> m_damageNumbers;
  std::chrono::duration<float> m_damageNotificationBatchDuration;

  std::shared_ptr<AudioInstance> m_space_sound;
  String m_active_space_sound;

  std::shared_ptr<AmbientNoisesDescription const> m_alt_music_description;

  int m_modifiedTilePredictionTimeout;
  HashMap<Vec2I, PredictedTile> m_predicted_tiles;
  HashSet<EntityId> m_startupHiddenEntities;

  HashMap<DungeonId, float> m_dungeonIdGravity;
  HashMap<DungeonId, bool> m_dungeonIdBreathable;
  StableHashSet<DungeonId> m_protectedDungeonIds;

  HashMap<String, List<RpcPromiseKeeper<Vec2F>>> m_findUniqueEntityResponses;
  HashMap<Uuid, RpcPromiseKeeper<Json>> m_entityMessageResponses;
  HashMap<Uuid, RpcPromise<Json>> m_entityMessagePromises;
  HashMap<Uuid, RpcPromiseKeeper<InteractAction>> m_entityInteractionResponses;

  List<PhysicsForceRegion> m_forceRegions;

  BroadcastCallback m_broadcastCallback;
  struct BroadcastVerifiers {
    std::weak_ptr<Player> player;
    std::optional<Curve25519::Verifier> modern;
    Curve25519::PublicKey modern_key{};
    std::optional<Curve25519::Verifier> legacy;
    Curve25519::PublicKey legacy_key{};
  };
  // Entries exist only for current player entities and are erased on removal.
  std::unordered_map<EntityId, BroadcastVerifiers> m_broadcast_verifiers;
  String const m_broadcast_public_key;

  // used to keep track of already-printed stack traces caused by remote entities, so they don't clog the log
  HashSet<std::uint64_t> m_entityExceptionsLogged;
  
  StringMap<std::shared_ptr<WorldClient::ScriptComponent>> m_scriptContexts;

  GameTimer m_expiryTimer;
  
  std::reference_wrapper<UniverseClient> m_universe;
  
  StringMap<LuaCallbacks> m_luaThreadCallbacks;
};

}

export module star.world_client;

export namespace Star {
  using ::Star::Biome;
  using ::Star::ClientContext;
  using ::Star::DamageManager;
  using ::Star::ClientLightingResult;
  using ::Star::EntityMap;
  using ::Star::gatherClientLightingTiles;
  using ::Star::Item;
  using ::Star::LuaRoot;
  using ::Star::OverheadBar;
  using ::Star::Parallax;
  using ::Star::ParallaxWindTime;
  using ::Star::ParticleManager;
  using ::Star::Player;
  using ::Star::PlayerStorage;
  using ::Star::Sky;
  using ::Star::UniverseClient;
  using ::Star::WorldClient;
  using ::Star::WorldClientException;
  using ::Star::WorldClientExceptionTag;
  using ::Star::WorldTemplate;
}
