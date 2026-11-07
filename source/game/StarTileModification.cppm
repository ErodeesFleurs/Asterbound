module;
import star.format;

import std;
import star.data_stream;
import star.vector;
import star.game_types;
import star.liquid_types;
import star.material_types;
import star.list;
import star.bi_map;
import star.collision_block;

namespace Star {

struct PlaceMaterial {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, PlaceMaterial& tileMaterialPlacement);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, PlaceMaterial const& tileMaterialPlacement);
  TileLayer layer;
  MaterialId material;
  // If the material hue shift is not set it will get the natural hue shift for
  // the environment.
  std::optional<MaterialHue> materialHueShift;
  TileCollisionOverride collisionOverride = TileCollisionOverride::None;
};
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, PlaceMaterial& tileMaterialPlacement);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, PlaceMaterial const& tileMaterialPlacement);

struct PlaceMod {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, PlaceMod& tileModPlacement);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, PlaceMod const& tileModPlacement);
  TileLayer layer;
  ModId mod;

  // If the mod hue shift is not set it will get the natural hue shift for the
  // environment.
  std::optional<MaterialHue> modHueShift;
};
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, PlaceMod& tileModPlacement);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, PlaceMod const& tileModPlacement);

struct PlaceMaterialColor {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, PlaceMaterialColor& tileMaterialColorPlacement);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, PlaceMaterialColor const& tileMaterialColorPlacement);
  TileLayer layer;
  MaterialColorVariant color;
};
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, PlaceMaterialColor& tileMaterialColorPlacement);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, PlaceMaterialColor const& tileMaterialColorPlacement);

struct PlaceLiquid {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, PlaceLiquid& tileLiquidPlacement);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, PlaceLiquid const& tileLiquidPlacement);
  LiquidId liquid;
  float liquidLevel;
};
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, PlaceLiquid& tileLiquidPlacement);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, PlaceLiquid const& tileLiquidPlacement);

using TileModification = std::variant<std::monostate, PlaceMaterial, PlaceMod, PlaceMaterialColor, PlaceLiquid>;
using TileModificationList = List<std::pair<Vec2I, TileModification>>;
}

export module star.tile_modification;

export namespace Star {
  using ::Star::PlaceMaterial;
  using ::Star::PlaceMod;
  using ::Star::PlaceMaterialColor;
  using ::Star::PlaceLiquid;
  using ::Star::TileModification;
  using ::Star::TileModificationList;
}
