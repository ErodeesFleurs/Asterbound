module;
import star.json;
import star.id_map;
import star.vector;
import star.spline;
import star.casting;
import star.strong_typedef;
import star.rpc_promise;
import star.a_star;
import std;
import star.format;
import star.type_identity;
import star.audio;
import star.thread;
import star.list;
import star.map;
import star.set;
import star.bi_map;
import star.color;
import star.string;
import star.data_stream;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.game_types;

import star.drawable;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.item;
import star.non_rotated_drawables_item;
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
import star.movement_controller;
import star.platformer_astar_types;
import star.anchorable_entity;
import star.game_timers;
import star.actor_movement_controller;
import star.mobile_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.tool_user_item;
import star.beam_item;
import star.status_effect_item;
import star.lua_components;
import star.fireable_item;
import star.swingable_item;
import star.durability_item;
import star.pointable_item;
import star.mixer;
import star.entity_rendering_types;
import star.animation;
import star.particle;
import star.light_source;
import star.entity_rendering;
struct PreviewTile;
import star.preview_tile_tool;

namespace Star {

class Root;
class World;
class WireConnector;
class ToolUserEntity;

class MiningTool : public Item, public SwingableItem, public DurabilityItem {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<MiningTool, Item, SwingableItem, DurabilityItem>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<MiningTool>();
  }

  [[nodiscard]]
  static std::expected<std::shared_ptr<MiningTool>, StarException> create(Root& root, Json const& config, String const& directory, Json const& parameters = JsonObject());

  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> clone() const override;

  [[nodiscard]]
  std::expected<List<Drawable>, StarException> drawables() const override;
  // In pixels, offset from image center
  Vec2F handPosition() const override;
  [[nodiscard]]
  std::expected<void, StarException> fire(FireMode mode, bool shifting, bool edgeTriggered) override;
  [[nodiscard]]
  std::expected<void, StarException> update(float dt, FireMode fireMode, bool shifting, HashSet<MoveControlType> const& moves) override;

  [[nodiscard]]
  std::expected<float, StarException> durabilityStatus() override;

  [[nodiscard]]
  std::expected<float, StarException> getAngle(float aimAngle) override;

private:
  explicit MiningTool(Root& root) : Item(root) {}
  [[nodiscard]]
  std::expected<void, StarException> changeDurability(float amount);

  String m_image;
  int m_frames;
  float m_frameCycle;
  float m_frameTiming;
  List<String> m_animationFrame;
  String m_idleFrame;

  Vec2F m_handPosition;
  float m_blockRadius;
  float m_altBlockRadius;

  StringList m_strikeSounds;
  String m_breakSound;
  float m_toolVolume;
  float m_blockVolume;

  bool m_pointable;
};

class HarvestingTool : public Item, public SwingableItem {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<HarvestingTool, Item, SwingableItem>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<HarvestingTool>();
  }

  [[nodiscard]]
  static std::expected<std::shared_ptr<HarvestingTool>, StarException> create(Root& root, Json const& config, String const& directory, Json const& parameters = JsonObject());

  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> clone() const override;

  [[nodiscard]]
  std::expected<List<Drawable>, StarException> drawables() const override;
  // In pixels, offset from image center
  Vec2F handPosition() const override;
  [[nodiscard]]
  std::expected<void, StarException> fire(FireMode mode, bool shifting, bool edgeTriggered) override;
  [[nodiscard]]
  std::expected<void, StarException> update(float dt, FireMode fireMode, bool shifting, HashSet<MoveControlType> const& moves) override;
  [[nodiscard]]
  std::expected<float, StarException> getAngle(float aimAngle) override;

private:
  explicit HarvestingTool(Root& root) : Item(root) {}
  String m_image;
  int m_frames;
  float m_frameCycle;
  float m_frameTiming;
  List<String> m_animationFrame;
  String m_idleFrame;

  Vec2F m_handPosition;

  String m_idleSound;
  StringList m_strikeSounds;
  float m_toolVolume;
  float m_harvestPower;
};


class WireTool : public Item, public FireableItem, public PointableItem, public BeamItem {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<WireTool, Item, FireableItem, PointableItem, BeamItem>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<WireTool>();
  }

  [[nodiscard]]
  static std::expected<std::shared_ptr<WireTool>, StarException> create(Root& root, Json const& config, String const& directory, Json const& parameters = JsonObject());

  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> clone() const override;

  [[nodiscard]]
  std::expected<void, StarException> init(ToolUserEntity& owner, ToolHand hand) override;
  [[nodiscard]]
  std::expected<void, StarException> uninit() override;
  [[nodiscard]]
  std::expected<void, StarException> update(float dt, FireMode fireMode, bool shifting, HashSet<MoveControlType> const& moves) override;

  [[nodiscard]]
  std::expected<List<Drawable>, StarException> drawables() const override;
  bool beamActive() const override;

  void setEnd(EndType type) override;

  // In pixels, offset from image center
  Vec2F handPosition() const override;
  [[nodiscard]]
  std::expected<void, StarException> fire(FireMode mode, bool shifting, bool edgeTriggered) override;
  [[nodiscard]]
  std::expected<float, StarException> getAngle(float aimAngle) override;

  void setConnector(std::optional<std::reference_wrapper<WireConnector>> connector);

private:
  explicit WireTool(Root& root) : Item(root), BeamItem(root) {}
  String m_image;
  Vec2F m_handPosition;

  StringList m_strikeSounds;
  float m_toolVolume;

  std::optional<std::reference_wrapper<WireConnector>> m_wireConnector;
};

class BeamMiningTool : public Item, public FireableItem, public PreviewTileTool, public PointableItem, public BeamItem {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<BeamMiningTool, Item, FireableItem, PreviewTileTool, PointableItem, BeamItem>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<BeamMiningTool>();
  }

  [[nodiscard]]
  static std::expected<std::shared_ptr<BeamMiningTool>, StarException> create(Root& root, Json const& config, String const& directory, Json const& parameters = JsonObject());

  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> clone() const override;

  [[nodiscard]]
  std::expected<List<Drawable>, StarException> drawables() const override;

  virtual void setEnd(EndType type) override;
  [[nodiscard]]
  std::expected<List<PreviewTile>, StarException> previewTiles(bool shifting) const override;
  bool beamActive() const override;
  [[nodiscard]]
  std::expected<void, StarException> fire(FireMode mode, bool shifting, bool edgeTriggered) override;

  [[nodiscard]]
  std::expected<float, StarException> getAngle(float angle) override;

  [[nodiscard]]
  std::expected<void, StarException> init(ToolUserEntity& owner, ToolHand hand) override;
  [[nodiscard]]
  std::expected<void, StarException> update(float dt, FireMode fireMode, bool shifting, HashSet<MoveControlType> const& moves) override;

  [[nodiscard]]
  std::expected<List<PersistentStatusEffect>, StarException> statusEffects() const override;

private:
  explicit BeamMiningTool(Root& root) : Item(root), BeamItem(root) {}
  float m_blockRadius;
  float m_altBlockRadius;

  float m_tileDamage;
  unsigned m_harvestLevel;
  bool m_canCollectLiquid;

  StringList m_strikeSounds;
  float m_toolVolume;
  float m_blockVolume;

  List<PersistentStatusEffect> m_inhandStatusEffects;
};


class PaintingBeamTool
  : public Item,
    public FireableItem,
    public PreviewTileTool,
    public PointableItem,
    public BeamItem {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<PaintingBeamTool, Item, FireableItem, PreviewTileTool, PointableItem, BeamItem>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<PaintingBeamTool>();
  }

  [[nodiscard]]
  static std::expected<std::shared_ptr<PaintingBeamTool>, StarException> create(Root& root, Json const& config, String const& directory, Json const& parameters = JsonObject());

  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> clone() const override;

  [[nodiscard]]
  std::expected<List<Drawable>, StarException> drawables() const override;

  void setEnd(EndType type) override;
  [[nodiscard]]
  std::expected<void, StarException> update(float dt, FireMode fireMode, bool shifting, HashSet<MoveControlType> const& moves) override;
  [[nodiscard]]
  std::expected<List<PreviewTile>, StarException> previewTiles(bool shifting) const override;
  [[nodiscard]]
  std::expected<void, StarException> init(ToolUserEntity& owner, ToolHand hand) override;
  bool beamActive() const override;
  [[nodiscard]]
  std::expected<void, StarException> fire(FireMode mode, bool shifting, bool edgeTriggered) override;

  [[nodiscard]]
  std::expected<float, StarException> getAngle(float angle) override;

private:
  explicit PaintingBeamTool(Root& root) : Item(root), BeamItem(root) {}
  List<Color> m_colors;
  List<String> m_colorKeys;
  int m_colorIndex;

  float m_blockRadius;
  float m_altBlockRadius;

  StringList m_strikeSounds;
  float m_toolVolume;
  float m_blockVolume;
};

}

export module star.tools;

export namespace Star {
  using ::Star::World;
  using ::Star::WireConnector;
  using ::Star::ToolUserEntity;
  using ::Star::MiningTool;
  using ::Star::HarvestingTool;
  using ::Star::WireTool;
  using ::Star::BeamMiningTool;
  using ::Star::PaintingBeamTool;
}

export {
  using ::PreviewTile;
}
