import star.json;
import star.xx_hash;
import star.json_extra;
import star.id_map;
import star.list;
import star.bi_map;
import star.data_stream;
import star.game_types;
import star.material_types;
import star.casting;
import star.vector;
import star.strong_typedef;
import star.rpc_promise;
import star.spline;
import star.a_star;
import std;
import star.memory;
import star.format;
import star.audio;
import star.thread;
import star.map;
import star.set;
import star.color;
import star.string;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import star.version;
import star.image;
import star.math_common;
import star.either;
import star.random;
import star.weighted_pool;
import star.net_compatibility;
import std;
import star.worker_pool;

import star.sector_array_2d;
import star.perlin;
import star.btree;
import star.lru_cache;
import star.data_stream_devices;
import star.btree_database;
import star.ordered_set;
import star.lua_root;
import star.lua_components;

import star.collision_block;
import star.item_descriptor;
import star.drawable;
import star.item;
import star.non_rotated_drawables_item;
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
import star.fireable_item;
import star.swingable_item;
import star.durability_item;
import star.pointable_item;
import star.mixer;
import star.entity_rendering_types;
import star.animation;
import star.particle;
import star.entity_rendering;
struct PreviewTile;
import star.preview_tile_tool;
import star.tools;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.world_geometry;
import star.wiring;
import star.damage_types;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.tile_damage;
import star.interaction_types;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.tile_entity;
import star.tile_modification;
import star.force_regions;
import star.world;
import star.liquid_types;
import star.tile_sector_array;
import star.weather_types;
import star.sky_types;
import star.world_parameters;
import star.celestial_parameters;
import star.world_layout;
import star.collision_generator;
import star.world_tiles;
import star.celestial_types;
import star.chat_types;
import star.uuid;
import star.warping;
import star.inspectable_entity;
import star.plant;
import star.plant_database;
import star.biome_placement;
import star.versioning_database;
import star.world_storage;
import star.player_types;
import star.sky_parameters;
import star.system_world;
import star.damage_manager;
import star.net_packets;
import star.sky_render_data;
import star.parallax;
import star.cellular_light_array;
import star.cellular_lighting;
import star.world_render_data;
import star.world_structure;
import star.chat_action;
import star.world_client_state;
import star.interpolation_tracker;
import star.ambient;
import star.weather;
import star.world_client;

import star.particle_database;
import star.material_database;


namespace Star {

std::expected<std::shared_ptr<MiningTool>, StarException> MiningTool::create(Root& root, Json const& config, String const& directory, Json const& parameters) {
  auto item = std::shared_ptr<MiningTool>(new MiningTool(root));
  auto initialized = item->initialize(config, directory, parameters);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  auto swingable = item->initializeSwingable(config);
  if (!swingable)
    return std::unexpected(std::move(swingable.error()));
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto image = item->instanceValue("image").and_then([](Json const& value) { return value.toString(); });
  if (!image)
    return std::unexpected(std::move(image.error()));
  item->m_image = AssetPath::relative_to(directory, *image);
  auto frames = item->instanceValue("frames", 1).and_then([](Json const& value) { return value.toInt(); });
  if (!frames)
    return std::unexpected(std::move(frames.error()));
  item->m_frames = *frames;
  auto cycle = item->instanceValue("animationCycle", 1.0f).and_then([](Json const& value) { return value.toFloat(); });
  if (!cycle)
    return std::unexpected(std::move(cycle.error()));
  item->m_frameCycle = *cycle;
  item->m_frameTiming = 0;
  for (std::size_t i = 0; i < static_cast<std::size_t>(item->m_frames); i++) {
    auto frame = item->m_image.replace("{frame}", toString(i));
    if (!frame)
      return std::unexpected(std::move(frame.error()));
    item->m_animationFrame.append(std::move(*frame));
  }
  auto idle = item->m_image.replace("{frame}", "idle");
  if (!idle)
    return std::unexpected(std::move(idle.error()));
  item->m_idleFrame = std::move(*idle);
  auto position = item->instanceValue("handPosition").and_then(jsonToVec2F);
  if (!position)
    return std::unexpected(std::move(position.error()));
  item->m_handPosition = *position;
  for (auto const& [key, target] : {std::pair{std::string_view("blockRadius"), std::ref(item->m_blockRadius)}, std::pair{std::string_view("altBlockRadius"), std::ref(item->m_altBlockRadius)}}) {
    auto value = item->instanceValue(key).and_then([](Json const& value) { return value.toFloat(); });
    if (!value)
      return std::unexpected(std::move(value.error()));
    target.get() = *value;
  }
  auto sounds = item->instanceValue("strikeSounds").and_then(jsonToStringList);
  if (!sounds)
    return std::unexpected(std::move(sounds.error()));
  item->m_strikeSounds = std::move(*sounds);
  auto breakSound = item->instanceValue("breakSound", "").and_then([](Json const& value) { return value.toString(); });
  if (!breakSound)
    return std::unexpected(std::move(breakSound.error()));
  item->m_breakSound = std::move(*breakSound);
  auto pointable = item->instanceValue("pointable", false).and_then([](Json const& value) { return value.toBool(); });
  if (!pointable)
    return std::unexpected(std::move(pointable.error()));
  item->m_pointable = *pointable;
  for (auto const& [path, target] : {std::pair{std::string_view("/sfx.config:miningToolVolume"), std::ref(item->m_toolVolume)}, std::pair{std::string_view("/sfx.config:miningBlockVolume"), std::ref(item->m_blockVolume)}}) {
    auto volume = (*assets)->json(path).and_then([](Json const& value) { return value.toFloat(); });
    if (!volume)
      return std::unexpected(std::move(volume.error()));
    target.get() = *volume;
  }
  return item;
}

std::expected<std::shared_ptr<Item>, StarException> MiningTool::clone() const {
  return std::make_shared<MiningTool>(*this);
}

std::expected<List<Drawable>, StarException> MiningTool::drawables() const {
  auto metadata = root().imageMetadataDatabase();
  if (!metadata)
    return std::unexpected(std::move(metadata.error()));
  auto const& image = m_frameTiming == 0
    ? m_idleFrame
    : m_animationFrame[std::max(
        0, std::min(m_frames - 1, static_cast<int>(std::floor((m_frameTiming / m_frameCycle) * m_frames))))];
  auto drawable = Drawable::makeImage(**metadata, image, 1.0f / TilePixels, true, -handPosition() / TilePixels);
  if (!drawable)
    return std::unexpected(std::move(drawable.error()));
  return List<Drawable>{std::move(*drawable)};
}

Vec2F MiningTool::handPosition() const {
  return m_handPosition;
}

std::expected<void, StarException> MiningTool::fire(FireMode mode, bool shifting, bool edgeTriggered) {
  if (!ready())
    return {};

  auto materialDatabase = root().materialDatabase();
  if (!materialDatabase)
    return std::unexpected(std::move(materialDatabase.error()));

  if (initialized()) {
    auto itemOwner = owner();
    if (!itemOwner)
      return std::unexpected(std::move(itemOwner.error()));
    auto itemWorld = world();
    if (!itemWorld)
      return std::unexpected(std::move(itemWorld.error()));
    bool used = false;
    int radius = !shifting ? m_blockRadius : m_altBlockRadius;
    String blockSound;
    List<Vec2I> brushArea;

    auto layer = (mode == FireMode::Primary ? TileLayer::Foreground : TileLayer::Background);
    bool inRange = itemOwner->get().isAdmin();
    if (!inRange) {
      auto range = itemOwner->get().inToolRange();
      if (!range)
        return std::unexpected(std::move(range.error()));
      inRange = *range;
    }
    if (inRange) {
      auto aimPosition = itemOwner->get().aimPosition();
      if (!aimPosition)
        return std::unexpected(std::move(aimPosition.error()));
      brushArea = tileAreaBrush(radius, *aimPosition, true);
      for (auto pos : brushArea) {
        auto sound = (*materialDatabase)->miningSound(itemWorld->get().material(pos, layer), itemWorld->get().mod(pos, layer));
        if (!sound)
          return std::unexpected(std::move(sound.error()));
        blockSound = std::move(*sound);
        if (!blockSound.empty())
          break;
      }
      if (blockSound.empty()) {
        for (auto pos : brushArea) {
          auto sound = (*materialDatabase)->footstepSound(itemWorld->get().material(pos, layer), itemWorld->get().mod(pos, layer));
          if (!sound)
            return std::unexpected(std::move(sound.error()));
          blockSound = std::move(*sound);
          if (!blockSound.empty()) {
            auto assets = root().assets();
            if (!assets)
              return std::unexpected(std::move(assets.error()));
            auto defaultSound = (*assets)->json("/client.config:defaultFootstepSound").and_then([](Json const& value) { return value.toString(); });
            if (!defaultSound)
              return std::unexpected(std::move(defaultSound.error()));
            if (blockSound != *defaultSound)
              break;
          }
        }
      }

      TileDamage damage;
      auto damageTypeName = instanceValue("tileDamageType", "blockish").and_then([](Json const& value) { return value.toString(); });
      if (!damageTypeName)
        return std::unexpected(std::move(damageTypeName.error()));
      if (!TileDamageTypeNames)
        return std::unexpected(TileDamageTypeNames.error());
      auto damageType = TileDamageTypeNames->get_left(*damageTypeName);
      if (!damageType)
        return std::unexpected(std::move(damageType.error()));
      damage.type = damageType->get();
      auto status = durabilityStatus();
      if (!status)
        return std::unexpected(std::move(status.error()));
      auto amount = (*status == 0 ? instanceValue("tileDamageBlunted", 0.1f) : instanceValue("tileDamage", 1.0f)).and_then([](Json const& value) { return value.toFloat(); });
      if (!amount)
        return std::unexpected(std::move(amount.error()));
      damage.amount = *amount;
      auto harvest = instanceValue("harvestLevel", 1).and_then([](Json const& value) { return value.toUInt(); });
      if (!harvest)
        return std::unexpected(std::move(harvest.error()));
      damage.harvestLevel = *harvest;
      auto damageResult = itemWorld->get().damageTiles(brushArea, layer, itemOwner->get().position(), damage, itemOwner->get().entityId());
      if (!damageResult)
        return std::unexpected(std::move(damageResult.error()));
      if (*damageResult != TileDamageResult::None) {
        used = true;
        if (!itemOwner->get().isAdmin()) {
          auto perUse = instanceValue("durabilityPerUse", 1.0f).and_then([](Json const& value) { return value.toFloat(); });
          if (!perUse)
            return std::unexpected(std::move(perUse.error()));
          auto changed = changeDurability(*perUse);
          if (!changed)
            return std::unexpected(std::move(changed.error()));
        }
      }
      if (*damageResult == TileDamageResult::Protected) {
        auto assets = root().assets();
        if (!assets)
          return std::unexpected(std::move(assets.error()));
        auto ding = (*assets)->json("/client.config:defaultDingSound").and_then([](Json const& value) { return value.toString(); });
        if (!ding)
          return std::unexpected(std::move(ding.error()));
        blockSound = std::move(*ding);
      }
    }

    if (used) {
      auto strike = itemOwner->get().addSound(root().random().randValueFrom(m_strikeSounds), m_toolVolume);
      if (!strike)
        return std::unexpected(std::move(strike.error()));
      auto sound = itemOwner->get().addSound(blockSound, m_blockVolume);
      if (!sound)
        return std::unexpected(std::move(sound.error()));
      List<Particle> miningParticles;
      for (auto pos : brushArea) {
        auto miningParticleConfig = (*materialDatabase)->miningParticle(itemWorld->get().material(pos, layer), itemWorld->get().mod(pos, layer));
        if (!miningParticleConfig)
          return std::unexpected(std::move(miningParticleConfig.error()));
        if (*miningParticleConfig) {
          auto miningParticle = (*miningParticleConfig)->instance();
          miningParticle.position += Vec2F(pos);
          miningParticles.append(miningParticle);
        }
      }
      auto particles = itemOwner->get().addParticles(miningParticles);
      if (!particles)
        return std::unexpected(std::move(particles.error()));
      return SwingableItem::fire(mode, shifting, edgeTriggered);
    }
  }
  return {};
}

std::expected<void, StarException> MiningTool::update(float dt, FireMode mode, bool shifting, HashSet<MoveControlType> const& moves) {
  auto updated = SwingableItem::update(dt, mode, shifting, moves);
  if (!updated)
    return std::unexpected(std::move(updated.error()));

  if (!ready() && !coolingDown())
    m_frameTiming = std::fmod((m_frameTiming + dt), m_frameCycle);
  else
    m_frameTiming = 0;
  return {};
}

std::expected<float, StarException> MiningTool::durabilityStatus() {
  auto hit = instanceValue("durabilityHit", 0.0f).and_then([](Json const& value) { return value.toFloat(); });
  if (!hit)
    return std::unexpected(std::move(hit.error()));
  auto durability = instanceValue("durability").and_then([](Json const& value) { return value.toFloat(); });
  if (!durability)
    return std::unexpected(std::move(durability.error()));
  return std::clamp(1.0f - *hit / *durability, 0.0f, 1.0f);
}

std::expected<float, StarException> MiningTool::getAngle(float aimAngle) {
  if ((!ready() && !coolingDown()) || !m_pointable)
    return SwingableItem::getAngle(aimAngle);
  return aimAngle;
}

std::expected<void, StarException> MiningTool::changeDurability(float amount) {
  auto hit = instanceValue("durabilityHit", 0.0f).and_then([](Json const& value) { return value.toFloat(); });
  if (!hit)
    return std::unexpected(std::move(hit.error()));
  auto durability = instanceValue("durability").and_then([](Json const& value) { return value.toFloat(); });
  if (!durability)
    return std::unexpected(std::move(durability.error()));
  auto stored = setInstanceValue("durabilityHit", clamp(*hit + amount, 0.0f, *durability));
  if (!stored)
    return std::unexpected(std::move(stored.error()));
  auto status = durabilityStatus();
  if (!status)
    return std::unexpected(std::move(status.error()));
  if (*status == 0.0f) {
    auto repairable = instanceValue("canBeRepaired", false).and_then([](Json const& value) { return value.toBool(); });
    if (!repairable)
      return std::unexpected(std::move(repairable.error()));
    if (!*repairable) {
      auto itemOwner = owner();
      if (!itemOwner)
        return std::unexpected(std::move(itemOwner.error()));
      auto sound = itemOwner->get().addSound(m_breakSound);
      if (!sound)
        return std::unexpected(std::move(sound.error()));
      consume(1);
    }
  }
  return {};
}

std::expected<std::shared_ptr<HarvestingTool>, StarException> HarvestingTool::create(Root& root, Json const& config, String const& directory, Json const& parameters) {
  auto item = std::shared_ptr<HarvestingTool>(new HarvestingTool(root));
  auto initialized = item->initialize(config, directory, parameters);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  auto swingable = item->initializeSwingable(config);
  if (!swingable)
    return std::unexpected(std::move(swingable.error()));
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto image = item->instanceValue("image").and_then([](Json const& value) { return value.toString(); });
  if (!image)
    return std::unexpected(std::move(image.error()));
  item->m_image = AssetPath::relative_to(directory, *image);
  auto frames = item->instanceValue("frames", 1).and_then([](Json const& value) { return value.toInt(); });
  if (!frames)
    return std::unexpected(std::move(frames.error()));
  item->m_frames = *frames;
  auto cycle = item->instanceValue("animationCycle", 1.0f).and_then([](Json const& value) { return value.toFloat(); });
  if (!cycle)
    return std::unexpected(std::move(cycle.error()));
  item->m_frameCycle = *cycle;
  for (std::size_t i = 0; i < static_cast<std::size_t>(item->m_frames); i++) {
    auto frame = item->m_image.replace("{frame}", toString(i));
    if (!frame)
      return std::unexpected(std::move(frame.error()));
    item->m_animationFrame.append(std::move(*frame));
  }
  auto idle = item->m_image.replace("{frame}", "idle");
  if (!idle)
    return std::unexpected(std::move(idle.error()));
  item->m_idleFrame = std::move(*idle);
  auto position = item->instanceValue("handPosition").and_then(jsonToVec2F);
  if (!position)
    return std::unexpected(std::move(position.error()));
  item->m_handPosition = *position;
  auto sounds = item->instanceValue("strikeSounds").and_then(jsonToStringList);
  if (!sounds)
    return std::unexpected(std::move(sounds.error()));
  item->m_strikeSounds = std::move(*sounds);
  auto volume = (*assets)->json("/sfx.config:harvestToolVolume").and_then([](Json const& value) { return value.toFloat(); });
  if (!volume)
    return std::unexpected(std::move(volume.error()));
  item->m_toolVolume = *volume;
  auto power = item->instanceValue("harvestPower", 1.0f).and_then([](Json const& value) { return value.toFloat(); });
  if (!power)
    return std::unexpected(std::move(power.error()));
  item->m_harvestPower = *power;
  item->m_frameTiming = 0;
  return item;
}

std::expected<std::shared_ptr<Item>, StarException> HarvestingTool::clone() const {
  return std::make_shared<HarvestingTool>(*this);
}

std::expected<List<Drawable>, StarException> HarvestingTool::drawables() const {
  auto metadata = root().imageMetadataDatabase();
  if (!metadata)
    return std::unexpected(std::move(metadata.error()));
  auto const& image = m_frameTiming == 0
    ? m_idleFrame
    : m_animationFrame[std::max(
        0, std::min(m_frames - 1, static_cast<int>(std::floor((m_frameTiming / m_frameCycle) * m_frames))))];
  auto drawable = Drawable::makeImage(**metadata, image, 1.0f / TilePixels, true, -handPosition() / TilePixels);
  if (!drawable)
    return std::unexpected(std::move(drawable.error()));
  return List<Drawable>{std::move(*drawable)};
}

Vec2F HarvestingTool::handPosition() const {
  return m_handPosition;
}

std::expected<void, StarException> HarvestingTool::fire(FireMode mode, bool shifting, bool edgeTriggered) {
  if (!ready())
    return {};
  auto itemOwner = owner();
  if (!itemOwner)
    return std::unexpected(std::move(itemOwner.error()));
  bool inRange = itemOwner->get().isAdmin();
  if (!inRange) {
    auto range = itemOwner->get().inToolRange();
    if (!range)
      return std::unexpected(std::move(range.error()));
    inRange = *range;
  }
  bool used = false;
  if (inRange) {
    auto itemWorld = world();
    if (!itemWorld)
      return std::unexpected(std::move(itemWorld.error()));
    auto layer = mode == FireMode::Primary ? TileLayer::Foreground : TileLayer::Background;
    auto aimPosition = itemOwner->get().aimPosition();
    if (!aimPosition)
      return std::unexpected(std::move(aimPosition.error()));
    auto damaged = itemWorld->get().damageTile(Vec2I::floor(*aimPosition), layer, itemOwner->get().position(), {TileDamageType::Plantish, m_harvestPower});
    if (!damaged)
      return std::unexpected(std::move(damaged.error()));
    used = *damaged != TileDamageResult::None;
  }
  if (used) {
    auto sound = itemOwner->get().addSound(root().random().randValueFrom(m_strikeSounds), m_toolVolume);
    if (!sound)
      return std::unexpected(std::move(sound.error()));
    return SwingableItem::fire(mode, shifting, edgeTriggered);
  }
  return {};
}

std::expected<void, StarException> HarvestingTool::update(float dt, FireMode fireMode, bool shifting, HashSet<MoveControlType> const& moves) {
  auto updated = SwingableItem::update(dt, fireMode, shifting, moves);
  if (!updated)
    return std::unexpected(std::move(updated.error()));

  if (!ready() && !coolingDown())
    m_frameTiming = std::fmod((m_frameTiming + dt), m_frameCycle);
  else
    m_frameTiming = 0;
  return {};
}

std::expected<float, StarException> HarvestingTool::getAngle(float aimAngle) {
  if (!ready() && !coolingDown())
    return SwingableItem::getAngle(aimAngle);
  return aimAngle;
}


std::expected<std::shared_ptr<WireTool>, StarException> WireTool::create(Root& root, Json const& config, String const& directory, Json const& parameters) {
  auto item = std::shared_ptr<WireTool>(new WireTool(root));
  auto initialized = item->initialize(config, directory, parameters);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  auto fireable = item->initializeFireable(config);
  if (!fireable)
    return std::unexpected(std::move(fireable.error()));
  auto overrides = parameters.toObject();
  if (!overrides)
    return std::unexpected(std::move(overrides.error()));
  auto merged = config.setAll(*overrides);
  if (!merged)
    return std::unexpected(std::move(merged.error()));
  auto beam = item->initializeBeam(std::move(*merged));
  if (!beam)
    return std::unexpected(std::move(beam.error()));
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto position = item->instanceValue("handPosition").and_then(jsonToVec2F);
  if (!position)
    return std::unexpected(std::move(position.error()));
  item->m_handPosition = *position;
  auto sounds = item->instanceValue("strikeSounds").and_then(jsonToStringList);
  if (!sounds)
    return std::unexpected(std::move(sounds.error()));
  item->m_strikeSounds = std::move(*sounds);
  auto volume = (*assets)->json("/sfx.config:miningToolVolume").and_then([](Json const& value) { return value.toFloat(); });
  if (!volume)
    return std::unexpected(std::move(volume.error()));
  item->m_toolVolume = *volume;
  item->m_wireConnector = std::nullopt;
  item->m_endType = EndType::Wire;
  return item;
}

std::expected<std::shared_ptr<Item>, StarException> WireTool::clone() const {
  return std::make_shared<WireTool>(*this);
}

std::expected<void, StarException> WireTool::init(ToolUserEntity& owner, ToolHand hand) {
  auto fireable = FireableItem::init(owner, hand);
  if (!fireable)
    return std::unexpected(std::move(fireable.error()));
  auto beam = BeamItem::init(owner, hand);
  if (!beam)
    return std::unexpected(std::move(beam.error()));
  m_wireConnector = std::nullopt;
  return {};
}

std::expected<void, StarException> WireTool::uninit() {
  m_wireConnector.reset();
  return FireableItem::uninit();
}

std::expected<void, StarException> WireTool::update(float dt, FireMode fireMode, bool shifting, HashSet<MoveControlType> const& moves) {
  auto fireable = FireableItem::update(dt, fireMode, shifting, moves);
  if (!fireable)
    return std::unexpected(std::move(fireable.error()));
  return BeamItem::update(dt, fireMode, shifting, moves);
}

std::expected<List<Drawable>, StarException> WireTool::drawables() const {
  return BeamItem::drawables();
}

bool WireTool::beamActive() const {
  return m_wireConnector && m_wireConnector->get().connecting();
}

void WireTool::setEnd(EndType) {
  m_endType = EndType::Wire;
}

Vec2F WireTool::handPosition() const {
  return m_handPosition;
}

std::expected<void, StarException> WireTool::fire(FireMode mode, bool shifting, bool edgeTriggered) {
  if (!ready())
    return {};
  auto ownerp = owner();
  if (!ownerp)
    return std::unexpected(std::move(ownerp.error()));
  auto worldp = world();
  if (!worldp)
    return std::unexpected(std::move(worldp.error()));
  if (m_wireConnector) {
    auto aimPosition = ownerp->get().aimPosition();
    if (!aimPosition)
      return std::unexpected(std::move(aimPosition.error()));
    Vec2F pos(*aimPosition);
    bool inRange = ownerp->get().isAdmin();
    if (!inRange) {
      auto range = ownerp->get().inToolRange();
      if (!range)
        return std::unexpected(std::move(range.error()));
      inRange = *range;
    }
    if (inRange) {
      auto swingResult = m_wireConnector->get().swing(worldp->get().geometry(), pos, mode);
      if (!swingResult)
        return std::unexpected(std::move(swingResult.error()));
      if (*swingResult == WireConnector::Connect) {
        auto sound = ownerp->get().addSound(root().random().randValueFrom(m_strikeSounds), m_toolVolume);
        if (!sound)
          return std::unexpected(std::move(sound.error()));
        return FireableItem::fire(mode, shifting, edgeTriggered);
      } else if (*swingResult == WireConnector::Mismatch || *swingResult == WireConnector::Protected) {
        auto assets = root().assets();
        if (!assets)
          return std::unexpected(std::move(assets.error()));
        auto wireErrorSound = (*assets)->json("/client.config:wireFailSound").and_then([](Json const& value) { return value.toString(); });
        if (!wireErrorSound)
          return std::unexpected(std::move(wireErrorSound.error()));
        auto sound = ownerp->get().addSound(*wireErrorSound, m_toolVolume);
        if (!sound)
          return std::unexpected(std::move(sound.error()));
        return FireableItem::fire(mode, shifting, edgeTriggered);
      }
    }
  }
  return {};
}

std::expected<float, StarException> WireTool::getAngle(float aimAngle) {
  return BeamItem::getAngle(aimAngle);
}

void WireTool::setConnector(std::optional<std::reference_wrapper<WireConnector>> connector) {
  m_wireConnector = connector;
}

std::expected<std::shared_ptr<BeamMiningTool>, StarException> BeamMiningTool::create(Root& root, Json const& config, String const& directory, Json const& directoryParameters) {
  auto item = std::shared_ptr<BeamMiningTool>(new BeamMiningTool(root));
  auto initialized = item->initialize(config, directory, directoryParameters);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  auto fireable = item->initializeFireable(config);
  if (!fireable)
    return std::unexpected(std::move(fireable.error()));
  auto overrides = directoryParameters.toObject();
  if (!overrides)
    return std::unexpected(std::move(overrides.error()));
  auto merged = config.setAll(*overrides);
  if (!merged)
    return std::unexpected(std::move(merged.error()));
  auto beam = item->initializeBeam(std::move(*merged));
  if (!beam)
    return std::unexpected(std::move(beam.error()));
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  for (auto const& [key, target] : {std::pair{std::string_view("blockRadius"), std::ref(item->m_blockRadius)}, std::pair{std::string_view("altBlockRadius"), std::ref(item->m_altBlockRadius)}}) {
    auto value = item->instanceValue(key).and_then([](Json const& value) { return value.toFloat(); });
    if (!value)
      return std::unexpected(std::move(value.error()));
    target.get() = *value;
  }
  auto damage = item->instanceValue("tileDamage", 1.0f).and_then([](Json const& value) { return value.toFloat(); });
  if (!damage)
    return std::unexpected(std::move(damage.error()));
  item->m_tileDamage = *damage;
  auto harvest = item->instanceValue("harvestLevel", 1).and_then([](Json const& value) { return value.toUInt(); });
  if (!harvest)
    return std::unexpected(std::move(harvest.error()));
  item->m_harvestLevel = *harvest;
  auto collect = item->instanceValue("canCollectLiquid", false).and_then([](Json const& value) { return value.toBool(); });
  if (!collect)
    return std::unexpected(std::move(collect.error()));
  item->m_canCollectLiquid = *collect;
  auto sounds = item->instanceValue("strikeSounds").and_then(jsonToStringList);
  if (!sounds)
    return std::unexpected(std::move(sounds.error()));
  item->m_strikeSounds = std::move(*sounds);
  for (auto const& [path, target] : {std::pair{std::string_view("/sfx.config:miningToolVolume"), std::ref(item->m_toolVolume)}, std::pair{std::string_view("/sfx.config:miningBlockVolume"), std::ref(item->m_blockVolume)}}) {
    auto volume = (*assets)->json(path).and_then([](Json const& value) { return value.toFloat(); });
    if (!volume)
      return std::unexpected(std::move(volume.error()));
    target.get() = *volume;
  }
  item->m_endType = EndType::Object;
  auto rateValue = item->instanceValue("scaleRate");
  if (!rateValue)
    return std::unexpected(std::move(rateValue.error()));
  if (*rateValue && rateValue->canConvert(Json::Type::Float)) {
    auto rate = rateValue->toFloat();
    if (!rate)
      return std::unexpected(std::move(rate.error()));
    item->m_tileDamage /= *rate;
    item->m_cooldownTime /= *rate;
  }
  auto effects = item->instanceValue("inhandStatusEffects", JsonArray()).and_then([](Json const& value) { return value.toArray(); });
  if (!effects)
    return std::unexpected(std::move(effects.error()));
  for (auto const& value : *effects) {
    auto effect = jsonToPersistentStatusEffect(value);
    if (!effect)
      return std::unexpected(std::move(effect.error()));
    item->m_inhandStatusEffects.append(std::move(*effect));
  }
  return item;
}

std::expected<std::shared_ptr<Item>, StarException> BeamMiningTool::clone() const {
  return std::make_shared<BeamMiningTool>(*this);
}

std::expected<List<Drawable>, StarException> BeamMiningTool::drawables() const {
  return BeamItem::drawables();
}

void BeamMiningTool::setEnd(EndType) {
  m_endType = EndType::Object;
}

std::expected<List<PreviewTile>, StarException> BeamMiningTool::previewTiles(bool shifting) const {
  List<PreviewTile> result;
  auto ownerp = owner();
  if (!ownerp)
    return std::unexpected(std::move(ownerp.error()));
  auto worldp = world();
  if (!worldp)
    return std::unexpected(std::move(worldp.error()));
  bool inRange = ownerp->get().isAdmin();
  if (!inRange) {
    auto range = ownerp->get().inToolRange();
    if (!range)
      return std::unexpected(std::move(range.error()));
    inRange = *range;
  }
  if (inRange) {
    Color lightColor = ownerp->get().favoriteColor();
    if (!ready())
      lightColor *= Color::rgbaf(0.75f, 0.75f, 0.75f, 1.0f);
    Vec3B light = lightColor.toRgb();
    int radius = !shifting ? m_blockRadius : m_altBlockRadius;
    auto aimPosition = ownerp->get().aimPosition();
    if (!aimPosition)
      return std::unexpected(std::move(aimPosition.error()));
    for (auto pos : tileAreaBrush(radius, *aimPosition, true)) {
      auto foreground = worldp->get().tileIsOccupied(pos, TileLayer::Foreground, true);
      if (!foreground)
        return std::unexpected(std::move(foreground.error()));
      if (*foreground) {
        result.append({pos, true, light, true});
      } else {
        auto background = worldp->get().tileIsOccupied(pos, TileLayer::Background, true);
        if (!background)
          return std::unexpected(std::move(background.error()));
        if (*background)
          result.append({pos, false, light, true});
      }
    }
  }
  return result;
}

std::expected<void, StarException> BeamMiningTool::init(ToolUserEntity& owner, ToolHand hand) {
  auto fireable = FireableItem::init(owner, hand);
  if (!fireable)
    return std::unexpected(std::move(fireable.error()));
  return BeamItem::init(owner, hand);
}

std::expected<void, StarException> BeamMiningTool::update(float dt, FireMode fireMode, bool shifting, HashSet<MoveControlType> const& moves) {
  auto fireable = FireableItem::update(dt, fireMode, shifting, moves);
  if (!fireable)
    return std::unexpected(std::move(fireable.error()));
  return BeamItem::update(dt, fireMode, shifting, moves);
}

std::expected<List<PersistentStatusEffect>, StarException> BeamMiningTool::statusEffects() const {
  return m_inhandStatusEffects;
}

bool BeamMiningTool::beamActive() const {
  return !ready() && !coolingDown();
}

std::expected<void, StarException> BeamMiningTool::fire(FireMode mode, bool shifting, bool edgeTriggered) {
  if (!ready())
    return {};

  auto database = root().materialDatabase();
  if (!database)
    return std::unexpected(std::move(database.error()));
  auto materialDatabase = *database;
  auto worldResult = world();
  if (!worldResult)
    return std::unexpected(std::move(worldResult.error()));
  auto& worldp = worldResult->get();
  auto ownerResult = owner();
  if (!ownerResult)
    return std::unexpected(std::move(ownerResult.error()));
  auto& ownerp = ownerResult->get();
  {
    bool used = false;
    int radius = !shifting ? m_blockRadius : m_altBlockRadius;
    String blockSound;
    List<Vec2I> brushArea;

    auto layer = (mode == FireMode::Primary ? TileLayer::Foreground : TileLayer::Background);
    bool inRange = ownerp.isAdmin();
    if (!inRange) {
      auto range = ownerp.inToolRange();
      if (!range)
        return std::unexpected(std::move(range.error()));
      inRange = *range;
    }
    if (inRange) {
      auto brushAimPosition = ownerp.aimPosition();
      if (!brushAimPosition)
        return std::unexpected(std::move(brushAimPosition.error()));
      brushArea = tileAreaBrush(radius, *brushAimPosition, true);
      auto tileAimPosition = ownerp.aimPosition();
      if (!tileAimPosition)
        return std::unexpected(std::move(tileAimPosition.error()));
      auto aimPosition = Vec2I(*tileAimPosition);

      for (auto pos : brushArea) {
        auto sound = materialDatabase->miningSound(worldp.material(pos, layer), worldp.mod(pos, layer));
        if (!sound)
          return std::unexpected(std::move(sound.error()));
        blockSound = std::move(*sound);
        if (!blockSound.empty())
          break;
      }
      if (blockSound.empty()) {
        for (auto pos : brushArea) {
          auto sound = materialDatabase->footstepSound(worldp.material(pos, layer), worldp.mod(pos, layer));
          if (!sound)
            return std::unexpected(std::move(sound.error()));
          blockSound = std::move(*sound);
          if (!blockSound.empty()) {
            auto assets = root().assets();
            if (!assets)
              return std::unexpected(std::move(assets.error()));
            auto defaultSound = (*assets)->json("/client.config:defaultFootstepSound").and_then([](Json const& value) { return value.toString(); });
            if (!defaultSound)
              return std::unexpected(std::move(defaultSound.error()));
            if (blockSound != *defaultSound)
              break;
          }
        }
      }

      auto damageResult = worldp.damageTiles(List<Vec2I>{brushArea}, layer, ownerp.position(), {TileDamageType::Beamish, m_tileDamage, m_harvestLevel}, ownerp.entityId());
      if (!damageResult)
        return std::unexpected(std::move(damageResult.error()));
      used = *damageResult != TileDamageResult::None;

      if (*damageResult == TileDamageResult::Protected) {
        auto assets = root().assets();
        if (!assets)
          return std::unexpected(std::move(assets.error()));
        auto ding = (*assets)->json("/client.config:defaultDingSound").and_then([](Json const& value) { return value.toString(); });
        if (!ding)
          return std::unexpected(std::move(ding.error()));
        blockSound = std::move(*ding);
      }

      if (!used && m_canCollectLiquid && layer == TileLayer::Foreground && worldp.material(aimPosition, TileLayer::Foreground) == EmptyMaterialId) {
        auto targetLiquid = worldp.liquidLevel(aimPosition).liquid;
        List<Vec2I> drainTiles;
        float totalLiquid = 0;
        for (auto pos : brushArea) {
          if (worldp.isTileProtected(pos))
            continue;

          auto liquid = worldp.liquidLevel(pos);
          if (liquid.liquid != EmptyLiquidId) {
            if (targetLiquid == EmptyLiquidId)
              targetLiquid = liquid.liquid;

            if (liquid.liquid == targetLiquid) {
              totalLiquid += liquid.level;
              drainTiles.append(pos);
            }
          }
        }

        auto assets = root().assets();
        if (!assets)
          return std::unexpected(std::move(assets.error()));
        auto bucketSize = (*assets)->json("/items/defaultParameters.config:liquidItems.bucketSize").and_then([](Json const& value) { return value.toUInt(); });
        if (!bucketSize)
          return std::unexpected(std::move(bucketSize.error()));
        if (totalLiquid >= *bucketSize) {
          if (auto clientWorld = asRef<WorldClient>(worldp)) {
            auto collected = clientWorld->get().collectLiquid(drainTiles, targetLiquid);
            if (!collected)
              return std::unexpected(std::move(collected.error()));
          }
          auto sound = (*assets)->json("/items/defaultParameters.config:liquidBlockSound").and_then([](Json const& value) { return value.toString(); });
          if (!sound)
            return std::unexpected(std::move(sound.error()));
          blockSound = std::move(*sound);

          used = true;
        }
      }
    }

    if (used) {
      auto strike = ownerp.addSound(root().random().randValueFrom(m_strikeSounds), m_toolVolume);
      if (!strike)
        return std::unexpected(std::move(strike.error()));
      auto sound = ownerp.addSound(blockSound, m_blockVolume);
      if (!sound)
        return std::unexpected(std::move(sound.error()));
      List<Particle> miningParticles;
      for (auto pos : brushArea) {
        auto miningParticleConfig = materialDatabase->miningParticle(worldp.material(pos, layer), worldp.mod(pos, layer));
        if (!miningParticleConfig)
          return std::unexpected(std::move(miningParticleConfig.error()));
        if (*miningParticleConfig) {
          auto miningParticle = (*miningParticleConfig)->instance();
          miningParticle.position += Vec2F(pos);
          miningParticles.append(miningParticle);
        }
      }
      auto particles = ownerp.addParticles(miningParticles);
      if (!particles)
        return std::unexpected(std::move(particles.error()));
      return FireableItem::fire(mode, shifting, edgeTriggered);
    }
  }
  return {};
}

std::expected<float, StarException> BeamMiningTool::getAngle(float angle) {
  return BeamItem::getAngle(angle);
}


std::expected<std::shared_ptr<PaintingBeamTool>, StarException> PaintingBeamTool::create(Root& root, Json const& config, String const& directory, Json const& parameters) {
  auto item = std::shared_ptr<PaintingBeamTool>(new PaintingBeamTool(root));
  auto initialized = item->initialize(config, directory, parameters);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  auto fireable = item->initializeFireable(config);
  if (!fireable)
    return std::unexpected(std::move(fireable.error()));
  auto beam = item->initializeBeam(config);
  if (!beam)
    return std::unexpected(std::move(beam.error()));
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  for (auto const& [key, target] : {std::pair{std::string_view("blockRadius"), std::ref(item->m_blockRadius)}, std::pair{std::string_view("altBlockRadius"), std::ref(item->m_altBlockRadius)}}) {
    auto value = item->instanceValue(key).and_then([](Json const& value) { return value.toFloat(); });
    if (!value)
      return std::unexpected(std::move(value.error()));
    target.get() = *value;
  }
  auto sounds = item->instanceValue("strikeSounds").and_then(jsonToStringList);
  if (!sounds)
    return std::unexpected(std::move(sounds.error()));
  item->m_strikeSounds = std::move(*sounds);
  for (auto const& [path, target] : {std::pair{std::string_view("/sfx.config:miningToolVolume"), std::ref(item->m_toolVolume)}, std::pair{std::string_view("/sfx.config:miningBlockVolume"), std::ref(item->m_blockVolume)}}) {
    auto volume = (*assets)->json(path).and_then([](Json const& value) { return value.toFloat(); });
    if (!volume)
      return std::unexpected(std::move(volume.error()));
    target.get() = *volume;
  }
  item->m_endType = EndType::Object;
  auto colors = item->instanceValue("colorNumbers").and_then([](Json const& value) { return value.toArray(); });
  if (!colors)
    return std::unexpected(std::move(colors.error()));
  for (auto const& color : *colors) {
    auto value = jsonToColor(color);
    if (!value)
      return std::unexpected(std::move(value.error()));
    item->m_colors.append(*value);
  }
  auto keys = item->instanceValue("colorKeys").and_then(jsonToStringList);
  if (!keys)
    return std::unexpected(std::move(keys.error()));
  item->m_colorKeys = std::move(*keys);
  auto index = item->instanceValue("colorIndex", 0).and_then([](Json const& value) { return value.toInt(); });
  if (!index)
    return std::unexpected(std::move(index.error()));
  item->m_colorIndex = *index;
  item->m_color = item->m_colors[item->m_colorIndex];
  return item;
}

std::expected<std::shared_ptr<Item>, StarException> PaintingBeamTool::clone() const {
  return std::make_shared<PaintingBeamTool>(*this);
}

std::expected<List<Drawable>, StarException> PaintingBeamTool::drawables() const {
  auto result = BeamItem::drawables();
  if (!result)
    return std::unexpected(std::move(result.error()));
  for (auto& entry : *result) {
    if (entry.isImage()) {
      auto& image = entry.imagePart().image;
      auto& color = m_colorKeys[m_colorIndex];
      auto directivesStart = color.find('?');
      if (!directivesStart)
        return std::unexpected(std::move(directivesStart.error()));
      if (*directivesStart != NPos) {
        auto index = color.substr(0, *directivesStart);
        if (!index)
          return std::unexpected(std::move(index.error()));
        auto directives = color.substr(*directivesStart);
        if (!directives)
          return std::unexpected(std::move(directives.error()));
        image.subPath = image.subPath.value_or("0") + *index;
        image.directives += *directives;
      } else {
        image.subPath = image.subPath.value_or("0") + color;
      }
    }
  }
  return result;
}

void PaintingBeamTool::setEnd(EndType type) {
  ((void)(type));
  m_endType = EndType::Object;
}

std::expected<void, StarException> PaintingBeamTool::update(float dt, FireMode fireMode, bool shifting, HashSet<MoveControlType> const& moves) {
  auto beam = BeamItem::update(dt, fireMode, shifting, moves);
  if (!beam)
    return std::unexpected(std::move(beam.error()));
  return FireableItem::update(dt, fireMode, shifting, moves);
}

std::expected<List<PreviewTile>, StarException> PaintingBeamTool::previewTiles(bool shifting) const {
  List<PreviewTile> result;
  auto ownerResult = owner();
  if (!ownerResult)
    return std::unexpected(std::move(ownerResult.error()));
  auto& ownerp = ownerResult->get();
  auto worldResult = world();
  if (!worldResult)
    return std::unexpected(std::move(worldResult.error()));
  auto& worldp = worldResult->get();
  Vec3B light = Color::White.toRgb();
  bool inRange = ownerp.isAdmin();
  if (!inRange) {
    auto range = ownerp.inToolRange();
    if (!range)
      return std::unexpected(std::move(range.error()));
    inRange = *range;
  }
  if (inRange) {
    int radius = !shifting ? m_blockRadius : m_altBlockRadius;
    auto aimPosition = ownerp.aimPosition();
    if (!aimPosition)
      return std::unexpected(std::move(aimPosition.error()));
    for (auto pos : tileAreaBrush(radius, *aimPosition, true)) {
      for (auto const& [layer, color] :
           {std::pair{TileLayer::Foreground, static_cast<MaterialColorVariant>(m_colorIndex)},
            std::pair{TileLayer::Background, static_cast<MaterialColorVariant>(m_colorIndex)},
            std::pair{TileLayer::Foreground, DefaultMaterialColorVariant},
            std::pair{TileLayer::Background, DefaultMaterialColorVariant}}) {
        auto placeable = worldp.canModifyTile(pos, PlaceMaterialColor{.layer = layer, .color = color}, true);
        if (!placeable)
          return std::unexpected(std::move(placeable.error()));
        if (*placeable) {
          result.append({pos, layer == TileLayer::Foreground, NullMaterialId, MaterialHue(), false, light, true, color});
          break;
        }
      }
    }
  }
  return result;
}

std::expected<void, StarException> PaintingBeamTool::init(ToolUserEntity& owner, ToolHand hand) {
  auto fireable = FireableItem::init(owner, hand);
  if (!fireable)
    return std::unexpected(std::move(fireable.error()));
  auto beam = BeamItem::init(owner, hand);
  if (!beam)
    return std::unexpected(std::move(beam.error()));
  m_color = m_colors[m_colorIndex];
  return {};
}

bool PaintingBeamTool::beamActive() const {
  return !coolingDown();
}

std::expected<void, StarException> PaintingBeamTool::fire(FireMode mode, bool shifting, bool edgeTriggered) {
  if (!ready())
    return {};

  if (mode == FireMode::Alt && edgeTriggered) {
    m_colorIndex = (m_colorIndex + 1) % m_colors.size();
    m_color = m_colors[m_colorIndex];
    return setInstanceValue("colorIndex", m_colorIndex);
  }

  if (mode == FireMode::Primary) {
    auto worldResult = world();
    if (!worldResult)
      return std::unexpected(std::move(worldResult.error()));
    auto& worldp = worldResult->get();
    auto ownerResult = owner();
    if (!ownerResult)
      return std::unexpected(std::move(ownerResult.error()));
    auto& ownerp = ownerResult->get();
    {
      bool used = false;
      int radius = !shifting ? m_blockRadius : m_altBlockRadius;

      bool inRange = ownerp.isAdmin();
      if (!inRange) {
        auto range = ownerp.inToolRange();
        if (!range)
          return std::unexpected(std::move(range.error()));
        inRange = *range;
      }
      if (inRange) {
        auto aimPosition = ownerp.aimPosition();
        if (!aimPosition)
          return std::unexpected(std::move(aimPosition.error()));
        for (auto pos : tileAreaBrush(radius, *aimPosition, true)) {
          TileModificationList modifications = {
            {pos,
             PlaceMaterialColor{.layer = TileLayer::Foreground,
                                .color = static_cast<MaterialColorVariant>(m_colorIndex)}},
            {pos,
             PlaceMaterialColor{.layer = TileLayer::Background,
                                .color = static_cast<MaterialColorVariant>(m_colorIndex)}}};
          auto failed = worldp.applyTileModifications(modifications, true);
          if (!failed)
            return std::unexpected(std::move(failed.error()));
          if (failed->count() < 2)
            used = true;
        }
      }

      if (used) {
        auto sound = ownerp.addSound(root().random().randValueFrom(m_strikeSounds), m_toolVolume);
        if (!sound)
          return std::unexpected(std::move(sound.error()));
        return FireableItem::fire(mode, shifting, edgeTriggered);
      }
    }
  }
  return {};
}

std::expected<float, StarException> PaintingBeamTool::getAngle(float angle) {
  return BeamItem::getAngle(angle);
}

}
