module;
#include <stddef.h>
import star.format;
import star.xx_hash;
import star.json;
import star.list;
import star.bi_map;
import star.multi_array;
import std;
import star.game_types;
import star.material_types;
import star.vector;
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

namespace Star {

struct WorldTile {
  WorldTile();

  // Copy constructor and operator= do not preserve collision cache.
  WorldTile(WorldTile const& worldTile);
  WorldTile& operator=(WorldTile const& worldTile);

  MaterialId material(TileLayer layer) const;
  ModId mod(TileLayer layer) const;
  MaterialColorVariant materialColor(TileLayer layer) const;
  CollisionKind getCollision() const;
  std::tuple<MaterialId, MaterialHue, MaterialColorVariant> materialAndColor(TileLayer layer) const;
  bool isConnectable(TileLayer layer, bool materialOnly) const;
  bool isColliding(CollisionSet const& collisionSet) const;

  MaterialId foreground;
  MaterialHue foregroundHueShift;
  ModId foregroundMod;
  MaterialHue foregroundModHueShift;
  MaterialColorVariant foregroundColorVariant;

  MaterialId background;
  MaterialHue backgroundHueShift;
  ModId backgroundMod;
  MaterialHue backgroundModHueShift;
  MaterialColorVariant backgroundColorVariant;

  // Same values as WorldTile::WorldTile(), for the paths that do not construct
  // a tile through it (the members used to be indeterminate there).
  CollisionKind collision = CollisionKind::Null;

  bool collisionCacheDirty = true;
  StaticList<CollisionBlock, CollisionGenerator::MaximumCollisionsPerSpace> collisionCache;

  BiomeIndex blockBiomeIndex;
  BiomeIndex environmentBiomeIndex;

  bool biomeTransition = false;

  TileDamageStatus foregroundDamage;
  TileDamageStatus backgroundDamage;

  // If block is part of a dungeon then that affects spawns/drops,
  // as well as governing block protection
  DungeonId dungeonId;
};

struct ServerTile : public WorldTile {
  static VersionNumber const CurrentSerializationVersion;

  ServerTile();

  ServerTile(ServerTile const& serverTile);
  ServerTile& operator=(ServerTile const& serverTile);

  bool isColliding(CollisionSet const& collisionSet) const;

  [[nodiscard]]
  std::expected<void, StarException> write(DataStream& ds) const;
  [[nodiscard]]
  std::expected<void, StarException> read(DataStream& ds, VersionNumber serializationVersion);

  // Updates collision, clears cache, and if the collision kind does not
  // support liquid destroys it.
  bool updateCollision(CollisionKind kind);
  // Used for setting the second collision kind calculated by object material spaces.
  bool updateObjectCollision(CollisionKind kind);

  // Calculates the actually-used collision kind based on the tile and object collision kinds.
  CollisionKind getCollision() const;

  LiquidStore liquid;

  // If set, a plant or object is rooted to the tile and tile damage
  // should be redirected to this position
  std::optional<Vec2I> rootSource;

  // Do not serialize - calculated at runtime
  CollisionKind objectCollision;
};
typedef TileSectorArray<ServerTile, WorldSectorSize> ServerTileSectorArray;

struct ClientTile : public WorldTile {
  ClientTile();

  ClientTile(ClientTile const& clientTile);
  ClientTile& operator=(ClientTile const& clientTile);

  bool backgroundLightTransparent;
  bool foregroundLightTransparent;

  LiquidLevel liquid;

  float gravity;
};
typedef TileSectorArray<ClientTile, WorldSectorSize> ClientTileSectorArray;

// Tile structure to transfer all data from client to server
struct NetTile {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, NetTile& value);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, NetTile const& value);
  NetTile();

  MaterialId background;
  MaterialHue backgroundHueShift;
  MaterialColorVariant backgroundColorVariant;
  ModId backgroundMod;
  MaterialHue backgroundModHueShift;
  MaterialId foreground;
  MaterialHue foregroundHueShift;
  MaterialColorVariant foregroundColorVariant;
  ModId foregroundMod;
  MaterialHue foregroundModHueShift;
  CollisionKind collision = CollisionKind::Null;
  BiomeIndex blockBiomeIndex;
  BiomeIndex environmentBiomeIndex;
  LiquidNetUpdate liquid;
  DungeonId dungeonId;
};
[[nodiscard]]
std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, NetTile& tile);
[[nodiscard]]
std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, NetTile const& tile);

// For storing predicted tile state.
struct PredictedTile {
  std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds> time;
  std::optional<MaterialId> background;
  std::optional<MaterialHue> backgroundHueShift;
  std::optional<MaterialColorVariant> backgroundColorVariant;
  std::optional<ModId> backgroundMod;
  std::optional<MaterialHue> backgroundModHueShift;
  std::optional<MaterialId> foreground;
  std::optional<MaterialHue> foregroundHueShift;
  std::optional<MaterialColorVariant> foregroundColorVariant;
  std::optional<ModId> foregroundMod;
  std::optional<MaterialHue> foregroundModHueShift;
  std::optional<LiquidLevel> liquid;
  std::optional<CollisionKind> collision;

  operator bool() const;
  template <typename Tile>
  void apply(Tile& tile) {
    if (foreground) tile.foreground = *foreground;
    if (foregroundMod) tile.foregroundMod = *foregroundMod;
    if (foregroundHueShift) tile.foregroundHueShift = *foregroundHueShift;
    if (foregroundModHueShift) tile.foregroundModHueShift = *foregroundModHueShift;

    if (background) tile.background = *background;
    if (backgroundMod) tile.backgroundMod = *backgroundMod;
    if (backgroundHueShift) tile.backgroundHueShift = *backgroundHueShift;
    if (backgroundModHueShift) tile.backgroundModHueShift = *backgroundModHueShift;
  }
};

// Just the parts of a tile that are used to render.  The members here are laid
// out specifically to avoid padding bytes so that a fast path can be taken
// when hashing for chunk render caching.
struct RenderTile {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, RenderTile& value);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, RenderTile const& value);
  MaterialId foreground;
  ModId foregroundMod;

  MaterialId background;
  ModId backgroundMod;

  MaterialHue foregroundHueShift;
  MaterialHue foregroundModHueShift;
  MaterialColorVariant foregroundColorVariant;
  TileDamageType foregroundDamageType;
  std::uint8_t foregroundDamageLevel;

  MaterialHue backgroundHueShift;
  MaterialHue backgroundModHueShift;
  MaterialColorVariant backgroundColorVariant;
  TileDamageType backgroundDamageType;
  std::uint8_t backgroundDamageLevel;

  LiquidId liquidId;
  std::uint8_t liquidLevel;

  template <typename Hasher>
  void hashPushTerrain(Hasher& hasher) const;

  template <typename Hasher>
  void hashPushLiquid(Hasher& hasher) const;
};
[[nodiscard]]
std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, RenderTile& tile);
[[nodiscard]]
std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, RenderTile const& tile);

typedef MultiArray<RenderTile, 2> RenderTileArray;

inline WorldTile::WorldTile()
  : foreground(NullMaterialId),
    foregroundHueShift(),
    foregroundMod(NoModId),
    foregroundModHueShift(),
    foregroundColorVariant(DefaultMaterialColorVariant),
    background(NullMaterialId),
    backgroundHueShift(),
    backgroundMod(NoModId),
    backgroundModHueShift(),
    backgroundColorVariant(DefaultMaterialColorVariant),
    collision(CollisionKind::Null),
    collisionCacheDirty(true),
    blockBiomeIndex(),
    environmentBiomeIndex(),
    dungeonId(NoDungeonId) {}

inline WorldTile::WorldTile(WorldTile const& worldTile) {
  *this = worldTile;
}

inline WorldTile& WorldTile::operator=(WorldTile const& worldTile) {
  foreground = worldTile.foreground;
  foregroundHueShift = worldTile.foregroundHueShift;
  foregroundMod = worldTile.foregroundMod;
  foregroundModHueShift = worldTile.foregroundModHueShift;
  foregroundColorVariant = worldTile.foregroundColorVariant;

  background = worldTile.background;
  backgroundHueShift = worldTile.backgroundHueShift;
  backgroundMod = worldTile.backgroundMod;
  backgroundModHueShift = worldTile.backgroundModHueShift;
  backgroundColorVariant = worldTile.backgroundColorVariant;

  // Don't bother copying collision cache
  collisionCacheDirty = true;

  collision = worldTile.collision;
  blockBiomeIndex = worldTile.blockBiomeIndex;
  environmentBiomeIndex = worldTile.environmentBiomeIndex;

  foregroundDamage = worldTile.foregroundDamage;
  backgroundDamage = worldTile.backgroundDamage;

  dungeonId = worldTile.dungeonId;

  return *this;
}

inline MaterialId WorldTile::material(TileLayer layer) const {
  if (layer == TileLayer::Foreground)
    return foreground;
  else
    return background;
}

inline ModId WorldTile::mod(TileLayer layer) const {
  if (layer == TileLayer::Foreground)
    return foregroundMod;
  else
    return backgroundMod;
}

inline MaterialColorVariant WorldTile::materialColor(TileLayer layer) const {
  if (layer == TileLayer::Foreground)
    return foregroundColorVariant;
  else
    return backgroundColorVariant;
}

inline CollisionKind WorldTile::getCollision() const {
  return collision;
}

inline std::tuple<MaterialId, MaterialHue, MaterialColorVariant> WorldTile::materialAndColor(TileLayer layer) const {
  if (layer == TileLayer::Foreground)
    return std::tuple<MaterialId, MaterialHue, MaterialColorVariant>{
        foreground, foregroundHueShift, foregroundColorVariant};
  else
    return std::tuple<MaterialId, MaterialHue, MaterialColorVariant>{
        background, backgroundHueShift, backgroundColorVariant};
}

inline ClientTile::ClientTile() : backgroundLightTransparent(true), foregroundLightTransparent(true), gravity() {}

inline ClientTile::ClientTile(ClientTile const& clientTile) : WorldTile() {
  *this = clientTile;
}

inline ClientTile& ClientTile::operator=(ClientTile const& clientTile) {
  WorldTile::operator=(clientTile);

  backgroundLightTransparent = clientTile.backgroundLightTransparent;
  foregroundLightTransparent = clientTile.foregroundLightTransparent;
  liquid = clientTile.liquid;
  gravity = clientTile.gravity;

  return *this;
}

inline NetTile::NetTile()
  : background(NullMaterialId),
    backgroundHueShift(),
    backgroundColorVariant(),
    backgroundMod(NoModId),
    backgroundModHueShift(),
    foreground(NullMaterialId),
    foregroundHueShift(),
    foregroundColorVariant(),
    foregroundMod(NoModId),
    foregroundModHueShift(),
    collision(),
    blockBiomeIndex(),
    environmentBiomeIndex(),
    dungeonId(NoDungeonId) {}

template <typename Hasher>
inline void RenderTile::hashPushTerrain(Hasher& hasher) const {
  // Do the fast path hash if the last (terrain relevant) field is at byte 20, because that means
  // there are no padding bytes between any field and we can simply pass the
  // entire tile as one block of memory.
  static std::size_t const TerrainEndOffset = offsetof(RenderTile, liquidId);
  static std::size_t const TotalTerrainSize =
    sizeof(MaterialId) * 2 + sizeof(ModId) * 2 + sizeof(MaterialHue) * 4 + sizeof(MaterialColorVariant) * 2 * sizeof(TileDamageType) * 2 + 2;

#ifndef NDEBUG
  static bool const FastHash = false;
#else
  static bool const FastHash = TerrainEndOffset == TotalTerrainSize;
#endif

  if (FastHash) {
    hasher.push(std::span(reinterpret_cast<char const*>(this), TerrainEndOffset));
  } else {
    char buffer[TotalTerrainSize];
    std::size_t bufferSize = 0;

    auto hashTilePart = [&]<typename T>(T const& value) {
      std::memcpy(buffer + bufferSize, std::addressof(value), sizeof(value));
      bufferSize += sizeof(value);
    };

    hashTilePart(foreground);
    hashTilePart(foregroundMod);

    hashTilePart(background);
    hashTilePart(backgroundMod);

    hashTilePart(foregroundHueShift);
    hashTilePart(foregroundModHueShift);
    hashTilePart(foregroundColorVariant);
    hashTilePart(foregroundDamageType);
    hashTilePart(foregroundDamageLevel);

    hashTilePart(backgroundHueShift);
    hashTilePart(backgroundModHueShift);
    hashTilePart(backgroundColorVariant);
    hashTilePart(backgroundDamageType);
    hashTilePart(backgroundDamageLevel);

    hasher.push(std::span<char const>(buffer));
  }
}

template <typename Hasher>
inline void RenderTile::hashPushLiquid(Hasher& hasher) const {
  char buffer[sizeof(liquidLevel) + sizeof(liquidId)];

  std::memcpy(buffer, &liquidLevel, sizeof(liquidLevel));
  std::memcpy(buffer + sizeof(liquidLevel), &liquidId, sizeof(liquidId));

  hasher.push(std::span<char const>(buffer));
}

}

export module star.world_tiles;

export namespace Star {
  using ::Star::WorldTile;
  using ::Star::ServerTile;
  using ::Star::ServerTileSectorArray;
  using ::Star::ClientTile;
  using ::Star::ClientTileSectorArray;
  using ::Star::NetTile;
  using ::Star::PredictedTile;
  using ::Star::RenderTile;
  using ::Star::RenderTileArray;
}
