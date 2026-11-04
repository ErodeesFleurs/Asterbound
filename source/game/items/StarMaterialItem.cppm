module;
import std;
import star.type_identity;
import star.xx_hash;
import star.json;
import star.json_extra;
import star.id_map;
import star.data_stream;
import star.bi_map;
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
import star.color;
import star.string;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.game_types;
import star.material_types;
import star.ordered_map;
import star.lru_cache;
import star.time;
import star.random;
import star.ttl_cache;
import star.image;
import star.math_common;
import std;
import star.format;
import star.version;
import star.either;
import star.weighted_pool;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import star.net_compatibility;
import star.worker_pool;

import std;
import star.sector_array_2d;
import star.perlin;
import star.btree;
import star.data_stream_devices;
import star.btree_database;
import star.ordered_set;
import star.input_event;
import star.periodic_function;


import star.lua_root;
import star.lua_components;
import star.lua_actor_movement_component;
import star.lua_animation_component;
import star.item_descriptor;
import star.item;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.entity;
import star.interaction_types;
import star.quest_descriptor;
import star.interactive_entity;
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
import star.status_effect_item;
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
import star.renderable_item;
import star.previewable_item;
import star.collision_block;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.tile_sector_array;
import star.world_parameters;
import star.celestial_parameters;
import star.world_layout;
import star.celestial_types;
import star.chat_types;
import star.uuid;
import star.warping;
import star.wiring;
import star.inspectable_entity;
import star.plant;
import star.biome_placement;
import star.versioning_database;
import star.world_storage;
import star.player_types;
import star.system_world;
import star.damage_manager;
import star.net_packets;
import star.world_structure;
import star.chat_action;
import star.world_client_state;
import star.interpolation_tracker;
import star.ambient;
import star.weather;
import star.world_client;
import star.world_template;
import star.input;
import star.liquid_types;
import star.tile_damage;
import star.collision_generator;
import star.world_tiles;
import star.sky_types;
import star.celestial_coordinate;
import star.sky_parameters;
import star.sky_render_data;
import star.plant_database;
import star.parallax;
import star.weather_types;
import star.cellular_light_array;
import star.cellular_lighting;
import star.world_render_data;
import star.material_render_profile;
import star.animated_part_set;
import star.networked_animator;
import star.humanoid;
import star.lounging_entities;
import star.chatty_entity;
import star.emote_entity;
import star.portrait_entity;
import star.damage_bar_entity;
import star.nametag_entity;
import star.inventory_types;
import star.ai_types;
import star.radio_message_database;
import star.player;

export module star.material_item;
import star.material_database;
import star.tile_drawer;


export namespace Star {

class MaterialItem : public Item, public FireableItem, public PreviewTileTool, public RenderableItem, public PreviewableItem, public BeamItem {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<MaterialItem, Item, FireableItem, PreviewTileTool, RenderableItem, PreviewableItem, BeamItem>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<MaterialItem>();
  }

  [[nodiscard]]
  static std::expected<std::shared_ptr<MaterialItem>, StarException> create(Root& root, Json const& config, String const& directory, Json const& settings);
  virtual ~MaterialItem() {}

  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> clone() const override;

  [[nodiscard]]
  std::expected<void, StarException> init(ToolUserEntity& owner, ToolHand hand) override;
  [[nodiscard]]
  std::expected<void, StarException> uninit() override;
  [[nodiscard]]
  std::expected<void, StarException> update(float dt, FireMode fireMode, bool shifting, HashSet<MoveControlType> const& moves) override;
  [[nodiscard]]
  std::expected<void, StarException> render(RenderCallback& renderCallback, EntityRenderLayer renderLayer) override;

  [[nodiscard]]
  std::expected<List<Drawable>, StarException> preview(std::optional<std::reference_wrapper<TileDrawer>> tileDrawer, std::shared_ptr<Player> const& viewer = {}) const override;
  [[nodiscard]]
  std::expected<List<Drawable>, StarException> dropDrawables(std::optional<std::reference_wrapper<TileDrawer>> tileDrawer) const override;
  [[nodiscard]]
  std::expected<bool, StarException> beamCanPlace() const override;

  [[nodiscard]]
  std::expected<void, StarException> fire(FireMode mode, bool shifting, bool edgeTriggered) override;
  [[nodiscard]]
  std::expected<void, StarException> endFire(FireMode mode, bool shifting) override;

  MaterialId materialId() const;
  MaterialHue materialHueShift() const;

  [[nodiscard]]
  std::expected<bool, StarException> canPlace(bool shifting) const;
  bool multiplaceEnabled() const;

  float& blockRadius();
  float& altBlockRadius();
  TileCollisionOverride& collisionOverride();

  [[nodiscard]]
  std::expected<List<PreviewTile>, StarException> previewTiles(bool shifting) const override;
  [[nodiscard]]
  std::expected<std::reference_wrapper<List<Drawable> const>, StarException> generatedPreview(std::optional<std::reference_wrapper<TileDrawer>> tileDrawer, Vec2I position = {}) const;
private:
  explicit MaterialItem(Root& root) : Item(root), BeamItem(root) {}
  [[nodiscard]]
  std::expected<std::size_t, StarException> blockSwap(float radius, TileLayer layer);
  [[nodiscard]]
  std::expected<void, StarException> updatePropertiesFromPlayer(Player& player);
  float calcRadius(bool shifting) const;
  List<Vec2I>& tileArea(float radius, Vec2F const& position) const;
  [[nodiscard]]
  std::expected<MaterialHue, StarException> placementHueShift(Vec2I const& position) const;

  MaterialId m_material;
  MaterialHue m_materialHueShift;

  float m_blockRadius;
  float m_altBlockRadius;
  bool m_blockSwap;
  bool m_shifting;
  bool m_multiplace;
  StringList m_placeSounds;
  std::optional<Vec2F> m_lastAimPosition;
  TileCollisionOverride m_collisionOverride;

  mutable Vec2F m_lastTileAreaOriginCache;
  mutable float m_lastTileAreaRadiusCache;
  mutable List<Vec2I> m_tileAreasCache;

  mutable std::optional<List<Drawable>> m_generatedPreviewCache;
  mutable bool m_generatedPreviewUsesTileDrawer = false;
};

}


namespace Star {

constexpr int BlockRadiusLimit = 16;
const String BlockRadiusPropertyKey = "building.blockRadius";
const String AltBlockRadiusPropertyKey = "building.altBlockRadius";
const String CollisionOverridePropertyKey = "building.collisionOverride";
const String BlockSwapPropertyKey = "building.blockSwap";

std::expected<std::shared_ptr<MaterialItem>, StarException> MaterialItem::create(Root& root, Json const& config, String const& directory, Json const& settings) {
  auto item = std::shared_ptr<MaterialItem>(new MaterialItem(root));
  auto initialized = item->initialize(config, directory, settings);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  auto fireable = item->initializeFireable(config);
  if (!fireable)
    return std::unexpected(std::move(fireable.error()));
  auto beam = item->initializeBeam(config);
  if (!beam)
    return std::unexpected(std::move(beam.error()));
  auto material = config.getInt("materialId");
  if (!material)
    return std::unexpected(std::move(material.error()));
  item->m_material = *material;
  auto hue = item->instanceValue("materialHueShift", 0).and_then([](Json const& value) { return value.toFloat(); });
  if (!hue)
    return std::unexpected(std::move(hue.error()));
  item->m_materialHueShift = materialHueFromDegrees(*hue);
  auto database = root.materialDatabase();
  if (!database)
    return std::unexpected(std::move(database.error()));
  if (item->materialHueShift() != MaterialHue()) {
    auto drawables = item->iconDrawables();
    if (!drawables)
      return std::unexpected(std::move(drawables.error()));
    for (auto& drawable : *drawables) {
      if (drawable.isImage()) {
        auto metadata = root.imageMetadataDatabase();
        if (!metadata)
          return std::unexpected(std::move(metadata.error()));
        auto added = drawable.imagePart().addDirectives(**metadata, Directives(String(strf("?hueshift={}", materialHueToDegrees(item->m_materialHueShift)))), false);
        if (!added)
          return std::unexpected(std::move(added.error()));
      }
    }
    auto icons = item->setIconDrawables(std::move(*drawables));
    if (!icons)
      return std::unexpected(std::move(icons.error()));
  }
  auto twoHanded = config.getBool("twoHanded", true);
  if (!twoHanded)
    return std::unexpected(std::move(twoHanded.error()));
  item->setTwoHanded(*twoHanded);
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto defaults = (*assets)->json("/items/defaultParameters.config");
  if (!defaults)
    return std::unexpected(std::move(defaults.error()));
  auto defaultCooldown = defaults->queryFloat("materialItems.cooldown");
  if (!defaultCooldown)
    return std::unexpected(std::move(defaultCooldown.error()));
  auto cooldown = config.queryFloat("materialItems.cooldown", *defaultCooldown);
  if (!cooldown)
    return std::unexpected(std::move(cooldown.error()));
  item->setCooldownTime(*cooldown);
  auto defaultRadius = defaults->getFloat("blockRadius");
  if (!defaultRadius)
    return std::unexpected(std::move(defaultRadius.error()));
  auto radius = config.getFloat("blockRadius", *defaultRadius);
  if (!radius)
    return std::unexpected(std::move(radius.error()));
  item->m_blockRadius = *radius;
  auto defaultAlt = defaults->getFloat("altBlockRadius");
  if (!defaultAlt)
    return std::unexpected(std::move(defaultAlt.error()));
  auto alt = config.getFloat("altBlockRadius", *defaultAlt);
  if (!alt)
    return std::unexpected(std::move(alt.error()));
  item->m_altBlockRadius = *alt;
  auto overrideName = config.getString("collisionOverride", "None");
  if (!overrideName)
    return std::unexpected(std::move(overrideName.error()));
  if (!TileCollisionOverrideNames)
    return std::unexpected(TileCollisionOverrideNames.error());
  auto collisionOverride = TileCollisionOverrideNames->maybeLeft(*overrideName);
  if (!collisionOverride)
    return std::unexpected(std::move(collisionOverride.error()));
  item->m_collisionOverride = collisionOverride->value_or(TileCollisionOverride::None);
  item->m_blockSwap = false;
  auto collision = (*database)->materialCollisionKind(item->m_material);
  if (!collision)
    return std::unexpected(std::move(collision.error()));
  auto multiplace = config.getBool("allowMultiplace", BlockCollisionSet.contains(*collision));
  if (!multiplace)
    return std::unexpected(std::move(multiplace.error()));
  item->m_multiplace = *multiplace;
  auto sounds = config.get("placeSounds", JsonArray()).and_then(jsonToStringList);
  if (!sounds)
    return std::unexpected(std::move(sounds.error()));
  item->m_placeSounds = std::move(*sounds);
  if (item->m_placeSounds.empty()) {
    auto mining = (*database)->miningSound(item->m_material);
    if (!mining)
      return std::unexpected(std::move(mining.error()));
    if (!mining->empty())
      item->m_placeSounds.append(std::move(*mining));
    auto step = (*database)->footstepSound(item->m_material);
    if (!step)
      return std::unexpected(std::move(step.error()));
    if (!step->empty())
      item->m_placeSounds.append(std::move(*step));
    else if (item->m_placeSounds.empty())
      item->m_placeSounds.append((*database)->defaultFootstepSound());
  }
  item->m_shifting = false;
  item->m_lastTileAreaRadiusCache = 0.0f;
  return item;
}

std::expected<std::shared_ptr<Item>, StarException> MaterialItem::clone() const {
  return std::make_shared<MaterialItem>(*this);
}

std::expected<void, StarException> MaterialItem::init(ToolUserEntity& owner, ToolHand hand) {
  auto fireable = FireableItem::init(owner, hand);
  if (!fireable)
    return std::unexpected(std::move(fireable.error()));
  auto beam = BeamItem::init(owner, hand);
  if (!beam)
    return std::unexpected(std::move(beam.error()));
  auto sound = owner.addSound(root().random().randValueFrom(m_placeSounds), 1.0f, 2.0f);
  if (!sound)
    return std::unexpected(std::move(sound.error()));
  if (auto player = asRef<Player>(owner))
    return updatePropertiesFromPlayer(player->get());
  return {};
}

std::expected<void, StarException> MaterialItem::uninit() {
  auto uninitialized = FireableItem::uninit();
  if (!uninitialized)
    return std::unexpected(std::move(uninitialized.error()));
  m_lastAimPosition.reset();
  return {};
}

std::expected<void, StarException> MaterialItem::update(float dt, FireMode fireMode, bool shifting, HashSet<MoveControlType> const& moves) {
  auto fireable = FireableItem::update(dt, fireMode, shifting, moves);
  if (!fireable)
    return std::unexpected(std::move(fireable.error()));
  float radius = calcRadius(shifting);
  if (radius == 1)
    setEnd(BeamItem::EndType::Tile);
  else
    setEnd(BeamItem::EndType::TileGroup);
  m_shifting = shifting;
  auto beam = BeamItem::update(dt, fireMode, shifting, moves);
  if (!beam)
    return std::unexpected(std::move(beam.error()));

  auto itemOwner = owner();
  if (!itemOwner)
    return std::unexpected(std::move(itemOwner.error()));
  if (auto player = asRef<Player>(itemOwner->get())) {
    if (auto input = itemOwner->get().isMaster() ? player->get().input() : std::nullopt) {
      if (auto presses = input->get().bindDown("opensb", "materialCollisionCycle")) {
        auto database = root().materialDatabase();
        if (!database)
          return std::unexpected(std::move(database.error()));
        auto baseKind = (*database)->materialCollisionKind(m_material);
        if (!baseKind)
          return std::unexpected(std::move(baseKind.error()));
        for (std::size_t i = 0; i != *presses; ++i) {
          constexpr auto limit = (std::uint8_t)TileCollisionOverride::Block + 1;
          while (true) {
            m_collisionOverride = TileCollisionOverride(((std::uint8_t)m_collisionOverride + 1) % limit);
            if (collisionKindFromOverride(m_collisionOverride) != *baseKind)
              break;
          }
          if (!TileCollisionOverrideNames)
            return std::unexpected(TileCollisionOverrideNames.error());
          auto name = TileCollisionOverrideNames->getRight(m_collisionOverride);
          if (!name)
            return std::unexpected(std::move(name.error()));
          auto stored = player->get().setSecretProperty(CollisionOverridePropertyKey, name->get());
          if (!stored)
            return std::unexpected(std::move(stored.error()));
        }
        auto pitch = root().random().randf(0.9f, 1.1f);
        if (!pitch)
          return std::unexpected(std::move(pitch.error()));
        auto sound = itemOwner->get().addSound("/sfx/tools/cyclematcollision.ogg", 1.0f, *pitch);
        if (!sound)
          return std::unexpected(std::move(sound.error()));
      }

      if (auto presses = input->get().bindDown("opensb", "buildingRadiusGrow")) {
        m_blockRadius = std::min(BlockRadiusLimit, int(m_blockRadius + *presses));
        auto stored = player->get().setSecretProperty(BlockRadiusPropertyKey, m_blockRadius);
        if (!stored)
          return std::unexpected(std::move(stored.error()));
        auto sound = itemOwner->get().addSound("/sfx/tools/buildradiusgrow.wav", 1.0f, 1.0f + m_blockRadius / BlockRadiusLimit);
        if (!sound)
          return std::unexpected(std::move(sound.error()));
      }

      if (auto presses = input->get().bindDown("opensb", "buildingRadiusShrink")) {
        m_blockRadius = std::max(1, int(m_blockRadius - *presses));
        auto stored = player->get().setSecretProperty(BlockRadiusPropertyKey, m_blockRadius);
        if (!stored)
          return std::unexpected(std::move(stored.error()));
        auto sound = itemOwner->get().addSound("/sfx/tools/buildradiusshrink.wav", 1.0f, 1.0f + m_blockRadius / BlockRadiusLimit);
        if (!sound)
          return std::unexpected(std::move(sound.error()));
      }

      if (auto presses = input->get().bindDown("opensb", "blockSwapToggle")) {
        if (*presses % 2 != 0)
          m_blockSwap = !m_blockSwap;
        auto stored = player->get().setSecretProperty(BlockSwapPropertyKey, m_blockSwap);
        if (!stored)
          return std::unexpected(std::move(stored.error()));
        auto pitch = root().random().randf(0.9f, 1.1f);
        if (!pitch)
          return std::unexpected(std::move(pitch.error()));
        auto sound = itemOwner->get().addSound(m_blockSwap ? std::string_view("/sfx/interface/button/click.wav") : std::string_view("/sfx/interface/button/release.wav"), 1.0f, *pitch);
        if (!sound)
          return std::unexpected(std::move(sound.error()));
      }
    }
    else
      return updatePropertiesFromPlayer(player->get());
  }
  return {};
}

std::expected<void, StarException> MaterialItem::render(RenderCallback& renderCallback, EntityRenderLayer) {
  if (m_blockSwap || m_collisionOverride != TileCollisionOverride::None) {
    auto itemOwner = owner();
    if (!itemOwner)
      return std::unexpected(std::move(itemOwner.error()));
    float pulse = (float)std::sin(2 * std::numbers::pi * 4.0 * Time::monotonicTime());
    float pulseA = 0.85 - pulse * 0.15f;
    float pulseB = 0.85 + pulse * 0.15f;
    Color color = itemOwner->get().favoriteColor().mix(Color::White);
    float alpha = color.alphaF();
    color.setAlphaF(alpha * pulseA * 0.95f);
    auto addIndicator = [&](String const& path) -> std::expected<void, StarException> {
      Vec2F basePosition = Vec2F(0.5f, 0.5f);
      auto metadata = root().imageMetadataDatabase();
      if (!metadata)
        return std::unexpected(std::move(metadata.error()));
      auto indicator = Drawable::makeImage(**metadata, path, 1.0f / TilePixels, true, basePosition);
      if (!indicator)
        return std::unexpected(std::move(indicator.error()));
      indicator->fullbright = true;
      indicator->color = color;
      auto aimPosition = itemOwner->get().aimPosition();
      if (!aimPosition)
        return std::unexpected(std::move(aimPosition.error()));
      for (auto& tilePos : tileArea(calcRadius(m_shifting), *aimPosition)) {
        indicator->position = basePosition + Vec2F(tilePos);
        auto added = renderCallback.addDrawable(*indicator, RenderLayerForegroundTile);
        if (!added)
          return std::unexpected(std::move(added.error()));
      }
      return {};
    };

    if (m_blockSwap) {
      auto prev = color;
      color.hueShift(0.167f);
      color.setAlphaF(alpha * pulseB * 0.95f);
      auto added = addIndicator("/interface/building/blockswap.png");
      if (!added)
        return std::unexpected(std::move(added.error()));
      color = prev;
    }

    if (m_collisionOverride == TileCollisionOverride::Empty)
      return addIndicator("/interface/building/collisionempty.png");
    else if (m_collisionOverride == TileCollisionOverride::Platform)
      return addIndicator("/interface/building/collisionplatform.png");
    else if (m_collisionOverride == TileCollisionOverride::Block)
      return addIndicator("/interface/building/collisionblock.png");
  }
  return {};
}

std::expected<List<Drawable>, StarException> MaterialItem::preview(std::optional<std::reference_wrapper<TileDrawer>> tileDrawer, std::shared_ptr<Player> const&) const {
  return generatedPreview(tileDrawer).transform([](std::reference_wrapper<List<Drawable> const> drawables) { return drawables.get(); });
}

std::expected<List<Drawable>, StarException> MaterialItem::dropDrawables(std::optional<std::reference_wrapper<TileDrawer>> tileDrawer) const {
  return generatedPreview(tileDrawer).transform([](std::reference_wrapper<List<Drawable> const> drawables) { return drawables.get(); });
}

std::expected<bool, StarException> MaterialItem::beamCanPlace() const {
  return canPlace(m_shifting);
}

std::expected<void, StarException> MaterialItem::fire(FireMode mode, bool shifting, bool edgeTriggered) {
  if (!initialized() || !ready())
    return {};

  auto layer = (mode == FireMode::Primary || !twoHanded() ? TileLayer::Foreground : TileLayer::Background);
  TileModificationList modifications;

  float radius = calcRadius(shifting);

  auto itemWorld = world();
  if (!itemWorld)
    return std::unexpected(std::move(itemWorld.error()));
  auto itemOwner = owner();
  if (!itemOwner)
    return std::unexpected(std::move(itemOwner.error()));
  auto geo = itemWorld->get().geometry();
  auto aim = itemOwner->get().aimPosition();
  if (!aim)
    return std::unexpected(std::move(aim.error()));
  auto aimPosition = *aim;

  if (!m_lastAimPosition)
    m_lastAimPosition = aimPosition;

  unsigned steps = 1;
  Vec2F diff = {};
  if (*m_lastAimPosition != aimPosition) {
    diff = geo.diff(*m_lastAimPosition, aimPosition);
    float magnitude = diff.magnitude();
    float limit = std::max(4.f, 64.f / radius);
    if (magnitude > limit) {
      diff = diff.normalized() * limit;
      magnitude = limit;
    }

    steps = (unsigned)std::ceil(magnitude * (std::numbers::pi / 2));
  }

  CollisionKind collisionKind;
  if (m_collisionOverride != TileCollisionOverride::None) {
    collisionKind = collisionKindFromOverride(m_collisionOverride);
  } else {
    auto database = root().materialDatabase();
    if (!database)
      return std::unexpected(std::move(database.error()));
    auto kind = (*database)->materialCollisionKind(m_material);
    if (!kind)
      return std::unexpected(std::move(kind.error()));
    collisionKind = *kind;
  }

  std::size_t total = 0;

  if (m_blockSwap) {
    auto range = itemOwner->get().inToolRange(aimPosition);
    if (!range)
      return std::unexpected(std::move(range.error()));
    if (*range) {
      auto swapped = blockSwap(radius, layer);
      if (!swapped)
        return std::unexpected(std::move(swapped.error()));
      total += *swapped;
    }
  }
  
  for (unsigned i = 0; i != steps; ++i) {
    auto placementOrigin = aimPosition + diff * (1.0f - (static_cast<float>(i) / steps));
    auto range = itemOwner->get().inToolRange(placementOrigin);
    if (!range)
      return std::unexpected(std::move(range.error()));
    if (!*range)
      continue;

    for (Vec2I& pos : tileArea(radius, placementOrigin)) {
      auto hue = placementHueShift(pos);
      if (!hue)
        return std::unexpected(std::move(hue.error()));
      modifications.emplaceAppend(pos, PlaceMaterial{layer, materialId(), *hue, m_collisionOverride});
    }

    // Make sure not to make any more modifications than we have consumables.
    if (modifications.size() > count())
      modifications.resize(count());
    auto failed = itemWorld->get().applyTileModifications(modifications, collisionKind <= CollisionKind::Platform);
    if (!failed)
      return std::unexpected(std::move(failed.error()));
    if (failed->size() < modifications.size()) {
      std::size_t placed = modifications.size() - failed->size();
      consume(placed);
      total += placed;
    }
  }

  if (total) {
    float intensity = std::clamp(std::sqrt((float)total) / 16, 0.0f, 1.0f);
    auto sound = root().random().randValueFrom(m_placeSounds);
    auto pitch = root().random().randf(0.9f, 1.1f);
    if (!pitch)
      return std::unexpected(std::move(pitch.error()));
    auto played = itemOwner->get().addSound(sound, 1.0f + intensity, (1.125f - intensity * 0.75f) * *pitch);
    if (!played)
      return std::unexpected(std::move(played.error()));
    auto fired = FireableItem::fire(mode, shifting, edgeTriggered);
    if (!fired)
      return std::unexpected(std::move(fired.error()));
  }

  m_lastAimPosition = aimPosition;
  return {};
}

std::expected<void, StarException> MaterialItem::endFire(FireMode, bool) {
  m_lastAimPosition.reset();
  return {};
}

std::expected<std::size_t, StarException> MaterialItem::blockSwap(float radius, TileLayer layer) {
  auto itemOwner = owner();
  if (!itemOwner)
    return std::unexpected(std::move(itemOwner.error()));
  auto player = asRef<Player>(itemOwner->get());
  if (!player)
    return 0;
  
  std::shared_ptr<Item> beamAxePtr = player->get().essentialItem(EssentialItem::BeamAxe);
  if (!beamAxePtr)
    return 0;
  
  Item& beamAxe = *beamAxePtr;
  auto itemWorld = world();
  if (!itemWorld)
    return std::unexpected(std::move(itemWorld.error()));

  List<Vec2I> swapPositions;
  auto aimPosition = itemOwner->get().aimPosition();
  if (!aimPosition)
    return std::unexpected(std::move(aimPosition.error()));
  for (Vec2I& pos : tileArea(radius, *aimPosition)) {
    auto connectable = itemWorld->get().isTileConnectable(pos, layer, true);
    if (!connectable)
      return std::unexpected(std::move(connectable.error()));
    if (!*connectable)
      continue;
    if (itemWorld->get().isTileProtected(pos))
      continue;
    if (itemWorld->get().material(pos, layer) == materialId())
      continue;
    swapPositions.append(pos);
  }

  if (swapPositions.empty())
    return 0;
  if (swapPositions.size() > count())
    swapPositions.resize(count());

  TileDamage damage;
  damage.type = TileDamageType::Beamish;
  auto defaultDamage = beamAxe.instanceValue("primaryAbility.tileDamage", 1.0f);
  if (!defaultDamage)
    return std::unexpected(std::move(defaultDamage.error()));
  auto amount = beamAxe.instanceValue("tileDamage", *defaultDamage).and_then([](Json const& value) { return value.toFloat(); });
  if (!amount)
    return std::unexpected(std::move(amount.error()));
  damage.amount = *amount;
  auto defaultHarvest = beamAxe.instanceValue("primaryAbility.harvestLevel", 1);
  if (!defaultHarvest)
    return std::unexpected(std::move(defaultHarvest.error()));
  auto harvest = beamAxe.instanceValue("harvestLevel", *defaultHarvest).and_then([](Json const& value) { return value.toUInt(); });
  if (!harvest)
    return std::unexpected(std::move(harvest.error()));
  damage.harvestLevel = *harvest;

  TileModificationList toSwap;
  List<Vec2I> willDamage;
  for (auto pos : swapPositions) {
    auto hue = placementHueShift(pos);
    if (!hue)
      return std::unexpected(std::move(hue.error()));
    toSwap.emplaceAppend(pos, PlaceMaterial{layer, materialId(), *hue, m_collisionOverride});
    auto destroys = itemWorld->get().damageWouldDestroy(pos, layer, damage);
    if (!destroys)
      return std::unexpected(std::move(destroys.error()));
    if (!*destroys)
      willDamage.append(pos);
  }
  
  std::size_t success;
  auto failed = itemWorld->get().replaceTiles(toSwap, damage, true);
  if (!failed)
    return std::unexpected(std::move(failed.error()));

  if (failed->size() < toSwap.size()) {
    success = toSwap.size() - failed->size();
    consume(success);
  } else {
    List<Vec2I> toDamage;

    for (auto pair : toSwap)
      toDamage.append(pair.first);
    
    auto damaged = itemWorld->get().damageTiles(toDamage, layer, itemOwner->get().position(), damage, itemOwner->get().entityId());
    if (!damaged)
      return std::unexpected(std::move(damaged.error()));
  }

  if (willDamage.empty())
    return success;

  auto materialDatabase = root().materialDatabase();
  if (!materialDatabase)
    return std::unexpected(std::move(materialDatabase.error()));
  auto assets = root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  String blockSound;

  for (auto pos : willDamage) {
    auto sound = (*materialDatabase)->miningSound(itemWorld->get().material(pos, layer), itemWorld->get().mod(pos, layer));
    if (!sound)
      return std::unexpected(std::move(sound.error()));
    blockSound = std::move(*sound);
    if (!blockSound.empty())
      break;
  }

  if (blockSound.empty()) {
    for (auto pos : willDamage) {
      auto sound = (*materialDatabase)->footstepSound(itemWorld->get().material(pos, layer), itemWorld->get().mod(pos, layer));
      if (!sound)
        return std::unexpected(std::move(sound.error()));
      blockSound = std::move(*sound);
      if (!blockSound.empty()) {
        auto defaultSound = (*assets)->json("/client.config:defaultFootstepSound").and_then([](Json const& value) { return value.toString(); });
        if (!defaultSound)
          return std::unexpected(std::move(defaultSound.error()));
        if (blockSound != *defaultSound)
          break;
      }
    }
  }

  auto blockVolume = (*assets)->json("/sfx.config:miningBlockVolume").and_then([](Json const& value) { return value.toFloat(); });
  if (!blockVolume)
    return std::unexpected(std::move(blockVolume.error()));
  auto played = itemOwner->get().addSound(blockSound, *blockVolume);
  if (!played)
    return std::unexpected(std::move(played.error()));
  auto strikeSounds = beamAxe.instanceValue("strikeSounds");
  if (!strikeSounds)
    return std::unexpected(std::move(strikeSounds.error()));
  if (!strikeSounds->isNull()) {
    auto sounds = jsonToStringList(*strikeSounds);
    if (!sounds)
      return std::unexpected(std::move(sounds.error()));
    auto sound = root().random().randValueFrom(*sounds);
    auto toolVolume = (*assets)->json("/sfx.config:miningToolVolume").and_then([](Json const& value) { return value.toFloat(); });
    if (!toolVolume)
      return std::unexpected(std::move(toolVolume.error()));
    auto played = itemOwner->get().addSound(sound, *toolVolume);
    if (!played)
      return std::unexpected(std::move(played.error()));
  }

  if (auto item = asRef<FireableItem>(beamAxe))
    setFireTimer(item->get().windupTime() + item->get().cooldownTime());
  
  return success;
}

MaterialId MaterialItem::materialId() const {
  return m_material;
}

std::expected<std::reference_wrapper<List<Drawable> const>, StarException> MaterialItem::generatedPreview(std::optional<std::reference_wrapper<TileDrawer>> tileDrawer, Vec2I position) const {
  if (!m_generatedPreviewCache || m_generatedPreviewUsesTileDrawer != tileDrawer.has_value()) {
    if (tileDrawer) {
      auto locker = tileDrawer->get().lockRenderData();
      WorldRenderData& renderData = tileDrawer->get().renderData();
      renderData.geometry = WorldGeometry(3, 3);
      renderData.tiles.resize({ 3, 3 });
      renderData.tiles.fill(TileDrawer::DefaultRenderTile);
      renderData.tileMinPosition = { 0, 0 };
      auto tile = renderData.tiles.at({1, 1});
      if (!tile)
        return std::unexpected(std::move(tile.error()));
      tile->get().foreground = m_material;
      tile->get().foregroundHueShift = m_materialHueShift;
      tile->get().foregroundColorVariant = 0;

      List<Drawable> drawables;
      TileDrawer::Drawables tileDrawables;
      auto database = root().materialDatabase();
      if (!database)
        return std::unexpected(std::move(database.error()));
      auto collision = (*database)->materialCollisionKind(m_material);
      if (!collision)
        return std::unexpected(std::move(collision.error()));
      bool isBlock = BlockCollisionSet.contains(*collision);
      TileDrawer::TerrainLayer layer = isBlock ? TileDrawer::TerrainLayer::Foreground : TileDrawer::TerrainLayer::Midground;
      for (int x = 0; x != 3; ++x) {
        for (int y = 0; y != 3; ++y) {
          auto produced = tileDrawer->get().produceTerrainDrawables(tileDrawables, layer, {x, y}, renderData, 1.0f / TilePixels, position - Vec2I(1, 1));
          if (!produced)
            return std::unexpected(std::move(produced.error()));
        }
      }

      locker.unlock();
      for (auto& index : tileDrawables.keys()) {
        auto taken = tileDrawables.take(index);
        if (!taken)
          return std::unexpected(std::move(taken.error()));
        drawables.appendAll(std::move(*taken));
      }
      auto metadata = root().imageMetadataDatabase();
      if (!metadata)
        return std::unexpected(std::move(metadata.error()));
      auto boundBox = Drawable::boundBoxAll(**metadata, drawables, true);
      if (!boundBox)
        return std::unexpected(std::move(boundBox.error()));
      if (!boundBox->isEmpty()) {
        for (auto& drawable : drawables)
          drawable.translate(-boundBox->center());
      }

      m_generatedPreviewCache.emplace(std::move(drawables));
    }
    else {
      auto icons = iconDrawables();
      if (!icons)
        return std::unexpected(std::move(icons.error()));
      m_generatedPreviewCache.emplace(std::move(*icons));
    }
    m_generatedPreviewUsesTileDrawer = tileDrawer.has_value();
  }
  return std::cref(*m_generatedPreviewCache);
}

std::expected<void, StarException> MaterialItem::updatePropertiesFromPlayer(Player& player) {
  auto blockRadius = player.getSecretProperty(BlockRadiusPropertyKey);
  if (!blockRadius)
    return std::unexpected(std::move(blockRadius.error()));
  if (blockRadius->isType(Json::Type::Float)) {
    auto value = blockRadius->toFloat();
    if (!value)
      return std::unexpected(std::move(value.error()));
    m_blockRadius = *value;
  }
  auto altRadius = player.getSecretProperty(AltBlockRadiusPropertyKey);
  if (!altRadius)
    return std::unexpected(std::move(altRadius.error()));
  if (altRadius->isType(Json::Type::Float)) {
    auto value = altRadius->toFloat();
    if (!value)
      return std::unexpected(std::move(value.error()));
    m_altBlockRadius = *value;
  }
  auto collision = player.getSecretProperty(CollisionOverridePropertyKey);
  if (!collision)
    return std::unexpected(std::move(collision.error()));
  if (collision->isType(Json::Type::String)) {
    auto name = collision->toString();
    if (!name)
      return std::unexpected(std::move(name.error()));
    if (!TileCollisionOverrideNames)
      return std::unexpected(TileCollisionOverrideNames.error());
    auto value = TileCollisionOverrideNames->maybeLeft(*name);
    if (!value)
      return std::unexpected(std::move(value.error()));
    m_collisionOverride = value->value_or(TileCollisionOverride::None);
  }
  auto blockSwap = player.getSecretProperty(BlockSwapPropertyKey);
  if (!blockSwap)
    return std::unexpected(std::move(blockSwap.error()));
  if (blockSwap->isType(Json::Type::Bool)) {
    auto value = blockSwap->toBool();
    if (!value)
      return std::unexpected(std::move(value.error()));
    m_blockSwap = *value;
  }
  return {};
}

float MaterialItem::calcRadius(bool shifting) const {
  if (!multiplaceEnabled())
    return 1;
  else
    return !shifting ? m_blockRadius : m_altBlockRadius;
}

List<Vec2I>& MaterialItem::tileArea(float radius, Vec2F const& position) const {
  if (m_lastTileAreaOriginCache != position || m_lastTileAreaRadiusCache != radius) {
    m_lastTileAreaOriginCache = position;
    m_lastTileAreaRadiusCache = radius;
    m_tileAreasCache = tileAreaBrush(radius, position, true);
  }
  return m_tileAreasCache;
}

MaterialHue MaterialItem::materialHueShift() const {
  return m_materialHueShift;
}

std::expected<bool, StarException> MaterialItem::canPlace(bool shifting) const {
  if (initialized()) {
    auto itemOwner = owner();
    if (!itemOwner)
      return std::unexpected(std::move(itemOwner.error()));
    auto itemWorld = world();
    if (!itemWorld)
      return std::unexpected(std::move(itemWorld.error()));
    MaterialId material = materialId();
    float radius = calcRadius(shifting);
    auto aimPosition = itemOwner->get().aimPosition();
    if (!aimPosition)
      return std::unexpected(std::move(aimPosition.error()));
    for (auto& pos : tileArea(radius, *aimPosition)) {
      auto hue = placementHueShift(pos);
      if (!hue)
        return std::unexpected(std::move(hue.error()));
      auto foreground = itemWorld->get().canModifyTile(pos, PlaceMaterial{TileLayer::Foreground, material, *hue}, false);
      if (!foreground)
        return std::unexpected(std::move(foreground.error()));
      if (*foreground)
        return true;
      auto background = itemWorld->get().canModifyTile(pos, PlaceMaterial{TileLayer::Background, material, *hue}, false);
      if (!background)
        return std::unexpected(std::move(background.error()));
      if (*background)
        return true;
    }
  }
  return false;
}

bool MaterialItem::multiplaceEnabled() const {
  return m_multiplace && count() > 1;
}

float& MaterialItem::blockRadius() {
  return m_blockRadius;
}

float& MaterialItem::altBlockRadius() {
  return m_altBlockRadius;
}

TileCollisionOverride& MaterialItem::collisionOverride() {
  return m_collisionOverride;
}

std::expected<List<PreviewTile>, StarException> MaterialItem::previewTiles(bool shifting) const {
  List<PreviewTile> result;
  if (initialized()) {
    auto itemOwner = owner();
    if (!itemOwner)
      return std::unexpected(std::move(itemOwner.error()));
    auto itemWorld = world();
    if (!itemWorld)
      return std::unexpected(std::move(itemWorld.error()));
    Color lightColor = itemOwner->get().favoriteColor();
    Vec3B light = lightColor.toRgb();
    auto material = materialId();
    auto color = DefaultMaterialColorVariant;
    std::size_t c = 0;
    auto aimPosition = itemOwner->get().aimPosition();
    if (!aimPosition)
      return std::unexpected(std::move(aimPosition.error()));
    for (auto& pos : tileArea(calcRadius(shifting), *aimPosition)) {
      auto hue = placementHueShift(pos);
      if (!hue)
        return std::unexpected(std::move(hue.error()));
      if (c >= count())
        break;
      auto foreground = itemWorld->get().canModifyTile(pos, PlaceMaterial{TileLayer::Foreground, material, *hue}, false);
      if (!foreground)
        return std::unexpected(std::move(foreground.error()));
      if (*foreground) {
        result.append({pos, true, material, *hue, true});
        c++;
      } else {
        bool backPlaceable = false;
        if (twoHanded()) {
          auto background = itemWorld->get().canModifyTile(pos, PlaceMaterial{TileLayer::Background, material, *hue}, false);
          if (!background)
            return std::unexpected(std::move(background.error()));
          backPlaceable = *background;
        }
        if (backPlaceable) {
          result.append({pos, true, material, *hue, true, light, true, color});
          c++;
        } else {
          result.append({pos, true, material, *hue, true});
        }
      }
    }
  }
  return result;
}

std::expected<MaterialHue, StarException> MaterialItem::placementHueShift(Vec2I const& pos) const {
  auto hue = instanceValue("materialHueShift");
  if (!hue)
    return std::unexpected(std::move(hue.error()));
  if (*hue) {
    return hue->toFloat().transform(materialHueFromDegrees);
  }
  auto itemWorld = world();
  if (!itemWorld)
    return std::unexpected(std::move(itemWorld.error()));
  if (auto worldClient = asRef<WorldClient>(itemWorld->get())) {
    auto worldTemplate = worldClient->get().currentTemplate();
    auto biomeIndex = worldTemplate->blockBiomeIndex(pos[0], pos[1]);
    if (!biomeIndex)
      return std::unexpected(std::move(biomeIndex.error()));
    return worldTemplate->biomeMaterialHueShift(*biomeIndex, m_material);
  }
  return materialHueShift();
}

}
