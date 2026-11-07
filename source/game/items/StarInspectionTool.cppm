module;
import std;
import star.format;
import star.type_identity;
import star.json;
import star.json_extra;
import star.math_common;
import star.algorithm;
import star.random;
import star.id_map;
import star.list;
import star.bi_map;
import star.data_stream;
import star.casting;
import star.strong_typedef;
import star.rpc_promise;
import star.rect;
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
import star.liquid_types;
import star.material_types;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.version;


import star.collision_block;
import star.item_descriptor;
import star.item;
import star.pointable_item;
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
import star.mixer;
import star.drawable;
import star.entity_rendering_types;
import star.animation;
import star.particle;

import star.light_source;
import star.entity_rendering;
import star.inspectable_entity;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;

export module star.inspection_tool;
import star.liquids_database;
import star.material_database;


export namespace Star {

class InspectionTool
  : public Item,
    public PointableItem,
    public ToolUserItem {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<InspectionTool, Item, PointableItem, ToolUserItem>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<InspectionTool>();
  }


  struct InspectionResult {
    String message;
    std::optional<String> objectName = {};
    std::optional<EntityId> entityId = {};
  };

  [[nodiscard]]
  static std::expected<std::shared_ptr<InspectionTool>, StarException> create(Root& root, Json const& config, String const& directory, Json const& parameters = JsonObject());

  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> clone() const override;

  [[nodiscard]]
  std::expected<void, StarException> update(float dt, FireMode fireMode, bool shifting, HashSet<MoveControlType> const& moves) override;

  [[nodiscard]]
  std::expected<List<Drawable>, StarException> drawables() const override;

  [[nodiscard]]
  std::expected<List<LightSource>, StarException> lightSources() const;

  [[nodiscard]]
  std::expected<float, StarException> inspectionHighlightLevel(std::shared_ptr<InspectableEntity> const& inspectableEntity) const;

  List<InspectionResult> pullInspectionResults();

private:
  explicit InspectionTool(Root& root) : Item(root) {}
  [[nodiscard]]
  std::expected<InspectionResult, StarException> inspect(Vec2F const& position);

  [[nodiscard]]
  std::expected<float, StarException> inspectionLevel(std::shared_ptr<InspectableEntity> const& inspectableEntity) const;
  [[nodiscard]]
  std::expected<float, StarException> pointInspectionLevel(Vec2F const& position) const;
  [[nodiscard]]
  std::expected<bool, StarException> hasLineOfSight(Vec2I const& targetPosition, Set<Vec2I> const& targetSpaces = {}) const;

  [[nodiscard]]
  std::expected<String, StarException> inspectionFailureText(String const& failureType, String const& species) const;

  float m_currentAngle;
  Vec2F m_currentPosition;

  String m_image;
  Vec2F m_handPosition;
  Vec2F m_lightPosition;
  Color m_lightColor;
  float m_beamWidth;
  float m_ambientFactor;

  bool m_showHighlights;
  bool m_allowScanning;
  bool m_requireLineOfSight;

  Vec2F m_inspectionAngles;
  Vec2F m_inspectionRanges;
  float m_ambientInspectionRadius;
  std::size_t m_fullInspectionSpaces;
  float m_minimumInspectionLevel;
  std::optional<HashSet<EntityType>> m_inspectableTypeFilter;

  FireMode m_lastFireMode;
  List<InspectionResult> m_inspectionResults;
};

}

namespace Star {

std::expected<std::shared_ptr<InspectionTool>, StarException> InspectionTool::create(Root& root, Json const& config, String const& directory, Json const& parameters) {
  auto item = std::shared_ptr<InspectionTool>(new InspectionTool(root));
  auto initialized = item->initialize(config, directory, parameters);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  auto image = item->instanceValue("image").and_then([](Json const& value) { return value.toString(); });
  if (!image)
    return std::unexpected(std::move(image.error()));
  item->m_image = AssetPath::relativeTo(directory, *image);
  for (auto const& [key, target] : {std::pair{std::string_view("handPosition"), std::ref(item->m_handPosition)}, std::pair{std::string_view("lightPosition"), std::ref(item->m_lightPosition)}}) {
    auto value = item->instanceValue(key).and_then(jsonToVec2F);
    if (!value)
      return std::unexpected(std::move(value.error()));
    target.get() = *value;
  }
  auto color = item->instanceValue("lightColor").and_then(jsonToColor);
  if (!color)
    return std::unexpected(std::move(color.error()));
  item->m_lightColor = *color;
  for (auto const& [key, target] : {std::pair{std::string_view("beamLevel"), std::ref(item->m_beamWidth)}, std::pair{std::string_view("beamAmbience"), std::ref(item->m_ambientFactor)}}) {
    auto value = item->instanceValue(key).and_then([](Json const& value) { return value.toFloat(); });
    if (!value)
      return std::unexpected(std::move(value.error()));
    target.get() = *value;
  }
  for (auto const& [key, target] : {std::pair{std::string_view("showHighlights"), std::ref(item->m_showHighlights)}, std::pair{std::string_view("allowScanning"), std::ref(item->m_allowScanning)}}) {
    auto value = item->instanceValue(key).and_then([](Json const& value) { return value.toBool(); });
    if (!value)
      return std::unexpected(std::move(value.error()));
    target.get() = *value;
  }
  auto lineOfSight = item->instanceValue("requireLineOfSight", true);
  if (!lineOfSight)
    return std::unexpected(std::move(lineOfSight.error()));
  item->m_requireLineOfSight = *lineOfSight == Json(true);
  for (auto const& [key, target] : {std::pair{std::string_view("inspectionAngles"), std::ref(item->m_inspectionAngles)}, std::pair{std::string_view("inspectionRanges"), std::ref(item->m_inspectionRanges)}}) {
    auto value = item->instanceValue(key).and_then(jsonToVec2F);
    if (!value)
      return std::unexpected(std::move(value.error()));
    target.get() = *value;
  }
  auto ambient = item->instanceValue("ambientInspectionRadius").and_then([](Json const& value) { return value.toFloat(); });
  if (!ambient)
    return std::unexpected(std::move(ambient.error()));
  item->m_ambientInspectionRadius = *ambient;
  auto spaces = item->instanceValue("fullInspectionSpaces").and_then([](Json const& value) { return value.toUInt(); });
  if (!spaces)
    return std::unexpected(std::move(spaces.error()));
  item->m_fullInspectionSpaces = *spaces;
  auto minimum = item->instanceValue("minimumInspectionLevel").and_then([](Json const& value) { return value.toFloat(); });
  if (!minimum)
    return std::unexpected(std::move(minimum.error()));
  item->m_minimumInspectionLevel = *minimum;
  auto filter = item->instanceValue("inspectableTypeFilter");
  if (!filter)
    return std::unexpected(std::move(filter.error()));
  if (filter->isType(Json::Type::Array)) {
    auto values = filter->toArray();
    if (!values)
      return std::unexpected(std::move(values.error()));
    item->m_inspectableTypeFilter.emplace();
    for (auto const& value : *values) {
      if (!value.isType(Json::Type::String))
        continue;
      auto name = value.toString();
      if (!name)
        return std::unexpected(std::move(name.error()));
      if (!EntityTypeNames)
        return std::unexpected(EntityTypeNames.error());
      auto type = EntityTypeNames->maybeLeftRef(*name);
      if (!type)
        return std::unexpected(std::move(type.error()));
      if (*type)
        item->m_inspectableTypeFilter->add((*type)->get());
    }
  }
  item->m_lastFireMode = FireMode::None;
  return item;
}

std::expected<std::shared_ptr<Item>, StarException> InspectionTool::clone() const {
  return std::make_shared<InspectionTool>(*this);
}

std::expected<void, StarException> InspectionTool::update(float, FireMode fireMode, bool, HashSet<MoveControlType> const&) {
  auto itemWorld = world();
  if (!itemWorld)
    return std::unexpected(std::move(itemWorld.error()));
  auto itemOwner = owner();
  if (!itemOwner)
    return std::unexpected(std::move(itemOwner.error()));
  auto itemHand = hand();
  if (!itemHand)
    return std::unexpected(std::move(itemHand.error()));
  auto aimPosition = itemOwner->get().aimPosition();
  if (!aimPosition)
    return std::unexpected(std::move(aimPosition.error()));
  m_currentAngle = itemWorld->get().geometry().diff(*aimPosition, itemOwner->get().position()).angle();
  auto position = itemOwner->get().handPosition(*itemHand, m_lightPosition - m_handPosition);
  if (!position)
    return std::unexpected(std::move(position.error()));
  m_currentPosition = itemOwner->get().position() + *position;
  root().spatialLogger().logPoint("world", m_currentPosition, {0, 0, 255, 255});
  if (fireMode != m_lastFireMode && fireMode != FireMode::None) {
    auto inspectionPosition = itemOwner->get().aimPosition();
    if (!inspectionPosition)
      return std::unexpected(std::move(inspectionPosition.error()));
    auto result = inspect(*inspectionPosition);
    if (!result)
      return std::unexpected(std::move(result.error()));
    m_inspectionResults.append(std::move(*result));
  }
  m_lastFireMode = fireMode;
  return {};
}

std::expected<List<Drawable>, StarException> InspectionTool::drawables() const {
  auto metadata = root().imageMetadataDatabase();
  if (!metadata)
    return std::unexpected(std::move(metadata.error()));
  auto drawable = Drawable::makeImage(**metadata, m_image, 1.0f / TilePixels, true, -m_handPosition);
  if (!drawable)
    return std::unexpected(std::move(drawable.error()));
  return List<Drawable>{std::move(*drawable)};
}

std::expected<List<LightSource>, StarException> InspectionTool::lightSources() const {
  if (!initialized())
    return List<LightSource>{};
  auto itemWorld = world();
  if (!itemWorld)
    return std::unexpected(std::move(itemWorld.error()));
  auto itemOwner = owner();
  if (!itemOwner)
    return std::unexpected(std::move(itemOwner.error()));
  auto itemHand = hand();
  if (!itemHand)
    return std::unexpected(std::move(itemHand.error()));
  auto aimPosition = itemOwner->get().aimPosition();
  if (!aimPosition)
    return std::unexpected(std::move(aimPosition.error()));
  float angle = itemWorld->get().geometry().diff(*aimPosition, itemOwner->get().position()).angle();
  auto position = itemOwner->get().handPosition(*itemHand, m_lightPosition - m_handPosition);
  if (!position)
    return std::unexpected(std::move(position.error()));
  LightSource lightSource;
  lightSource.type = LightType::Point;
  lightSource.position = itemOwner->get().position() + *position;
  lightSource.color = m_lightColor.toRgbF();
  lightSource.pointBeam = m_beamWidth;
  lightSource.beamAngle = angle;
  lightSource.beamAmbience = m_ambientFactor;
  return List<LightSource>{std::move(lightSource)};
}

std::expected<float, StarException> InspectionTool::inspectionHighlightLevel(std::shared_ptr<InspectableEntity> const& inspectable) const {
  if (m_showHighlights)
    return inspectionLevel(inspectable);
  return 0;
}

List<InspectionTool::InspectionResult> InspectionTool::pullInspectionResults() {
  return std::exchange(m_inspectionResults, {});
}

std::expected<float, StarException> InspectionTool::inspectionLevel(std::shared_ptr<InspectableEntity> const& inspectable) const {
  if (!initialized() || !inspectable->inspectable())
    return 0;

  if (m_inspectableTypeFilter && !m_inspectableTypeFilter->contains(inspectable->entityType()))
    return 0;

  if (auto tileEntity = as<TileEntity>(inspectable)) {
    float totalLevel = 0;

    // convert spaces to a set of world positions
    Set<Vec2I> spaceSet;
    for (auto space : tileEntity->spaces())
      spaceSet.add(tileEntity->tilePosition() + space);

    for (auto space : spaceSet) {
      auto pointLevel = pointInspectionLevel(centerOfTile(space));
      if (!pointLevel)
        return std::unexpected(std::move(pointLevel.error()));
      if (*pointLevel > 0) {
        auto lineOfSight = hasLineOfSight(space, spaceSet);
        if (!lineOfSight)
          return std::unexpected(std::move(lineOfSight.error()));
        if (*lineOfSight)
          totalLevel += *pointLevel;
      }
    }
    return std::clamp(totalLevel / std::min(spaceSet.size(), m_fullInspectionSpaces), 0.0f, 1.0f);
  }
  else
    return pointInspectionLevel(inspectable->position());
}

std::expected<float, StarException> InspectionTool::pointInspectionLevel(Vec2F const& position) const {
  auto itemWorld = world();
  if (!itemWorld)
    return std::unexpected(std::move(itemWorld.error()));
  Vec2F gdiff = itemWorld->get().geometry().diff(position, m_currentPosition);
  float gdist = gdiff.magnitude();
  float angleFactor = (std::abs(angleDiff(gdiff.angle(), m_currentAngle)) - m_inspectionAngles[0]) / (m_inspectionAngles[1] - m_inspectionAngles[0]);
  float distFactor = (gdist - m_inspectionRanges[0]) / (m_inspectionRanges[1] - m_inspectionRanges[0]);
  float ambientFactor = gdist / m_ambientInspectionRadius;
  return 1 - std::clamp(std::max(distFactor, std::min(ambientFactor, angleFactor)), 0.0f, 1.0f);
}

std::expected<bool, StarException> InspectionTool::hasLineOfSight(Vec2I const& position, Set<Vec2I> const& targetSpaces) const {
  if (!m_requireLineOfSight)
    return true;
  auto itemWorld = world();
  if (!itemWorld)
    return std::unexpected(std::move(itemWorld.error()));
  auto collisions = itemWorld->get().collidingTilesAlongLine(centerOfTile(m_currentPosition), centerOfTile(position));
  if (!collisions)
    return std::unexpected(std::move(collisions.error()));
  for (auto collision : *collisions) {
    if (collision != position && !targetSpaces.contains(collision))
      return false;
  }
  return true;
}

std::expected<InspectionTool::InspectionResult, StarException> InspectionTool::inspect(Vec2F const& position) {
  auto assets = root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto itemOwner = owner();
  if (!itemOwner)
    return std::unexpected(std::move(itemOwner.error()));
  auto species = itemOwner->get().species();
  auto check = [&](std::shared_ptr<InspectableEntity> const& entity) -> std::expected<std::optional<InspectionResult>, StarException> {
    if (m_inspectableTypeFilter && !m_inspectableTypeFilter->contains(entity->entityType()))
      return {};
    if (entity->inspectable()) {
      auto level = inspectionLevel(entity);
      if (!level)
        return std::unexpected(std::move(level.error()));
      if (*level >= m_minimumInspectionLevel) {
        auto description = entity->inspectionDescription(species);
        if (!description)
          return std::unexpected(std::move(description.error()));
        if (m_allowScanning) {
          auto logName = entity->inspectionLogName();
          if (!logName)
            return std::unexpected(std::move(logName.error()));
          return InspectionResult{.message = description->value_or(String{}),
                                  .objectName = std::move(*logName),
                                  .entityId = entity->entityId()};
        }
        return InspectionResult{.message = description->value_or(String{}), .objectName = {}, .entityId = {}};
      }
    }
    return {};
  };
  auto itemWorld = world();
  if (!itemWorld)
    return std::unexpected(std::move(itemWorld.error()));
  WorldGeometry geometry = itemWorld->get().geometry();
  auto candidates = itemWorld->get().query<InspectableEntity>(RectF::withCenter(position, Vec2F()), [&](std::shared_ptr<InspectableEntity> const& entity) -> std::expected<bool, StarException> {
    if (entity->entityType() == EntityType::Object)
      return false;
    auto contained = geometry.rectContains(entity->metaBoundBox().translated(entity->position()), position);
    if (!contained)
      return std::unexpected(std::move(contained.error()));
    if (!*contained)
      return false;
    auto hitPoly = entity->hitPoly();
    if (!hitPoly)
      return std::unexpected(std::move(hitPoly.error()));
    if (!*hitPoly)
      return false;
    return geometry.polyContains(**hitPoly, position);
  });
  if (!candidates)
    return std::unexpected(std::move(candidates.error()));
  for (auto const& entity : *candidates) {
    auto result = check(entity);
    if (!result)
      return std::unexpected(std::move(result.error()));
    if (*result)
      return std::move(**result);
  }
  auto atTile = itemWorld->get().atTile<InspectableEntity>(Vec2I::floor(position));
  if (!atTile)
    return std::unexpected(std::move(atTile.error()));
  for (auto const& entity : *atTile) {
    auto result = check(entity);
    if (!result)
      return std::unexpected(std::move(result.error()));
    if (*result)
      return std::move(**result);
  }
  auto lineOfSight = hasLineOfSight(Vec2I::floor(position));
  if (!lineOfSight)
    return std::unexpected(std::move(lineOfSight.error()));
  bool outOfRange = !*lineOfSight;
  if (!outOfRange) {
    auto level = pointInspectionLevel(centerOfTile(position));
    if (!level)
      return std::unexpected(std::move(level.error()));
    outOfRange = *level < m_minimumInspectionLevel;
  }
  if (outOfRange)
    return inspectionFailureText("outOfRangeText", species).transform([](String message) {
      return InspectionResult{.message = std::move(message), .objectName = {}};
    });
  MaterialId fgMaterial = itemWorld->get().material(Vec2I::floor(position), TileLayer::Foreground);
  MaterialId fgMod = itemWorld->get().mod(Vec2I(position.floor()), TileLayer::Foreground);
  auto materialDatabase = root().materialDatabase();
  if (!materialDatabase)
    return std::unexpected(std::move(materialDatabase.error()));
  if (isRealMaterial(fgMaterial)) {
    auto description = isRealMod(fgMod) ? (*materialDatabase)->modDescription(fgMod, species) : (*materialDatabase)->materialDescription(fgMaterial, species);
    return std::move(description).transform([](String message) {
      return InspectionResult{.message = std::move(message), .objectName = {}};
    });
  }
  auto liquidLevel = itemWorld->get().liquidLevel(Vec2I::floor(position));
  auto liquidsDatabase = root().liquidsDatabase();
  if (!liquidsDatabase)
    return std::unexpected(std::move(liquidsDatabase.error()));
  if (liquidLevel.liquid != EmptyLiquidId)
    return (*liquidsDatabase)->liquidDescription(liquidLevel.liquid, species).transform([](String message) {
      return InspectionResult{.message = std::move(message), .objectName = {}};
    });
  MaterialId bgMaterial = itemWorld->get().material(Vec2I::floor(position), TileLayer::Background);
  MaterialId bgMod = itemWorld->get().mod(Vec2I(position.floor()), TileLayer::Background);
  if (isRealMaterial(bgMaterial)) {
    auto description = isRealMod(bgMod) ? (*materialDatabase)->modDescription(bgMod, species) : (*materialDatabase)->materialDescription(bgMaterial, species);
    return std::move(description).transform([](String message) {
      return InspectionResult{.message = std::move(message), .objectName = {}};
    });
  }
  return inspectionFailureText("nothingThereText", species).transform([](String message) {
    return InspectionResult{.message = std::move(message), .objectName = {}};
  });
}

std::expected<String, StarException> InspectionTool::inspectionFailureText(String const& failureType, String const& species) const {
  auto nothingThere = instanceValue(failureType);
  if (!nothingThere)
    return std::unexpected(std::move(nothingThere.error()));
  auto contains = nothingThere->contains(species);
  if (!contains)
    return std::unexpected(std::move(contains.error()));
  auto options = nothingThere->getArray(*contains ? species : String("default"));
  if (!options)
    return std::unexpected(std::move(options.error()));
  auto option = options->wrap(root().random().randu64());
  if (!option)
    return std::unexpected(std::move(option.error()));
  return option->get().toString();
}

}
