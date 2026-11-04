module;
import std;
import star.format;
import star.type_identity;
import star.json;
import star.id_map;
import star.game_types;
import star.casting;
import star.vector;
import star.strong_typedef;
import star.list;
import star.rpc_promise;
import star.data_stream;
import star.bi_map;
import star.lua;
import star.string;
import star.color;
import star.rect;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;

import star.lua_root;
import star.lua_components;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.tile_damage;
import star.interaction_types;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_modification;
import star.force_regions;
import star.world;
import star.physics_entity;
import star.movement_controller;
import star.item_descriptor;
import star.mobile_entity;
import star.scripted_entity;
import star.drawable;
import star.game_timers;

import star.net_element_basic_fields;
import star.net_element_system;

import star.net_compatibility;
import star.byte_array;
namespace Star {
class Root;

class Item;

class ItemDrop : public virtual MobileEntity, public virtual ScriptedEntity {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<ItemDrop, MobileEntity, ScriptedEntity>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<ItemDrop>();
  }

  // Creates a drop at the given position and adds a hard-coded amount of
  // randomness to the drop position / velocity.
  [[nodiscard]]
  static std::expected<std::shared_ptr<ItemDrop>, StarException> createRandomizedDrop(Root& rootContext, std::shared_ptr<Item> const& item, Vec2F const& position, bool eternal = false);
  [[nodiscard]]
  static std::expected<std::shared_ptr<ItemDrop>, StarException> createRandomizedDrop(Root& rootContext, ItemDescriptor const& itemDescriptor, Vec2F const& position, bool eternal = false);

  // Create a drop and throw in the given direction with a hard-coded initial
  // throw velocity (unrelated to magnitude of direction, direction is
  // normalized first).  Initially intangible for 1 second.
  [[nodiscard]]
  static std::expected<std::shared_ptr<ItemDrop>, StarException> throwDrop(Root& rootContext, std::shared_ptr<Item> const& item, Vec2F const& position, Vec2F const& velocity, Vec2F const& direction, bool eternal = false);
  [[nodiscard]]
  static std::expected<std::shared_ptr<ItemDrop>, StarException> throwDrop(Root& rootContext, ItemDescriptor const& itemDescriptor, Vec2F const& position, Vec2F const& velocity, Vec2F const& direction, bool eternal = false);

  [[nodiscard]]
  static std::expected<std::shared_ptr<ItemDrop>, StarException> create(Root& rootContext, std::shared_ptr<Item> item);
  [[nodiscard]]
  static std::expected<std::shared_ptr<ItemDrop>, StarException> create(Root& rootContext, Json const& diskStore);
  [[nodiscard]]
  static std::expected<std::shared_ptr<ItemDrop>, StarException> create(Root& rootContext, ByteArray netStore, NetCompatibilityRules rules = {});

  [[nodiscard]]
  std::expected<Json, StarException> diskStore() const;
  [[nodiscard]]
  std::expected<ByteArray, StarException> netStore(NetCompatibilityRules rules = {}) const;

  EntityType entityType() const override;

  [[nodiscard]]
  std::expected<void, StarException> init(World& world, EntityId entityId, EntityMode mode) override;
  [[nodiscard]]
  std::expected<void, StarException> uninit() override;

  [[nodiscard]]
  std::expected<String, StarException> name() const override;
  [[nodiscard]]
  std::expected<String, StarException> description() const override;

  [[nodiscard]]
  std::expected<std::pair<ByteArray, std::uint64_t>, StarException> writeNetState(std::uint64_t fromVersion = 0, NetCompatibilityRules rules = {}) override;
  [[nodiscard]]
  std::expected<void, StarException> readNetState(ByteArray data, float interpolationTime = 0.0f, NetCompatibilityRules rules = {}) override;

  [[nodiscard]]
  std::expected<void, StarException> enableInterpolation(float extrapolationHint = 0.0f) override;
  [[nodiscard]]
  std::expected<void, StarException> disableInterpolation() override;

  Vec2F position() const override;
  RectF metaBoundBox() const override;

  bool ephemeral() const override;

  RectF collisionArea() const override;

  [[nodiscard]]
  std::expected<void, StarException> update(float dt, std::uint64_t currentStep) override;

  [[nodiscard]]
  std::expected<bool, StarException> shouldDestroy() const override;

  std::uint64_t presentationEpoch() const override;

  [[nodiscard]]
  std::expected<void, StarException> render(RenderCallback& renderCallback) override;
  [[nodiscard]]
  std::expected<void, StarException> renderLightSources(RenderCallback& renderCallback) override;
  
  MovementController& movementController() override;
  
  // The item that this drop contains
  std::shared_ptr<Item> item() const;

  void setEternal(bool eternal);

  // If intangibleTime is set, will be intangible and unable to be picked up
  // until that amount of time has passed.
  void setIntangibleTime(float intangibleTime);

  // Mark this drop as taken by the given entity.  The drop will animate
  // towards them for a while and then disappear.
  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> takeBy(EntityId entityId, float timeOffset = 0.0f);

  // Mark this drop as taken, but do not animate it towards a player simply
  // disappear next step.
  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> take();

  // Item is not taken and is not intangible
  [[nodiscard]]
  std::expected<bool, StarException> canTake() const;

  void setPosition(Vec2F const& position);

  Vec2F velocity() const;
  void setVelocity(Vec2F const& position);
  
  [[nodiscard]]
  std::expected<Json, StarException> configValue(String const& name, Json const& def = Json()) const;

  [[nodiscard]]
  std::expected<std::optional<LuaValue>, StarException> callScript(String const& func, LuaVariadic<LuaValue> const& args) override;
  [[nodiscard]]
  std::expected<std::optional<LuaValue>, StarException> evalScript(String const& code) override;
  
  ClientEntityMode clientEntityMode() const override;

private:
  enum class Mode { Intangible, Available, Taken, Dead };
  static std::expected<EnumMap<Mode>, StarException> const ModeNames;

  explicit ItemDrop(Root& rootContext);
  [[nodiscard]]
  std::expected<void, StarException> initialize();
  [[nodiscard]]
  std::expected<void, StarException> initializeItem(std::shared_ptr<Item> item);
  [[nodiscard]]
  std::expected<void, StarException> updateTaken(bool master);

  [[nodiscard]]
  std::expected<LuaCallbacks, StarException> makeItemDropCallbacks();

  Json m_config;
  Json m_parameters;
  std::shared_ptr<Item> m_item;
  RectF m_boundBox;
  float m_afterTakenLife;
  float m_overheadTime;
  float m_pickupDistance;
  float m_velocity;
  float m_velocityApproach;
  float m_overheadApproach;
  Vec2F m_overheadOffset;

  float m_combineChance;
  float m_combineRadius;
  double m_ageItemsEvery;

  NetElementTopGroup m_netGroup;
  NetElementEnum<Mode> m_mode;
  NetElementIntegral<EntityId> m_owningEntity;
  NetElementData<ItemDescriptor> m_itemDescriptor;
  std::shared_ptr<MovementController> m_movementController;

  // Only updated on master
  bool m_eternal;
  EpochTimer m_dropAge;
  GameTimer m_intangibleTimer;
  EpochTimer m_ageItemsTimer;

  bool m_drawRarityBeam;
  bool m_overForeground;
  std::optional<List<Drawable>> m_drawables;
  
  ClientEntityMode m_clientEntityMode;
  
  mutable LuaEntityScript m_scriptComponent;
  std::optional<Mode> m_overrideMode;
};

}

export module star.item_drop;

export namespace Star {
  using ::Star::Item;
  using ::Star::ItemDrop;
}
