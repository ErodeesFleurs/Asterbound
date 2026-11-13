module;
import std;
import star.type_identity;
import star.json;
import star.json_extra;
import star.random;
import star.id_map;
import star.list;
import star.bi_map;
import star.data_stream;
import star.casting;
import star.strong_typedef;
import star.rpc_promise;
import star.a_star;
import star.audio;
import star.thread;
import star.map;
import star.set;
import star.vector;
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
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.format;
import star.version;


import star.collision_block;
import star.item_descriptor;
import star.item;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.entity;
import star.interaction_types;
import star.tile_damage;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
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
import star.swingable_item;
import star.mixer;
import star.drawable;
import star.entity_rendering_types;
import star.animation;
import star.particle;

import star.light_source;
import star.entity_rendering;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;

export module star.tilling_tool;
import star.material_database;


export namespace Star {

class TillingTool : public Item, public SwingableItem {
public:
  void const* query_type(TypeKey key) const noexcept override {
    return query_type_of<TillingTool, Item, SwingableItem>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<TillingTool>(); }

  [[nodiscard]]
  static std::expected<std::shared_ptr<TillingTool>, StarException> create(Root& root, Json const& config, String const& directory, Json const& parameters = JsonObject());

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
  explicit TillingTool(Root& root) : Item(root) {}
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
};

}

namespace Star {

std::expected<std::shared_ptr<TillingTool>, StarException> TillingTool::create(Root& root, Json const& config, String const& directory, Json const& parameters) {
  auto item = std::shared_ptr<TillingTool>(new TillingTool(root));
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
    auto frame = item->m_image.replace("{frame}", Star::to_string(i));
    if (!frame)
      return std::unexpected(std::move(frame.error()));
    item->m_animationFrame.push_back(std::move(*frame));
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
  item->m_frameTiming = 0;
  return item;
}

std::expected<std::shared_ptr<Item>, StarException> TillingTool::clone() const {
  return std::make_shared<TillingTool>(*this);
}

std::expected<List<Drawable>, StarException> TillingTool::drawables() const {
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

Vec2F TillingTool::handPosition() const {
  return m_handPosition;
}

std::expected<void, StarException> TillingTool::fire(FireMode mode, bool shifting, bool edgeTriggered) {
  if (!ready())
    return {};
  auto strikeSound = root().random().randValueFrom(m_strikeSounds);
  auto itemOwner = owner();
  if (!itemOwner)
    return std::unexpected(std::move(itemOwner.error()));
  auto itemWorld = world();
  if (!itemWorld)
    return std::unexpected(std::move(itemWorld.error()));
  auto database = root().materialDatabase();
  if (!database)
    return std::unexpected(std::move(database.error()));
  auto aimPosition = itemOwner->get().aimPosition();
  if (!aimPosition)
    return std::unexpected(std::move(aimPosition.error()));
  Vec2I pos(aimPosition->floor());
  if (itemWorld->get().material(pos + Vec2I(0, 1), TileLayer::Foreground) != EmptyMaterialId)
    return {};
  bool used = false;
  for (auto layer : {TileLayer::Foreground, TileLayer::Background}) {
    if (itemWorld->get().material(pos, layer) == EmptyMaterialId)
      pos = pos - Vec2I(0, 1);
    if (layer == TileLayer::Background && itemWorld->get().material(pos + Vec2I(0, 1), TileLayer::Background) != EmptyMaterialId)
      continue;
    bool inRange = itemOwner->get().isAdmin();
    if (!inRange) {
      auto range = itemOwner->get().inToolRange();
      if (!range)
        return std::unexpected(std::move(range.error()));
      inRange = *range;
    }
    if (inRange) {
      auto currentMod = itemWorld->get().mod(pos, layer);
      auto material = itemWorld->get().material(pos, layer);
      auto tilledMod = (*database)->tilledModFor(material);
      if (!tilledMod)
        return std::unexpected(std::move(tilledMod.error()));
      if (*tilledMod != NoModId && currentMod == NoModId) {
        auto modified = itemWorld->get().modifyTile(
          pos, PlaceMod{.layer = layer, .mod = *tilledMod, .modHueShift = MaterialHue()}, true);
        if (!modified)
          return std::unexpected(std::move(modified.error()));
        if (*modified)
          used = true;
      } else if (currentMod != *tilledMod) {
        auto damageResult = itemWorld->get().damageTile(pos, layer, itemOwner->get().position(), {TileDamageType::Tilling, 1.0f});
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
          strikeSound = std::move(*ding);
        }
      }
    }
  }
  if (used) {
    auto sound = itemOwner->get().addSound(strikeSound, m_toolVolume);
    if (!sound)
      return std::unexpected(std::move(sound.error()));
    return SwingableItem::fire(mode, shifting, edgeTriggered);
  }
  return {};
}

std::expected<void, StarException> TillingTool::update(float dt, FireMode fireMode, bool shifting, HashSet<MoveControlType> const& moves) {
  auto updated = SwingableItem::update(dt, fireMode, shifting, moves);
  if (!updated)
    return std::unexpected(std::move(updated.error()));

  if (!ready() && !coolingDown())
    m_frameTiming = std::fmod((m_frameTiming + dt), m_frameCycle);
  else
    m_frameTiming = 0;
  return {};
}

std::expected<float, StarException> TillingTool::getAngle(float aimAngle) {
  if (!ready() && !coolingDown())
    return SwingableItem::getAngle(aimAngle);
  return aimAngle;
}

}
