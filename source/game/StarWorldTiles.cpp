import star.xx_hash;
import star.json;
import star.data_stream_extra;
import star.list;
import star.bi_map;
import std;
import star.game_types;
import star.material_types;
import star.set;
import star.thread;
import star.worker_pool;

import star.sector_array_2d;
import star.random;
import star.perlin;
import star.weighted_pool;
import star.color;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.directives;
import star.asset_path;
import star.format;
import star.version;

import star.collision_block;
import star.liquid_types;
import star.tile_damage;
import star.tile_sector_array;
import star.animation;
import star.particle;
import star.weather_types;
import star.celestial_coordinate;
import star.sky_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.world_layout;
import star.collision_generator;
import star.world_tiles;

namespace Star {

bool WorldTile::isConnectable(TileLayer layer, bool materialOnly) const {
  if (layer == TileLayer::Foreground) {
    if (isConnectableMaterial(foreground))
      return true;
  } else {
    if (isConnectableMaterial(background))
      return true;
  }
  if (materialOnly)
    return false;

  if (layer == TileLayer::Foreground)
    return collision == CollisionKind::Block || collision == CollisionKind::Platform;
  else
    return false;
}

bool WorldTile::isColliding(CollisionSet const& collisionSet) const {
  return Star::isColliding(collision, collisionSet);
}

VersionNumber const ServerTile::CurrentSerializationVersion = 418;

ServerTile::ServerTile() : objectCollision(CollisionKind::None) {}

ServerTile::ServerTile(ServerTile const& serverTile) : WorldTile() {
  *this = serverTile;
}

bool ServerTile::isColliding(CollisionSet const& collisionSet) const {
  return Star::isColliding(getCollision(), collisionSet);
}

ServerTile& ServerTile::operator=(ServerTile const& serverTile) = default;

std::expected<void, StarException> ServerTile::write(DataStream& ds) const {
  auto result = ds << foreground << foregroundHueShift << foregroundColorVariant
      << foregroundMod << foregroundModHueShift << background << backgroundHueShift
      << backgroundColorVariant << backgroundMod << backgroundModHueShift
      << liquid.liquid << liquid.level << liquid.pressure << liquid.source
      << collision << dungeonId << blockBiomeIndex << environmentBiomeIndex
      << biomeTransition << rootSource;
  if (!result)
    return std::unexpected(std::move(result.error()));
  return {};
}

std::expected<void, StarException> ServerTile::read(DataStream& ds, VersionNumber serializationVersion) {
  if (serializationVersion < 416 || serializationVersion > CurrentSerializationVersion)
    return std::unexpected(StarException::format("Cannot read ServerTile - serialization version {} incompatible with current version {}\n", serializationVersion, CurrentSerializationVersion));

  auto result = ds >> foreground >> foregroundHueShift >> foregroundColorVariant
      >> foregroundMod >> foregroundModHueShift >> background >> backgroundHueShift
      >> backgroundColorVariant >> backgroundMod >> backgroundModHueShift
      >> liquid.liquid >> liquid.level >> liquid.pressure >> liquid.source
      >> collision >> dungeonId >> blockBiomeIndex >> environmentBiomeIndex;
  if (!result)
    return std::unexpected(std::move(result.error()));
  if (serializationVersion < 417) {
    biomeTransition = false;
  } else {
    auto transition = ds.read(biomeTransition);
    if (!transition)
      return std::unexpected(std::move(transition.error()));
  }
  if (serializationVersion < 418) {
    auto legacyRoot = ds.read_bytes(1);
    if (!legacyRoot)
      return std::unexpected(std::move(legacyRoot.error()));
    rootSource = {};
  } else {
    auto source = ds.read(rootSource);
    if (!source)
      return std::unexpected(std::move(source.error()));
  }
  collisionCacheDirty = true;
  return {};
}

bool ServerTile::updateCollision(CollisionKind kind) {
  if (collision != kind) {
    collision = kind;
    collisionCacheDirty = true;
    collisionCache.clear();
    return true;
  }
  return false;
}

bool ServerTile::updateObjectCollision(CollisionKind kind) {
  if (objectCollision != kind) {
    objectCollision = kind;
    collisionCacheDirty = true;
    collisionCache.clear();
    return true;
  }
  return false;
}

CollisionKind ServerTile::getCollision() const {
  CollisionKind kind = collision;
  if (objectCollision != CollisionKind::None
      && (objectCollision != CollisionKind::Platform || kind == CollisionKind::None)) {
    kind = objectCollision;
  }
  return kind;
}

PredictedTile::operator bool() const {
  return
     background
  || backgroundHueShift
  || backgroundColorVariant
  || backgroundMod
  || backgroundModHueShift
  || foreground
  || foregroundHueShift
  || foregroundColorVariant
  || foregroundMod
  || foregroundModHueShift
  || liquid
  || collision;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, NetTile& tile) {
  auto background = ds.read(tile.background);
  if (!background)
    return std::unexpected(std::move(background.error()));
  if (tile.background == 0) {
    tile.background = EmptyMaterialId;
    tile.backgroundHueShift = MaterialHue();
    tile.backgroundColorVariant = DefaultMaterialColorVariant;
    tile.backgroundMod = NoModId;
    tile.backgroundModHueShift = MaterialHue();
  } else {
    auto appearance = ds >> tile.backgroundHueShift >> tile.backgroundColorVariant >> tile.backgroundMod;
    if (!appearance)
      return std::unexpected(std::move(appearance.error()));
    if (tile.backgroundMod == 0) {
      tile.backgroundMod = NoModId;
      tile.backgroundModHueShift = MaterialHue();
    } else {
      auto hue = ds.read(tile.backgroundModHueShift);
      if (!hue)
        return std::unexpected(std::move(hue.error()));
    }
  }
  auto foreground = ds.read(tile.foreground);
  if (!foreground)
    return std::unexpected(std::move(foreground.error()));
  if (tile.foreground == 0) {
    tile.foreground = EmptyMaterialId;
    tile.foregroundHueShift = MaterialHue();
    tile.foregroundColorVariant = DefaultMaterialColorVariant;
    tile.foregroundMod = NoModId;
    tile.foregroundModHueShift = MaterialHue();
  } else {
    auto appearance = ds >> tile.foregroundHueShift >> tile.foregroundColorVariant >> tile.foregroundMod;
    if (!appearance)
      return std::unexpected(std::move(appearance.error()));
    if (tile.foregroundMod == 0) {
      tile.foregroundMod = NoModId;
      tile.foregroundModHueShift = MaterialHue();
    } else {
      auto hue = ds.read(tile.foregroundModHueShift);
      if (!hue)
        return std::unexpected(std::move(hue.error()));
    }
  }
  auto state = ds >> tile.collision >> tile.blockBiomeIndex >> tile.environmentBiomeIndex >> tile.liquid.liquid;
  if (!state)
    return std::unexpected(std::move(state.error()));
  if (tile.liquid.liquid != EmptyLiquidId) {
    auto level = ds.read(tile.liquid.level);
    if (!level)
      return std::unexpected(std::move(level.error()));
  } else {
    tile.liquid.level = 0.0f;
  }
  auto dungeon = ds.vuread(tile.dungeonId);
  if (!dungeon)
    return std::unexpected(std::move(dungeon.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, NetTile const& tile) {
  if (tile.background == EmptyMaterialId) {
    auto empty = ds.cwrite<MaterialId>(0);
    if (!empty)
      return std::unexpected(std::move(empty.error()));
  } else {
    auto appearance = ds << tile.background << tile.backgroundHueShift << tile.backgroundColorVariant;
    if (!appearance)
      return std::unexpected(std::move(appearance.error()));
    if (tile.backgroundMod == NoModId) {
      auto empty = ds.write<ModId>(0);
      if (!empty)
        return std::unexpected(std::move(empty.error()));
    } else {
      auto mod = ds << tile.backgroundMod << tile.backgroundModHueShift;
      if (!mod)
        return std::unexpected(std::move(mod.error()));
    }
  }
  if (tile.foreground == EmptyMaterialId) {
    auto empty = ds.cwrite<MaterialId>(0);
    if (!empty)
      return std::unexpected(std::move(empty.error()));
  } else {
    auto appearance = ds << tile.foreground << tile.foregroundHueShift << tile.foregroundColorVariant;
    if (!appearance)
      return std::unexpected(std::move(appearance.error()));
    if (tile.foregroundMod == NoModId) {
      auto empty = ds.write<ModId>(0);
      if (!empty)
        return std::unexpected(std::move(empty.error()));
    } else {
      auto mod = ds << tile.foregroundMod << tile.foregroundModHueShift;
      if (!mod)
        return std::unexpected(std::move(mod.error()));
    }
  }
  auto state = ds << tile.collision << tile.blockBiomeIndex << tile.environmentBiomeIndex << tile.liquid.liquid;
  if (!state)
    return std::unexpected(std::move(state.error()));
  if (tile.liquid.liquid != EmptyLiquidId) {
    auto level = ds.write(tile.liquid.level);
    if (!level)
      return std::unexpected(std::move(level.error()));
  }
  auto dungeon = ds.vuwrite(tile.dungeonId);
  if (!dungeon)
    return std::unexpected(std::move(dungeon.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, RenderTile& tile) {
  return ds >> tile.foreground >> tile.foregroundHueShift >> tile.foregroundMod
      >> tile.foregroundModHueShift >> tile.foregroundColorVariant
      >> tile.foregroundDamageLevel >> tile.foregroundDamageType
      >> tile.background >> tile.backgroundHueShift >> tile.backgroundMod
      >> tile.backgroundModHueShift >> tile.backgroundColorVariant
      >> tile.backgroundDamageLevel >> tile.backgroundDamageType
      >> tile.liquidId >> tile.liquidLevel;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, RenderTile const& tile) {
  return ds << tile.foreground << tile.foregroundHueShift << tile.foregroundMod
      << tile.foregroundModHueShift << tile.foregroundColorVariant
      << tile.foregroundDamageLevel << tile.foregroundDamageType
      << tile.background << tile.backgroundHueShift << tile.backgroundMod
      << tile.backgroundModHueShift << tile.backgroundColorVariant
      << tile.backgroundDamageLevel << tile.backgroundDamageType
      << tile.liquidId << tile.liquidLevel;
}

}
