module;
import std;
import star.format;
import star.type_identity;
import star.json;
import star.json_extra;
import star.id_map;
import star.casting;
import star.strong_typedef;
import star.rpc_promise;
import star.a_star;
import star.audio;
import star.thread;
import star.list;
import star.map;
import star.set;
import star.vector;
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
import star.ordered_map;
import star.io_device;
import star.ref_ptr;


import star.item;
import star.pointable_item;
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
import star.mixer;
import star.drawable;
import star.entity_rendering_types;
import star.animation;
import star.particle;

import star.light_source;
import star.entity_rendering;
import star.asset_source;
import star.assets;
import star.root;

export module star.flashlight;

export namespace Star {

class Flashlight : public Item, public PointableItem, public ToolUserItem {
public:
  void const* query_type(TypeKey key) const noexcept override {
    return query_type_of<Flashlight, Item, PointableItem, ToolUserItem>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<Flashlight>(); }

  [[nodiscard]]
  static std::expected<std::shared_ptr<Flashlight>, StarException> create(Root& root, Json const& config, String const& directory, Json const& parameters = JsonObject());

  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> clone() const override;

  [[nodiscard]]
  std::expected<List<Drawable>, StarException> drawables() const override;

  [[nodiscard]]
  std::expected<List<LightSource>, StarException> lightSources() const;

private:
  explicit Flashlight(Root& root) : Item(root) {}
  String m_image;
  Vec2F m_handPosition;
  Vec2F m_lightPosition;
  Color m_lightColor;
  float m_beamWidth;
  float m_ambientFactor;
};

}

namespace Star {

std::expected<std::shared_ptr<Flashlight>, StarException> Flashlight::create(Root& root, Json const& config, String const& directory, Json const& parameters) {
  auto item = std::shared_ptr<Flashlight>(new Flashlight(root));
  auto initialized = item->initialize(config, directory, parameters);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  auto image = item->instanceValue("image").and_then([](Json const& value) { return value.toString(); });
  if (!image)
    return std::unexpected(std::move(image.error()));
  item->m_image = AssetPath::relative_to(directory, *image);
  auto handPosition = item->instanceValue("handPosition").and_then(jsonToVec2F);
  if (!handPosition)
    return std::unexpected(std::move(handPosition.error()));
  item->m_handPosition = *handPosition;
  auto lightPosition = item->instanceValue("lightPosition").and_then(jsonToVec2F);
  if (!lightPosition)
    return std::unexpected(std::move(lightPosition.error()));
  item->m_lightPosition = *lightPosition;
  auto lightColor = item->instanceValue("lightColor").and_then(jsonToColor);
  if (!lightColor)
    return std::unexpected(std::move(lightColor.error()));
  item->m_lightColor = *lightColor;
  auto beamWidth = item->instanceValue("beamLevel").and_then([](Json const& value) { return value.toFloat(); });
  if (!beamWidth)
    return std::unexpected(std::move(beamWidth.error()));
  item->m_beamWidth = *beamWidth;
  auto ambientFactor = item->instanceValue("beamAmbience").and_then([](Json const& value) { return value.toFloat(); });
  if (!ambientFactor)
    return std::unexpected(std::move(ambientFactor.error()));
  item->m_ambientFactor = *ambientFactor;
  return item;
}

std::expected<std::shared_ptr<Item>, StarException> Flashlight::clone() const {
  return std::make_shared<Flashlight>(*this);
}

std::expected<List<Drawable>, StarException> Flashlight::drawables() const {
  auto metadata = root().imageMetadataDatabase();
  if (!metadata)
    return std::unexpected(std::move(metadata.error()));
  auto drawable = Drawable::makeImage(**metadata, m_image, 1.0f / TilePixels, true, -m_handPosition / TilePixels);
  if (!drawable)
    return std::unexpected(std::move(drawable.error()));
  return List<Drawable>{std::move(*drawable)};
}

std::expected<List<LightSource>, StarException> Flashlight::lightSources() const {
  if (!initialized())
    return List<LightSource>{};
  auto ownerResult = owner();
  if (!ownerResult)
    return std::unexpected(std::move(ownerResult.error()));
  auto worldResult = world();
  if (!worldResult)
    return std::unexpected(std::move(worldResult.error()));
  auto handResult = hand();
  if (!handResult)
    return std::unexpected(std::move(handResult.error()));
  auto ownerHandPosition = ownerResult->get().handPosition(*handResult, (m_lightPosition - m_handPosition) / TilePixels);
  if (!ownerHandPosition)
    return std::unexpected(std::move(ownerHandPosition.error()));
  auto aimPosition = ownerResult->get().aimPosition();
  if (!aimPosition)
    return std::unexpected(std::move(aimPosition.error()));
  float angle = worldResult->get().geometry().diff(*aimPosition, ownerResult->get().position()).angle();
  LightSource lightSource;
  lightSource.type = LightType::Point;
  lightSource.position = ownerResult->get().position() + *ownerHandPosition;
  lightSource.color = m_lightColor.toRgbF();
  lightSource.pointBeam = m_beamWidth;
  lightSource.beamAngle = angle;
  lightSource.beamAmbience = m_ambientFactor;
  return List<LightSource>{std::move(lightSource)};
}

}
