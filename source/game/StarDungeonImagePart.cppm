module;
import star.json;
import star.json_extra;
import star.data_stream;
import star.bi_map;
import star.vector;
import star.game_types;
import star.random;
import star.set;
import star.thread;
import star.ordered_map;
import star.lru_cache;
import star.io_device;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.optional;
import star.string;
import star.map;
import star.format;
import star.version;
import star.casting;
import star.image;


import star.item_descriptor;
import star.world_geometry;
import star.dungeon_generator;
import star.asset_source;
import star.assets;
import star.configuration;
import star.root;

export module star.dungeon_image_part;

export namespace Star::Dungeon {

class ImageTileset;

class ImagePartReader : public PartReader {
  Root& m_root;
public:
  ImagePartReader(Root& root, std::shared_ptr<ImageTileset const> tileset)
      : m_root(root), m_tileset(std::move(tileset)) {}

  [[nodiscard]] std::expected<void, StarException> readAsset(String const& asset) override;
  Vec2U size() const override;

  [[nodiscard]] std::expected<void, StarException> forEachTile(TileCallback const& callback) const override;
  [[nodiscard]] std::expected<void, StarException> forEachTileAt(Vec2I pos,
                                                                 TileCallback const& callback) const override;

private:
  List<std::pair<String, std::shared_ptr<Image const>>> m_images;
  std::shared_ptr<ImageTileset const> m_tileset;
};

class ImageTileset {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<ImageTileset>, StarException> create(Root& root, Json const& tileset);

  [[nodiscard]] std::expected<std::optional<std::reference_wrapper<Tile const>>, StarException> getTile(Vec4B color) const;

private:
  ImageTileset() = default;
  unsigned colorAsInt(Vec4B color) const;

  Map<unsigned, Tile> m_tiles;
};

}

namespace Star {

namespace Dungeon {

  [[nodiscard]] std::expected<void, StarException> ImagePartReader::readAsset(String const& asset) {
    auto assets = m_root.assets();
    if (!assets) return std::unexpected(std::move(assets.error()));
    auto image = (*assets)->image(asset);
    if (!image) return std::unexpected(std::move(image.error()));
    m_images.push_back(std::make_pair(asset, std::move(*image)));
    return {};
  }

  Vec2U ImagePartReader::size() const {
    if (m_images.empty())
      return Vec2U{0, 0};
    return m_images[0].second->size();
  }

  [[nodiscard]] std::expected<void, StarException> ImagePartReader::forEachTile(TileCallback const& callback) const {
    for (auto const& entry : m_images) {
      auto const& image = entry.second;
      for (std::size_t y = 0; y < image->height(); y++) {
        for (std::size_t x = 0; x < image->width(); x++) {
          auto color = image->get(x, y);
          if (!color) return std::unexpected(std::move(color.error()));
          auto tile = m_tileset->getTile(*color);
          if (!tile) return std::unexpected(std::move(tile.error()));
          if (!*tile)
            return std::unexpected(StarException::format("Dungeon image {} uses unknown tile color: #{:02x}{:02x}{:02x}{:02x}",
                entry.first, (*color)[0], (*color)[1], (*color)[2], (*color)[3]));
          if (!callback) return std::unexpected(DungeonException("Empty dungeon tile callback"));
          auto result = callback(Vec2I(x, y), (*tile)->get());
          if (!result) return std::unexpected(std::move(result.error()));
          if (*result) return {};
        }
      }
    }
    return {};
  }

  [[nodiscard]] std::expected<void, StarException> ImagePartReader::forEachTileAt(Vec2I pos, TileCallback const& callback) const {
    for (auto const& entry : m_images) {
      auto color = entry.second->get(pos.x(), pos.y());
      if (!color) return std::unexpected(std::move(color.error()));
      auto tile = m_tileset->getTile(*color);
      if (!tile) return std::unexpected(std::move(tile.error()));
      if (!*tile)
        return std::unexpected(StarException::format("Dungeon image {} uses unknown tile color: #{:02x}{:02x}{:02x}{:02x}",
            entry.first, (*color)[0], (*color)[1], (*color)[2], (*color)[3]));
      if (!callback) return std::unexpected(DungeonException("Empty dungeon tile callback"));
      auto result = callback(pos, (*tile)->get());
      if (!result) return std::unexpected(std::move(result.error()));
      if (*result) return {};
    }
    return {};
  }

  String connectorColorValue(Vec4B const& color) {
    return strf("{},{},{},{}", color[0], color[1], color[2], color[3]);
  }

  [[nodiscard]] std::expected<Tile, StarException> variantMapToTile(Root& root, JsonObject const& tile) {
    Tile result;
    if (auto brush = tile.maybe("brush")) {
      auto brushes = Brush::readBrushes(root, *brush);
      if (!brushes) return std::unexpected(std::move(brushes.error()));
      result.brushes = std::move(*brushes);
    }
    if (auto rule = tile.maybe("rules")) {
      auto rules = Rule::readRules(root, *rule);
      if (!rules) return std::unexpected(std::move(rules.error()));
      result.rules = std::move(*rules);
    }
    if (auto connectorDef = tile.maybe("connector")) {
      auto enabled = connectorDef->toBool();
      if (!enabled) return std::unexpected(std::move(enabled.error()));
      if (*enabled) {
        TileConnector connector;
        connector.forwardOnly = false;
        if (auto forward = tile.maybe("connectForwardOnly")) {
          auto value = forward->toBool();
          if (!value) return std::unexpected(std::move(value.error()));
          connector.forwardOnly = *value;
        }
        auto value = tile.get("value");
        if (!value) return std::unexpected(std::move(value.error()));
        Json connectorValue = value->get();
        if (auto customValue = tile.maybe("connector-value"))
          connectorValue = *customValue;
        if (connectorValue.isType(Json::Type::String)) {
          auto text = connectorValue.toString();
          if (!text) return std::unexpected(std::move(text.error()));
          connector.value = std::move(*text);
        } else {
          auto color = jsonToVec4B(connectorValue);
          if (!color) return std::unexpected(std::move(color.error()));
          connector.value = connectorColorValue(*color);
        }
        if (auto direction = tile.maybe("direction")) {
          auto text = direction->toString();
          if (!text) return std::unexpected(std::move(text.error()));
          auto parsed = enumValue<DungeonDirectionNames>(*text);
          if (!parsed) return std::unexpected(std::move(parsed.error()));
          connector.direction = *parsed;
        }
        result.connector = std::move(connector);
      }
    }
    return result;
  }

  [[nodiscard]] std::expected<std::shared_ptr<ImageTileset>, StarException> ImageTileset::create(Root& root, Json const& tileset) {
    auto result = std::shared_ptr<ImageTileset>(new ImageTileset);
    auto definitions = tileset.iterateArray();
    if (!definitions) return std::unexpected(std::move(definitions.error()));
    for (Json const& definition : *definitions) {
      auto value = definition.get("value");
      if (!value) return std::unexpected(std::move(value.error()));
      auto color = jsonToVec4B(*value);
      if (!color) return std::unexpected(std::move(color.error()));
      auto object = definition.toObject();
      if (!object) return std::unexpected(std::move(object.error()));
      auto tile = variantMapToTile(root, *object);
      if (!tile) return std::unexpected(std::move(tile.error()));
      result->m_tiles.insert(result->colorAsInt(*color), std::move(*tile));
    }
    return result;
  }

  [[nodiscard]] std::expected<std::optional<std::reference_wrapper<Tile const>>, StarException> ImageTileset::getTile(Vec4B color) const {
    if (color[3] == 0)
      color = Vec4B(255, 255, 255, 0);
    if (color[3] != 255)
      return std::nullopt;
    auto tile = m_tiles.get(colorAsInt(color));
    if (!tile) return std::unexpected(std::move(tile.error()));
    return std::optional{*tile};
  }

  unsigned ImageTileset::colorAsInt(Vec4B color) const {
    if ((color[3] != 0) && (color[3] != 255)) {
      if constexpr (Star::DebugEnabled) {
        Star::starAssertFailed((false));
      }
      return 0;
    }
    if (color[3] == 0)
      color = Vec4B(255, 255, 255, 0);
    return (static_cast<unsigned>(color[2]) << 16) | (static_cast<unsigned>(color[1]) << 8)
      | static_cast<unsigned>(color[0]);
  }
}

}
