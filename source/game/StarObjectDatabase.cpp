import star.output_proxy;
import star.memory;
import star.json_extra;
import star.json;
import star.id_map;
import star.random;
import star.periodic_function;
import star.ordered_map;
import star.lru_cache;
import star.thread;
import star.time;
import star.ttl_cache;
import star.game_types;
import star.material_types;
import star.data_stream;
import star.bi_map;
import star.string;
import star.color;
import star.list;
import star.rect;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.set;
import star.vector;
import star.strong_typedef;
import star.casting;
import star.rpc_promise;
import star.logging;
import star.math_common;
import star.periodic;
import star.lua;
import star.audio;
import star.map;
import star.io_device;
import star.ref_ptr;
import star.listener;
import std;
import star.format;
import star.version;
import star.weighted_pool;

import star.lua_root;
import star.utility_lua_bindings;
import star.item_descriptor;
import star.drawable;
import star.animation;
import star.particle;
import star.tile_damage;
import star.damage_types;
import star.status_types;
import star.entity_rendering_types;
import star.lua_components;
import star.status_effect_entity;
import star.scripted_entity;
import star.chat_action;
import star.chatty_entity;
import star.wiring;
import star.wire_entity;
import star.inspectable_entity;
import star.animated_part_set;
import star.mixer;
import star.networked_animator;
import star.entity_rendering;
import star.object;
import star.world_geometry;
import star.damage;
import star.entity;
import star.interaction_types;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.tile_entity;
import star.tile_modification;
import star.force_regions;
import star.world;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.container_entity;
import star.item_recipe;
import star.container_object;
import star.collision_block;

import star.object_database;
import star.teleporter_object;
import star.physics_object;
import star.loungeable_object;
import star.farmable_object;
import star.material_database;
import star.image_metadata_database;
import star.rebuilder;

import star.light_source;

import star.data_stream_devices;

import star.net_compatibility;
import star.byte_array;
namespace Star {

std::expected<ObjectOrientation::ParticleEmissionEntry, StarException> ObjectOrientation::parseParticleEmitter(Root& root, String const& path, Json const& config) {
  ParticleEmissionEntry result;
  auto particleEmissionRateResult = config.getFloat("emissionRate", 0.0);
  if (!particleEmissionRateResult)
    return std::unexpected(std::move(particleEmissionRateResult.error()));
  auto particleEmissionRate = std::move(*particleEmissionRateResult);
  result.particleEmissionRate = particleEmissionRate;
  auto particleEmissionRateVarianceResult = config.getFloat("emissionVariance", 0.0);
  if (!particleEmissionRateVarianceResult)
    return std::unexpected(std::move(particleEmissionRateVarianceResult.error()));
  auto particleEmissionRateVariance = std::move(*particleEmissionRateVarianceResult);
  result.particleEmissionRateVariance = particleEmissionRateVariance;
  auto particleConfigResult = config.getObject("particle", JsonObject());
  if (!particleConfigResult)
    return std::unexpected(std::move(particleConfigResult.error()));
  auto particleConfig = std::move(*particleConfigResult);
  auto particleResult = Particle::create(root, particleConfig, path);
  if (!particleResult)
    return std::unexpected(std::move(particleResult.error()));
  auto particle = std::move(*particleResult);
  result.particle = std::move(particle);
  auto particleVarianceConfigResult = config.getObject("particleVariance", JsonObject());
  if (!particleVarianceConfigResult)
    return std::unexpected(std::move(particleVarianceConfigResult.error()));
  auto particleVarianceConfig = std::move(*particleVarianceConfigResult);
  auto particleVarianceResult = Particle::create(root, particleVarianceConfig, path);
  if (!particleVarianceResult)
    return std::unexpected(std::move(particleVarianceResult.error()));
  auto particleVariance = std::move(*particleVarianceResult);
  result.particleVariance = std::move(particleVariance);
  auto originConfigResult = config.get("pixelOrigin", JsonArray{TilePixels / 2, TilePixels / 2});
  if (!originConfigResult)
    return std::unexpected(std::move(originConfigResult.error()));
  auto originConfig = std::move(*originConfigResult);
  auto originResult = jsonToVec2F(originConfig);
  if (!originResult)
    return std::unexpected(std::move(originResult.error()));
  auto origin = std::move(*originResult);
  result.particle.position += origin / TilePixels;
  auto placeInSpacesResult = config.getBool("placeInSpaces", false);
  if (!placeInSpacesResult)
    return std::unexpected(std::move(placeInSpacesResult.error()));
  auto placeInSpaces = std::move(*placeInSpacesResult);
  result.placeInSpaces = placeInSpaces;
  return result;
}

std::expected<bool, StarException> ObjectOrientation::placementValid(World const& world, Vec2I const& position) const {
  for (Vec2I space : spaces) {
    space += position;
    auto occupiedResult = world.tileIsOccupied(space, TileLayer::Foreground, false, true);
    if (!occupiedResult)
      return std::unexpected(std::move(occupiedResult.error()));
    auto occupied = std::move(*occupiedResult);
    if (occupied)
      return false;
    auto protectedTile = world.isTileProtected(space);
    if (protectedTile)
      return false;
  }
  return true;
}

std::expected<bool, StarException> ObjectOrientation::anchorsValid(World const& world, Vec2I const& position) const {
  if (anchors.empty())
    return true;
  auto materialDatabaseResult = world.root().materialDatabase();
  if (!materialDatabaseResult)
    return std::unexpected(std::move(materialDatabaseResult.error()));
  auto materialDatabase = std::move(*materialDatabaseResult);
  bool anyValid = false;
  for (auto const& anchor : anchors) {
    auto space = position + anchor.position;
    auto valid = [&]() -> std::expected<bool, StarException> {
      auto connectableResult = world.isTileConnectable(space, anchor.layer);
      if (!connectableResult)
        return std::unexpected(std::move(connectableResult.error()));
      auto connectable = std::move(*connectableResult);
      if (!connectable)
        return false;
      if (anchor.tilled) {
        auto tilledResult = materialDatabase->isTilledMod(world.mod(space, anchor.layer));
        if (!tilledResult)
          return std::unexpected(std::move(tilledResult.error()));
        auto tilled = std::move(*tilledResult);
        if (!tilled)
          return false;
      }
      if (anchor.soil) {
        auto soilResult = materialDatabase->isSoil(world.material(space, anchor.layer));
        if (!soilResult)
          return std::unexpected(std::move(soilResult.error()));
        auto soil = std::move(*soilResult);
        if (!soil)
          return false;
      }
      if (anchor.material && world.material(space, anchor.layer) != *anchor.material)
        return false;
      return true;
    }();
    if (!valid)
      return std::unexpected(std::move(valid.error()));
    if (*valid)
      anyValid = true;
    else if (!anchorAny)
      return false;
  }
  return anyValid;
}

std::expected<std::size_t, StarException> ObjectConfig::findValidOrientation(World const& world, Vec2I const& position, std::optional<Direction> directionAffinity) const {
  auto valid = [&orientations = orientations, &world, &position](std::size_t index) -> std::expected<bool, StarException> {
    auto placementResult = orientations[index]->placementValid(world, position);
    if (!placementResult)
      return std::unexpected(std::move(placementResult.error()));
    auto placement = std::move(*placementResult);
    if (!placement)
      return false;
    return orientations[index]->anchorsValid(world, position);
  };
  // Try the requested affinity first, preserving orientation ordering.
  if (directionAffinity) {
    for (std::size_t i = 0; i < orientations.size(); ++i) {
      if (!orientations[i]->directionAffinity || *directionAffinity != *orientations[i]->directionAffinity)
        continue;
      auto matchesResult = valid(i);
      if (!matchesResult)
        return std::unexpected(std::move(matchesResult.error()));
      auto matches = std::move(*matchesResult);
      if (matches)
        return i;
    }
  }
  for (std::size_t i = 0; i < orientations.size(); ++i) {
    auto matchesResult = valid(i);
    if (!matchesResult)
      return std::unexpected(std::move(matchesResult.error()));
    auto matches = std::move(*matchesResult);
    if (matches)
      return i;
  }
  return NPos;
}

std::expected<Json, StarException> ObjectDatabase::parseTouchDamage(Root& root, String const& path, Json const& config) {
  auto touchDamageResult = config.get("touchDamage", Json());
  if (!touchDamageResult)
    return std::unexpected(std::move(touchDamageResult.error()));
  auto touchDamage = std::move(*touchDamageResult);
  if (touchDamage.isType(Json::Type::String)) {
    auto assetsResult = root.assets();
    if (!assetsResult)
      return std::unexpected(std::move(assetsResult.error()));
    auto assets = std::move(*assetsResult);
    auto damagePathResult = touchDamage.toString();
    if (!damagePathResult)
      return std::unexpected(std::move(damagePathResult.error()));
    auto damagePath = AssetPath::relative_to(path, std::move(*damagePathResult));
    auto damage = assets->json(damagePath);
    if (!damage)
      return std::unexpected(assets->withAssetContext(std::move(damage.error()), damagePath, "Loading object touch damage"));
    return damage;
  }
  return touchDamage;
}

std::expected<List<std::shared_ptr<ObjectOrientation>>, StarException> ObjectDatabase::parseOrientations(Root& root, String const& path, Json const& configList, Json const& baseConfig) {
  auto materialDatabaseResult = root.materialDatabase();
  if (!materialDatabaseResult)
    return std::unexpected(std::move(materialDatabaseResult.error()));
  auto materialDatabase = std::move(*materialDatabaseResult);
  auto imageMetadataResult = root.imageMetadataDatabase();
  if (!imageMetadataResult)
    return std::unexpected(std::move(imageMetadataResult.error()));
  auto imageMetadata = std::move(*imageMetadataResult);
  List<std::shared_ptr<ObjectOrientation>> res;
  auto configsResult = configList.toArray();
  if (!configsResult)
    return std::unexpected(std::move(configsResult.error()));
  auto configs = std::move(*configsResult);
  // Expand legacy dual/left/right image definitions without changing order.
  for (auto it = configs.begin(); it != configs.end();) {
    auto configResult = it->toObject();
    if (!configResult)
      return std::unexpected(std::move(configResult.error()));
    auto config = std::move(*configResult);
    if (config.contains("dualImage")) {
      it = configs.erase(it);
      JsonObject configLeft = config;
      configLeft["image"] = config["dualImage"];
      configLeft["flipImages"] = true;
      configLeft["direction"] = "left";
      it = std::next(configs.insert(it, configLeft));
      JsonObject configRight = config;
      configRight["image"] = config["dualImage"];
      configRight["direction"] = "right";
      it = std::next(configs.insert(it, configRight));
    } else if (config.contains("leftImage")) {
      it = configs.erase(it);
      JsonObject configLeft = config;
      configLeft["image"] = config["leftImage"];
      configLeft["direction"] = "left";
      it = std::next(configs.insert(it, configLeft));
      JsonObject configRight = config;
      configRight["image"] = config["rightImage"];
      configRight["direction"] = "right";
      it = std::next(configs.insert(it, configRight));
    } else {
      ++it;
    }
  }
  for (auto const& orientationSettings : configs) {
    auto orientation = std::make_shared<ObjectOrientation>();
    orientation->config = orientationSettings;
    if (auto containsResult = orientationSettings.contains("imageLayers"); !containsResult)
      return std::unexpected(std::move(containsResult.error()));
    else if (*containsResult) {
      auto layersResult = orientationSettings.getArray("imageLayers");
      if (!layersResult)
        return std::unexpected(std::move(layersResult.error()));
      auto layers = std::move(*layersResult);
      for (Json layer : layers) {
        auto imageResult = layer.opt("image");
        if (!imageResult)
          return std::unexpected(std::move(imageResult.error()));
        auto image = std::move(*imageResult);
        if (image) {
          auto imageNameResult = image->toString();
          if (!imageNameResult)
            return std::unexpected(std::move(imageNameResult.error()));
          auto imageName = std::move(*imageNameResult);
          auto resolvedLayerResult = layer.set("image", AssetPath::relative_to(path, imageName));
          if (!resolvedLayerResult)
            return std::unexpected(std::move(resolvedLayerResult.error()));
          auto resolvedLayer = std::move(*resolvedLayerResult);
          layer = std::move(resolvedLayer);
        }
        auto centeredResult = layer.getBool("centered", false);
        if (!centeredResult)
          return std::unexpected(std::move(centeredResult.error()));
        auto centered = std::move(*centeredResult);
        auto drawableConfigResult = layer.set("centered", centered);
        if (!drawableConfigResult)
          return std::unexpected(std::move(drawableConfigResult.error()));
        auto drawableConfig = std::move(*drawableConfigResult);
        auto drawableResult = Drawable::create(*imageMetadata, drawableConfig);
        if (!drawableResult)
          return std::unexpected(std::move(drawableResult.error()));
        auto drawable = std::move(*drawableResult);
        drawable.scale(1.0f / TilePixels);
        orientation->imageLayers.append(std::move(drawable));
      }
    } else {
      auto imageResult = orientationSettings.getString("image");
      if (!imageResult)
        return std::unexpected(std::move(imageResult.error()));
      auto image = std::move(*imageResult);
      auto drawableResult =
        Drawable::makeImage(*imageMetadata, AssetPath::relative_to(path, image), 1.0 / TilePixels, false, {});
      if (!drawableResult)
        return std::unexpected(std::move(drawableResult.error()));
      auto drawable = std::move(*drawableResult);
      auto fullbrightResult = orientationSettings.getBool("fullbright", false);
      if (!fullbrightResult)
        return std::unexpected(std::move(fullbrightResult.error()));
      auto fullbright = std::move(*fullbrightResult);
      drawable.fullbright = fullbright;
      orientation->imageLayers.append(std::move(drawable));
    }
    auto renderLayerNameResult = orientationSettings.getString("renderLayer", "Object");
    if (!renderLayerNameResult)
      return std::unexpected(std::move(renderLayerNameResult.error()));
    auto renderLayerName = std::move(*renderLayerNameResult);
    auto renderLayerResult = parseRenderLayer(renderLayerName);
    if (!renderLayerResult)
      return std::unexpected(std::move(renderLayerResult.error()));
    auto renderLayer = std::move(*renderLayerResult);
    orientation->renderLayer = std::move(renderLayer);
    auto flipImagesResult = orientationSettings.getBool("flipImages", false);
    if (!flipImagesResult)
      return std::unexpected(std::move(flipImagesResult.error()));
    auto flipImages = std::move(*flipImagesResult);
    orientation->flipImages = std::move(flipImages);
    auto imagePositionConfigResult = orientationSettings.getArray("imagePosition", {0, 0});
    if (!imagePositionConfigResult)
      return std::unexpected(std::move(imagePositionConfigResult.error()));
    auto imagePositionConfig = std::move(*imagePositionConfigResult);
    auto imagePositionResult = jsonToVec2F(imagePositionConfig);
    if (!imagePositionResult)
      return std::unexpected(std::move(imagePositionResult.error()));
    auto imagePosition = std::move(*imagePositionResult);
    orientation->imagePosition = imagePosition / TilePixels;
    auto framesResult = orientationSettings.getInt("frames", 1);
    if (!framesResult)
      return std::unexpected(std::move(framesResult.error()));
    auto frames = std::move(*framesResult);
    orientation->frames = std::move(frames);
    auto animationCycleResult = orientationSettings.getDouble("animationCycle", 1.0);
    if (!animationCycleResult)
      return std::unexpected(std::move(animationCycleResult.error()));
    auto animationCycle = std::move(*animationCycleResult);
    orientation->animationCycle = std::move(animationCycle);
    if (auto containsResult = orientationSettings.contains("spaces"); !containsResult)
      return std::unexpected(std::move(containsResult.error()));
    else if (*containsResult) {
      auto spacesResult = orientationSettings.getArray("spaces");
      if (!spacesResult)
        return std::unexpected(std::move(spacesResult.error()));
      auto spaces = std::move(*spacesResult);
      for (auto const& space : spaces) {
        auto parsedSpaceResult = jsonToVec2I(space);
        if (!parsedSpaceResult)
          return std::unexpected(std::move(parsedSpaceResult.error()));
        auto parsedSpace = std::move(*parsedSpaceResult);
        orientation->spaces.append(parsedSpace);
      }
    } else {
      orientation->spaces = {{0, 0}};
    }
    if (auto containsResult = orientationSettings.contains("spaceScan"); !containsResult)
      return std::unexpected(std::move(containsResult.error()));
    else if (*containsResult) {
      auto spaceScanSpaces = Set<Vec2I>::from(orientation->spaces);
      StringMap<String> imageKeys;
      auto colorConfigResult = orientationSettings.get("color", "default");
      if (!colorConfigResult)
        return std::unexpected(std::move(colorConfigResult.error()));
      auto colorConfig = std::move(*colorConfigResult);
      auto colorResult = colorConfig.toString();
      if (!colorResult)
        return std::unexpected(std::move(colorResult.error()));
      auto color = std::move(*colorResult);
      imageKeys.set("color", color.takeUtf8());
      auto keysConfigResult = jsonMergeQueryDef("imageKeys", JsonObject(), baseConfig, orientation->config);
      if (!keysConfigResult)
        return std::unexpected(std::move(keysConfigResult.error()));
      auto keysConfig = std::move(*keysConfigResult);
      auto keysResult = keysConfig.toObject();
      if (!keysResult)
        return std::unexpected(std::move(keysResult.error()));
      auto keys = std::move(*keysResult);
      for (auto const& pair : keys) {
        auto valueResult = pair.second.toString();
        if (!valueResult)
          return std::unexpected(std::move(valueResult.error()));
        auto value = std::move(*valueResult);
        imageKeys.set(pair.first, value);
      }
      for (auto const& layer : orientation->imageLayers) {
        if (layer.isImage()) {
          auto image = AssetPath::join(layer.imagePart().image).replaceTags(imageKeys, true, "default");
          auto thresholdResult = orientationSettings.getDouble("spaceScan");
          if (!thresholdResult)
            return std::unexpected(std::move(thresholdResult.error()));
          auto threshold = std::move(*thresholdResult);
          auto scannedResult = imageMetadata->imageSpaces(image, imagePosition + (layer.position + layer.imagePart().transformation.transformVec2(Vec2F())) * TilePixels, threshold, orientation->flipImages);
          if (!scannedResult)
            return std::unexpected(std::move(scannedResult.error()));
          auto scanned = std::move(*scannedResult);
          spaceScanSpaces.addAll(scanned);
        }
      }
      orientation->spaces = spaceScanSpaces.values();
    }
    orientation->boundBox = RectI::boundBoxOfPoints(orientation->spaces);
    auto metaBoxResult = orientationSettings.opt("metaBoundBox");
    if (!metaBoxResult)
      return std::unexpected(std::move(metaBoxResult.error()));
    auto metaBox = std::move(*metaBoxResult);
    if (metaBox) {
      auto metaBoundBoxResult = jsonToRectF(*metaBox);
      if (!metaBoundBoxResult)
        return std::unexpected(std::move(metaBoundBoxResult.error()));
      auto metaBoundBox = std::move(*metaBoundBoxResult);
      orientation->metaBoundBox = std::move(metaBoundBox);
    }
    auto tilledResult = orientationSettings.getBool("requireTilledAnchors", false);
    if (!tilledResult)
      return std::unexpected(std::move(tilledResult.error()));
    auto tilled = std::move(*tilledResult);
    auto soilResult = orientationSettings.getBool("requireSoilAnchors", false);
    if (!soilResult)
      return std::unexpected(std::move(soilResult.error()));
    auto soil = std::move(*soilResult);
    std::optional<MaterialId> anchorMaterial;
    auto anchorMaterialNameResult = orientationSettings.optString("anchorMaterial");
    if (!anchorMaterialNameResult)
      return std::unexpected(std::move(anchorMaterialNameResult.error()));
    auto anchorMaterialName = std::move(*anchorMaterialNameResult);
    if (anchorMaterialName) {
      auto materialResult = materialDatabase->materialId(*anchorMaterialName);
      if (!materialResult)
        return std::unexpected(std::move(materialResult.error()));
      auto material = std::move(*materialResult);
      anchorMaterial = material;
    }
    auto anchorsResult = orientationSettings.getArray("anchors", JsonArray());
    if (!anchorsResult)
      return std::unexpected(std::move(anchorsResult.error()));
    auto anchors = std::move(*anchorsResult);
    for (auto const& type : anchors) {
      auto anchorTypeResult = type.toString();
      if (!anchorTypeResult)
        return std::unexpected(std::move(anchorTypeResult.error()));
      auto anchorType = std::move(*anchorTypeResult);
      if (anchorType == "left") {
        for (auto space : orientation->spaces) {
          if (space[0] == orientation->boundBox.xMin())
            orientation->anchors.append({TileLayer::Foreground, space + Vec2I(-1, 0), tilled, soil, anchorMaterial});
        }
      } else if (anchorType == "bottom") {
        for (auto space : orientation->spaces) {
          if (space[1] == orientation->boundBox.yMin())
            orientation->anchors.append({TileLayer::Foreground, space + Vec2I(0, -1), tilled, soil, anchorMaterial});
        }
      } else if (anchorType == "right") {
        for (auto space : orientation->spaces) {
          if (space[0] == orientation->boundBox.xMax())
            orientation->anchors.append({TileLayer::Foreground, space + Vec2I(1, 0), tilled, soil, anchorMaterial});
        }
      } else if (anchorType == "top") {
        for (auto space : orientation->spaces) {
          if (space[1] == orientation->boundBox.yMax())
            orientation->anchors.append({TileLayer::Foreground, space + Vec2I(0, 1), tilled, soil, anchorMaterial});
        }
      } else if (anchorType == "background") {
        for (auto space : orientation->spaces)
          orientation->anchors.append({TileLayer::Background, space, tilled, soil, anchorMaterial});
      } else {
        return std::unexpected(ObjectException::format("Unknown anchor type: {}", anchorType));
      }
    }
    auto bgAnchorsResult = orientationSettings.getArray("bgAnchors", JsonArray());
    if (!bgAnchorsResult)
      return std::unexpected(std::move(bgAnchorsResult.error()));
    auto bgAnchors = std::move(*bgAnchorsResult);
    for (auto const& anchor : bgAnchors) {
      auto positionResult = jsonToVec2I(anchor);
      if (!positionResult)
        return std::unexpected(std::move(positionResult.error()));
      auto position = std::move(*positionResult);
      orientation->anchors.append({TileLayer::Background, position, tilled, soil, anchorMaterial});
    }
    auto fgAnchorsResult = orientationSettings.getArray("fgAnchors", JsonArray());
    if (!fgAnchorsResult)
      return std::unexpected(std::move(fgAnchorsResult.error()));
    auto fgAnchors = std::move(*fgAnchorsResult);
    for (auto const& anchor : fgAnchors) {
      auto positionResult = jsonToVec2I(anchor);
      if (!positionResult)
        return std::unexpected(std::move(positionResult.error()));
      auto position = std::move(*positionResult);
      orientation->anchors.append({TileLayer::Foreground, position, tilled, soil, anchorMaterial});
    }
    auto anchorAnyResult = orientationSettings.getBool("anchorAny", false);
    if (!anchorAnyResult)
      return std::unexpected(std::move(anchorAnyResult.error()));
    auto anchorAny = std::move(*anchorAnyResult);
    orientation->anchorAny = std::move(anchorAny);
    if (auto containsResult = orientationSettings.contains("direction"); !containsResult)
      return std::unexpected(std::move(containsResult.error()));
    else if (*containsResult) {
      auto directionResult = orientationSettings.getString("direction", "left");
      if (!directionResult)
        return std::unexpected(std::move(directionResult.error()));
      auto direction = std::move(*directionResult);
      auto affinityResult = enumValue<DirectionNames>(direction);
      if (!affinityResult)
        return std::unexpected(std::move(affinityResult.error()));
      orientation->directionAffinity = *affinityResult;
    }
    auto collisionTypeResult = orientationSettings.getString("collision", "none");
    if (!collisionTypeResult)
      return std::unexpected(std::move(collisionTypeResult.error()));
    auto collisionType = std::move(*collisionTypeResult);
    if (auto containsResult = orientationSettings.contains("materialSpaces"); !containsResult)
      return std::unexpected(std::move(containsResult.error()));
    else if (*containsResult) {
      auto materialSpacesResult = orientationSettings.getArray("materialSpaces");
      if (!materialSpacesResult)
        return std::unexpected(std::move(materialSpacesResult.error()));
      auto materialSpaces = std::move(*materialSpacesResult);
      for (auto const& space : materialSpaces) {
        auto materialNameResult = space.getString(1);
        if (!materialNameResult)
          return std::unexpected(std::move(materialNameResult.error()));
        auto materialName = std::move(*materialNameResult);
        auto spaceConfigResult = space.get(0);
        if (!spaceConfigResult)
          return std::unexpected(std::move(spaceConfigResult.error()));
        auto spaceConfig = std::move(*spaceConfigResult);
        auto spacePositionResult = jsonToVec2I(spaceConfig);
        if (!spacePositionResult)
          return std::unexpected(std::move(spacePositionResult.error()));
        auto spacePosition = std::move(*spacePositionResult);
        auto materialResult = materialDatabase->materialId(materialName);
        if (!materialResult)
          return std::unexpected(std::move(materialResult.error()));
        auto material = std::move(*materialResult);
        orientation->materialSpaces.append({spacePosition, material});
      }
    } else if (collisionType == "solid" || collisionType == "platform") {
      auto material = collisionType == "solid" ? ObjectSolidMaterialId : ObjectPlatformMaterialId;
      if (auto containsResult = orientationSettings.contains("collisionSpaces"); !containsResult)
        return std::unexpected(std::move(containsResult.error()));
      else if (*containsResult) {
        auto collisionSpacesResult = orientationSettings.getArray("collisionSpaces");
        if (!collisionSpacesResult)
          return std::unexpected(std::move(collisionSpacesResult.error()));
        auto collisionSpaces = std::move(*collisionSpacesResult);
        for (auto const& space : collisionSpaces) {
          auto spacePositionResult = jsonToVec2I(space);
          if (!spacePositionResult)
            return std::unexpected(std::move(spacePositionResult.error()));
          auto spacePosition = std::move(*spacePositionResult);
          orientation->materialSpaces.append({spacePosition, material});
        }
      } else {
        for (auto const& space : orientation->spaces) {
          if (collisionType == "solid" || space[1] == orientation->boundBox.yMax())
            orientation->materialSpaces.append({space, material});
        }
      }
    }
    if (auto containsResult = orientationSettings.contains("interactiveSpaces"); !containsResult)
      return std::unexpected(std::move(containsResult.error()));
    else if (*containsResult) {
      auto interactiveSpacesResult = orientationSettings.getArray("interactiveSpaces");
      if (!interactiveSpacesResult)
        return std::unexpected(std::move(interactiveSpacesResult.error()));
      auto interactiveSpaces = std::move(*interactiveSpacesResult);
      List<Vec2I> parsedSpaces;
      for (auto const& space : interactiveSpaces) {
        auto positionResult = jsonToVec2I(space);
        if (!positionResult)
          return std::unexpected(std::move(positionResult.error()));
        auto position = std::move(*positionResult);
        parsedSpaces.append(position);
      }
      orientation->interactiveSpaces = std::move(parsedSpaces);
    }
    auto lightPositionConfigResult = orientationSettings.getArray("lightPosition", {0, 0});
    if (!lightPositionConfigResult)
      return std::unexpected(std::move(lightPositionConfigResult.error()));
    auto lightPositionConfig = std::move(*lightPositionConfigResult);
    auto lightPositionResult = jsonToVec2F(lightPositionConfig);
    if (!lightPositionResult)
      return std::unexpected(std::move(lightPositionResult.error()));
    auto lightPosition = std::move(*lightPositionResult);
    orientation->lightPosition = std::move(lightPosition);
    auto beamAngleResult = orientationSettings.getFloat("beamAngle", 0.0f);
    if (!beamAngleResult)
      return std::unexpected(std::move(beamAngleResult.error()));
    auto beamAngle = std::move(*beamAngleResult);
    orientation->beamAngle = beamAngle * Constants::deg2rad;
    if (auto containsResult = orientationSettings.contains("particleEmitter"); !containsResult)
      return std::unexpected(std::move(containsResult.error()));
    else if (*containsResult) {
      auto emitterConfigResult = orientationSettings.get("particleEmitter");
      if (!emitterConfigResult)
        return std::unexpected(std::move(emitterConfigResult.error()));
      auto emitterConfig = std::move(*emitterConfigResult);
      auto emitterResult = ObjectOrientation::parseParticleEmitter(root, path, emitterConfig);
      if (!emitterResult)
        return std::unexpected(std::move(emitterResult.error()));
      auto emitter = std::move(*emitterResult);
      orientation->particleEmitters.append(std::move(emitter));
    }
    auto emittersResult = orientationSettings.getArray("particleEmitters", JsonArray());
    if (!emittersResult)
      return std::unexpected(std::move(emittersResult.error()));
    auto emitters = std::move(*emittersResult);
    for (auto const& config : emitters) {
      auto emitterResult = ObjectOrientation::parseParticleEmitter(root, path, config);
      if (!emitterResult)
        return std::unexpected(std::move(emitterResult.error()));
      auto emitter = std::move(*emitterResult);
      orientation->particleEmitters.append(std::move(emitter));
    }
    auto areaResult = orientationSettings.opt("statusEffectArea");
    if (!areaResult)
      return std::unexpected(std::move(areaResult.error()));
    auto area = std::move(*areaResult);
    if (area) {
      auto statusEffectAreaResult = jsonToPolyF(*area);
      if (!statusEffectAreaResult)
        return std::unexpected(std::move(statusEffectAreaResult.error()));
      auto statusEffectArea = std::move(*statusEffectAreaResult);
      orientation->statusEffectArea = std::move(statusEffectArea);
    }
    auto touchDamageConfigResult = parseTouchDamage(root, path, orientationSettings);
    if (!touchDamageConfigResult)
      return std::unexpected(std::move(touchDamageConfigResult.error()));
    auto touchDamageConfig = std::move(*touchDamageConfigResult);
    orientation->touchDamageConfig = std::move(touchDamageConfig);
    res.append(std::move(orientation));
  }
  return res;
}

ObjectDatabase::ObjectDatabase(Root& root) : m_root(root), m_configCache(root.presentationRandom()) {}

std::expected<std::shared_ptr<ObjectDatabase>, StarException> ObjectDatabase::create(Root& root) {
  auto database = std::shared_ptr<ObjectDatabase>(new ObjectDatabase(root));
  auto rebuilder = Rebuilder::create(root, "object");
  if (!rebuilder)
    return std::unexpected(std::move(rebuilder.error()));
  database->m_rebuilder = std::move(*rebuilder);
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto files = (*assets)->scanExtension("object");
  if (!files)
    return std::unexpected(std::move(files.error()));
  auto queued = (*assets)->queueJsons(files->get());
  if (!queued)
    return std::unexpected(std::move(queued.error()));
  for (auto const& entry : files->get()) {
    auto const& file = entry.string();
    auto name = [&]() -> std::expected<String, StarException> {
      auto config = (*assets)->json(file);
      if (!config)
        return std::unexpected(std::move(config.error()));
      return config->getString("objectName");
    }();
    if (!name) {
      auto logged = root.logger().errorException(strf("Error loading object file {}", file), ExceptionDiagnostic((*assets)->withAssetContext(std::move(name.error()), file, "Loading object")));
      if (!logged)
        return std::unexpected(std::move(logged.error()));
    } else if (database->m_paths.contains(*name)) {
      auto error = (*assets)->withAssetContext(ObjectException::format("Object {} defined twice, second time from {}", *name, file), file, "Loading object");
      auto logged = root.logger().errorException("Error loading object", ExceptionDiagnostic(error));
      if (!logged)
        return std::unexpected(std::move(logged.error()));
    } else {
      database->m_paths[*name] = file;
    }
  }
  return database;
}

void ObjectDatabase::cleanup() {
  std::lock_guard locker(m_cacheMutex);
  m_configCache.cleanup([](String const&, std::shared_ptr<ObjectConfig> const& config) {
      return config.use_count() != 1;
    });
}

StringList ObjectDatabase::allObjects() const {
  return m_paths.keys();
}

bool ObjectDatabase::isObject(String const& objectName) const {
  return m_paths.contains(objectName);
}

std::expected<std::shared_ptr<ObjectConfig>, StarException> ObjectDatabase::getConfig(String const& objectName) const {
  std::lock_guard locker(m_cacheMutex);
  auto result = m_configCache.get(
    objectName, [&database = *this](String const& name) -> std::expected<std::shared_ptr<ObjectConfig>, StarException> {
      if (auto path = database.m_paths.find_ref(name))
        return database.readConfig(path->get());
      return std::unexpected(ObjectException::format("No such object named '{}'", name));
    });
  if (!result)
    return std::unexpected(std::move(result.error()));
  return result->get();
}

std::expected<std::shared_ptr<List<std::shared_ptr<ObjectOrientation>> const>, StarException> ObjectDatabase::getOrientations(String const& objectName) const {
  auto config = getConfig(objectName);
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto const& orientations = (*config)->orientations;
  return std::shared_ptr<List<std::shared_ptr<ObjectOrientation>> const>(std::move(*config), std::addressof(orientations));
}

std::expected<std::shared_ptr<Object>, StarException> ObjectDatabase::createObject(String const& objectName, Json const& parameters) const {
  auto config = getConfig(objectName);
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto created = [&]() -> std::expected<std::shared_ptr<Object>, StarException> {
    auto const& type = (*config)->type;
    if (type == "object")
      return Object::create(m_root, *config, parameters);
    if (type == "loungeable")
      return LoungeableObject::create(m_root, *config, parameters);
    if (type == "container")
      return ContainerObject::create(m_root, *config, parameters);
    if (type == "farmable")
      return FarmableObject::create(m_root, *config, parameters);
    if (type == "teleporter")
      return TeleporterObject::create(m_root, *config, parameters);
    if (type == "physics")
      return PhysicsObject::create(m_root, *config, parameters);
    return std::unexpected(ObjectException::format("Unknown objectType '{}' constructing object '{}'", type, objectName));
  }();
  if (!created) {
    auto assets = m_root.assets();
    if (!assets)
      return std::unexpected(std::move(assets.error()));
    return std::unexpected((*assets)->withAssetContext(std::move(created.error()), (*config)->path, "Constructing object"));
  }
  return created;
}

std::expected<std::shared_ptr<Object>, StarException> ObjectDatabase::diskLoadObject(Json const& diskStore) const {
  auto originalName = diskStore.getString("name");
  if (!originalName)
    return std::unexpected(std::move(originalName.error()));
  auto originalParams = diskStore.get("parameters");
  if (!originalParams)
    return std::unexpected(std::move(originalParams.error()));
  Json newStore = diskStore;
  std::shared_ptr<Object> object;
  auto attempt = [&, &database = *this](Json const& store) -> std::expected<void, StarException> {
    auto name = store.getString("name");
    if (!name)
      return std::unexpected(std::move(name.error()));
    auto parameters = store.get("parameters");
    if (!parameters)
      return std::unexpected(std::move(parameters.error()));
    auto candidate = database.createObject(*name, *parameters);
    if (!candidate)
      return std::unexpected(std::move(candidate.error()));
    auto loaded = (*candidate)->readStoredData(store);
    if (!loaded)
      return std::unexpected(std::move(loaded.error()));
    auto state = (*candidate)->setNetStates();
    if (!state)
      return std::unexpected(std::move(state.error()));
    object = std::move(*candidate);
    return {};
  };
  auto loaded = [&]() -> std::expected<void, StarException> {
    if (*originalName == "perfectlygenericitem") {
      auto contains = originalParams->contains("genericItemStorage");
      if (!contains)
        return std::unexpected(std::move(contains.error()));
      if (*contains) {
        auto stored = diskStore.get("genericItemStorage");
        if (!stored)
          return std::unexpected(std::move(stored.error()));
        newStore = std::move(*stored);
      }
    }
    return attempt(newStore);
  }();
  if (loaded)
    return object;
  auto originalError = std::move(loaded.error());
  auto rebuilt = m_rebuilder->rebuild(newStore, originalError, attempt);
  if (!rebuilt)
    return std::unexpected(std::move(rebuilt.error()));
  if (*rebuilt)
    return object;
  if (m_root.logger().loggable(LogLevel::Debug)) {
    auto storedText = diskStore.repr();
    if (!storedText)
      return std::unexpected(std::move(storedText.error()));
    if (auto details = m_root.logger().debug("Object disk store: {}", *storedText); !details)
      return std::unexpected(std::move(details.error()));
  }
  if (*originalName == "perfectlygenericitem") {
    auto logged = m_root.logger().errorException(strf("Could not re-instantiate object '{}'", *originalName), ExceptionDiagnostic(originalError));
    if (!logged)
      return std::unexpected(std::move(logged.error()));
    return createObject(*originalName, *originalParams);
  }
  auto logged = m_root.logger().errorException(strf("Could not instantiate object '{}'", *originalName), ExceptionDiagnostic(originalError));
  if (!logged)
    return std::unexpected(std::move(logged.error()));
  return createObject("perfectlygenericitem", JsonObject{
      {"genericItemStorage", diskStore},
      {"shortdescription", *originalName},
      {"description", "Reinstall the parent mod to return this item to normal"}
    });
}

std::expected<std::shared_ptr<Object>, StarException> ObjectDatabase::netLoadObject(ByteArray const& netStore, NetCompatibilityRules rules) const {
  DataStreamBuffer ds(netStore);
  ds.setStreamCompatibilityVersion(rules);
  auto name = ds.read<String>();
  if (!name)
    return std::unexpected(std::move(name.error()));
  auto parameters = ds.read<Json>();
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  return createObject(*name, *parameters);
}

std::expected<bool, StarException> ObjectDatabase::canPlaceObject(World const& world, Vec2I const& position, String const& objectName) const {
  auto config = getConfig(objectName);
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto orientation = (*config)->findValidOrientation(world, position, {});
  if (!orientation)
    return std::unexpected(std::move(orientation.error()));
  return *orientation != NPos;
}

std::expected<std::shared_ptr<Object>, StarException> ObjectDatabase::createForPlacement(World const& world, String const& objectName, Vec2I const& position,
    Direction direction, Json const& parameters) const {
  auto placeable = canPlaceObject(world, position, objectName);
  if (!placeable)
    return std::unexpected(std::move(placeable.error()));
  if (!*placeable)
    return std::shared_ptr<Object>();
  auto object = createObject(objectName, parameters);
  if (!object)
    return std::unexpected(std::move(object.error()));
  auto positioned = (*object)->setTilePosition(world.geometry().xwrap(position));
  if (!positioned)
    return std::unexpected(std::move(positioned.error()));
  (*object)->setDirection(direction);
  return std::move(*object);
}

namespace {

std::expected<void, StarException> readObjectDamageTeam(Json const& config, EntityDamageTeam& team) {
  auto present = config.contains("damageTeam");
  if (!present)
    return std::unexpected(std::move(present.error()));
  if (!*present)
    return {};
  auto damageTeam = config.get("damageTeam");
  if (!damageTeam)
    return std::unexpected(std::move(damageTeam.error()));
  auto type = damageTeam->getString("type", "environment")
                .and_then([](String const& name) -> std::expected<TeamType, StarException> {
                  return enumValue<TeamTypeNames>(name);
                });
  if (!type)
    return std::unexpected(std::move(type.error()));
  team.type = *type;
  auto number = damageTeam->getUInt("team", 0);
  if (!number)
    return std::unexpected(std::move(number.error()));
  team.team = *number;
  return {};
}

std::expected<void, StarException> readObjectLighting(Root& root, Json const& config, ObjectConfig& object) {
  auto singleColor = config.contains("lightColor");
  if (!singleColor)
    return std::unexpected(std::move(singleColor.error()));
  if (*singleColor) {
    auto color = config.get("lightColor").and_then(jsonToColor);
    if (!color)
      return std::unexpected(std::move(color.error()));
    object.lightColors["default"] = *color;
  } else {
    auto multipleColors = config.contains("lightColors");
    if (!multipleColors)
      return std::unexpected(std::move(multipleColors.error()));
    if (*multipleColors) {
      auto colors = config.getObject("lightColors");
      if (!colors)
        return std::unexpected(std::move(colors.error()));
      for (auto const& [name, value] : *colors) {
        auto color = jsonToColor(value);
        if (!color)
          return std::unexpected(std::move(color.error()));
        object.lightColors[name] = *color;
      }
    }
  }
  auto type = config.optString("lightType");
  if (!type)
    return std::unexpected(std::move(type.error()));
  if (*type) {
    auto namedType = enumValue<LightTypeNames>(**type);
    if (!namedType)
      return std::unexpected(std::move(namedType.error()));
    object.lightType = *namedType;
  } else {
    auto pointLight = config.getBool("pointLight", false);
    if (!pointLight)
      return std::unexpected(std::move(pointLight.error()));
    object.lightType = static_cast<LightType>(*pointLight);
  }
  auto pointBeam = config.getFloat("pointBeam", 0.0f);
  if (!pointBeam)
    return std::unexpected(std::move(pointBeam.error()));
  object.pointBeam = *pointBeam;
  auto ambience = config.getFloat("beamAmbience", 0.0f);
  if (!ambience)
    return std::unexpected(std::move(ambience.error()));
  object.beamAmbience = *ambience;
  auto flickering = config.contains("flickerPeriod");
  if (!flickering)
    return std::unexpected(std::move(flickering.error()));
  if (*flickering) {
    auto period = config.getFloat("flickerPeriod");
    if (!period)
      return std::unexpected(std::move(period.error()));
    auto minimum = config.getFloat("flickerMinIntensity", 0.0);
    if (!minimum)
      return std::unexpected(std::move(minimum.error()));
    auto maximum = config.getFloat("flickerMaxIntensity", 0.0);
    if (!maximum)
      return std::unexpected(std::move(maximum.error()));
    auto periodVariance = config.getFloat("flickerPeriodVariance", 0.0);
    if (!periodVariance)
      return std::unexpected(std::move(periodVariance.error()));
    auto intensityVariance = config.getFloat("flickerIntensityVariance", 0.0);
    if (!intensityVariance)
      return std::unexpected(std::move(intensityVariance.error()));
    object.lightFlickering =
      PeriodicFunction<float>(root.presentationRandom(), *period, *minimum, *maximum, *periodVariance, *intensityVariance);
  }
  return {};
}

}// namespace

std::expected<std::shared_ptr<ObjectConfig>, StarException> ObjectDatabase::readConfig(String const& path) const {
  auto assetsResult = m_root.assets();
  if (!assetsResult)
    return std::unexpected(ObjectException(strf("Error loading object '{}'", path), std::move(assetsResult.error())));
  auto assets = std::move(*assetsResult);
  auto result = [&, &root = m_root]() -> std::expected<std::shared_ptr<ObjectConfig>, StarException> {
    auto configResult = assets->json(path);
    if (!configResult)
      return std::unexpected(std::move(configResult.error()));
    auto config = std::move(*configResult);
    auto objectConfig = std::make_shared<ObjectConfig>();
    objectConfig->path = path;
    objectConfig->config = config;
    auto nameResult = config.getString("objectName");
    if (!nameResult)
      return std::unexpected(std::move(nameResult.error()));
    auto name = std::move(*nameResult);
    objectConfig->name = std::move(name);
    auto typeResult = config.getString("objectType", "object");
    if (!typeResult)
      return std::unexpected(std::move(typeResult.error()));
    auto type = std::move(*typeResult);
    objectConfig->type = std::move(type);
    auto raceResult = config.getString("race", "generic");
    if (!raceResult)
      return std::unexpected(std::move(raceResult.error()));
    auto race = std::move(*raceResult);
    objectConfig->race = std::move(race);
    auto categoryResult = config.getString("category", "other");
    if (!categoryResult)
      return std::unexpected(std::move(categoryResult.error()));
    auto category = std::move(*categoryResult);
    objectConfig->category = std::move(category);
    auto colonyTags = config.get("colonyTags", JsonArray()).and_then(jsonToStringList);
    if (!colonyTags)
      return std::unexpected(std::move(colonyTags.error()));
    objectConfig->colonyTags = std::move(*colonyTags);
    auto scripts = config.get("scripts", JsonArray()).and_then(jsonToStringList);
    if (!scripts)
      return std::unexpected(std::move(scripts.error()));
    std::ranges::for_each(*scripts, [&](String& script) { script = AssetPath::relative_to(path, script); });
    objectConfig->scripts = std::move(*scripts);
    auto animationScripts = config.get("animationScripts", JsonArray()).and_then(jsonToStringList);
    if (!animationScripts)
      return std::unexpected(std::move(animationScripts.error()));
    std::ranges::for_each(*animationScripts, [&](String& script) { script = AssetPath::relative_to(path, script); });
    objectConfig->animationScripts = std::move(*animationScripts);
    auto priceResult = config.getInt("price", 0);
    if (!priceResult)
      return std::unexpected(std::move(priceResult.error()));
    auto price = std::move(*priceResult);
    objectConfig->price = std::move(price);
    if (objectConfig->price == 0)
      objectConfig->price = 1;
    auto hasObjectItemResult = config.getBool("hasObjectItem", true);
    if (!hasObjectItemResult)
      return std::unexpected(std::move(hasObjectItemResult.error()));
    auto hasObjectItem = std::move(*hasObjectItemResult);
    objectConfig->hasObjectItem = std::move(hasObjectItem);
    auto scannableResult = config.getBool("scannable", true);
    if (!scannableResult)
      return std::unexpected(std::move(scannableResult.error()));
    auto scannable = std::move(*scannableResult);
    objectConfig->scannable = std::move(scannable);
    if (objectConfig->hasObjectItem) {
    auto printableResult = config.getBool("printable", objectConfig->scannable);
    if (!printableResult)
      return std::unexpected(std::move(printableResult.error()));
    auto printable = std::move(*printableResult);
    objectConfig->printable = std::move(printable);
    } else {
      objectConfig->printable = false;
    }
    auto retainObjectParametersInItemResult = config.getBool("retainObjectParametersInItem", false);
    if (!retainObjectParametersInItemResult)
      return std::unexpected(std::move(retainObjectParametersInItemResult.error()));
    auto retainObjectParametersInItem = std::move(*retainObjectParametersInItemResult);
    objectConfig->retainObjectParametersInItem = std::move(retainObjectParametersInItem);
    if (auto containsResult = config.contains("breakDropPool"); !containsResult)
      return std::unexpected(std::move(containsResult.error()));
    else if (*containsResult) {
    auto breakDropPoolResult = config.getString("breakDropPool");
    if (!breakDropPoolResult)
      return std::unexpected(std::move(breakDropPoolResult.error()));
    auto breakDropPool = std::move(*breakDropPoolResult);
    objectConfig->breakDropPool = std::move(breakDropPool);
    }
    if (auto containsResult = config.contains("breakDropOptions"); !containsResult)
      return std::unexpected(std::move(containsResult.error()));
    else if (*containsResult) {
    auto field6Result = config.getArray("breakDropOptions");
    if (!field6Result)
      return std::unexpected(std::move(field6Result.error()));
    auto field6 = std::move(*field6Result);
      for (auto const& choices : field6) {
    auto choicesArrayResult = choices.toArray();
    if (!choicesArrayResult)
      return std::unexpected(std::move(choicesArrayResult.error()));
    auto choicesArray = std::move(*choicesArrayResult);
        List<ItemDescriptor> group;
        for (auto const& choice : choicesArray) {
    auto field7Result = choice.getString(0);
    if (!field7Result)
      return std::unexpected(std::move(field7Result.error()));
    auto field7 = std::move(*field7Result);
    auto field8Result = choice.getUInt(1);
    if (!field8Result)
      return std::unexpected(std::move(field8Result.error()));
    auto field8 = std::move(*field8Result);
    auto field9Result = choice.getObject(2);
    if (!field9Result)
      return std::unexpected(std::move(field9Result.error()));
    auto field9 = std::move(*field9Result);
    auto descriptor = ItemDescriptor::create(field7, static_cast<std::size_t>(field8), field9);
    if (!descriptor)
      return std::unexpected(std::move(descriptor.error()));
    group.append(std::move(*descriptor));
        }
        objectConfig->breakDropOptions.append(std::move(group));
      }
      // Explicit empty break options mean always dropping nothing.
      if (objectConfig->breakDropOptions.empty())
        objectConfig->breakDropOptions.append({});
    }
    if (auto containsResult = config.contains("smashDropPool"); !containsResult)
      return std::unexpected(std::move(containsResult.error()));
    else if (*containsResult) {
    auto smashDropPoolResult = config.getString("smashDropPool");
    if (!smashDropPoolResult)
      return std::unexpected(std::move(smashDropPoolResult.error()));
    auto smashDropPool = std::move(*smashDropPoolResult);
    objectConfig->smashDropPool = std::move(smashDropPool);
    }
    if (auto containsResult = config.contains("smashDropOptions"); !containsResult)
      return std::unexpected(std::move(containsResult.error()));
    else if (*containsResult) {
    auto field10Result = config.getArray("smashDropOptions");
    if (!field10Result)
      return std::unexpected(std::move(field10Result.error()));
    auto field10 = std::move(*field10Result);
      for (auto const& choices : field10) {
    auto choicesArrayResult = choices.toArray();
    if (!choicesArrayResult)
      return std::unexpected(std::move(choicesArrayResult.error()));
    auto choicesArray = std::move(*choicesArrayResult);
        List<ItemDescriptor> group;
        for (auto const& choice : choicesArray) {
    auto field11Result = ItemDescriptor::create(choice);
    if (!field11Result)
      return std::unexpected(std::move(field11Result.error()));
    auto field11 = std::move(*field11Result);
          group.append(std::move(field11));
        }
        objectConfig->smashDropOptions.append(std::move(group));
      }
    }
    auto field12Result = config.getArray("smashSounds", JsonArray());
    if (!field12Result)
      return std::unexpected(std::move(field12Result.error()));
    auto field12 = std::move(*field12Result);
    for (auto const& sound : field12) {
    auto field13Result = sound.toString();
    if (!field13Result)
      return std::unexpected(std::move(field13Result.error()));
    auto field13 = std::move(*field13Result);
    objectConfig->smashSoundOptions.append(AssetPath::relative_to(path, field13));
    }
    if (auto containsResult = config.contains("smashParticles"); !containsResult)
      return std::unexpected(std::move(containsResult.error()));
    else if (*containsResult) {
    auto smashParticlesResult = config.getArray("smashParticles");
    if (!smashParticlesResult)
      return std::unexpected(std::move(smashParticlesResult.error()));
    auto smashParticles = std::move(*smashParticlesResult);
    objectConfig->smashParticles = std::move(smashParticles);
    }
    auto smashableResult = config.getBool("smashable", false);
    if (!smashableResult)
      return std::unexpected(std::move(smashableResult.error()));
    auto smashable = std::move(*smashableResult);
    objectConfig->smashable = std::move(smashable);
    auto smashOnBreakResult = config.getBool("smashOnBreak", objectConfig->smashable);
    if (!smashOnBreakResult)
      return std::unexpected(std::move(smashOnBreakResult.error()));
    auto smashOnBreak = std::move(*smashOnBreakResult);
    objectConfig->smashOnBreak = std::move(smashOnBreak);
    auto unbreakableResult = config.getBool("unbreakable", false);
    if (!unbreakableResult)
      return std::unexpected(std::move(unbreakableResult.error()));
    auto unbreakable = std::move(*unbreakableResult);
    objectConfig->unbreakable = std::move(unbreakable);
    if (objectConfig->unbreakable)
      objectConfig->smashable = false;
    auto damageTableSource = config.get("damageTable", "/objects/defaultParameters.config:damageTable");
    if (!damageTableSource)
      return std::unexpected(std::move(damageTableSource.error()));
    std::optional<String> damageTablePath;
    if (damageTableSource->isType(Json::Type::String)) {
      auto sourcePath = damageTableSource->toString();
      if (!sourcePath)
        return std::unexpected(std::move(sourcePath.error()));
      damageTablePath = std::move(*sourcePath);
    }
    auto damageTable = assets->fetchJson(*damageTableSource);
    if (!damageTable)
      return std::unexpected(assets->withAssetContext(std::move(damageTable.error()), damageTablePath ? *damageTablePath : path, "Loading object damage table"));
    auto damageHealth = config.optFloat("health");
    if (!damageHealth)
      return std::unexpected(std::move(damageHealth.error()));
    auto harvestLevel = config.optUInt("harvestLevel");
    if (!harvestLevel)
      return std::unexpected(std::move(harvestLevel.error()));
    auto tileDamage = TileDamageParameters::create(*assets, *damageTable, *damageHealth, *harvestLevel);
    if (!tileDamage)
      return std::unexpected(assets->withAssetContext(std::move(tileDamage.error()), damageTablePath ? *damageTablePath : path, "Loading object damage table"));
    objectConfig->tileDamageParameters = std::move(*tileDamage);
    auto damageShakeMagnitudeResult = config.getFloat("damageShakeMagnitude", 0.2f);
    if (!damageShakeMagnitudeResult)
      return std::unexpected(std::move(damageShakeMagnitudeResult.error()));
    auto damageShakeMagnitude = std::move(*damageShakeMagnitudeResult);
    objectConfig->damageShakeMagnitude = std::move(damageShakeMagnitude);
    auto damageMaterialKindResult = config.getString("damageMaterialKind", "solid");
    if (!damageMaterialKindResult)
      return std::unexpected(std::move(damageMaterialKindResult.error()));
    auto damageMaterialKind = std::move(*damageMaterialKindResult);
    objectConfig->damageMaterialKind = std::move(damageMaterialKind);
    if (auto damageTeam = readObjectDamageTeam(config, objectConfig->damageTeam); !damageTeam)
      return std::unexpected(std::move(damageTeam.error()));
    if (auto lighting = readObjectLighting(root, config, *objectConfig); !lighting)
      return std::unexpected(std::move(lighting.error()));
    auto soundEffectResult = config.getString("soundEffect", "");
    if (!soundEffectResult)
      return std::unexpected(std::move(soundEffectResult.error()));
    auto soundEffect = std::move(*soundEffectResult);
    objectConfig->soundEffect = std::move(soundEffect);
    auto soundEffectRangeMultiplierResult = config.getFloat("soundEffectRangeMultiplier", 1.0f);
    if (!soundEffectRangeMultiplierResult)
      return std::unexpected(std::move(soundEffectRangeMultiplierResult.error()));
    auto soundEffectRangeMultiplier = std::move(*soundEffectRangeMultiplierResult);
    objectConfig->soundEffectRangeMultiplier = std::move(soundEffectRangeMultiplier);
    auto field34Result = config.getArray("statusEffects", JsonArray());
    if (!field34Result)
      return std::unexpected(std::move(field34Result.error()));
    auto field34 = std::move(*field34Result);
    for (auto const& effect : field34) {
    auto field35Result = jsonToPersistentStatusEffect(effect);
    if (!field35Result)
      return std::unexpected(std::move(field35Result.error()));
    auto field35 = std::move(*field35Result);
      objectConfig->statusEffects.append(std::move(field35));
    }
    auto touchDamageConfigResult = parseTouchDamage(root, path, config);
    if (!touchDamageConfigResult)
      return std::unexpected(std::move(touchDamageConfigResult.error()));
    auto touchDamageConfig = std::move(*touchDamageConfigResult);
    objectConfig->touchDamageConfig = std::move(touchDamageConfig);
    auto minimumLiquidLevelResult = config.optFloat("minimumLiquidLevel");
    if (!minimumLiquidLevelResult)
      return std::unexpected(std::move(minimumLiquidLevelResult.error()));
    auto minimumLiquidLevel = std::move(*minimumLiquidLevelResult);
    objectConfig->minimumLiquidLevel = std::move(minimumLiquidLevel);
    auto maximumLiquidLevelResult = config.optFloat("maximumLiquidLevel");
    if (!maximumLiquidLevelResult)
      return std::unexpected(std::move(maximumLiquidLevelResult.error()));
    auto maximumLiquidLevel = std::move(*maximumLiquidLevelResult);
    objectConfig->maximumLiquidLevel = std::move(maximumLiquidLevel);
    auto liquidCheckIntervalResult = config.getFloat("liquidCheckInterval", 0.5);
    if (!liquidCheckIntervalResult)
      return std::unexpected(std::move(liquidCheckIntervalResult.error()));
    auto liquidCheckInterval = std::move(*liquidCheckIntervalResult);
    objectConfig->liquidCheckInterval = std::move(liquidCheckInterval);
    auto healthResult = config.getFloat("health", 1);
    if (!healthResult)
      return std::unexpected(std::move(healthResult.error()));
    auto health = std::move(*healthResult);
    objectConfig->health = std::move(health);
    auto readAnimation = [&](Json const& value) -> std::expected<Json, StarException> {
      if (!value.isType(Json::Type::String))
        return value;
      auto reference = value.toString();
      if (!reference)
        return std::unexpected(std::move(reference.error()));
      auto animationPath = AssetPath::relative_to(path, *reference);
      auto animation = assets->json(animationPath);
      if (!animation)
        return std::unexpected(assets->withAssetContext(std::move(animation.error()), animationPath, "Loading object animation"));
      return animation;
    };
    auto field36Result = config.get("animation", Json());
    if (!field36Result)
      return std::unexpected(std::move(field36Result.error()));
    auto field36 = std::move(*field36Result);
    if (field36) {
    auto animationConfigResult = readAnimation(field36);
    if (!animationConfigResult)
      return std::unexpected(std::move(animationConfigResult.error()));
    auto animationConfig = std::move(*animationConfigResult);
    objectConfig->animationConfig = std::move(animationConfig);
    auto field37Result = config.get("animationCustom", Json());
    if (!field37Result)
      return std::unexpected(std::move(field37Result.error()));
    auto field37 = std::move(*field37Result);
      if (field37) {
    auto field38Result = readAnimation(field37);
    if (!field38Result)
      return std::unexpected(std::move(field38Result.error()));
    auto field38 = std::move(*field38Result);
    auto field39 = jsonMerge(objectConfig->animationConfig, field38);
        objectConfig->animationConfig = std::move(field39);
      }
    }
    auto field40Result = config.get("orientations");
    if (!field40Result)
      return std::unexpected(std::move(field40Result.error()));
    auto field40 = std::move(*field40Result);
    auto orientationsResult = parseOrientations(root, path, field40, config);
    if (!orientationsResult)
      return std::unexpected(std::move(orientationsResult.error()));
    auto orientations = std::move(*orientationsResult);
    objectConfig->orientations = std::move(orientations);
    // Keep compatibility with base-level particle emitter specifications.
    List<ObjectOrientation::ParticleEmissionEntry> particleEmitters;
    if (auto containsResult = config.contains("particleEmitter"); !containsResult)
      return std::unexpected(std::move(containsResult.error()));
    else if (*containsResult) {
    auto field41Result = config.get("particleEmitter");
    if (!field41Result)
      return std::unexpected(std::move(field41Result.error()));
    auto field41 = std::move(*field41Result);
    auto field42Result = ObjectOrientation::parseParticleEmitter(root, path, field41);
    if (!field42Result)
      return std::unexpected(std::move(field42Result.error()));
    auto field42 = std::move(*field42Result);
      particleEmitters.append(std::move(field42));
    }
    auto field43Result = config.getArray("particleEmitters", JsonArray());
    if (!field43Result)
      return std::unexpected(std::move(field43Result.error()));
    auto field43 = std::move(*field43Result);
    for (auto const& emitter : field43) {
    auto field44Result = ObjectOrientation::parseParticleEmitter(root, path, emitter);
    if (!field44Result)
      return std::unexpected(std::move(field44Result.error()));
    auto field44 = std::move(*field44Result);
      particleEmitters.append(std::move(field44));
    }
    for (auto const& orientation : objectConfig->orientations)
      orientation->particleEmitters.appendAll(particleEmitters);
    auto rootingResult = config.getBool("rooting", false);
    if (!rootingResult)
      return std::unexpected(std::move(rootingResult.error()));
    auto rooting = std::move(*rootingResult);
    objectConfig->rooting = std::move(rooting);
    auto biomePlacedResult = config.getBool("biomePlaced", false);
    if (!biomePlacedResult)
      return std::unexpected(std::move(biomePlacedResult.error()));
    auto biomePlaced = std::move(*biomePlacedResult);
    objectConfig->biomePlaced = std::move(biomePlaced);
    return objectConfig;
  }();
  if (!result)
    return std::unexpected(ObjectException(strf("Error loading object '{}'", path), assets->withAssetContext(std::move(result.error()), path, "Loading object configuration")));
  return result;
}

std::expected<List<Drawable>, StarException> ObjectDatabase::cursorHintDrawables(World const& world, String const& objectName, Vec2I const& position, Direction direction, Json parameters) const {
  List<Drawable> drawables;
  auto configResult = getConfig(objectName);
  if (!configResult)
    return std::unexpected(std::move(configResult.error()));
  auto config = std::move(*configResult);
  auto mergeConfig = jsonMerge(config->config, parameters);
  auto imageMetadataResult = m_root.imageMetadataDatabase();
  if (!imageMetadataResult)
    return std::unexpected(std::move(imageMetadataResult.error()));
  auto imageMetadata = std::move(*imageMetadataResult);
  auto placementImageResult = mergeConfig.optString("placementImage");
  if (!placementImageResult)
    return std::unexpected(std::move(placementImageResult.error()));
  auto placementImage = std::move(*placementImageResult);
  if (placementImage) {
    if (direction == Direction::Left)
      *placementImage += "?flipx";
    auto positionConfigResult = mergeConfig.get("placementImagePosition", jsonFromVec2F(Vec2F()));
    if (!positionConfigResult)
      return std::unexpected(std::move(positionConfigResult.error()));
    auto positionConfig = std::move(*positionConfigResult);
    auto imagePositionResult = jsonToVec2F(positionConfig);
    if (!imagePositionResult)
      return std::unexpected(std::move(imagePositionResult.error()));
    auto imagePosition = std::move(*imagePositionResult);
    auto drawableResult = Drawable::makeImage(*imageMetadata, AssetPath::relative_to(config->path, *placementImage),
                                              1.0 / TilePixels, false, Vec2F(position) + imagePosition / TilePixels);
    if (!drawableResult)
      return std::unexpected(std::move(drawableResult.error()));
    auto drawable = std::move(*drawableResult);
    drawables.append(std::move(drawable));
  } else {
    auto orientationIndexResult = config->findValidOrientation(world, position, direction);
    if (!orientationIndexResult)
      return std::unexpected(std::move(orientationIndexResult.error()));
    auto orientationIndex = std::move(*orientationIndexResult);
    if (orientationIndex == NPos) {
      // Retain the existing affinity fallback order for invalid placements.
      for (std::size_t i = 0; i < config->orientations.size(); ++i) {
        if (config->orientations[i]->directionAffinity == direction)
          orientationIndex = i;
      }
      if (orientationIndex == NPos)
        orientationIndex = 0;
    }
    auto orientationEntryResult = config->orientations.at(orientationIndex);
    if (!orientationEntryResult)
      return std::unexpected(std::move(orientationEntryResult.error()));
    auto const& orientation = orientationEntryResult->get();
    StringMap<String> imageKeys;
    auto colorConfigResult = orientation->config.get("color", "default");
    if (!colorConfigResult)
      return std::unexpected(std::move(colorConfigResult.error()));
    auto colorConfig = std::move(*colorConfigResult);
    auto colorResult = colorConfig.toString();
    if (!colorResult)
      return std::unexpected(std::move(colorResult.error()));
    auto color = std::move(*colorResult);
    imageKeys.set("color", color.takeUtf8());
    auto keysConfigResult = jsonMergeQueryDef("imageKeys", JsonObject(), config->config, orientation->config, parameters);
    if (!keysConfigResult)
      return std::unexpected(std::move(keysConfigResult.error()));
    auto keysConfig = std::move(*keysConfigResult);
    auto keysResult = keysConfig.toObject();
    if (!keysResult)
      return std::unexpected(std::move(keysResult.error()));
    auto keys = std::move(*keysResult);
    for (auto const& pair : keys) {
      auto valueResult = pair.second.toString();
      if (!valueResult)
        return std::unexpected(std::move(valueResult.error()));
      auto value = std::move(*valueResult);
      imageKeys.set(pair.first, value);
    }
    for (auto const& layer : orientation->imageLayers) {
      Drawable drawable = layer;
      auto& image = drawable.imagePart().image;
      auto taggedImage = AssetPath::join(image).replaceTags(imageKeys, true, "default");
      image = std::move(taggedImage);
      if (orientation->flipImages) {
        auto boundsResult = drawable.boundBox(*imageMetadata, false);
        if (!boundsResult)
          return std::unexpected(std::move(boundsResult.error()));
        auto bounds = std::move(*boundsResult);
        drawable.scale(Vec2F(-1, 1), bounds.center() - drawable.position);
      }
      drawables.append(std::move(drawable));
    }
    Drawable::translateAll(drawables, Vec2F(position) + orientation->imagePosition);
  }
  return drawables;
}

}
