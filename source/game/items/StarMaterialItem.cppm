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
import star.random;
import star.ttl_cache;
import star.image;
import star.math_common;
import std;
import star.format;
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
  struct ConstructionKey { explicit ConstructionKey() = default; };

public:
  MaterialItem(ConstructionKey, Root& root) : Item(root), BeamItem(root) {}

  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<MaterialItem, Item, FireableItem, PreviewTileTool, RenderableItem, PreviewableItem, BeamItem>(
      *this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<MaterialItem>(); }

  [[nodiscard]]
  static std::expected<std::shared_ptr<MaterialItem>, StarException> create(Root& root, Json const& config, String const& directory, Json const& settings);
  ~MaterialItem() override = default;

  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> clone() const override;

  [[nodiscard]]
  std::expected<void, StarException> init(ToolUserEntity& owner, ToolHand hand) override;
  [[nodiscard]]
  std::expected<void, StarException> uninit() override;
  [[nodiscard]]
  std::expected<void, StarException> update(float dt, FireMode fireMode, bool shifting, HashSet<MoveControlType> const& moves) override;
  [[nodiscard]]
  std::expected<void, StarException> render(RenderCallback& render_callback, EntityRenderLayer renderLayer) override;

  [[nodiscard]]
  std::expected<List<Drawable>, StarException> preview(std::optional<std::reference_wrapper<TileDrawer>> tile_drawer, std::shared_ptr<Player> const& viewer = {}) const override;
  [[nodiscard]]
  std::expected<List<Drawable>, StarException> drop_drawables(std::optional<std::reference_wrapper<TileDrawer>> tile_drawer) const override;
  [[nodiscard]]
  std::expected<bool, StarException> beam_can_place() const override;

  [[nodiscard]]
  std::expected<void, StarException> fire(FireMode mode, bool shifting, bool edge_triggered) override;
  [[nodiscard]]
  std::expected<void, StarException> endFire(FireMode mode, bool shifting) override;

  MaterialId material_id() const;
  MaterialHue material_hue_shift() const;

  [[nodiscard]]
  std::expected<bool, StarException> can_place(bool shifting) const;
  bool multiplace_enabled() const;

  float& block_radius();
  float& alt_block_radius();
  TileCollisionOverride& collision_override();

  [[nodiscard]]
  std::expected<List<PreviewTile>, StarException> preview_tiles(bool shifting) const override;
  [[nodiscard]]
  std::expected<std::reference_wrapper<List<Drawable> const>, StarException> generated_preview(std::optional<std::reference_wrapper<TileDrawer>> tile_drawer, Vec2I position = {}) const;
private:
  using PlacementHue = std::variant<MaterialHue, std::shared_ptr<WorldTemplate const>>;

  [[nodiscard]]
  std::expected<std::size_t, StarException> block_swap(float radius, TileLayer layer, std::optional<PlacementHue>& placement_hue);
  [[nodiscard]]
  std::expected<void, StarException> updatePropertiesFromPlayer(Player& player);
  float calc_radius(bool shifting) const;
  List<Vec2I> const& tile_area(float radius, Vec2F const& position) const;
  [[nodiscard]]
  std::expected<void, StarException> prepare_placement_hue(std::optional<PlacementHue>& placement_hue, World const& world) const;
  [[nodiscard]]
  std::expected<MaterialHue, StarException> placement_hue_at(PlacementHue const& placement_hue, Vec2I const& position) const;

  MaterialId m_material;
  MaterialHue m_material_hue_shift;

  float m_blockRadius;
  float m_altBlockRadius;
  bool m_blockSwap;
  bool m_shifting;
  bool m_multiplace;
  StringList m_placeSounds;
  std::optional<Vec2F> m_last_aim_position;
  TileCollisionOverride m_collisionOverride;

  mutable Vec2F m_lastTileAreaOriginCache;
  mutable float m_lastTileAreaRadiusCache;
  mutable List<Vec2I> m_tileAreasCache;

  mutable std::optional<List<Drawable>> m_generated_preview_cache;
  mutable bool m_generated_preview_uses_tile_drawer = false;
};

}


namespace Star {

constexpr int BlockRadiusLimit = 16;
const String BlockRadiusPropertyKey = "building.blockRadius";
const String AltBlockRadiusPropertyKey = "building.altBlockRadius";
const String CollisionOverridePropertyKey = "building.collisionOverride";
const String BlockSwapPropertyKey = "building.blockSwap";

std::expected<std::shared_ptr<MaterialItem>, StarException> MaterialItem::create(Root& root, Json const& config, String const& directory, Json const& settings) {
  auto item = std::make_shared<MaterialItem>(ConstructionKey{}, root);
  auto initialized = item->initialize(config, directory, settings);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  auto fireable = item->initializeFireable(config);
  if (!fireable)
    return std::unexpected(std::move(fireable.error()));
  auto beam = item->initializeBeam(config);
  if (!beam)
    return std::unexpected(std::move(beam.error()));
  auto material = config.get("materialId").and_then([](Json const& value) -> std::expected<MaterialId, StarException> {
    return checked_material_value<MaterialId>(value, "materialId");
  });
  if (!material)
    return std::unexpected(std::move(material.error()));
  item->m_material = *material;
  auto hue = item->instance_value("materialHueShift", 0)
    .and_then([](Json const& value) { return value.to_float(); })
    .and_then(material_hue_from_degrees);
  if (!hue)
    return std::unexpected(std::move(hue.error()));
  item->m_material_hue_shift = *hue;
  auto database = root.material_database();
  if (!database)
    return std::unexpected(std::move(database.error()));
  if (item->material_hue_shift() != MaterialHue()) {
    auto drawables = item->icon_drawables();
    if (!drawables)
      return std::unexpected(std::move(drawables.error()));
    for (auto& drawable : *drawables) {
      if (drawable.is_image()) {
        auto metadata = root.image_metadata_database();
        if (!metadata)
          return std::unexpected(std::move(metadata.error()));
        auto added = drawable.image_part().add_directives(**metadata, Directives(String(Star::format("?hueshift={}", material_hue_to_degrees(item->m_material_hue_shift)))), false);
        if (!added)
          return std::unexpected(std::move(added.error()));
      }
    }
    auto icons = item->set_icon_drawables(std::move(*drawables));
    if (!icons)
      return std::unexpected(std::move(icons.error()));
  }
  auto twoHanded = config.get_bool("twoHanded", true);
  if (!twoHanded)
    return std::unexpected(std::move(twoHanded.error()));
  item->set_two_handed(*twoHanded);
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto defaults = (*assets)->json("/items/defaultParameters.config");
  if (!defaults)
    return std::unexpected(std::move(defaults.error()));
  auto defaultCooldown = defaults->query_float("materialItems.cooldown");
  if (!defaultCooldown)
    return std::unexpected(std::move(defaultCooldown.error()));
  auto cooldown = config.query_float("materialItems.cooldown", *defaultCooldown);
  if (!cooldown)
    return std::unexpected(std::move(cooldown.error()));
  item->setCooldownTime(*cooldown);
  auto defaultRadius = defaults->get_float("blockRadius");
  if (!defaultRadius)
    return std::unexpected(std::move(defaultRadius.error()));
  auto radius = config.get_float("blockRadius", *defaultRadius);
  if (!radius)
    return std::unexpected(std::move(radius.error()));
  item->m_blockRadius = *radius;
  auto defaultAlt = defaults->get_float("altBlockRadius");
  if (!defaultAlt)
    return std::unexpected(std::move(defaultAlt.error()));
  auto alt = config.get_float("altBlockRadius", *defaultAlt);
  if (!alt)
    return std::unexpected(std::move(alt.error()));
  item->m_altBlockRadius = *alt;
  auto overrideName = config.get_string("collisionOverride", "None");
  if (!overrideName)
    return std::unexpected(std::move(overrideName.error()));
  auto collisionOverride = maybe_enum_value<TileCollisionOverrideNames>(*overrideName);
  if (!collisionOverride)
    return std::unexpected(std::move(collisionOverride.error()));
  item->m_collisionOverride = collisionOverride->value_or(TileCollisionOverride::None);
  item->m_blockSwap = false;
  auto collision = (*database)->material_collision_kind(item->m_material);
  if (!collision)
    return std::unexpected(std::move(collision.error()));
  auto multiplace = config.get_bool("allowMultiplace", BlockCollisionSet.contains(*collision));
  if (!multiplace)
    return std::unexpected(std::move(multiplace.error()));
  item->m_multiplace = *multiplace;
  auto sounds = config.get("placeSounds", JsonArray()).and_then(json_to_string_list);
  if (!sounds)
    return std::unexpected(std::move(sounds.error()));
  item->m_placeSounds = std::move(*sounds);
  if (item->m_placeSounds.empty()) {
    auto mining = (*database)->mining_sound(item->m_material);
    if (!mining)
      return std::unexpected(std::move(mining.error()));
    if (!mining->empty())
      item->m_placeSounds.push_back(std::move(*mining));
    auto step = (*database)->footstep_sound(item->m_material);
    if (!step)
      return std::unexpected(std::move(step.error()));
    if (!step->empty())
      item->m_placeSounds.push_back(std::move(*step));
    else if (item->m_placeSounds.empty())
      item->m_placeSounds.push_back((*database)->default_footstep_sound());
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
  auto selectedSound = root().random().sample_value(m_placeSounds);
  if (!selectedSound)
    return std::unexpected(std::move(selectedSound.error()));
  auto sound = owner.add_sound(*selectedSound, 1.0f, 2.0f);
  if (!sound)
    return std::unexpected(std::move(sound.error()));
  if (auto player = as_ref<Player>(owner))
    return updatePropertiesFromPlayer(player->get());
  return {};
}

std::expected<void, StarException> MaterialItem::uninit() {
  auto uninitialized = FireableItem::uninit();
  if (!uninitialized)
    return std::unexpected(std::move(uninitialized.error()));
  m_last_aim_position.reset();
  return {};
}

std::expected<void, StarException> MaterialItem::update(float dt, FireMode fireMode, bool shifting, HashSet<MoveControlType> const& moves) {
  auto fireable = FireableItem::update(dt, fireMode, shifting, moves);
  if (!fireable)
    return std::unexpected(std::move(fireable.error()));
  float radius = calc_radius(shifting);
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
  if (auto player = as_ref<Player>(itemOwner->get())) {
    if (auto input = itemOwner->get().isMaster() ? player->get().input() : std::nullopt) {
      if (auto presses = input->get().bind_down("opensb", "materialCollisionCycle")) {
        auto database = root().material_database();
        if (!database)
          return std::unexpected(std::move(database.error()));
        auto base_kind = (*database)->material_collision_kind(m_material);
        if (!base_kind)
          return std::unexpected(std::move(base_kind.error()));
        for (std::size_t i = 0; i != *presses; ++i) {
          constexpr auto limit = static_cast<std::uint8_t>(TileCollisionOverride::Block) + 1;
          while (true) {
            m_collisionOverride = TileCollisionOverride((static_cast<std::uint8_t>(m_collisionOverride) + 1) % limit);
            if (collisionKindFromOverride(m_collisionOverride) != *base_kind)
              break;
          }
          auto name = enum_name<TileCollisionOverrideNames>(m_collisionOverride);
          if (!name)
            return std::unexpected(std::move(name.error()));
          auto stored = player->get().set_secret_property(CollisionOverridePropertyKey, *name);
          if (!stored)
            return std::unexpected(std::move(stored.error()));
        }
        auto pitch = root().random().uniform_float(0.9f, 1.1f);
        if (!pitch)
          return std::unexpected(std::move(pitch.error()));
        auto sound = itemOwner->get().add_sound("/sfx/tools/cyclematcollision.ogg", 1.0f, *pitch);
        if (!sound)
          return std::unexpected(std::move(sound.error()));
      }

      if (auto presses = input->get().bind_down("opensb", "buildingRadiusGrow")) {
        m_blockRadius = std::min(BlockRadiusLimit, int(m_blockRadius + *presses));
        auto stored = player->get().set_secret_property(BlockRadiusPropertyKey, m_blockRadius);
        if (!stored)
          return std::unexpected(std::move(stored.error()));
        auto sound = itemOwner->get().add_sound("/sfx/tools/buildradiusgrow.wav", 1.0f, 1.0f + m_blockRadius / BlockRadiusLimit);
        if (!sound)
          return std::unexpected(std::move(sound.error()));
      }

      if (auto presses = input->get().bind_down("opensb", "buildingRadiusShrink")) {
        m_blockRadius = std::max(1, int(m_blockRadius - *presses));
        auto stored = player->get().set_secret_property(BlockRadiusPropertyKey, m_blockRadius);
        if (!stored)
          return std::unexpected(std::move(stored.error()));
        auto sound = itemOwner->get().add_sound("/sfx/tools/buildradiusshrink.wav", 1.0f, 1.0f + m_blockRadius / BlockRadiusLimit);
        if (!sound)
          return std::unexpected(std::move(sound.error()));
      }

      if (auto presses = input->get().bind_down("opensb", "blockSwapToggle")) {
        if (*presses % 2 != 0)
          m_blockSwap = !m_blockSwap;
        auto stored = player->get().set_secret_property(BlockSwapPropertyKey, m_blockSwap);
        if (!stored)
          return std::unexpected(std::move(stored.error()));
        auto pitch = root().random().uniform_float(0.9f, 1.1f);
        if (!pitch)
          return std::unexpected(std::move(pitch.error()));
        auto sound = itemOwner->get().add_sound(m_blockSwap ? std::string_view("/sfx/interface/button/click.wav") : std::string_view("/sfx/interface/button/release.wav"), 1.0f, *pitch);
        if (!sound)
          return std::unexpected(std::move(sound.error()));
      }
    }
    else
      return updatePropertiesFromPlayer(player->get());
  }
  return {};
}

std::expected<void, StarException> MaterialItem::render(RenderCallback& render_callback, EntityRenderLayer) {
  if (m_blockSwap || m_collisionOverride != TileCollisionOverride::None) {
    auto itemOwner = owner();
    if (!itemOwner)
      return std::unexpected(std::move(itemOwner.error()));
    float pulse = static_cast<float>(
      std::sin(2 * std::numbers::pi * 4.0
               * std::chrono::time_point_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now())
                   .time_since_epoch()
                   .count()));
    float pulseA = 0.85 - pulse * 0.15f;
    float pulseB = 0.85 + pulse * 0.15f;
    Color color = itemOwner->get().favorite_color().mix(Color::palette_white);
    float alpha = color.alpha_f();
    color.set_alpha_f(alpha * pulseA * 0.95f);
    auto addIndicator = [&](String const& path) -> std::expected<void, StarException> {
      Vec2F basePosition = Vec2F(0.5f, 0.5f);
      auto metadata = root().image_metadata_database();
      if (!metadata)
        return std::unexpected(std::move(metadata.error()));
      auto indicator = Drawable::make_image(**metadata, path, 1.0f / tile_pixels, true, basePosition);
      if (!indicator)
        return std::unexpected(std::move(indicator.error()));
      indicator->fullbright = true;
      indicator->color = color;
      auto aimPosition = itemOwner->get().aim_position();
      if (!aimPosition)
        return std::unexpected(std::move(aimPosition.error()));
      for (auto const& tile_pos : tile_area(calc_radius(m_shifting), *aimPosition)) {
        indicator->position = basePosition + Vec2F(tile_pos);
        auto added = render_callback.add_drawable(*indicator, render_layer_foreground_tile);
        if (!added)
          return std::unexpected(std::move(added.error()));
      }
      return {};
    };

    if (m_blockSwap) {
      auto prev = color;
      color.hue_shift(0.167f);
      color.set_alpha_f(alpha * pulseB * 0.95f);
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

std::expected<List<Drawable>, StarException> MaterialItem::preview(std::optional<std::reference_wrapper<TileDrawer>> tile_drawer, std::shared_ptr<Player> const&) const {
  return generated_preview(tile_drawer).transform([](std::reference_wrapper<List<Drawable> const> drawables) { return drawables.get(); });
}

std::expected<List<Drawable>, StarException> MaterialItem::drop_drawables(std::optional<std::reference_wrapper<TileDrawer>> tile_drawer) const {
  return generated_preview(tile_drawer).transform([](std::reference_wrapper<List<Drawable> const> drawables) { return drawables.get(); });
}

std::expected<bool, StarException> MaterialItem::beam_can_place() const {
  return can_place(m_shifting);
}

std::expected<void, StarException> MaterialItem::fire(FireMode mode, bool shifting, bool edge_triggered) {
  if (!initialized() || !ready())
    return {};

  auto layer = (mode == FireMode::Primary || !two_handed() ? TileLayer::Foreground : TileLayer::Background);
  TileModificationList modifications;

  float radius = calc_radius(shifting);

  auto item_world = world();
  if (!item_world)
    return std::unexpected(std::move(item_world.error()));
  auto item_owner = owner();
  if (!item_owner)
    return std::unexpected(std::move(item_owner.error()));
  auto geo = item_world->get().geometry();
  auto aim = item_owner->get().aim_position();
  if (!aim)
    return std::unexpected(std::move(aim.error()));
  auto aim_position = *aim;

  if (!m_last_aim_position)
    m_last_aim_position = aim_position;

  unsigned steps = 1;
  Vec2F diff = {};
  if (*m_last_aim_position != aim_position) {
    diff = geo.diff(*m_last_aim_position, aim_position);
    float magnitude = diff.magnitude();
    float limit = std::max(4.f, 64.f / radius);
    if (magnitude > limit) {
      diff = diff.normalized() * limit;
      magnitude = limit;
    }

    steps = static_cast<unsigned>(std::ceil(magnitude * (std::numbers::pi / 2)));
  }

  CollisionKind collision_kind;
  if (m_collisionOverride != TileCollisionOverride::None) {
    collision_kind = collisionKindFromOverride(m_collisionOverride);
  } else {
    auto database = root().material_database();
    if (!database)
      return std::unexpected(std::move(database.error()));
    auto kind = (*database)->material_collision_kind(m_material);
    if (!kind)
      return std::unexpected(std::move(kind.error()));
    collision_kind = *kind;
  }

  std::size_t total = 0;
  std::optional<PlacementHue> placement_hue;

  if (m_blockSwap) {
    auto range = item_owner->get().in_tool_range(aim_position);
    if (!range)
      return std::unexpected(std::move(range.error()));
    if (*range) {
      auto swapped = block_swap(radius, layer, placement_hue);
      if (!swapped)
        return std::unexpected(std::move(swapped.error()));
      total += *swapped;
    }
  }
  
  for (unsigned i = 0; i != steps; ++i) {
    auto placement_origin = aim_position + diff * (1.0f - (static_cast<float>(i) / steps));
    auto range = item_owner->get().in_tool_range(placement_origin);
    if (!range)
      return std::unexpected(std::move(range.error()));
    if (!*range)
      continue;

    auto const& positions = tile_area(radius, placement_origin);
    if (!placement_hue && !positions.empty()) {
      auto prepared = prepare_placement_hue(placement_hue, item_world->get());
      if (!prepared)
        return std::unexpected(std::move(prepared.error()));
    }
    for (auto const& pos : positions) {
      auto hue = placement_hue_at(*placement_hue, pos);
      if (!hue)
        return std::unexpected(std::move(hue.error()));
      modifications.emplace_back(pos,
                                 PlaceMaterial{.layer = layer,
                                               .material = material_id(),
                                               .materialHueShift = *hue,
                                               .collisionOverride = m_collisionOverride});
    }

    // Make sure not to make any more modifications than we have consumables.
    if (modifications.size() > count())
      modifications.resize(count());
    auto failed = item_world->get().applyTileModifications(modifications, collision_kind <= CollisionKind::Platform);
    if (!failed)
      return std::unexpected(std::move(failed.error()));
    if (failed->size() < modifications.size()) {
      std::size_t placed = modifications.size() - failed->size();
      consume(placed);
      total += placed;
    }
  }

  if (total) {
    float intensity = std::clamp(std::sqrt(static_cast<float>(total)) / 16, 0.0f, 1.0f);
    auto sound_result = root().random().sample_value(m_placeSounds);
    if (!sound_result)
      return std::unexpected(std::move(sound_result.error()));
    auto sound = std::move(*sound_result);
    auto pitch = root().random().uniform_float(0.9f, 1.1f);
    if (!pitch)
      return std::unexpected(std::move(pitch.error()));
    auto played = item_owner->get().add_sound(sound, 1.0f + intensity, (1.125f - intensity * 0.75f) * *pitch);
    if (!played)
      return std::unexpected(std::move(played.error()));
    auto fired = FireableItem::fire(mode, shifting, edge_triggered);
    if (!fired)
      return std::unexpected(std::move(fired.error()));
  }

  m_last_aim_position = aim_position;
  return {};
}

std::expected<void, StarException> MaterialItem::endFire(FireMode, bool) {
  m_last_aim_position.reset();
  return {};
}

std::expected<std::size_t, StarException> MaterialItem::block_swap(float radius, TileLayer layer, std::optional<PlacementHue>& placement_hue) {
  auto item_owner = owner();
  if (!item_owner)
    return std::unexpected(std::move(item_owner.error()));
  auto player = as_ref<Player>(item_owner->get());
  if (!player)
    return 0;
  
  std::shared_ptr<Item> beamAxePtr = player->get().essential_item(EssentialItem::BeamAxe);
  if (!beamAxePtr)
    return 0;
  
  Item& beamAxe = *beamAxePtr;
  auto item_world = world();
  if (!item_world)
    return std::unexpected(std::move(item_world.error()));

  List<Vec2I> swap_positions;
  auto aim_position = item_owner->get().aim_position();
  if (!aim_position)
    return std::unexpected(std::move(aim_position.error()));
  for (auto const& pos : tile_area(radius, *aim_position)) {
    auto connectable = item_world->get().isTileConnectable(pos, layer, true);
    if (!connectable)
      return std::unexpected(std::move(connectable.error()));
    if (!*connectable)
      continue;
    if (item_world->get().isTileProtected(pos))
      continue;
    if (item_world->get().material(pos, layer) == material_id())
      continue;
    swap_positions.push_back(pos);
  }

  if (swap_positions.empty())
    return 0;
  if (swap_positions.size() > count())
    swap_positions.resize(count());

  TileDamage damage;
  damage.type = TileDamageType::Beamish;
  auto defaultDamage = beamAxe.instance_value("primaryAbility.tileDamage", 1.0f);
  if (!defaultDamage)
    return std::unexpected(std::move(defaultDamage.error()));
  auto amount = beamAxe.instance_value("tileDamage", *defaultDamage).and_then([](Json const& value) { return value.to_float(); });
  if (!amount)
    return std::unexpected(std::move(amount.error()));
  damage.amount = *amount;
  auto defaultHarvest = beamAxe.instance_value("primaryAbility.harvestLevel", 1);
  if (!defaultHarvest)
    return std::unexpected(std::move(defaultHarvest.error()));
  auto harvest = beamAxe.instance_value("harvestLevel", *defaultHarvest).and_then([](Json const& value) { return value.to_uint(); });
  if (!harvest)
    return std::unexpected(std::move(harvest.error()));
  damage.harvest_level = *harvest;

  if (!placement_hue && !swap_positions.empty()) {
    auto prepared = prepare_placement_hue(placement_hue, item_world->get());
    if (!prepared)
      return std::unexpected(std::move(prepared.error()));
  }

  TileModificationList to_swap;
  List<Vec2I> willDamage;
  for (auto const& pos : swap_positions) {
    auto hue = placement_hue_at(*placement_hue, pos);
    if (!hue)
      return std::unexpected(std::move(hue.error()));
    to_swap.emplace_back(pos,
                         PlaceMaterial{.layer = layer,
                                       .material = material_id(),
                                       .materialHueShift = *hue,
                                       .collisionOverride = m_collisionOverride});
    auto destroys = item_world->get().damageWouldDestroy(pos, layer, damage);
    if (!destroys)
      return std::unexpected(std::move(destroys.error()));
    if (!*destroys)
      willDamage.push_back(pos);
  }
  
  std::size_t success = 0;
  auto failed = item_world->get().replaceTiles(to_swap, damage, true);
  if (!failed)
    return std::unexpected(std::move(failed.error()));

  if (failed->size() < to_swap.size()) {
    success = to_swap.size() - failed->size();
    consume(success);
  } else {
    List<Vec2I> to_damage;

    for (auto const& pair : to_swap)
      to_damage.push_back(pair.first);

    auto damaged = item_world->get().damageTiles(to_damage, layer, item_owner->get().position(), damage, item_owner->get().entityId());
    if (!damaged)
      return std::unexpected(std::move(damaged.error()));
  }

  if (willDamage.empty())
    return success;

  auto material_database = root().material_database();
  if (!material_database)
    return std::unexpected(std::move(material_database.error()));
  auto assets = root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  String blockSound;

  for (auto pos : willDamage) {
    auto sound = (*material_database)->mining_sound(item_world->get().material(pos, layer), item_world->get().mod(pos, layer));
    if (!sound)
      return std::unexpected(std::move(sound.error()));
    blockSound = std::move(*sound);
    if (!blockSound.empty())
      break;
  }

  if (blockSound.empty()) {
    for (auto pos : willDamage) {
      auto sound = (*material_database)->footstep_sound(item_world->get().material(pos, layer), item_world->get().mod(pos, layer));
      if (!sound)
        return std::unexpected(std::move(sound.error()));
      blockSound = std::move(*sound);
      if (!blockSound.empty()) {
        auto defaultSound = (*assets)->json("/client.config:defaultFootstepSound").and_then([](Json const& value) { return value.to_string(); });
        if (!defaultSound)
          return std::unexpected(std::move(defaultSound.error()));
        if (blockSound != *defaultSound)
          break;
      }
    }
  }

  auto blockVolume = (*assets)->json("/sfx.config:miningBlockVolume").and_then([](Json const& value) { return value.to_float(); });
  if (!blockVolume)
    return std::unexpected(std::move(blockVolume.error()));
  auto played = item_owner->get().add_sound(blockSound, *blockVolume);
  if (!played)
    return std::unexpected(std::move(played.error()));
  auto strikeSounds = beamAxe.instance_value("strikeSounds");
  if (!strikeSounds)
    return std::unexpected(std::move(strikeSounds.error()));
  if (!strikeSounds->is_null()) {
    auto sounds = json_to_string_list(*strikeSounds);
    if (!sounds)
      return std::unexpected(std::move(sounds.error()));
    auto soundResult = root().random().sample_value(*sounds);
    if (!soundResult)
      return std::unexpected(std::move(soundResult.error()));
    auto sound = std::move(*soundResult);
    auto toolVolume = (*assets)->json("/sfx.config:miningToolVolume").and_then([](Json const& value) { return value.to_float(); });
    if (!toolVolume)
      return std::unexpected(std::move(toolVolume.error()));
    auto played = item_owner->get().add_sound(sound, *toolVolume);
    if (!played)
      return std::unexpected(std::move(played.error()));
  }

  if (auto item = as_ref<FireableItem>(beamAxe))
    setFireTimer(item->get().windupTime() + item->get().cooldown_time());
  
  return success;
}

MaterialId MaterialItem::material_id() const {
  return m_material;
}

std::expected<std::reference_wrapper<List<Drawable> const>, StarException> MaterialItem::generated_preview(std::optional<std::reference_wrapper<TileDrawer>> tile_drawer, Vec2I position) const {
  if (!m_generated_preview_cache || m_generated_preview_uses_tile_drawer != tile_drawer.has_value()) {
    if (tile_drawer) {
      auto locker = tile_drawer->get().lock_render_data();
      WorldRenderData& render_data = tile_drawer->get().render_data();
      render_data.geometry = WorldGeometry(3, 3);
      auto resized = render_data.tiles.set_size({3, 3});
      if (!resized) return std::unexpected(std::move(resized.error()));
      render_data.tiles.fill(TileDrawer::default_render_tile);
      render_data.tile_min_position = { 0, 0 };
      auto tile = render_data.tiles.at({1, 1});
      if (!tile)
        return std::unexpected(std::move(tile.error()));
      tile->get().foreground = m_material;
      tile->get().foregroundHueShift = m_material_hue_shift;
      tile->get().foregroundColorVariant = 0;

      List<Drawable> drawables;
      TileDrawer::Drawables tile_drawables;
      auto database = root().material_database();
      if (!database)
        return std::unexpected(std::move(database.error()));
      auto collision = (*database)->material_collision_kind(m_material);
      if (!collision)
        return std::unexpected(std::move(collision.error()));
      bool is_block = BlockCollisionSet.contains(*collision);
      TileDrawer::TerrainLayer layer = is_block ? TileDrawer::TerrainLayer::Foreground : TileDrawer::TerrainLayer::Midground;
      for (int x = 0; x != 3; ++x) {
        for (int y = 0; y != 3; ++y) {
          auto produced = tile_drawer->get().produce_terrain_drawables(tile_drawables, layer, {x, y}, render_data, 1.0f / tile_pixels, position - Vec2I(1, 1));
          if (!produced)
            return std::unexpected(std::move(produced.error()));
        }
      }

      locker.unlock();
      for (auto& index : tile_drawables.keys()) {
        auto taken = tile_drawables.extract_value(index);
        if (!taken)
          return std::unexpected(std::move(taken.error()));
        drawables.append_range(std::move(*taken));
      }
      auto metadata = root().image_metadata_database();
      if (!metadata)
        return std::unexpected(std::move(metadata.error()));
      auto bound_box = Drawable::bound_box_all(**metadata, drawables, true);
      if (!bound_box)
        return std::unexpected(std::move(bound_box.error()));
      if (!bound_box->is_empty()) {
        for (auto& drawable : drawables)
          drawable.translate(-bound_box->center());
      }

      m_generated_preview_cache.emplace(std::move(drawables));
    }
    else {
      auto icons = icon_drawables();
      if (!icons)
        return std::unexpected(std::move(icons.error()));
      m_generated_preview_cache.emplace(std::move(*icons));
    }
    m_generated_preview_uses_tile_drawer = tile_drawer.has_value();
  }
  return std::cref(*m_generated_preview_cache);
}

std::expected<void, StarException> MaterialItem::updatePropertiesFromPlayer(Player& player) {
  auto blockRadius = player.get_secret_property(BlockRadiusPropertyKey);
  if (!blockRadius)
    return std::unexpected(std::move(blockRadius.error()));
  if (blockRadius->is_type(Json::Type::Float)) {
    auto value = blockRadius->to_float();
    if (!value)
      return std::unexpected(std::move(value.error()));
    m_blockRadius = *value;
  }
  auto altRadius = player.get_secret_property(AltBlockRadiusPropertyKey);
  if (!altRadius)
    return std::unexpected(std::move(altRadius.error()));
  if (altRadius->is_type(Json::Type::Float)) {
    auto value = altRadius->to_float();
    if (!value)
      return std::unexpected(std::move(value.error()));
    m_altBlockRadius = *value;
  }
  auto collision = player.get_secret_property(CollisionOverridePropertyKey);
  if (!collision)
    return std::unexpected(std::move(collision.error()));
  if (collision->is_type(Json::Type::String)) {
    auto name = collision->to_string();
    if (!name)
      return std::unexpected(std::move(name.error()));
    auto value = maybe_enum_value<TileCollisionOverrideNames>(*name);
    if (!value)
      return std::unexpected(std::move(value.error()));
    m_collisionOverride = value->value_or(TileCollisionOverride::None);
  }
  auto blockSwap = player.get_secret_property(BlockSwapPropertyKey);
  if (!blockSwap)
    return std::unexpected(std::move(blockSwap.error()));
  if (blockSwap->is_type(Json::Type::Bool)) {
    auto value = blockSwap->to_bool();
    if (!value)
      return std::unexpected(std::move(value.error()));
    m_blockSwap = *value;
  }
  return {};
}

float MaterialItem::calc_radius(bool shifting) const {
  if (!multiplace_enabled())
    return 1;
  else
    return !shifting ? m_blockRadius : m_altBlockRadius;
}

List<Vec2I> const& MaterialItem::tile_area(float radius, Vec2F const& position) const {
  if (m_lastTileAreaOriginCache != position || m_lastTileAreaRadiusCache != radius) {
    m_lastTileAreaOriginCache = position;
    m_lastTileAreaRadiusCache = radius;
    m_tileAreasCache = tile_area_brush(radius, position, true);
  }
  return m_tileAreasCache;
}

MaterialHue MaterialItem::material_hue_shift() const {
  return m_material_hue_shift;
}

std::expected<bool, StarException> MaterialItem::can_place(bool shifting) const {
  if (initialized()) {
    auto item_owner = owner();
    if (!item_owner)
      return std::unexpected(std::move(item_owner.error()));
    auto item_world = world();
    if (!item_world)
      return std::unexpected(std::move(item_world.error()));
    MaterialId material = material_id();
    float radius = calc_radius(shifting);
    auto aim_position = item_owner->get().aim_position();
    if (!aim_position)
      return std::unexpected(std::move(aim_position.error()));
    auto const& positions = tile_area(radius, *aim_position);
    if (positions.empty())
      return false;
    std::optional<PlacementHue> placement_hue;
    auto prepared = prepare_placement_hue(placement_hue, item_world->get());
    if (!prepared)
      return std::unexpected(std::move(prepared.error()));
    for (auto const& pos : positions) {
      auto hue = placement_hue_at(*placement_hue, pos);
      if (!hue)
        return std::unexpected(std::move(hue.error()));
      auto foreground = item_world->get().canModifyTile(
        pos, PlaceMaterial{.layer = TileLayer::Foreground, .material = material, .materialHueShift = *hue}, false);
      if (!foreground)
        return std::unexpected(std::move(foreground.error()));
      if (*foreground)
        return true;
      auto background = item_world->get().canModifyTile(
        pos, PlaceMaterial{.layer = TileLayer::Background, .material = material, .materialHueShift = *hue}, false);
      if (!background)
        return std::unexpected(std::move(background.error()));
      if (*background)
        return true;
    }
  }
  return false;
}

bool MaterialItem::multiplace_enabled() const {
  return m_multiplace && count() > 1;
}

float& MaterialItem::block_radius() {
  return m_blockRadius;
}

float& MaterialItem::alt_block_radius() {
  return m_altBlockRadius;
}

TileCollisionOverride& MaterialItem::collision_override() {
  return m_collisionOverride;
}

std::expected<List<PreviewTile>, StarException> MaterialItem::preview_tiles(bool shifting) const {
  List<PreviewTile> result;
  if (initialized()) {
    auto item_owner = owner();
    if (!item_owner)
      return std::unexpected(std::move(item_owner.error()));
    auto item_world = world();
    if (!item_world)
      return std::unexpected(std::move(item_world.error()));
    Vec3B light = item_owner->get().favorite_color().to_rgb();
    auto material = material_id();
    auto color = default_material_color_variant;
    std::size_t placeable_count = 0;
    auto aim_position = item_owner->get().aim_position();
    if (!aim_position)
      return std::unexpected(std::move(aim_position.error()));
    auto const& positions = tile_area(calc_radius(shifting), *aim_position);
    if (positions.empty())
      return result;
    std::optional<PlacementHue> placement_hue;
    auto prepared = prepare_placement_hue(placement_hue, item_world->get());
    if (!prepared)
      return std::unexpected(std::move(prepared.error()));
    for (auto const& pos : positions) {
      auto hue = placement_hue_at(*placement_hue, pos);
      if (!hue)
        return std::unexpected(std::move(hue.error()));
      if (placeable_count >= count())
        break;
      auto foreground = item_world->get().canModifyTile(
        pos, PlaceMaterial{.layer = TileLayer::Foreground, .material = material, .materialHueShift = *hue}, false);
      if (!foreground)
        return std::unexpected(std::move(foreground.error()));
      if (*foreground) {
        result.push_back({pos, true, material, *hue, true});
        ++placeable_count;
      } else {
        bool back_placeable = false;
        if (two_handed()) {
          auto background = item_world->get().canModifyTile(
            pos, PlaceMaterial{.layer = TileLayer::Background, .material = material, .materialHueShift = *hue}, false);
          if (!background)
            return std::unexpected(std::move(background.error()));
          back_placeable = *background;
        }
        if (back_placeable) {
          result.push_back({pos, true, material, *hue, true, light, true, color});
          ++placeable_count;
        } else {
          result.push_back({pos, true, material, *hue, true});
        }
      }
    }
  }
  return result;
}

std::expected<void, StarException> MaterialItem::prepare_placement_hue(std::optional<PlacementHue>& placement_hue, World const& world) const {
  auto hue = instance_value("materialHueShift");
  if (!hue)
    return std::unexpected(std::move(hue.error()));
  if (!hue->is_null()) {
    auto explicit_hue = hue->to_float().and_then(material_hue_from_degrees);
    if (!explicit_hue)
      return std::unexpected(std::move(explicit_hue.error()));
    placement_hue.emplace(*explicit_hue);
  } else if (auto world_client = as_ref<WorldClient>(world)) {
    placement_hue.emplace(world_client->get().currentTemplate());
  } else {
    placement_hue.emplace(material_hue_shift());
  }
  return {};
}

std::expected<MaterialHue, StarException> MaterialItem::placement_hue_at(PlacementHue const& placement_hue, Vec2I const& position) const {
  if (std::holds_alternative<MaterialHue>(placement_hue))
    return std::get<MaterialHue>(placement_hue);
  auto const& world_template = *std::get<std::shared_ptr<WorldTemplate const>>(placement_hue);
  auto biome_index = world_template.blockBiomeIndex(position[0], position[1]);
  if (!biome_index)
    return std::unexpected(std::move(biome_index.error()));
  return world_template.biome_material_hue_shift(*biome_index, m_material);
}

}
