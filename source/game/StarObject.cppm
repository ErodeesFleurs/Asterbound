module;
import star.format;
import star.game_timers;
import star.json_extra;
import std;
import star.type_identity;
import star.json;
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
import star.game_types;
import star.audio;
import star.thread;
import star.list;
import star.map;
import star.bi_map;
import star.string;
import star.poly;
import star.rect;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ordered_map;

import star.net_element_basic_fields;
import star.net_element_float_fields;
import star.net_element_containers;
import star.net_element_system;

import star.net_compatibility;
import star.byte_array;
namespace Star {
class Root;
class AudioInstance;
class ObjectDatabase;
struct ObjectConfig;
struct ObjectOrientation;
class Object;
}

import star.lua_root;
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

namespace Star {


class Object
  : public virtual TileEntity,
    public virtual StatusEffectEntity,
    public virtual ScriptedEntity,
    public virtual ChattyEntity,
    public virtual InspectableEntity,
    public virtual WireEntity {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<Object, TileEntity, StatusEffectEntity, ScriptedEntity,
        ChattyEntity, InspectableEntity, WireEntity>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<Object>();
  }

  [[nodiscard]] static std::expected<std::shared_ptr<Object>, StarException> create(Root& rootContext, std::shared_ptr<ObjectConfig const> config, Json const& parameters = JsonObject());

  [[nodiscard]] std::expected<Json, StarException> diskStore() const;
  [[nodiscard]] std::expected<ByteArray, StarException> netStore(NetCompatibilityRules rules = {});

  EntityType entityType() const override;
  ClientEntityMode clientEntityMode() const override;

  [[nodiscard]] std::expected<void, StarException> init(World& world, EntityId entityId, EntityMode mode) override;
  [[nodiscard]] std::expected<void, StarException> uninit() override;

  Vec2F position() const override;
  RectF metaBoundBox() const override;

  [[nodiscard]] std::expected<std::pair<ByteArray, std::uint64_t>, StarException>
  writeNetState(std::uint64_t fromVersion = 0, NetCompatibilityRules rules = {}) override;
  [[nodiscard]] std::expected<void, StarException> readNetState(ByteArray data, float interpolationTime = 0.0f,
                                                                NetCompatibilityRules rules = {}) override;

  [[nodiscard]] std::expected<String, StarException> name() const override;
  [[nodiscard]] std::expected<String, StarException> description() const override;

  bool inspectable() const override;
  [[nodiscard]] std::expected<std::optional<String>, StarException> inspectionLogName() const override;
  [[nodiscard]] std::expected<std::optional<String>, StarException>
  inspectionDescription(String const& species) const override;

  [[nodiscard]] std::expected<List<LightSource>, StarException> lightSources() const override;

  [[nodiscard]] std::expected<bool, StarException> shouldDestroy() const override;
  [[nodiscard]] std::expected<void, StarException>
  destroy(std::optional<std::reference_wrapper<RenderCallback>> renderCallback) override;

  [[nodiscard]] std::expected<void, StarException> update(float dt, std::uint64_t currentStep) override;

  [[nodiscard]] std::expected<void, StarException> collectRenderEvents(RenderCallback& renderCallback) override;

  [[nodiscard]] std::expected<void, StarException> render(RenderCallback& renderCallback) override;

  [[nodiscard]] std::expected<void, StarException> renderLightSources(RenderCallback& renderCallback) override;

  [[nodiscard]] std::expected<bool, StarException> checkBroken() override;

  Vec2I tilePosition() const override;

  List<Vec2I> spaces() const override;
  List<MaterialSpace> materialSpaces() const override;
  List<Vec2I> roots() const override;

  Direction direction() const;
  void setDirection(Direction direction);

  // Updates tile position and calls updateOrientation
  [[nodiscard]] std::expected<void, StarException> setTilePosition(Vec2I const& pos) override;

  // Find a new valid orientation for the object
  [[nodiscard]] std::expected<void, StarException> updateOrientation();
  List<Vec2I> anchorPositions() const;

  [[nodiscard]] virtual std::expected<List<Drawable>, StarException> cursorHintDrawables() const;

  [[nodiscard]] std::expected<String, StarException> shortDescription() const;
  String category() const;

  virtual std::shared_ptr<ObjectOrientation> currentOrientation() const;

  [[nodiscard]] std::expected<List<PersistentStatusEffect>, StarException> statusEffects() const override;
  PolyF statusEffectArea() const override;

  [[nodiscard]] std::expected<List<DamageSource>, StarException> damageSources() const override;

  [[nodiscard]] std::expected<std::optional<HitType>, StarException>
  queryHit(DamageSource const& source) const override;
  [[nodiscard]] std::expected<std::optional<PolyF>, StarException> hitPoly() const override;

  [[nodiscard]] std::expected<List<DamageNotification>, StarException>
  applyDamage(DamageRequest const& damage) override;

  [[nodiscard]] std::expected<bool, StarException> damageTiles(List<Vec2I> const& position, Vec2F const& sourcePosition,
                                                               TileDamage const& tileDamage) override;
  [[nodiscard]] std::expected<bool, StarException> canBeDamaged() const override;

  RectF interactiveBoundBox() const override;

  bool isInteractive() const override;
  [[nodiscard]] std::expected<InteractAction, StarException> interact(InteractRequest const& request) override;
  List<Vec2I> interactiveSpaces() const override;

  [[nodiscard]] std::expected<std::optional<LuaValue>, StarException> callScript(String const& func, LuaVariadic<LuaValue> const& args) override;
  [[nodiscard]] std::expected<std::optional<LuaValue>, StarException> evalScript(String const& code) override;

  [[nodiscard]] std::expected<Vec2F, StarException> mouthPosition() const override;
  [[nodiscard]] std::expected<Vec2F, StarException> mouthPosition(bool ignoreAdjustments) const override;
  List<ChatAction> pullPendingChatActions() override;

  [[nodiscard]] std::expected<void, StarException> breakObject(bool smash = true);

  std::size_t nodeCount(WireDirection direction) const override;
  [[nodiscard]] std::expected<Vec2I, StarException> nodePosition(WireNode wireNode) const override;
  [[nodiscard]] std::expected<List<WireConnection>, StarException> connectionsForNode(WireNode wireNode) const override;
  [[nodiscard]] std::expected<bool, StarException> nodeState(WireNode wireNode) const override;

  [[nodiscard]] std::expected<String, StarException> nodeIcon(WireNode wireNode) const override;
  [[nodiscard]] std::expected<Color, StarException> nodeColor(WireNode wireNode) const override;

  [[nodiscard]] std::expected<void, StarException> addNodeConnection(WireNode wireNode,
                                                                     WireConnection nodeConnection) override;
  [[nodiscard]] std::expected<void, StarException> removeNodeConnection(WireNode wireNode,
                                                                        WireConnection nodeConnection) override;

  [[nodiscard]] std::expected<void, StarException> evaluate(WireCoordinator& coordinator) override;

  [[nodiscard]] std::expected<List<QuestArcDescriptor>, StarException> offeredQuests() const override;
  [[nodiscard]] std::expected<StringSet, StarException> turnInQuests() const override;
  [[nodiscard]] std::expected<Vec2F, StarException> questIndicatorPosition() const override;

  [[nodiscard]] std::expected<std::optional<ChainableJsonMessageResponse>, StarException> receiveMessage(ConnectionId sendingConnection, String const& message, JsonArray const& args = {}) override;

  // Check, in order, the passed in object parameters, the config parameters,
  // and then the orientation parameters for the given key.  Returns 'def' if
  // no value is found.
  [[nodiscard]] std::expected<Json, StarException> configValue(String const& name, Json const& def = Json()) const;

  std::shared_ptr<ObjectConfig const> config() const;

  [[nodiscard]] std::expected<float, StarException> liquidFillLevel() const;

  bool biomePlaced() const;

  using Entity::setUniqueId;

protected:
  friend class ObjectDatabase;
  explicit Object(Root& rootContext);
  [[nodiscard]] std::expected<void, StarException> initialize(std::shared_ptr<ObjectConfig const> config, Json const& parameters);

  // Will be automatically called at appropriate times.  Derived classes must
  // call base class versions.
  [[nodiscard]] virtual std::expected<void, StarException> getNetStates(bool initial);
  [[nodiscard]] virtual std::expected<void, StarException> setNetStates();

  [[nodiscard]] virtual std::expected<void, StarException> readStoredData(Json const& diskStore);
  [[nodiscard]] virtual std::expected<Json, StarException> writeStoredData() const;

  void setImageKey(String const& name, String const& value);

  std::size_t orientationIndex() const;
  [[nodiscard]] virtual std::expected<void, StarException> setOrientationIndex(std::size_t orientationIndex);

  PolyF volume() const;

  LuaStoredEntityScript m_scriptComponent;
  mutable LuaAnimationScript m_scriptedAnimator;

  NetElementTopGroup m_netGroup;
  NetElementBool m_interactive;
  NetElementData<List<MaterialSpace>> m_materialSpaces;

private:
  struct InputNode {
    [[nodiscard]] static std::expected<InputNode, StarException> create(Json const& positionConfig, Json const& config);
    Vec2I position;
    NetElementData<List<WireConnection>> connections;
    NetElementBool state;
    Color color;
    String icon;
  };

  struct OutputNode {
    [[nodiscard]] static std::expected<OutputNode, StarException> create(Json const& positionConfig, Json const& config);
    Vec2I position;
    NetElementData<List<WireConnection>> connections;
    NetElementBool state;
    Color color;
    String icon;
  };

  [[nodiscard]] std::expected<LuaCallbacks, StarException> makeObjectCallbacks();
  [[nodiscard]] std::expected<LuaCallbacks, StarException> makeAnimatorObjectCallbacks();

  [[nodiscard]] std::expected<List<Drawable>, StarException> orientationDrawables(std::size_t orientationIndex) const;

  [[nodiscard]] std::expected<void, StarException> addChatMessage(String const& message, Json const& config, String const& portrait = "");


  EntityRenderLayer renderLayer() const;

  // Lights are sampled for display; emissions are collected once per logic tick.
  [[nodiscard]] std::expected<List<LightSource>, StarException> lightSources(NetworkedAnimator::Sample const* sample) const;
  [[nodiscard]] std::expected<void, StarException> collectParticles(RenderCallback& renderCallback);
  [[nodiscard]] std::expected<void, StarException> collectSounds(RenderCallback& renderCallback);

  List<std::shared_ptr<ObjectOrientation>> const& getOrientations() const;

  [[nodiscard]] std::expected<Vec2F, StarException> damageShake() const;
  [[nodiscard]] std::expected<Vec2F, StarException> computeDamageShake() const;
  Vec2F m_presentationDamageShake;

  [[nodiscard]] std::expected<void, StarException> checkLiquidBroken();
  GameTimer m_liquidCheckTimer;

  std::shared_ptr<ObjectConfig const> m_config;
  std::optional<List<std::shared_ptr<ObjectOrientation>>> m_orientations;
  NetElementHashMap<String, Json> m_parameters;

  NetElementData<std::optional<String>> m_uniqueIdNetState;

  NetElementInt m_xTilePosition;
  NetElementInt m_yTilePosition;
  NetElementEnum<Direction> m_direction;
  float m_animationTimer;
  int m_currentFrame;

  Directives m_directives;
  Directives m_colorDirectives;
  String m_colorSuffix;

  std::optional<PeriodicFunction<float>> m_lightFlickering;

  std::shared_ptr<EntityTileDamageStatus> m_tileDamageStatus;

  bool m_broken;
  bool m_unbreakable;
  NetElementFloat m_health;

  std::size_t m_orientationIndex;
  NetElementSize m_orientationIndexNetState;
  NetElementHashMap<String, String> m_netImageKeys;
  mutable StringMap<String> m_imageKeys;

  void resetEmissionTimers();
  List<GameTimer> m_emissionTimers;

  NetElementBool m_soundEffectEnabled;
  std::shared_ptr<AudioInstance> m_soundEffect;

  NetElementData<Color> m_lightSourceColor;

  Vec2F m_animationPosition;
  float m_animationCenterLine;
  std::shared_ptr<NetworkedAnimator> m_networkedAnimator;
  NetworkedAnimator::DynamicTarget m_networkedAnimatorDynamicTarget;

  List<ChatAction> m_pendingChatActions;
  NetElementEvent m_newChatMessageEvent;
  NetElementString m_chatMessage;
  NetElementString m_chatPortrait;
  NetElementData<Json> m_chatConfig;

  mutable std::optional<std::pair<std::size_t, List<Drawable>>> m_orientationDrawablesCache;

  List<InputNode> m_inputNodes;
  List<OutputNode> m_outputNodes;

  NetElementData<List<QuestArcDescriptor>> m_offeredQuests;
  NetElementData<StringSet> m_turnInQuests;

  NetElementHashMap<String, Json> m_scriptedAnimationParameters;

  NetElementData<List<DamageSource>> m_damageSources;

  ClientEntityMode m_clientEntityMode;
};

}

export module star.object;

export namespace Star {
  using ::Star::AudioInstance;
  using ::Star::ObjectDatabase;
  using ::Star::ObjectConfig;
  using ::Star::ObjectOrientation;
  using ::Star::Object;
}
