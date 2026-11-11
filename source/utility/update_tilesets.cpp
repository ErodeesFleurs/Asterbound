#include <cstdio>
import star.tileset_database;
import star.native_runtime;
import star.json_extra;
import star.json;
import star.list;
import star.bi_map;
import star.data_stream;
import star.ordered_map;
import star.thread;
import star.io_device;
import star.image;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ref_ptr;
import star.id_map;
import star.random;
import star.periodic;
import star.periodic_function;
import star.casting;
import star.strong_typedef;
import star.set;
import star.color;
import star.lua;
import star.vector;
import star.game_types;
import star.material_types;
import star.audio;
import star.map;
import star.string;
import star.ordered_set;
import std;
import star.format;
import star.output_proxy;
import star.version;
import star.logging;
import star.listener;

import star.collision_block;
import star.item_descriptor;
import star.asset_source;
import star.assets;
import star.lua_components;
import star.lua_animation_component;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.entity;
import star.tile_damage;
import star.interaction_types;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.tile_entity;
import star.status_effect_entity;
import star.scripted_entity;
import star.chat_action;
import star.chatty_entity;
import star.wiring;
import star.wire_entity;
import star.inspectable_entity;
import star.animated_part_set;
import star.drawable;
import star.animation;
import star.particle;
import star.mixer;
import star.light_source;
import star.networked_animator;
import star.damage_types;
import star.entity_rendering_types;
import star.entity_rendering;
import star.object;
import star.option_parser;
import star.version_option_parser;
import star.root_base;
import star.configuration;
import star.root;
import star.root_loader;
import star.tileset_updater;

import star.liquids_database;
import star.material_database;
import star.object_database;


import star.algorithm;
using namespace Star;

String const InboundNode = "/tilesets/inboundnode.png";
String const OutboundNode = "/tilesets/outboundnode.png";
Vec3B const SourceLiquidBorderColor(0x80, 0x80, 0x00);

[[nodiscard]]
std::expected<void, StarException> scanMaterials(Root& root, TilesetUpdater& updater) {
  auto materialsResult = root.materialDatabase();
  if (!materialsResult)
    return std::unexpected(std::move(materialsResult.error()));
  auto materials = *materialsResult;
  auto assetsResult = root.assets();
  if (!assetsResult)
    return std::unexpected(std::move(assetsResult.error()));
  auto assets = *assetsResult;
  for (String materialName : materials->materialNames()) {
    auto id = materials->materialId(materialName);
    if (!id)
      return std::unexpected(std::move(id.error()));
    auto path = materials->materialPath(*id);
    if (!path)
      return std::unexpected(std::move(path.error()));
    if (!*path)
      continue;
    auto source = assets->assetSource(**path);
    if (!source)
      return std::unexpected(std::move(source.error()));
    auto renderProfile = materials->materialRenderProfile(*id);
    if (!renderProfile)
      continue;
    auto tileset = materials->materialCategory(*id);
    if (!tileset)
      return std::unexpected(std::move(tileset.error()));
    auto imagePath = renderProfile->pieceImage(renderProfile->representativePiece, 0);
    if (!imagePath)
      return std::unexpected(std::move(imagePath.error()));
    auto image = assets->image(*imagePath);
    if (!image)
      return std::unexpected(std::move(image.error()));
    auto shortDescription = materials->materialShortDescription(*id);
    if (!shortDescription)
      return std::unexpected(std::move(shortDescription.error()));
    auto description = materials->materialDescription(*id);
    if (!description)
      return std::unexpected(std::move(description.error()));
    Json properties = JsonObject{{"material", materialName}, {"//name", materialName},
        {"//shortdescription", *shortDescription}, {"//description", *description}};
    auto category = tileset->to_lower();
    if (!category)
      return std::unexpected(std::move(category.error()));
    updater.defineTile(std::make_shared<Tile>(Tile{*source, "materials", *category, materialName, *image, properties}));
  }
  return {};
}

// imagePosition might not be aligned to a whole number, i.e. the image origin
// might not align with the tile grid. We do, however want Tile Objects in Tiled
// to be grid-aligned (valid positions are offset relative to the grid not
// completely free-form), so we correct the alignment by adding padding to the
// image that we export.
// We're going to ignore the fact that some objects have imagePositions that
// aren't even aligned _to pixels_ (e.g. giftsmallmonsterbox).
Vec2U objectPositionPadding(Vec2I imagePosition) {
  int pixelsX = imagePosition.x();
  int pixelsY = imagePosition.y();

  // Unsigned modulo operation gives the padding to use (in pixels)
  unsigned padX = (unsigned)pixelsX % TilePixels;
  unsigned padY = (unsigned)pixelsY % TilePixels;
  return Vec2U(padX, padY);
}

[[nodiscard]]
std::expected<StringSet, StarException> categorizeObject(Root& root, String const& objectName, Vec2U imageSize) {
  if (imageSize[0] >= 256 || imageSize[1] >= 256)
    return StringSet{"huge-objects"};
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto objects = root.objectDatabase();
  if (!objects)
    return std::unexpected(std::move(objects.error()));
  auto defaults = (*assets)->json("/objects/defaultCategories.config");
  if (!defaults)
    return std::unexpected(std::move(defaults.error()));
  auto config = (*objects)->getConfig(objectName);
  if (!config)
    return std::unexpected(std::move(config.error()));
  StringSet categories;
  auto category = defaults->getString("category");
  if (!category)
    return std::unexpected(std::move(category.error()));
  if ((*config)->category != *category)
    categories.insert("objects-by-category/" + (*config)->category);
  for (auto const& tag : (*config)->colonyTags)
    categories.insert("objects-by-colonytag/" + tag);
  auto type = defaults->getString("objectType");
  if (!type)
    return std::unexpected(std::move(type.error()));
  if ((*config)->type != *type)
    categories.insert("objects-by-type/" + (*config)->type);
  auto race = defaults->getString("race");
  if (!race)
    return std::unexpected(std::move(race.error()));
  if ((*config)->race != *race)
    categories.insert("objects-by-race/" + (*config)->race);
  if (categories.empty())
    categories.insert("objects-uncategorized");
  StringSet result;
  for (auto const& entry : categories) {
    auto lowered = entry.to_lower();
    if (!lowered)
      return std::unexpected(std::move(lowered.error()));
    result.insert(std::move(*lowered));
  }
  return result;
}

[[nodiscard]]
std::expected<void, StarException> drawNodes(Assets const& assets, std::shared_ptr<::Star::Image> const& image, Vec2I imagePosition, JsonArray nodes, String nodeImagePath) {
  auto nodeImage = assets.image(nodeImagePath);
  if (!nodeImage)
    return std::unexpected(std::move(nodeImage.error()));
  for (Json const& node : nodes) {
    auto position = jsonToVec2I(node);
    if (!position)
      return std::unexpected(std::move(position.error()));
    Vec2I nodePos = *position * TilePixels + Vec2I(0, TilePixels - (*nodeImage)->height());
    Vec2U nodeImagePos = Vec2U(nodePos - imagePosition);
    auto drawn = image->draw_into(nodeImagePos, **nodeImage);
    if (!drawn)
      return std::unexpected(std::move(drawn.error()));
  }
  return {};
}

[[nodiscard]]
std::expected<void, StarException> defineObjectOrientation(Root& root, TilesetUpdater& updater,
    String const& objectName,
    List<std::shared_ptr<::Star::ObjectOrientation>> const& orientations,
    int orientationIndex) {
  auto assetsResult = root.assets();
  if (!assetsResult)
    return std::unexpected(std::move(assetsResult.error()));
  auto assets = *assetsResult;
  auto objectsResult = root.objectDatabase();
  if (!objectsResult)
    return std::unexpected(std::move(objectsResult.error()));
  auto objects = *objectsResult;

  std::shared_ptr<::Star::ObjectOrientation> orientation = orientations[orientationIndex];

  Vec2I imagePosition = Vec2I(orientation->imagePosition * TilePixels);

  List<std::shared_ptr<::Star::Image const>> layers;
  unsigned width = 0, height = 0;
  for (auto const& imageLayer : orientation->imageLayers) {
    auto path = AssetPath::join(imageLayer.imagePart().image);
    String imageName = path.replace_tags(StringMap<String>{}, true, "default");
    auto loaded = assets->image(imageName);
    if (!loaded)
      return std::unexpected(std::move(loaded.error()));
    auto image = *loaded;
    layers.push_back(image);
    width = std::max(width, image->width());
    height = std::max(height, image->height());
  }

  Vec2U imagePadding = objectPositionPadding(imagePosition);
  imagePosition -= Vec2I(imagePadding);

  // Padding is added to the right hand side as well as the left so that
  // when objects are flipped in the editor, they're still aligned correctly.
  Vec2U imageSize(width + 2 * imagePadding.x(), height + imagePadding.y());

  std::shared_ptr<::Star::Image> combinedImage = std::make_shared<Image>(imageSize, PixelFormat::RGBA32);
  combinedImage->fill(Vec4B(0, 0, 0, 0));
  for (std::shared_ptr<::Star::Image const> const& layer : layers) {
    auto drawn = combinedImage->draw_into(imagePadding, *layer);
    if (!drawn)
      return std::unexpected(std::move(drawn.error()));
  }

  // Overlay the image with the wiring nodes:
  auto objectConfig = objects->getConfig(objectName);
  if (!objectConfig)
    return std::unexpected(std::move(objectConfig.error()));
  auto inputNodes = (*objectConfig)->config.getArray("inputNodes", {});
  if (!inputNodes)
    return std::unexpected(std::move(inputNodes.error()));
  auto inputsDrawn = drawNodes(*assets, combinedImage, imagePosition, *inputNodes, InboundNode);
  if (!inputsDrawn)
    return std::unexpected(std::move(inputsDrawn.error()));
  auto outputNodes = (*objectConfig)->config.getArray("outputNodes", {});
  if (!outputNodes)
    return std::unexpected(std::move(outputNodes.error()));
  auto outputsDrawn = drawNodes(*assets, combinedImage, imagePosition, *outputNodes, OutboundNode);
  if (!outputsDrawn)
    return std::unexpected(std::move(outputsDrawn.error()));
  auto example = objects->createObject(objectName);
  if (!example)
    return std::unexpected(std::move(example.error()));
  auto shortDescription = (*example)->shortDescription();
  if (!shortDescription)
    return std::unexpected(std::move(shortDescription.error()));
  auto description = (*example)->description();
  if (!description)
    return std::unexpected(std::move(description.error()));
  JsonObject properties{{"object", objectName},
      {"imagePositionX", Star::to_string(imagePosition.x())},
      {"imagePositionY", Star::to_string(imagePosition.y())},
      {"//shortdescription", *shortDescription}, {"//description", *description}};
  if (orientation->directionAffinity.has_value()) {
    Direction direction = *orientation->directionAffinity;
    if (orientation->flipImages)
      direction = -direction;
    auto directionName = enumName<DirectionNames>(direction);
    if (!directionName)
      return std::unexpected(std::move(directionName.error()));
    properties["tilesetDirection"] = String(*directionName);
  }

  auto tilesets = categorizeObject(root, objectName, imageSize);
  if (!tilesets)
    return std::unexpected(std::move(tilesets.error()));

  // tileName becomes part of the filename for the tile's image. Different
  // orientations require different images, so the tileName must be different
  // for each orientation.
  String tileName = objectName;
  if (orientationIndex != 0)
    tileName += "_orientation" + Star::to_string(orientationIndex);
  properties["//name"] = tileName;
  auto source = assets->assetSource((*objectConfig)->path);
  if (!source)
    return std::unexpected(std::move(source.error()));
  for (String const& tileset : *tilesets) {
    std::shared_ptr<::Star::Tile> tile = std::make_shared<Tile>(Tile{*source, "objects", tileset, tileName, combinedImage, properties});
    updater.defineTile(tile);
  }
  return {};
}

[[nodiscard]]
std::expected<void, StarException> scanObjects(Root& root, TilesetUpdater& updater) {
  auto objects = root.objectDatabase();
  if (!objects)
    return std::unexpected(std::move(objects.error()));
  for (String const& objectName : (*objects)->allObjects()) {
    auto orientations = (*objects)->getOrientations(objectName);
    if (!orientations)
      return std::unexpected(std::move(orientations.error()));
    if ((*orientations)->empty()) {
      auto warning = root.logger().warn("Object {} has no orientations and will not be exported", objectName);
      if (!warning)
        return std::unexpected(std::move(warning.error()));
      continue;
    }
    auto const& orientation = (**orientations)[0];
    auto first = defineObjectOrientation(root, updater, objectName, **orientations, 0);
    if (!first)
      return std::unexpected(std::move(first.error()));
    for (unsigned i = 1; i < (*orientations)->size(); ++i) {
      if (i >= 2 || orientation->imagePosition != (**orientations)[i]->imagePosition) {
        auto exported = defineObjectOrientation(root, updater, objectName, **orientations, i);
        if (!exported)
          return std::unexpected(std::move(exported.error()));
      }
    }
  }
  return {};
}

[[nodiscard]]
std::expected<void, StarException> scanLiquids(Root& root, TilesetUpdater& updater) {
  auto liquids = root.liquidsDatabase();
  if (!liquids)
    return std::unexpected(std::move(liquids.error()));
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  Vec2U imageSize(TilePixels, TilePixels);
  for (auto const& liquid : (*liquids)->allLiquidSettings()) {
    auto image = std::make_shared<Image>(imageSize, PixelFormat::RGBA32);
    image->fill(liquid->liquidColor);
    auto assetSource = (*assets)->assetSource(liquid->path);
    if (!assetSource)
      return std::unexpected(std::move(assetSource.error()));
    JsonObject properties{{"liquid", liquid->name}, {"//name", liquid->name}};
    updater.defineTile(std::make_shared<Tile>(Tile{*assetSource, "liquids", "liquids", liquid->name, image, properties}));
    auto sourceImage = std::make_shared<Image>(imageSize, PixelFormat::RGBA32);
    auto copied = sourceImage->copy_into(Vec2U(), *image);
    if (!copied)
      return std::unexpected(std::move(copied.error()));
    auto horizontal = sourceImage->fill_rect(Vec2U(), Vec2U(image->width(), 1), SourceLiquidBorderColor);
    if (!horizontal)
      return std::unexpected(std::move(horizontal.error()));
    auto vertical = sourceImage->fill_rect(Vec2U(), Vec2U(1, image->height()), SourceLiquidBorderColor);
    if (!vertical)
      return std::unexpected(std::move(vertical.error()));
    String sourceName = liquid->name + "_source";
    properties["source"] = "true";
    properties["//name"] = sourceName;
    properties["//shortdescription"] = "Endless " + liquid->name;
    updater.defineTile(std::make_shared<Tile>(Tile{*assetSource, "liquids", "liquids", sourceName, sourceImage, properties}));
  }
  return {};
}

int main(int argc, char** argv) {
  auto nativeRuntime = NativeRuntime::create();
  if (!nativeRuntime) {
    if (auto written = Star::print(stderr, "exception caught: {}\n", Star::output_exception(ExceptionDiagnostic(nativeRuntime.error()), true)); !written)
      return 1;
    return 1;
  }
  auto run = [&]() -> std::expected<int, StarException> {
    auto rootLoader = RootLoader::create({{}, {}, {}, LogLevel::Error, false, {}});
    if (!rootLoader)
      return std::unexpected(std::move(rootLoader.error()));
    rootLoader->setSummary("Updates Tiled JSON tilesets in unpacked assets directories");
    auto initialized = rootLoader->commandInitOrDie(argc, argv);
    if (!initialized)
      return std::unexpected(std::move(initialized.error()));
    auto& root = initialized->first;
    TilesetUpdater updater(root->logger());
    auto assets = root->assets();
    if (!assets)
      return std::unexpected(std::move(assets.error()));
    for (String const& source : (*assets)->assetSources()) {
      auto logged = root->logger().info("Assets source: \"{}\"", source);
      if (!logged)
        return std::unexpected(std::move(logged.error()));
      auto defined = updater.defineAssetSource(source);
      if (!defined)
        return std::unexpected(std::move(defined.error()));
    }
    auto materials = scanMaterials(*root, updater);
    if (!materials)
      return std::unexpected(std::move(materials.error()));
    auto objects = scanObjects(*root, updater);
    if (!objects)
      return std::unexpected(std::move(objects.error()));
    auto liquids = scanLiquids(*root, updater);
    if (!liquids)
      return std::unexpected(std::move(liquids.error()));
    auto exported = updater.exportTilesets();
    if (!exported)
      return std::unexpected(std::move(exported.error()));
    return 0;
  };
  auto result = run();
  if (!result) {
    if (auto written = Star::print(stderr, "exception caught: {}\n", Star::output_exception(ExceptionDiagnostic(result.error()), true)); !written)
      return 1;
    return 1;
  }
  return *result;
}
