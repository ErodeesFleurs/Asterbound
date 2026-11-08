import star.json;
import star.json_extra;
import star.list;
import star.bi_map;
import star.data_stream;
import star.rect;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.vector;
import star.game_types;
import star.material_types;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.string;
import star.map;
import star.format;
import star.version;
import star.data_stream_extra;
import star.image;

import star.collision_block;
import star.item_descriptor;
import star.asset_source;
import star.assets;

import star.configuration;
import star.root;


import star.world_structure;
import star.material_database;
import star.image_metadata_database;

import star.algorithm;
namespace Star {

WorldStructure::WorldStructure() = default;

std::expected<WorldStructure, StarException> WorldStructure::create(Root& root, String const& configPath) {
  WorldStructure result;
  auto autoassetsResult1 = root.assets();
  if (!autoassetsResult1) return std::unexpected(std::move(autoassetsResult1.error()));
  auto assets = std::move((*autoassetsResult1));
  auto autoimgMetadataResult1 = root.imageMetadataDatabase();
  if (!autoimgMetadataResult1) return std::unexpected(std::move(autoimgMetadataResult1.error()));
  auto imgMetadata = std::move((*autoimgMetadataResult1));
  auto autosettingsResult1 = assets->json(configPath);
  if (!autosettingsResult1) return std::unexpected(std::move(autosettingsResult1.error()));
  auto settings = std::move((*autosettingsResult1));
  result.m_region = RectI::null();
  auto resultm_configResult1 = settings.getObject("config", JsonObject());
  if (!resultm_configResult1) return std::unexpected(std::move(resultm_configResult1.error()));
  result.m_config = std::move((*resultm_configResult1));
  auto autobackgroundOverlaysResult1 = settings.getArray("backgroundOverlays", JsonArray());
  if (!autobackgroundOverlaysResult1) return std::unexpected(std::move(autobackgroundOverlaysResult1.error()));
  auto backgroundOverlays = std::move((*autobackgroundOverlaysResult1));
  for (auto const& overlaySettings : backgroundOverlays) {
    auto autopositionResult1 = overlaySettings.get("position");
    if (!autopositionResult1) return std::unexpected(std::move(autopositionResult1.error()));
    auto autopositionResult2 = jsonToVec2F((*autopositionResult1));
    if (!autopositionResult2) return std::unexpected(std::move(autopositionResult2.error()));
    auto position = std::move((*autopositionResult2));
    auto overlay_image_name = overlaySettings.getString("image");
    if (!overlay_image_name)
      return std::unexpected(std::move(overlay_image_name.error()));
    auto image = AssetPath::relative_to(configPath, (*overlay_image_name));
    auto autofullbrightResult1 = overlaySettings.getBool("fullbright", false);
    if (!autofullbrightResult1) return std::unexpected(std::move(autofullbrightResult1.error()));
    auto fullbright = std::move((*autofullbrightResult1));
    Overlay overlay{.min = position, .image = std::move(image), .fullbright = fullbright};
    result.m_backgroundOverlays.push_back(overlay);
    auto autoimageSizeResult1 = imgMetadata->imageSize(overlay.image);
    if (!autoimageSizeResult1) return std::unexpected(std::move(autoimageSizeResult1.error()));
    auto imageSize = std::move((*autoimageSizeResult1));
    result.m_region.combine(RectI::withSize(Vec2I::floor(overlay.min), Vec2I::ceil(Vec2F(imageSize) / TilePixels)));
  }
  auto autoforegroundOverlaysResult1 = settings.getArray("foregroundOverlays", JsonArray());
  if (!autoforegroundOverlaysResult1) return std::unexpected(std::move(autoforegroundOverlaysResult1.error()));
  auto foregroundOverlays = std::move((*autoforegroundOverlaysResult1));
  for (auto const& overlaySettings : foregroundOverlays) {
    auto autopositionResult1 = overlaySettings.get("position");
    if (!autopositionResult1) return std::unexpected(std::move(autopositionResult1.error()));
    auto autopositionResult2 = jsonToVec2F((*autopositionResult1));
    if (!autopositionResult2) return std::unexpected(std::move(autopositionResult2.error()));
    auto position = std::move((*autopositionResult2));
    auto overlay_image_name = overlaySettings.getString("image");
    if (!overlay_image_name)
      return std::unexpected(std::move(overlay_image_name.error()));
    auto image = AssetPath::relative_to(configPath, (*overlay_image_name));
    auto autofullbrightResult1 = overlaySettings.getBool("fullbright", false);
    if (!autofullbrightResult1) return std::unexpected(std::move(autofullbrightResult1.error()));
    auto fullbright = std::move((*autofullbrightResult1));
    Overlay overlay{.min = position, .image = std::move(image), .fullbright = fullbright};
    result.m_foregroundOverlays.push_back(overlay);
    auto autoimageSizeResult1 = imgMetadata->imageSize(overlay.image);
    if (!autoimageSizeResult1) return std::unexpected(std::move(autoimageSizeResult1.error()));
    auto imageSize = std::move((*autoimageSizeResult1));
    result.m_region.combine(RectI::withSize(Vec2I::floor(overlay.min), Vec2I::ceil(Vec2F(imageSize) / TilePixels)));
  }
  auto autoblockPositionResult1 = settings.getArray("blocksPosition", JsonArray{0, 0});
  if (!autoblockPositionResult1) return std::unexpected(std::move(autoblockPositionResult1.error()));
  auto autoblockPositionResult2 = jsonToVec2I((*autoblockPositionResult1));
  if (!autoblockPositionResult2) return std::unexpected(std::move(autoblockPositionResult2.error()));
  auto blockPosition = std::move((*autoblockPositionResult2));
  HashMap<Vec4B, BlockKey> blockKeys;
  auto automatDbResult1 = root.materialDatabase();
  if (!automatDbResult1) return std::unexpected(std::move(automatDbResult1.error()));
  auto matDb = std::move((*automatDbResult1));
  auto autoblockKeyJsonResult1 = settings.get("blockKey", JsonArray());
  if (!autoblockKeyJsonResult1) return std::unexpected(std::move(autoblockKeyJsonResult1.error()));
  auto autoblockKeyJsonResult2 = assets->fetchJson((*autoblockKeyJsonResult1), configPath);
  if (!autoblockKeyJsonResult2) return std::unexpected(std::move(autoblockKeyJsonResult2.error()));
  auto blockKeyJson = std::move((*autoblockKeyJsonResult2));
  auto blockKeyConfigsResult = blockKeyJson.toArray();
  if (!blockKeyConfigsResult) return std::unexpected(std::move(blockKeyConfigsResult.error()));
  auto blockKeyConfigs = std::move(*blockKeyConfigsResult);
  for (auto const& blockKeyConfig : blockKeyConfigs) {
    auto autoforegroundMatResult1 = blockKeyConfig.getString("foregroundMat", "");
    if (!autoforegroundMatResult1) return std::unexpected(std::move(autoforegroundMatResult1.error()));
    auto foregroundMat = std::move((*autoforegroundMatResult1));
    MaterialId foregroundMatId;
    if (foregroundMat == "") {
      foregroundMatId = StructureMaterialId;
    } else {
      auto foregroundMatIdResult1 = matDb->materialId(foregroundMat);
      if (!foregroundMatIdResult1) return std::unexpected(std::move(foregroundMatIdResult1.error()));
      foregroundMatId = std::move((*foregroundMatIdResult1));
    }
    auto autoforegroundModResult1 = blockKeyConfig.getString("foregroundMod", "");
    if (!autoforegroundModResult1) return std::unexpected(std::move(autoforegroundModResult1.error()));
    auto foregroundMod = std::move((*autoforegroundModResult1));
    ModId foregroundModId;
    if (foregroundMod == "") {
      foregroundModId = NoModId;
    } else {
      auto foregroundModIdResult1 = matDb->modId(foregroundMod);
      if (!foregroundModIdResult1) return std::unexpected(std::move(foregroundModIdResult1.error()));
      foregroundModId = std::move((*foregroundModIdResult1));
    }
    auto autobackgroundMatResult1 = blockKeyConfig.getString("backgroundMat", "");
    if (!autobackgroundMatResult1) return std::unexpected(std::move(autobackgroundMatResult1.error()));
    auto backgroundMat = std::move((*autobackgroundMatResult1));
    MaterialId backgroundMatId;
    if (backgroundMat == "") {
      backgroundMatId = StructureMaterialId;
    } else {
      auto backgroundMatIdResult1 = matDb->materialId(backgroundMat);
      if (!backgroundMatIdResult1) return std::unexpected(std::move(backgroundMatIdResult1.error()));
      backgroundMatId = std::move((*backgroundMatIdResult1));
    }
    auto autobackgroundModResult1 = blockKeyConfig.getString("backgroundMod", "");
    if (!autobackgroundModResult1) return std::unexpected(std::move(autobackgroundModResult1.error()));
    auto backgroundMod = std::move((*autobackgroundModResult1));
    ModId backgroundModId;
    if (backgroundMod == "") {
      backgroundModId = NoModId;
    } else {
      auto backgroundModIdResult1 = matDb->modId(backgroundMod);
      if (!backgroundModIdResult1) return std::unexpected(std::move(backgroundModIdResult1.error()));
      backgroundModId = std::move((*backgroundModIdResult1));
    }
    auto autokeyField0Result1 = blockKeyConfig.getBool("anchor", false);
    if (!autokeyField0Result1) return std::unexpected(std::move(autokeyField0Result1.error()));
    auto keyField0 = std::move((*autokeyField0Result1));
    auto autokeyField1Result1 = blockKeyConfig.getBool("foregroundBlock", false);
    if (!autokeyField1Result1) return std::unexpected(std::move(autokeyField1Result1.error()));
    auto keyField1 = std::move((*autokeyField1Result1));
    auto keyField2 = std::move(foregroundMatId);
    auto autokeyField3Result1 = blockKeyConfig.getBool("foregroundResidual", false);
    if (!autokeyField3Result1) return std::unexpected(std::move(autokeyField3Result1.error()));
    auto keyField3 = std::move((*autokeyField3Result1));
    auto autokeyField4Result1 = blockKeyConfig.getBool("backgroundBlock", false);
    if (!autokeyField4Result1) return std::unexpected(std::move(autokeyField4Result1.error()));
    auto keyField4 = std::move((*autokeyField4Result1));
    auto keyField5 = std::move(backgroundMatId);
    auto autokeyField6Result1 = blockKeyConfig.getBool("backgroundResidual", false);
    if (!autokeyField6Result1) return std::unexpected(std::move(autokeyField6Result1.error()));
    auto keyField6 = std::move((*autokeyField6Result1));
    auto autokeyField7Result1 = blockKeyConfig.getString("object", "");
    if (!autokeyField7Result1) return std::unexpected(std::move(autokeyField7Result1.error()));
    auto keyField7 = std::move((*autokeyField7Result1));
    auto objectDirection = blockKeyConfig.getString("objectDirection", "left").and_then([](String const& name) {
      return enumValue<DirectionNames>(name);
    });
    if (!objectDirection)
      return std::unexpected(std::move(objectDirection.error()));
    auto keyField8 = *objectDirection;
    auto autokeyField9Result1 = blockKeyConfig.getObject("objectParameters", JsonObject());
    if (!autokeyField9Result1) return std::unexpected(std::move(autokeyField9Result1.error()));
    auto keyField9 = std::move((*autokeyField9Result1));
    auto autokeyField10Result1 = blockKeyConfig.getBool("objectResidual", false);
    if (!autokeyField10Result1) return std::unexpected(std::move(autokeyField10Result1.error()));
    auto keyField10 = std::move((*autokeyField10Result1));
    auto autokeyField11Result1 = blockKeyConfig.get("flags", JsonArray());
    if (!autokeyField11Result1) return std::unexpected(std::move(autokeyField11Result1.error()));
    auto autokeyField11Result2 = jsonToStringList((*autokeyField11Result1));
    if (!autokeyField11Result2) return std::unexpected(std::move(autokeyField11Result2.error()));
    auto keyField11 = std::move((*autokeyField11Result2));
    auto foregroundColor = blockKeyConfig.getUInt("foregroundMatColor", 0);
    if (!foregroundColor)
      return std::unexpected(std::move(foregroundColor.error()));
    auto keyField12 = std::move(static_cast<MaterialColorVariant>(*foregroundColor));
    auto backgroundColor = blockKeyConfig.getUInt("backgroundMatColor", 0);
    if (!backgroundColor)
      return std::unexpected(std::move(backgroundColor.error()));
    auto keyField13 = std::move(static_cast<MaterialColorVariant>(*backgroundColor));
    auto foregroundHue = blockKeyConfig.getUInt("foregroundMatHue", 0);
    if (!foregroundHue)
      return std::unexpected(std::move(foregroundHue.error()));
    auto keyField14 = std::move(static_cast<MaterialHue>(*foregroundHue));
    auto backgroundHue = blockKeyConfig.getUInt("backgroundMatHue", 0);
    if (!backgroundHue)
      return std::unexpected(std::move(backgroundHue.error()));
    auto keyField15 = std::move(static_cast<MaterialHue>(*backgroundHue));
    auto keyField16 = std::move(foregroundModId);
    auto keyField17 = std::move(backgroundModId);
    BlockKey blockKey{.anchor = keyField0,
                      .foregroundBlock = keyField1,
                      .foregroundMat = keyField2,
                      .foregroundResidual = keyField3,
                      .backgroundBlock = keyField4,
                      .backgroundMat = keyField5,
                      .backgroundResidual = keyField6,
                      .object = keyField7,
                      .objectDirection = keyField8,
                      .objectParameters = keyField9,
                      .objectResidual = keyField10,
                      .flags = keyField11,
                      .foregroundMatColor = keyField12,
                      .backgroundMatColor = keyField13,
                      .foregroundMatHue = keyField14,
                      .backgroundMatHue = keyField15,
                      .foregroundMatMod = keyField16,
                      .backgroundMatMod = keyField17};
    auto autocolorResult1 = blockKeyConfig.get("value");
    if (!autocolorResult1) return std::unexpected(std::move(autocolorResult1.error()));
    auto autocolorResult2 = jsonToColor((*autocolorResult1));
    if (!autocolorResult2) return std::unexpected(std::move(autocolorResult2.error()));
    auto color = std::move((*autocolorResult2));
    blockKeys[color.toRgba()] = std::move(blockKey);
  }
  std::optional<Vec2I> anchorPosition;
  auto autohasBlockImageResult1 = settings.contains("blockImage");
  if (!autohasBlockImageResult1) return std::unexpected(std::move(autohasBlockImageResult1.error()));
  auto hasBlockImage = std::move((*autohasBlockImageResult1));
  if (hasBlockImage) {
    auto block_image_name = settings.getString("blockImage");
    if (!block_image_name)
      return std::unexpected(std::move(block_image_name.error()));
    auto block_image = assets->image(AssetPath::relative_to(configPath, (*block_image_name)));
    if (!block_image)
      return std::unexpected(std::move(block_image.error()));
    auto blocksImage = std::move((*block_image));
    const BlockKey defaultBlockKey = {.anchor = false,
                                      .foregroundBlock = false,
                                      .foregroundMat = StructureMaterialId,
                                      .foregroundResidual = false,
                                      .backgroundBlock = false,
                                      .backgroundMat = StructureMaterialId,
                                      .backgroundResidual = false,
                                      .object = String(),
                                      .objectDirection = Direction::Left,
                                      .objectParameters = Json(),
                                      .objectResidual = false,
                                      .flags = StringList(),
                                      .foregroundMatColor = static_cast<MaterialColorVariant>(0),
                                      .backgroundMatColor = static_cast<MaterialColorVariant>(0),
                                      .foregroundMatHue = static_cast<MaterialHue>(0),
                                      .backgroundMatHue = static_cast<MaterialHue>(0),
                                      .foregroundMatMod = NoModId,
                                      .backgroundMatMod = NoModId};

    for (std::size_t y = 0; y < blocksImage->height(); ++y) {
      for (std::size_t x = 0; x < blocksImage->width(); ++x) {
        auto pixel = blocksImage->getrgb(x, y);
        if (!pixel) return std::unexpected(std::move(pixel.error()));
        auto blockKey = blockKeys.value(*pixel, defaultBlockKey);
        auto pos = blockPosition + Vec2I(x, y);

        if (blockKey.anchor) {
          if (anchorPosition)
            return std::unexpected(WorldStructureException(
                Star::format("Multiple anchor points defined in blockImage, first point is at {}, second at {}",
                    *anchorPosition,
                    pos)));
          anchorPosition = pos;
        }

        if (blockKey.foregroundBlock)
          result.m_foregroundBlocks.push_back({.position = pos,
                                               .materialId = blockKey.foregroundMat,
                                               .residual = blockKey.foregroundResidual,
                                               .materialColor = blockKey.foregroundMatColor,
                                               .materialHue = blockKey.foregroundMatHue,
                                               .materialMod = blockKey.foregroundMatMod});
        if (blockKey.backgroundBlock)
          result.m_backgroundBlocks.push_back({.position = pos,
                                               .materialId = blockKey.backgroundMat,
                                               .residual = blockKey.backgroundResidual,
                                               .materialColor = blockKey.backgroundMatColor,
                                               .materialHue = blockKey.foregroundMatHue,
                                               .materialMod = blockKey.foregroundMatMod});

        if (!blockKey.object.empty())
          result.m_objects.push_back(Object{.position = pos,
                                            .name = blockKey.object,
                                            .direction = blockKey.objectDirection,
                                            .parameters = blockKey.objectParameters,
                                            .residual = blockKey.objectResidual});

        for (auto const& flag : blockKey.flags)
          result.m_flaggedBlocks[flag].push_back(pos);

        result.m_region.combine(pos);
      }
    }

    if (anchorPosition)
      result.m_anchorPosition = *anchorPosition;
    else
      result.m_anchorPosition = result.m_region.center();

    // Objects put into the list are from top to bottom, need to place them
    // from bottom to top for objects on top of other objects.
    std::reverse(result.m_objects.begin(), result.m_objects.end());
  }
  return result;
}

std::expected<WorldStructure, StarException> WorldStructure::create(Json const& store) {
  WorldStructure result;
  auto overlayFromJson = [](Json const& v) -> std::expected<Overlay, StarException> {
    auto autofield0Result1 = v.get("min");
    if (!autofield0Result1) return std::unexpected(std::move(autofield0Result1.error()));
    auto autofield0Result2 = jsonToVec2F((*autofield0Result1));
    if (!autofield0Result2) return std::unexpected(std::move(autofield0Result2.error()));
    auto field0 = std::move((*autofield0Result2));
    auto imagePath = v.getString("image");
    if (!imagePath)
      return std::unexpected(std::move(imagePath.error()));
    auto field1 = std::move((*imagePath));
    auto autofield2Result1 = v.getBool("fullbright");
    if (!autofield2Result1) return std::unexpected(std::move(autofield2Result1.error()));
    auto field2 = std::move((*autofield2Result1));
    return Overlay{.min = std::move(field0), .image = std::move(field1), .fullbright = std::move(field2)};
  };
  auto blockFromJson = [](Json const& v) -> std::expected<Block, StarException> {
    auto autofield0Result1 = v.get("position");
    if (!autofield0Result1) return std::unexpected(std::move(autofield0Result1.error()));
    auto autofield0Result2 = jsonToVec2I((*autofield0Result1));
    if (!autofield0Result2) return std::unexpected(std::move(autofield0Result2.error()));
    auto field0 = std::move((*autofield0Result2));
    auto materialId = v.getUInt("materialId");
    if (!materialId)
      return std::unexpected(std::move(materialId.error()));
    auto field1 = std::move(static_cast<MaterialId>(*materialId));
    auto autofield2Result1 = v.getBool("residual");
    if (!autofield2Result1) return std::unexpected(std::move(autofield2Result1.error()));
    auto field2 = std::move((*autofield2Result1));
    auto materialColor = v.getUInt("materialColor", 0);
    if (!materialColor)
      return std::unexpected(std::move(materialColor.error()));
    auto field3 = std::move(static_cast<MaterialColorVariant>(*materialColor));
    auto materialHue = v.getUInt("materialHue", 0);
    if (!materialHue)
      return std::unexpected(std::move(materialHue.error()));
    auto field4 = std::move(static_cast<MaterialHue>(*materialHue));
    auto materialMod = v.getUInt("modId", NoModId);
    if (!materialMod)
      return std::unexpected(std::move(materialMod.error()));
    auto field5 = std::move(static_cast<ModId>(*materialMod));
    return Block{.position = std::move(field0),
                 .materialId = std::move(field1),
                 .residual = std::move(field2),
                 .materialColor = std::move(field3),
                 .materialHue = std::move(field4),
                 .materialMod = std::move(field5)};
  };
  auto objectFromJson = [](Json const& v) -> std::expected<Object, StarException> {
    auto autofield0Result1 = v.get("position");
    if (!autofield0Result1) return std::unexpected(std::move(autofield0Result1.error()));
    auto autofield0Result2 = jsonToVec2I((*autofield0Result1));
    if (!autofield0Result2) return std::unexpected(std::move(autofield0Result2.error()));
    auto field0 = std::move((*autofield0Result2));
    auto autofield1Result1 = v.getString("name");
    if (!autofield1Result1) return std::unexpected(std::move(autofield1Result1.error()));
    auto field1 = std::move((*autofield1Result1));
    auto direction =
      v.getString("direction").and_then([](String const& name) { return enumValue<DirectionNames>(name); });
    if (!direction)
      return std::unexpected(std::move(direction.error()));
    auto field2 = *direction;
    auto autofield3Result1 = v.get("parameters", {});
    if (!autofield3Result1) return std::unexpected(std::move(autofield3Result1.error()));
    auto field3 = std::move((*autofield3Result1));
    auto autofield4Result1 = v.getBool("residual", false);
    if (!autofield4Result1) return std::unexpected(std::move(autofield4Result1.error()));
    auto field4 = std::move((*autofield4Result1));
    return Object{.position = std::move(field0),
                  .name = std::move(field1),
                  .direction = std::move(field2),
                  .parameters = std::move(field3),
                  .residual = std::move(field4)};
  };
  auto resultm_regionResult1 = store.get("region");
  if (!resultm_regionResult1) return std::unexpected(std::move(resultm_regionResult1.error()));
  auto resultm_regionResult2 = jsonToRectI((*resultm_regionResult1));
  if (!resultm_regionResult2) return std::unexpected(std::move(resultm_regionResult2.error()));
  result.m_region = std::move((*resultm_regionResult2));
  auto resultm_anchorPositionResult1 = store.get("anchorPosition");
  if (!resultm_anchorPositionResult1) return std::unexpected(std::move(resultm_anchorPositionResult1.error()));
  auto resultm_anchorPositionResult2 = jsonToVec2I((*resultm_anchorPositionResult1));
  if (!resultm_anchorPositionResult2) return std::unexpected(std::move(resultm_anchorPositionResult2.error()));
  result.m_anchorPosition = std::move((*resultm_anchorPositionResult2));
  auto resultm_configResult1 = store.get("config");
  if (!resultm_configResult1) return std::unexpected(std::move(resultm_configResult1.error()));
  result.m_config = std::move((*resultm_configResult1));
  auto autobackgroundOverlaysResult1 = store.getArray("backgroundOverlays");
  if (!autobackgroundOverlaysResult1) return std::unexpected(std::move(autobackgroundOverlaysResult1.error()));
  auto backgroundOverlays = std::move((*autobackgroundOverlaysResult1));
  for (auto const& value : backgroundOverlays) {
    auto parsed = overlayFromJson(value);
    if (!parsed) return std::unexpected(std::move(parsed.error()));
    result.m_backgroundOverlays.push_back(std::move(*parsed));
  }
  auto autoforegroundOverlaysResult1 = store.getArray("foregroundOverlays");
  if (!autoforegroundOverlaysResult1) return std::unexpected(std::move(autoforegroundOverlaysResult1.error()));
  auto foregroundOverlays = std::move((*autoforegroundOverlaysResult1));
  for (auto const& value : foregroundOverlays) {
    auto parsed = overlayFromJson(value);
    if (!parsed) return std::unexpected(std::move(parsed.error()));
    result.m_foregroundOverlays.push_back(std::move(*parsed));
  }
  auto autobackgroundBlocksResult1 = store.getArray("backgroundBlocks");
  if (!autobackgroundBlocksResult1) return std::unexpected(std::move(autobackgroundBlocksResult1.error()));
  auto backgroundBlocks = std::move((*autobackgroundBlocksResult1));
  for (auto const& value : backgroundBlocks) {
    auto parsed = blockFromJson(value);
    if (!parsed) return std::unexpected(std::move(parsed.error()));
    result.m_backgroundBlocks.push_back(std::move(*parsed));
  }
  auto autoforegroundBlocksResult1 = store.getArray("foregroundBlocks");
  if (!autoforegroundBlocksResult1) return std::unexpected(std::move(autoforegroundBlocksResult1.error()));
  auto foregroundBlocks = std::move((*autoforegroundBlocksResult1));
  for (auto const& value : foregroundBlocks) {
    auto parsed = blockFromJson(value);
    if (!parsed) return std::unexpected(std::move(parsed.error()));
    result.m_foregroundBlocks.push_back(std::move(*parsed));
  }
  auto autoobjectsResult1 = store.getArray("objects");
  if (!autoobjectsResult1) return std::unexpected(std::move(autoobjectsResult1.error()));
  auto objects = std::move((*autoobjectsResult1));
  for (auto const& value : objects) {
    auto parsed = objectFromJson(value);
    if (!parsed) return std::unexpected(std::move(parsed.error()));
    result.m_objects.push_back(std::move(*parsed));
  }
  auto autoflaggedBlocksResult1 = store.getObject("flaggedBlocks");
  if (!autoflaggedBlocksResult1) return std::unexpected(std::move(autoflaggedBlocksResult1.error()));
  auto flaggedBlocks = std::move((*autoflaggedBlocksResult1));
  for (auto const& pair : flaggedBlocks) {
    auto values = pair.second.toArray();
    if (!values) return std::unexpected(std::move(values.error()));
    auto& blocks = result.m_flaggedBlocks[pair.first];
    for (auto const& value : *values) {
      auto position = jsonToVec2I(value);
      if (!position) return std::unexpected(std::move(position.error()));
      blocks.push_back(*position);
    }
  }
  return result;
}

std::expected<Json, StarException> WorldStructure::configValue(String const& name) const {
  return m_config.get(name, Json());
}

auto WorldStructure::backgroundOverlays() const -> List<Overlay> const & {
  return m_backgroundOverlays;
}

auto WorldStructure::foregroundOverlays() const -> List<Overlay> const & {
  return m_foregroundOverlays;
}

auto WorldStructure::backgroundBlocks() const -> List<Block> const & {
  return m_backgroundBlocks;
}

auto WorldStructure::foregroundBlocks() const -> List<Block> const & {
  return m_foregroundBlocks;
}

auto WorldStructure::objects() const -> List<Object> const & {
  return m_objects;
}

auto WorldStructure::flaggedBlocks(String const& flag) const -> List<Vec2I> {
  return m_flaggedBlocks.value(flag);
}

RectI WorldStructure::region() const {
  return m_region;
}

Vec2I WorldStructure::anchorPosition() const {
  return m_anchorPosition;
}

void WorldStructure::setAnchorPosition(Vec2I const& anchorPosition) {
  translate(anchorPosition - m_anchorPosition);
}

void WorldStructure::translate(Vec2I const& distance) {
  if (!m_region.isNull())
    m_region.translate(distance);

  m_anchorPosition += distance;

  for (auto& bg : m_backgroundOverlays)
    bg.min += Vec2F(distance);

  for (auto& fg : m_foregroundOverlays)
    fg.min += Vec2F(distance);

  for (auto& b : m_backgroundBlocks)
    b.position += distance;

  for (auto& b : m_foregroundBlocks)
    b.position += distance;

  for (auto& object : m_objects)
    object.position += distance;

  for (auto&& flagPair : m_flaggedBlocks) {
    for (auto& pos : flagPair.second)
      pos += distance;
  }
}

std::expected<Json, StarException> WorldStructure::store() const {
  auto overlayToJson = [](Overlay const& o) -> Json {
    return JsonObject{{"min", jsonFromVec2F(o.min)}, {"image", o.image}, {"fullbright", o.fullbright}};
  };

  auto blockToJson = [](Block const& b) -> Json {
    return JsonObject{
      {"position", jsonFromVec2I(b.position)},
      {"materialId", b.materialId},
      {"residual", b.residual},
      {"materialColor", b.materialColor},
      {"materialHue", b.materialHue},
      {"modId", b.materialMod}

    };
  };

  auto objectToJson = [](Object const& o) -> std::expected<Json, StarException> {
    auto direction = enumName<DirectionNames>(o.direction);
    if (!direction)
      return std::unexpected(std::move(direction.error()));
    return JsonObject{
      {"position", jsonFromVec2I(o.position)},
      {"name", o.name},
      {"direction", String(*direction)},
      {"parameters", o.parameters},
      {"residual", o.residual},
    };
  };

  JsonArray objects;
  for (auto const& object : m_objects) {
    auto stored = objectToJson(object);
    if (!stored) return std::unexpected(std::move(stored.error()));
    objects.push_back(std::move(*stored));
  }
  return JsonObject{{"region", jsonFromRectI(m_region)},
      {"anchorPosition", jsonFromVec2I(m_anchorPosition)},
      {"config", m_config},
      {"backgroundOverlays", m_backgroundOverlays.transformed(overlayToJson)},
      {"foregroundOverlays", m_foregroundOverlays.transformed(overlayToJson)},
      {"backgroundBlocks", m_backgroundBlocks.transformed(blockToJson)},
      {"foregroundBlocks", m_foregroundBlocks.transformed(blockToJson)},
      {"objects", std::move(objects)},
      {"flaggedBlocks",
          transform<JsonObject>(m_flaggedBlocks,
              [](std::pair<String, List<Vec2I>> const& p) {
                return std::pair<String, Json>(p.first, p.second.transformed(jsonFromVec2I));
              })}};
}

}
