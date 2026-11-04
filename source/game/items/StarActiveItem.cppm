module;
import std;
import star.format;
import star.type_identity;
import star.json;
import star.json_extra;
import star.id_map;
import star.casting;
import star.vector;
import star.data_stream;
import star.game_types;
import star.poly;
import star.strong_typedef;
import star.bi_map;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.rpc_promise;
import star.a_star;
import std;
import star.lua;
import star.data_stream_extra;
import star.set;
import star.string;
import star.random;
import star.periodic_function;
import star.ordered_map;
import star.audio;
import star.map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import star.version;
import star.ordered_set;
import star.either;


import star.lua_root;
import star.lua_components;
import star.lua_actor_movement_component;
import star.lua_animation_component;
import star.drawable;
import star.item;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.animation;
import star.particle;
import star.interaction_types;
import star.tile_damage;
import star.item_descriptor;
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
import star.platformer_astar_types;
import star.anchorable_entity;
import star.game_timers;
import star.actor_movement_controller;
import star.mobile_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.tool_user_item;
import star.animated_part_set;
import star.mixer;
import star.networked_animator;
import star.durability_item;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.uuid;
import star.humanoid;
import star.lounging_entities;
import star.chat_action;
import star.chatty_entity;
import star.portrait_entity;
import star.damage_bar_entity;
import star.nametag_entity;
import star.inspectable_entity;
import star.inventory_types;
import star.ai_types;
import star.entity_rendering_types;
import star.entity_rendering;
import star.player_types;
import star.radio_message_database;
import star.player;
import star.emote_entity;


import star.net_element_basic_fields;
import star.net_element_float_fields;
import star.net_element_containers;
import star.net_element_group;

namespace Star {

class ActiveItem :
  public Item,
  public DurabilityItem,
  public virtual ToolUserItem,
  public virtual NetElementGroup {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<ActiveItem, Item, DurabilityItem, ToolUserItem, NetElementGroup>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<ActiveItem>();
  }

  [[nodiscard]] static std::expected<std::shared_ptr<ActiveItem>, StarException> create(Root& root, Json const& config, String const& directory, Json const& parameters = JsonObject());

  [[nodiscard]] std::expected<std::shared_ptr<Item>, StarException> clone() const override;

  [[nodiscard]] std::expected<void, StarException> init(ToolUserEntity& owner, ToolHand hand) override;
  [[nodiscard]] std::expected<void, StarException> uninit() override;

  [[nodiscard]] std::expected<void, StarException> update(float dt, FireMode fireMode, bool shifting, HashSet<MoveControlType> const& moves) override;

  [[nodiscard]] std::expected<List<DamageSource>, StarException> damageSources() const override;
  [[nodiscard]] std::expected<List<PolyF>, StarException> shieldPolys() const override;

  [[nodiscard]] std::expected<List<PhysicsForceRegion>, StarException> forceRegions() const override;

  bool holdingItem() const;
  std::optional<String> backArmFrame() const;
  std::optional<String> frontArmFrame() const;
  bool twoHandedGrip() const;
  bool recoil() const;
  bool outsideOfHand() const;

  float armAngle() const;
  std::optional<Direction> facingDirection() const;

  // Hand drawables are in hand-space, everything else is in world space.
  [[nodiscard]] std::expected<List<Drawable>, StarException> handDrawables(float interpolation = 1.0f) const;
  [[nodiscard]] std::expected<List<std::pair<Drawable, std::optional<EntityRenderLayer>>>, StarException> entityDrawables() const;
  [[nodiscard]] std::expected<List<LightSource>, StarException> lights(float interpolation = 1.0f) const;
  [[nodiscard]] std::expected<List<std::shared_ptr<AudioInstance>>, StarException> pullNewAudios();
  [[nodiscard]] std::expected<List<Particle>, StarException> pullNewParticles();

  std::optional<String> cursor() const;

  [[nodiscard]] std::expected<std::optional<ChainableJsonMessageResponse>, StarException> receiveMessage(String const& message, bool localMessage, JsonArray const& args = {});

  [[nodiscard]] std::expected<float, StarException> durabilityStatus() override;

private:
  explicit ActiveItem(Root& root) : Item(root), m_script(root), m_scriptedAnimator(root) {}
  [[nodiscard]]
  std::expected<Vec2F, StarException> armPosition(Vec2F const& offset) const;
  [[nodiscard]]
  std::expected<Vec2F, StarException> handPosition(Vec2F const& offset) const;

  [[nodiscard]]
  std::expected<LuaCallbacks, StarException> makeActiveItemCallbacks();
  [[nodiscard]]
  std::expected<LuaCallbacks, StarException> makeScriptedAnimationCallbacks();

  mutable LuaActionScript m_script;

  std::shared_ptr<NetworkedAnimator> m_itemAnimator;
  NetworkedAnimator::DynamicTarget m_itemAnimatorDynamicTarget;

  mutable LuaAnimationScript m_scriptedAnimator;

  HashMap<std::shared_ptr<AudioInstance>, Vec2F> m_activeAudio;

  FireMode m_currentFireMode;
  std::optional<String> m_cursor;

  NetElementBool m_holdingItem;
  NetElementData<std::optional<String>> m_backArmFrame;
  NetElementData<std::optional<String>> m_frontArmFrame;
  NetElementBool m_twoHandedGrip;
  NetElementBool m_recoil;
  NetElementBool m_outsideOfHand;
  NetElementFloat m_armAngle;
  NetElementData<std::optional<Direction>> m_facingDirection;
  NetElementData<List<DamageSource>> m_damageSources;
  NetElementData<List<DamageSource>> m_itemDamageSources;
  NetElementData<List<PolyF>> m_shieldPolys;
  NetElementData<List<PolyF>> m_itemShieldPolys;
  NetElementData<List<PhysicsForceRegion>> m_forceRegions;
  NetElementData<List<PhysicsForceRegion>> m_itemForceRegions;
  NetElementHashMap<String, Json> m_scriptedAnimationParameters;
};

}

export module star.active_item;

export namespace Star {
  using ::Star::ActiveItem;
}

