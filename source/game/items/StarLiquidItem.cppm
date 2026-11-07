module;
import std;
import star.format;
import star.type_identity;
import star.json;
import star.id_map;
import star.casting;
import star.strong_typedef;
import star.rpc_promise;
import star.a_star;
import star.vector;
import star.spline;
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
import star.liquid_types;
import star.material_types;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.version;


import star.item;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.entity;
import star.interaction_types;
import star.tile_damage;
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
import star.mobile_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.tool_user_item;
import star.status_effect_item;
import star.lua_components;
import star.fireable_item;
import star.non_rotated_drawables_item;
import star.beam_item;
import star.mixer;
import star.drawable;
import star.entity_rendering_types;
import star.animation;
import star.particle;

import star.light_source;
import star.entity_rendering;
struct PreviewTile;
import star.preview_tile_tool;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;

export module star.liquid_item;
import star.liquids_database;

export namespace Star {

class LiquidItem : public Item, public FireableItem, public PreviewTileTool, public BeamItem {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<LiquidItem, Item, FireableItem, PreviewTileTool, BeamItem>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<LiquidItem>();
  }

  [[nodiscard]]
  static std::expected<std::shared_ptr<LiquidItem>, StarException> create(Root& root, Json const& config, String const& directory, Json const& settings);
  ~LiquidItem() override = default;

  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> clone() const override;

  [[nodiscard]]
  std::expected<void, StarException> init(ToolUserEntity& owner, ToolHand hand) override;
  [[nodiscard]]
  std::expected<void, StarException> update(float dt, FireMode fireMode, bool shifting, HashSet<MoveControlType> const& moves) override;

  [[nodiscard]]
  std::expected<bool, StarException> beamCanPlace() const override;

  [[nodiscard]]
  std::expected<void, StarException> fire(FireMode mode, bool shifting, bool edgeTriggered) override;

  LiquidId liquidId() const;
  float liquidQuantity() const;

  [[nodiscard]]
  std::expected<List<PreviewTile>, StarException> previewTiles(bool shifting) const override;

  [[nodiscard]]
  std::expected<bool, StarException> canPlace(bool shifting) const;
  [[nodiscard]]
  std::expected<bool, StarException> canPlaceAtTile(Vec2I pos) const;
  bool multiplaceEnabled() const;

private:
  explicit LiquidItem(Root& root) : Item(root), BeamItem(root) {}
  LiquidId m_liquidId;
  float m_quantity;

  float m_blockRadius;
  float m_altBlockRadius;
  bool m_shifting;
};

}

namespace Star {

std::expected<std::shared_ptr<LiquidItem>, StarException> LiquidItem::create(Root& root, Json const& config, String const& directory, Json const& settings) {
  auto item = std::shared_ptr<LiquidItem>(new LiquidItem(root));
  auto initialized = item->initialize(config, directory, settings);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  auto fireable = item->initializeFireable(config);
  if (!fireable)
    return std::unexpected(std::move(fireable.error()));
  auto beam = item->initializeBeam(config);
  if (!beam)
    return std::unexpected(std::move(beam.error()));
  auto database = root.liquidsDatabase();
  if (!database)
    return std::unexpected(std::move(database.error()));
  auto liquid = config.getString("liquid");
  if (!liquid)
    return std::unexpected(std::move(liquid.error()));
  auto id = (*database)->liquidId(*liquid);
  if (!id)
    return std::unexpected(std::move(id.error()));
  item->m_liquidId = *id;
  auto twoHanded = config.getBool("twoHanded", true);
  if (!twoHanded)
    return std::unexpected(std::move(twoHanded.error()));
  item->setTwoHanded(*twoHanded);
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto quantity = (*assets)->json("/items/defaultParameters.config:liquidItems.bucketSize").and_then([](Json const& value) { return value.toUInt(); });
  if (!quantity)
    return std::unexpected(std::move(quantity.error()));
  item->m_quantity = *quantity;
  auto cooldown = (*assets)->json("/items/defaultParameters.config:liquidItems.cooldown").and_then([](Json const& value) { return value.toFloat(); });
  if (!cooldown)
    return std::unexpected(std::move(cooldown.error()));
  item->setCooldownTime(*cooldown);
  auto radius = (*assets)->json("/items/defaultParameters.config:blockRadius").and_then([](Json const& value) { return value.toFloat(); });
  if (!radius)
    return std::unexpected(std::move(radius.error()));
  item->m_blockRadius = *radius;
  auto altRadius = (*assets)->json("/items/defaultParameters.config:altBlockRadius").and_then([](Json const& value) { return value.toFloat(); });
  if (!altRadius)
    return std::unexpected(std::move(altRadius.error()));
  item->m_altBlockRadius = *altRadius;
  item->m_shifting = false;
  return item;
}

std::expected<std::shared_ptr<Item>, StarException> LiquidItem::clone() const {
  return std::make_shared<LiquidItem>(*this);
}

std::expected<void, StarException> LiquidItem::init(ToolUserEntity& owner, ToolHand hand) {
  auto initialized = FireableItem::init(owner, hand);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return BeamItem::init(owner, hand);
}

std::expected<void, StarException> LiquidItem::update(float dt, FireMode fireMode, bool shifting, HashSet<MoveControlType> const& moves) {
  auto fireable = FireableItem::update(dt, fireMode, shifting, moves);
  if (!fireable)
    return std::unexpected(std::move(fireable.error()));
  if (shifting || !multiplaceEnabled())
    setEnd(BeamItem::EndType::Tile);
  else
    setEnd(BeamItem::EndType::TileGroup);
  m_shifting = shifting;
  auto beam = BeamItem::update(dt, fireMode, shifting, moves);
  if (!beam)
    return std::unexpected(std::move(beam.error()));
  return {};
}

std::expected<bool, StarException> LiquidItem::beamCanPlace() const {
  return canPlace(m_shifting);
}

std::expected<void, StarException> LiquidItem::fire(FireMode mode, bool shifting, bool edgeTriggered) {
  if (!initialized() || !ready())
    return {};
  auto itemOwner = owner();
  if (!itemOwner)
    return std::unexpected(std::move(itemOwner.error()));
  auto range = itemOwner->get().inToolRange();
  if (!range)
    return std::unexpected(std::move(range.error()));
  if (!*range)
    return {};
  PlaceLiquid placeLiquid{.liquid = liquidId(), .liquidLevel = liquidQuantity()};
  TileModificationList modifications;
  float radius = shifting ? m_altBlockRadius : m_blockRadius;
  if (!multiplaceEnabled())
    radius = 1;
  auto aimPosition = itemOwner->get().aimPosition();
  if (!aimPosition)
    return std::unexpected(std::move(aimPosition.error()));
  for (auto pos : tileAreaBrush(radius, *aimPosition, true)) {
    auto placeable = canPlaceAtTile(pos);
    if (!placeable)
      return std::unexpected(std::move(placeable.error()));
    if (*placeable)
      modifications.append({pos, placeLiquid});
  }
  // Cap the batch before submitting or consuming.
  if (modifications.size() > count())
    modifications.resize(count());
  auto itemWorld = world();
  if (!itemWorld)
    return std::unexpected(std::move(itemWorld.error()));
  auto failed = itemWorld->get().applyTileModifications(modifications, false);
  if (!failed)
    return std::unexpected(std::move(failed.error()));
  if (failed->size() < modifications.size()) {
    auto fired = FireableItem::fire(mode, shifting, edgeTriggered);
    if (!fired)
      return std::unexpected(std::move(fired.error()));
    consume(modifications.size() - failed->size());
  }
  return {};
}

LiquidId LiquidItem::liquidId() const {
  return m_liquidId;
}

float LiquidItem::liquidQuantity() const {
  return m_quantity;
}

std::expected<List<PreviewTile>, StarException> LiquidItem::previewTiles(bool shifting) const {
  List<PreviewTile> result;
  if (initialized()) {
    auto itemOwner = owner();
    if (!itemOwner)
      return std::unexpected(std::move(itemOwner.error()));
    auto liquid = liquidId();
    float radius = shifting ? m_altBlockRadius : m_blockRadius;
    if (!multiplaceEnabled())
      radius = 1;
    std::size_t c = 0;
    auto aimPosition = itemOwner->get().aimPosition();
    if (!aimPosition)
      return std::unexpected(std::move(aimPosition.error()));
    for (auto pos : tileAreaBrush(radius, *aimPosition, true)) {
      if (c >= count())
        break;
      auto placeable = canPlaceAtTile(pos);
      if (!placeable)
        return std::unexpected(std::move(placeable.error()));
      if (*placeable)
        c++;
      result.append({pos, liquid});
    }
  }
  return result;
}

std::expected<bool, StarException> LiquidItem::canPlace(bool shifting) const {
  if (initialized()) {
    auto itemOwner = owner();
    if (!itemOwner)
      return std::unexpected(std::move(itemOwner.error()));
    float radius = shifting ? m_altBlockRadius : m_blockRadius;
    if (!multiplaceEnabled())
      radius = 1;
    auto aimPosition = itemOwner->get().aimPosition();
    if (!aimPosition)
      return std::unexpected(std::move(aimPosition.error()));
    for (auto pos : tileAreaBrush(radius, *aimPosition, true)) {
      auto placeable = canPlaceAtTile(pos);
      if (!placeable)
        return std::unexpected(std::move(placeable.error()));
      if (*placeable)
        return true;
    }
  }
  return false;
}

std::expected<bool, StarException> LiquidItem::canPlaceAtTile(Vec2I pos) const {
  auto itemWorld = world();
  if (!itemWorld)
    return std::unexpected(std::move(itemWorld.error()));
  auto bgTileMaterial = itemWorld->get().material(pos, TileLayer::Background);
  if (bgTileMaterial != EmptyMaterialId) {
    auto fgTileMaterial = itemWorld->get().material(pos, TileLayer::Foreground);
    if (fgTileMaterial == EmptyMaterialId) {
      auto tileLiquid = itemWorld->get().liquidLevel(pos).liquid;
      if (tileLiquid == EmptyLiquidId || tileLiquid == liquidId())
        return true;
    }
  }
  return false;
}

bool LiquidItem::multiplaceEnabled() const {
  return (count() > 1);
}

}
