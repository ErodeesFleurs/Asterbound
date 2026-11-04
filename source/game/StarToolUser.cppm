module;
import std;
import star.format;
import star.set;
import star.json;
import star.net_compatibility;
import star.id_map;
import star.game_types;
import star.string;
import star.data_stream;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.bi_map;
import star.strong_typedef;
import star.vector;
import star.casting;

import star.drawable;
import star.item_descriptor;
import star.status_types;
import star.damage_types;
import star.world_geometry;
import star.damage;
import star.entity_rendering_types;
import star.entity;
import star.collision_block;
import star.force_regions;
import star.physics_entity;

import star.light_source;

import star.data_stream_devices;
import star.net_element;
import star.net_element_basic_fields;
import star.net_element_float_fields;
import star.net_element_sync_group;

namespace Star {

class Humanoid;

class ToolUserEntity;
class Item;
class World;
class EffectEmitter;
class Root;

class ToolUser : public NetElementSyncGroup {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<ToolUser>, StarException> create(Root& root);

  [[nodiscard]] std::expected<Json, StarException> diskStore() const;
  [[nodiscard]] std::expected<void, StarException> diskLoad(Json const& diskStore);

  [[nodiscard]] std::expected<void, StarException> init(ToolUserEntity& user);
  [[nodiscard]] std::expected<void, StarException> uninit();

  std::shared_ptr<Item> primaryHandItem() const;
  std::shared_ptr<Item> altHandItem() const;
  ItemDescriptor primaryHandItemDescriptor() const;
  ItemDescriptor altHandItemDescriptor() const;

  [[nodiscard]] std::expected<List<LightSource>, StarException> lightSources(float interpolation = 1.0f) const;
  [[nodiscard]] std::expected<void, StarException> effects(EffectEmitter& emitter) const;
  [[nodiscard]] std::expected<List<PersistentStatusEffect>, StarException> statusEffects() const;

  [[nodiscard]] std::expected<std::optional<float>, StarException> toolRadius() const;
  // FIXME: There is a render method in ToolUser, why can't this be rendered
  // with the rest of everything else, there are TILE previews and OBJECT
  // previews, but of course one has to go through the render method and the
  // other has to be rendered separately.
  [[nodiscard]] std::expected<List<Drawable>, StarException> renderObjectPreviews(Vec2F aimPosition, Direction walkingDirection, bool inToolRange, Color favoriteColor);
  // Returns the facing override direction if there is one.
  [[nodiscard]] std::expected<std::optional<Direction>, StarException> setupHumanoidHandItems(Humanoid& humanoid, Vec2F position, Vec2F aimPosition) const;
  [[nodiscard]] std::expected<void, StarException> setupHumanoidHandItemDrawables(Humanoid& humanoid, float interpolation = 1.0f) const;

  [[nodiscard]] std::expected<Vec2F, StarException> armPosition(Humanoid const& humanoid, ToolHand hand, Direction facingDirection, float armAngle, Vec2F offset) const;
  [[nodiscard]] std::expected<Vec2F, StarException> handOffset(Humanoid const& humanoid, ToolHand hand, Direction facingDirection) const;
  [[nodiscard]] std::expected<Vec2F, StarException> handPosition(ToolHand hand, Humanoid const& humanoid, Vec2F const& handOffset) const;
  [[nodiscard]] std::expected<bool, StarException> queryShieldHit(DamageSource const& source) const;

  [[nodiscard]] std::expected<void, StarException> tick(float dt, bool shifting, HashSet<MoveControlType> const& moves);

  void beginPrimaryFire();
  void beginAltFire();
  void endPrimaryFire();
  void endAltFire();

  bool firingPrimary() const;
  bool firingAlt() const;

  [[nodiscard]] std::expected<List<DamageSource>, StarException> damageSources() const;
  [[nodiscard]] std::expected<List<PhysicsForceRegion>, StarException> forceRegions() const;

  [[nodiscard]] std::expected<void, StarException> collectRenderEvents(RenderCallback& renderCallback);

  [[nodiscard]] std::expected<void, StarException> render(RenderCallback& renderCallback, bool inToolRange, bool shifting, EntityRenderLayer renderLayer);

  [[nodiscard]] std::expected<void, StarException> setItems(std::shared_ptr<Item> primaryHandItem, std::shared_ptr<Item> altHandItem);

  void suppressItems(bool suppress);

  [[nodiscard]] std::expected<std::optional<ChainableJsonMessageResponse>, StarException> receiveMessage(String const& message, bool localMessage, JsonArray const& args = {});

  [[nodiscard]] std::expected<float, StarException> beamGunRadius() const;

private:
  template <typename> friend class NetElementTop;
  explicit ToolUser(Root& root);
  [[nodiscard]] std::expected<void, StarException> initialize();
  void setupHumanoidHandItemPose(Humanoid& humanoid) const;

  class NetItem : public NetElement {
  public:
    explicit NetItem(Root& root);

    void initNetVersion(std::optional<std::reference_wrapper<NetElementVersion const>> version = std::nullopt) override;

    [[nodiscard]] std::expected<void, StarException> netStore(DataStream& ds, NetCompatibilityRules rules = {}) const override;
    [[nodiscard]] std::expected<void, StarException> netLoad(DataStream& ds, NetCompatibilityRules rules) override;

    [[nodiscard]] std::expected<void, StarException> enableNetInterpolation(float extrapolationHint = 0.0f) override;
    [[nodiscard]] std::expected<void, StarException> disableNetInterpolation() override;
    [[nodiscard]] std::expected<void, StarException> tickNetInterpolation(float dt) override;

    [[nodiscard]] std::expected<bool, StarException> writeNetDelta(DataStream& ds, std::uint64_t fromVersion, NetCompatibilityRules rules = {}) const override;
    [[nodiscard]] std::expected<void, StarException> readNetDelta(DataStream& ds, float interpolationTime = 0.0f, NetCompatibilityRules rules = {}) override;
    [[nodiscard]] std::expected<void, StarException> blankNetDelta(float interpolationTime) override;

    std::shared_ptr<Item> const& get() const;
    [[nodiscard]] std::expected<void, StarException> set(std::shared_ptr<Item> item);

    bool pullNewItem();

  private:
    void updateItemDescriptor();

    std::reference_wrapper<Root> m_root;
    NetElementData<ItemDescriptor> m_itemDescriptor;
    std::shared_ptr<Item> m_item;
    std::optional<std::reference_wrapper<NetElementVersion const>> m_netVersion;
    bool m_netInterpolationEnabled = false;
    float m_netExtrapolationHint = 0;
    bool m_newItem = false;
    mutable DataStreamBuffer m_buffer;
  };

  [[nodiscard]] std::expected<void, StarException> initPrimaryHandItem();
  [[nodiscard]] std::expected<void, StarException> initAltHandItem();
  [[nodiscard]] std::expected<void, StarException> uninitItem(std::shared_ptr<Item> const& item);

  [[nodiscard]] std::expected<void, StarException> netElementsNeedLoad(bool full) override;
  [[nodiscard]] std::expected<void, StarException> netElementsNeedStore() override;

  // Borrowed for this tool user's lifetime; catalog lookups remain reload-aware.
  std::reference_wrapper<Root> m_root;

  float m_beamGunRadius;
  unsigned m_beamGunGlowBorder;
  float m_objectPreviewInnerAlpha;
  float m_objectPreviewOuterAlpha;

  std::optional<std::reference_wrapper<ToolUserEntity>> m_user;

  NetItem m_primaryHandItem;
  NetItem m_altHandItem;

  bool m_fireMain;
  bool m_fireAlt;
  bool m_edgeTriggeredMain;
  bool m_edgeTriggeredAlt;
  bool m_edgeSuppressedMain;
  bool m_edgeSuppressedAlt;

  NetElementBool m_suppress;

  NetElementFloat m_primaryFireTimerNetState;
  NetElementFloat m_altFireTimerNetState;
  NetElementFloat m_primaryTimeFiringNetState;
  NetElementFloat m_altTimeFiringNetState;
  NetElementBool m_primaryItemActiveNetState;
  NetElementBool m_altItemActiveNetState;

  List<Drawable> m_cachedObjectPreview;
  Vec2I m_cachedObjectPreviewPosition;
  std::shared_ptr<Item> m_cachedObjectItem;
};

}

export module star.tool_user;

export namespace Star {
  using ::Star::ToolUserEntity;
  using ::Star::Item;
  using ::Star::World;
  using ::Star::EffectEmitter;
  using ::Star::ToolUser;
}
