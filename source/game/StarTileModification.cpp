import star.format;
import std;
import star.data_stream;
import star.game_types;
import star.list;
import star.bi_map;
import star.collision_block;
import star.data_stream_extra;
import star.tile_modification;

namespace Star {

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, PlaceMaterial& tileMaterialPlacement) {
  std::uint8_t layer;
  if (auto r = ds.read(layer); !r) return std::unexpected(std::move(r.error()));
  if (layer > 1) {
    tileMaterialPlacement.layer = TileLayer::Foreground;
    tileMaterialPlacement.collisionOverride = static_cast<TileCollisionOverride>(layer - 1);
  }
  else {
    tileMaterialPlacement.layer = static_cast<TileLayer>(layer);
    tileMaterialPlacement.collisionOverride = TileCollisionOverride::None;
  }
  if (auto r = ds.read(tileMaterialPlacement.material); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(tileMaterialPlacement.materialHueShift); !r) return std::unexpected(std::move(r.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, PlaceMaterial const& tileMaterialPlacement) {
  if (tileMaterialPlacement.collisionOverride != TileCollisionOverride::None
    && tileMaterialPlacement.layer == TileLayer::Foreground) {
    std::uint8_t layer = static_cast<std::uint8_t>(tileMaterialPlacement.collisionOverride);
    if (auto r = ds.write(++layer); !r) return std::unexpected(std::move(r.error()));
  } else {
    if (auto r = ds.write(tileMaterialPlacement.layer); !r) return std::unexpected(std::move(r.error()));
  }
  if (auto r = ds.write(tileMaterialPlacement.material); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(tileMaterialPlacement.materialHueShift); !r) return std::unexpected(std::move(r.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, PlaceMod& tileModPlacement) {
  if (auto r = ds.read(tileModPlacement.layer); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(tileModPlacement.mod); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(tileModPlacement.modHueShift); !r) return std::unexpected(std::move(r.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, PlaceMod const& tileModPlacement) {
  if (auto r = ds.write(tileModPlacement.layer); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(tileModPlacement.mod); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(tileModPlacement.modHueShift); !r) return std::unexpected(std::move(r.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, PlaceMaterialColor& tileMaterialColorPlacement) {
  if (auto r = ds.read(tileMaterialColorPlacement.layer); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(tileMaterialColorPlacement.color); !r) return std::unexpected(std::move(r.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, PlaceMaterialColor const& tileMaterialColorPlacement) {
  if (auto r = ds.write(tileMaterialColorPlacement.layer); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(tileMaterialColorPlacement.color); !r) return std::unexpected(std::move(r.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, PlaceLiquid& tileLiquidPlacement) {
  if (auto r = ds.read(tileLiquidPlacement.liquid); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(tileLiquidPlacement.liquidLevel); !r) return std::unexpected(std::move(r.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, PlaceLiquid const& tileLiquidPlacement) {
  if (auto r = ds.write(tileLiquidPlacement.liquid); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(tileLiquidPlacement.liquidLevel); !r) return std::unexpected(std::move(r.error()));
  return std::ref(ds);
}

}
