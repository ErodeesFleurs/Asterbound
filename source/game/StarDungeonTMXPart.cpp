module;
import star.memory;
import star.json;
import star.json_extra;
import star.byte_array;
import star.data_stream_devices;

using ZSTD_CCtx = struct ZSTD_CCtx_s;
using ZSTD_DCtx = struct ZSTD_DCtx_s;
using ZSTD_DStream = ZSTD_DCtx;
using ZSTD_CStream = ZSTD_CCtx;
import star.zstd_compression;
import star.string;
import star.encode;
import star.ordered_map;
import star.bi_map;
import star.thread;
import star.io_device;
import star.list;
import star.rect;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.format;
import star.optional;
import star.version;
import star.vector;
import star.game_types;
import star.casting;
import star.lexical_cast;
import star.image;
import star.random;
import star.set;
import star.lru_cache;


import star.asset_source;
import star.assets;
import star.configuration;
import star.root;
import star.item_descriptor;
import star.world_geometry;
import star.dungeon_generator;

module star.dungeon_tmx_part;
import star.tileset_database;
import star.compression;
import star.object_database;



namespace Star::Dungeon {

std::expected<bool, StarException> TMXMap::forEachTile(TileCallback const& callback) const {
  for (auto const& layer : tileLayers()) {
    auto result = layer->forEachTile(*this, callback);
    if (!result) return std::unexpected(std::move(result.error()));
    if (*result) return true;
  }
  for (auto const& group : objectGroups()) {
    auto result = group->forEachTile(*this, callback);
    if (!result) return std::unexpected(std::move(result.error()));
    if (*result) return true;
  }
  return false;
}

std::expected<bool, StarException> TMXTileLayer::forEachTile(TMXMap const& map, TileCallback const& callback) const {
  for (int y = m_rect.yMin(); y <= m_rect.yMax(); ++y) {
    for (int x = m_rect.xMin(); x <= m_rect.xMax(); ++x) {
      auto tile = getTile(map.tilesets(), Vec2I(x, y));
      if (!tile) return std::unexpected(std::move(tile.error()));
      if (!callback) return std::unexpected(DungeonException("Empty dungeon tile callback"));
      auto result = callback(Vec2I(x, map.height() - 1 - y), tile->get());
      if (!result) return std::unexpected(std::move(result.error()));
      if (*result) return true;
    }
  }
  return false;
}

String TMXObjectGroup::name() const {
  return m_name;
}

std::expected<bool, StarException> TMXObjectGroup::forEachTile(TMXMap const& map, TileCallback const& callback) const {
  for (auto const& object : objects()) {
    auto result = object->forEachTile(map, callback);
    if (!result) return std::unexpected(std::move(result.error()));
    if (*result) return true;
  }
  return false;
}

std::expected<bool, StarException> TMXObject::forEachTile(TMXMap const& map, TileCallback const& callback) const {
  if (m_kind == ObjectKind::Stagehand) {
    auto cPos = Vec2I(rect().center()[0], std::ceilf(RectF(rect()).center()[1]));
    if (!callback) return std::unexpected(DungeonException("Empty dungeon tile callback"));
    return callback(Vec2I(cPos[0], map.height() - cPos[1]), tile());
  }
  if (m_kind == ObjectKind::Tile) {
    if (!callback) return std::unexpected(DungeonException("Empty dungeon tile callback"));
    return callback(Vec2I(pos().x(), map.height() - pos().y()), tile());
  }
  if (m_kind == ObjectKind::Rectangle) {
    for (int x = m_rect.min().x(); x < m_rect.max().x(); ++x) {
      for (int y = m_rect.min().y(); y < m_rect.max().y(); ++y) {
        if (!callback) return std::unexpected(DungeonException("Empty dungeon tile callback"));
        auto result = callback(Vec2I(x, map.height() - 1 - y), tile());
        if (!result) return std::unexpected(std::move(result.error()));
        if (*result) return true;
      }
    }
    return false;
  }
  if constexpr (Star::DebugEnabled)
    Star::starAssertFailed((m_kind == ObjectKind::Polyline));
  for (Vec2I point : m_polyline) {
    Vec2I position(m_rect.min().x() + point.x(), map.height() - 1 - m_rect.min().y() - point.y());
    if (!callback) return std::unexpected(DungeonException("Empty dungeon tile callback"));
    auto result = callback(position, tile());
    if (!result) return std::unexpected(std::move(result.error()));
    if (*result) return true;
  }
  return false;
}

std::expected<bool, StarException> TMXMap::forEachTileAt(Vec2I pos, TileCallback const& callback) const {
  for (auto const& layer : tileLayers()) {
    auto result = layer->forEachTileAt(pos, *this, callback);
    if (!result) return std::unexpected(std::move(result.error()));
    if (*result) return true;
  }
  for (auto const& group : objectGroups()) {
    auto result = group->forEachTileAt(pos, *this, callback);
    if (!result) return std::unexpected(std::move(result.error()));
    if (*result) return true;
  }
  return false;
}

std::expected<bool, StarException> TMXTileLayer::forEachTileAt(Vec2I pos, TMXMap const& map, TileCallback const& callback) const {
  Vec2I tilePos(pos.x(), map.height() - 1 - pos.y());
  if (!m_rect.contains(tilePos)) return false;
  auto tile = getTile(map.tilesets(), tilePos);
  if (!tile) return std::unexpected(std::move(tile.error()));
  if (!callback) return std::unexpected(DungeonException("Empty dungeon tile callback"));
  return callback(pos, tile->get());
}

std::expected<bool, StarException> TMXObjectGroup::forEachTileAt(Vec2I pos, TMXMap const& map, TileCallback const& callback) const {
  for (auto const& object : objects()) {
    auto result = object->forEachTileAt(pos, map, callback);
    if (!result) return std::unexpected(std::move(result.error()));
    if (*result) return true;
  }
  return false;
}

std::expected<bool, StarException> TMXObject::forEachTileAt(Vec2I pos, TMXMap const& map, TileCallback const& callback) const {
  if (m_kind == ObjectKind::Stagehand) {
    Vec2I cPos(rect().center()[0], std::ceilf(RectF(rect()).center()[1]));
    if (pos == cPos) {
      if (!callback) return std::unexpected(DungeonException("Empty dungeon tile callback"));
      return callback(Vec2I(pos[0], map.height() - 1 - pos[1]), tile());
    }
    return false;
  }
  if (m_kind == ObjectKind::Tile) {
    if (Vec2I(pos.x(), map.height() - pos.y()) != m_rect.min()) return false;
    if (!callback) return std::unexpected(DungeonException("Empty dungeon tile callback"));
    return callback(pos, tile());
  }
  if (m_kind == ObjectKind::Rectangle) {
    if (!m_rect.contains(Vec2I(pos.x(), map.height() - 1 - pos.y()))) return false;
    if (!callback) return std::unexpected(DungeonException("Empty dungeon tile callback"));
    return callback(pos, tile());
  }
  if constexpr (Star::DebugEnabled)
    Star::starAssertFailed((m_kind == ObjectKind::Polyline));
  for (Vec2I point : m_polyline) {
    Vec2I pointPos(m_rect.min().x() + point.x(), map.height() - 1 - m_rect.min().y() - point.y());
    if (pos == pointPos) {
      if (!callback) return std::unexpected(DungeonException("Empty dungeon tile callback"));
      auto result = callback(pos, tile());
      if (!result) return std::unexpected(std::move(result.error()));
      if (*result) return true;
    }
  }
  return false;
}

std::expected<std::shared_ptr<TMXTileLayer>, StarException> TMXTileLayer::create(Json const& layer) {
  auto result = std::shared_ptr<TMXTileLayer>(new TMXTileLayer);
  auto width = layer.getInt("width");
  if (!width) return std::unexpected(std::move(width.error()));
  auto height = layer.getInt("height");
  if (!height) return std::unexpected(std::move(height.error()));
  auto x = layer.getInt("x", 0);
  if (!x) return std::unexpected(std::move(x.error()));
  auto y = layer.getInt("y", 0);
  if (!y) return std::unexpected(std::move(y.error()));
  int originX = *x, originY = *y;
  unsigned layerWidth = *width, layerHeight = *height;
  result->m_rect = RectI({originX, originY},
                         {originX + static_cast<int>(layerWidth) - 1, originY + static_cast<int>(layerHeight) - 1});
  auto name = layer.getString("name");
  if (!name) return std::unexpected(std::move(name.error()));
  result->m_name = std::move(*name);
  if (!Tiled::LayerNames) return std::unexpected(Tiled::LayerNames.error());
  auto tileLayer = Tiled::LayerNames->get_left(result->m_name);
  if (!tileLayer) return std::unexpected(std::move(tileLayer.error()));
  result->m_layer = tileLayer->get();
  auto compressed = layer.contains("compression");
  if (!compressed) return std::unexpected(std::move(compressed.error()));
  if (!*compressed) {
    auto data = layer.getArray("data");
    if (!data) return std::unexpected(std::move(data.error()));
    for (Json const& index : *data) {
      auto gid = index.toUInt();
      if (!gid) return std::unexpected(std::move(gid.error()));
      result->m_tileData.push_back(*gid & ~TileFlip::AllBits);
    }
  } else {
    auto compression = layer.getString("compression");
    if (!compression) return std::unexpected(std::move(compression.error()));
    auto data = layer.getString("data");
    if (!data) return std::unexpected(std::move(data.error()));
    auto base64Data = base64Decode(*data);
    if (!base64Data) return std::unexpected(std::move(base64Data.error()));
    ByteArray bytes;
    if (*compression == "") {
      bytes = std::move(*base64Data);
    } else {
      auto decoded = [&]() -> std::expected<ByteArray, StarException> {
        if (*compression == "gzip") return uncompressDataGzip(*base64Data, MaxUncompressedSize);
        if (*compression == "zlib") return uncompressData(*base64Data, MaxUncompressedSize);
        if (*compression == "zstd") return ZstdCompression::decompress(*base64Data);
        return std::unexpected(StarException::format("TMXTileLayer does not support compression mode {}", *compression));
      }();
      if (!decoded) return std::unexpected(std::move(decoded.error()));
      bytes = std::move(*decoded);
    }
    for (std::size_t i = 0; i + 3 < bytes.size(); i += 4) {
      std::uint32_t gid = static_cast<std::uint8_t>(bytes[i]) | (static_cast<std::uint8_t>(bytes[i + 1]) << 8)
        | (static_cast<std::uint8_t>(bytes[i + 2]) << 16) | (static_cast<std::uint8_t>(bytes[i + 3]) << 24);
      result->m_tileData.push_back(gid & ~TileFlip::AllBits);
    }
  }
  if (result->m_tileData.size() != layerWidth * layerHeight)
    return std::unexpected(StarException("TMXTileLayer data length was inconsistent with width/height"));
  return result;
}

std::expected<std::shared_ptr<TMXMap>, StarException> TMXMap::create(Root& root, Json const& tmx) {
  auto tileHeight = tmx.getUInt("tileheight");
  if (!tileHeight) return std::unexpected(std::move(tileHeight.error()));
  if (*tileHeight != 8) return std::unexpected(StarException("Invalid tile size"));
  auto tileWidth = tmx.getUInt("tilewidth");
  if (!tileWidth) return std::unexpected(std::move(tileWidth.error()));
  if (*tileWidth != 8) return std::unexpected(StarException("Invalid tile size"));
  auto result = std::shared_ptr<TMXMap>(new TMXMap);
  auto width = tmx.getUInt("width");
  if (!width) return std::unexpected(std::move(width.error()));
  result->m_width = *width;
  auto height = tmx.getUInt("height");
  if (!height) return std::unexpected(std::move(height.error()));
  result->m_height = *height;
  auto tilesetsJson = tmx.get("tilesets");
  if (!tilesetsJson) return std::unexpected(std::move(tilesetsJson.error()));
  auto tilesets = TMXTilesets::create(root, *tilesetsJson);
  if (!tilesets) return std::unexpected(std::move(tilesets.error()));
  result->m_tilesets = std::move(*tilesets);
  auto layers = tmx.getArray("layers");
  if (!layers) return std::unexpected(std::move(layers.error()));
  for (Json const& tmxLayer : *layers) {
    auto type = tmxLayer.getString("type");
    if (!type) return std::unexpected(std::move(type.error()));
    if (*type == "tilelayer") {
      auto layer = TMXTileLayer::create(tmxLayer);
      if (!layer) return std::unexpected(std::move(layer.error()));
      result->m_tileLayers.push_back(std::move(*layer));
    } else if (*type == "objectgroup") {
      auto group = TMXObjectGroup::create(root, tmxLayer, result->m_tilesets);
      if (!group) return std::unexpected(std::move(group.error()));
      result->m_objectGroups.push_back(std::move(*group));
    } else {
      return std::unexpected(StarException::format("Unknown layer type '{}'", *type));
    }
  }
  return result;
}

std::expected<std::reference_wrapper<Tiled::Tile const>, StarException> TMXTileLayer::getTile(std::shared_ptr<TMXTilesets> const& tilesets, Vec2I pos) const {
  if (!m_rect.contains(pos)) return std::cref(tilesets->nullTile());
  int dx = pos.x() - m_rect.xMin();
  int dy = pos.y() - m_rect.yMin();
  return tilesets->getTile(m_tileData[dx + dy * width()], m_layer);
}

[[nodiscard]] std::expected<String, StarException> tilesetAssetPath(String const& relativePath) {
  // Tiled can reference another package via parent paths; retain the suffix
  // beginning at the last known /tilesets/ segment.
  auto i = relativePath.findLast("/tilesets/", String::CaseInsensitive);
  if (!i) return std::unexpected(std::move(i.error()));
  if (*i == NPos) return relativePath;
  return relativePath.slice(*i);
}

std::expected<std::shared_ptr<TMXTilesets>, StarException> TMXTilesets::create(Root& root, Json const& tmx) {
  auto result = std::shared_ptr<TMXTilesets>(new TMXTilesets);
  auto definitions = tmx.iterateArray();
  if (!definitions) return std::unexpected(std::move(definitions.error()));
  for (Json const& definition : *definitions) {
    auto hasSource = definition.contains("source");
    if (!hasSource) return std::unexpected(std::move(hasSource.error()));
    if (!*hasSource) {
      auto name = definition.optString("name");
      if (!name) return std::unexpected(std::move(name.error()));
      return std::unexpected(StarException::format("Tiled map has embedded tileset {}", outputOptional(*name)));
    }
    auto source = definition.getString("source");
    if (!source) return std::unexpected(std::move(source.error()));
    auto path = tilesetAssetPath(*source);
    if (!path) return std::unexpected(std::move(path.error()));
    auto database = root.tilesetDatabase();
    if (!database) return std::unexpected(std::move(database.error()));
    auto tileset = (*database)->get(*path);
    if (!tileset) return std::unexpected(std::move(tileset.error()));
    result->m_tilesets.push_back(*tileset);
    auto firstGid = definition.getUInt("firstgid");
    if (!firstGid) return std::unexpected(std::move(firstGid.error()));
    for (std::size_t i = 0; i < (*tileset)->size(); ++i) {
      auto front = (*tileset)->getTile(i, TileLayer::Foreground);
      if (!front) return std::unexpected(std::move(front.error()));
      auto storedFront = result->m_foregroundTilesByGid.set(*firstGid + i, optionalRef(*front));
      if (!storedFront)
        return std::unexpected(std::move(storedFront.error()));
      auto back = (*tileset)->getTile(i, TileLayer::Background);
      if (!back) return std::unexpected(std::move(back.error()));
      auto storedBack = result->m_backgroundTilesByGid.set(*firstGid + i, optionalRef(*back));
      if (!storedBack)
        return std::unexpected(std::move(storedBack.error()));
    }
  }
  auto nullTile = Tiled::Tile::create(root, Tiled::Properties(), TileLayer::Foreground);
  if (!nullTile) return std::unexpected(std::move(nullTile.error()));
  result->m_nullTile = std::move(*nullTile);
  JsonObject emptyBackProperties;
  emptyBackProperties["clear"] = "true";
  auto emptyBackTile = Tiled::Tile::create(root, Tiled::Properties(emptyBackProperties), TileLayer::Background);
  if (!emptyBackTile) return std::unexpected(std::move(emptyBackTile.error()));
  result->m_emptyBackTile = std::move(*emptyBackTile);
  return result;
}

std::expected<std::reference_wrapper<Tiled::Tile const>, StarException> TMXTilesets::getTile(unsigned gid, TileLayer layer) const {
  auto const& tiles = layer == TileLayer::Foreground ? m_foregroundTilesByGid : m_backgroundTilesByGid;
  auto tile = tiles.at(gid);
  if (!tile) return std::unexpected(std::move(tile.error()));
  if (tile->get()) return *tile->get();
  return std::cref(layer == TileLayer::Foreground ? *m_nullTile : *m_emptyBackTile);
}


std::expected<std::shared_ptr<TMXObject>, StarException> TMXObject::create(Root& root, std::optional<Json> const& groupProperties, Json const& tmx, std::shared_ptr<TMXTilesets> tilesets) {
  auto result = std::shared_ptr<TMXObject>(new TMXObject);
  auto id = tmx.getUInt("id");
  if (!id) return std::unexpected(std::move(id.error()));
  result->m_objectId = *id;
  auto objectProperties = tmx.opt("properties");
  if (!objectProperties) return std::unexpected(std::move(objectProperties.error()));
  if (*objectProperties) {
    if ((**objectProperties).type() == Json::Type::Array) {
      JsonObject converted;
      auto properties = (**objectProperties).toArray();
      if (!properties) return std::unexpected(std::move(properties.error()));
      for (auto const& p : *properties) {
        auto name = p.getString("name");
        if (!name) return std::unexpected(std::move(name.error()));
        auto value = p.get("value");
        if (!value) return std::unexpected(std::move(value.error()));
        converted.set(*name, *value);
      }
      *objectProperties = Json(std::move(converted));
    } else {
      auto object = (**objectProperties).toObject();
      if (!object) return std::unexpected(std::move(object.error()));
      *objectProperties = Json(std::move(*object));
    }
  }
  auto layer = getLayer(groupProperties, *objectProperties);
  if (!layer) return std::unexpected(std::move(layer.error()));
  result->m_layer = *layer;
  auto info = getTileObjectInfo(tmx, std::move(tilesets), *layer);
  if (!info) return std::unexpected(std::move(info.error()));
  Tiled::Properties properties;
  if (*objectProperties) {
    auto inherited = properties.inherit(**objectProperties);
    if (!inherited) return std::unexpected(std::move(inherited.error()));
    properties = std::move(*inherited);
  }
  if (*info) {
    auto inherited = properties.inherit((**info).tileProperties);
    if (!inherited) return std::unexpected(std::move(inherited.error()));
    properties = std::move(*inherited);
  }
  if (groupProperties) {
    auto inherited = properties.inherit(*groupProperties);
    if (!inherited) return std::unexpected(std::move(inherited.error()));
    properties = std::move(*inherited);
  }
  bool flipX = *info && (((**info).flipBits & TileFlip::Horizontal) != 0);
  auto kind = getObjectKind(tmx, *objectProperties);
  if (!kind) return std::unexpected(std::move(kind.error()));
  result->m_kind = *kind;
  auto pos = getPos(tmx);
  if (!pos) return std::unexpected(std::move(pos.error()));
  auto imagePosition = getImagePosition(properties);
  if (!imagePosition) return std::unexpected(std::move(imagePosition.error()));
  auto size = getSize(tmx);
  if (!size) return std::unexpected(std::move(size.error()));
  Vec2I rectPos = *pos - *imagePosition;
  result->m_rect = RectI(rectPos, rectPos + *size);
  JsonObject computedProperties;
  if (*kind == ObjectKind::Stagehand) {
    Vec2I center = result->rect().center();
    auto area = jsonFromRectI(RectI(result->rect().min() - center, result->rect().max() - center)).repr();
    if (!area) return std::unexpected(std::move(area.error()));
    computedProperties["broadcastArea"] = *area;
  }
  auto rotation = tmx.optFloat("rotation");
  if (!rotation) return std::unexpected(std::move(rotation.error()));
  if (*rotation && **rotation != 0.0f)
    return std::unexpected(tmxObjectError(*pos, "object is rotated, which is not supported"));
  auto polyline = tmx.optArray("polyline");
  if (!polyline) return std::unexpected(std::move(polyline.error()));
  if (*polyline) {
    for (auto const& point : **polyline) {
      auto vertex = getPos(point);
      if (!vertex) return std::unexpected(std::move(vertex.error()));
      result->m_polyline.push_back(*vertex);
    }
    computedProperties["wire"] = "_polylineWire" + toString(result->m_objectId);
    computedProperties["local"] = "true";
  }
  auto inherited = properties.inherit(computedProperties);
  if (!inherited) return std::unexpected(std::move(inherited.error()));
  auto tile = Tiled::Tile::create(root, *inherited, *layer, flipX);
  if (!tile) return std::unexpected(std::move(tile.error()));
  result->m_tile = std::move(*tile);
  return result;
}

std::expected<Vec2I, StarException> TMXObject::getSize(Json const& tmx) {
  auto width = tmx.contains("width");
  if (!width) return std::unexpected(std::move(width.error()));
  if (*width) {
    auto height = tmx.contains("height");
    if (!height) return std::unexpected(std::move(height.error()));
    if (*height) {
      auto w = tmx.getUInt("width");
      if (!w) return std::unexpected(std::move(w.error()));
      auto h = tmx.getUInt("height");
      if (!h) return std::unexpected(std::move(h.error()));
      return Vec2I(*w, *h) / TilePixels;
    }
  }
  return Vec2I();
}

std::expected<Vec2I, StarException> TMXObject::getImagePosition(Tiled::Properties const& properties) {
  auto x = properties.opt<float>("imagePositionX");
  if (!x) return std::unexpected(std::move(x.error()));
  auto y = properties.opt<float>("imagePositionY");
  if (!y) return std::unexpected(std::move(y.error()));
  return Vec2I(static_cast<int>(x->value_or(0) / TilePixels), -static_cast<int>(y->value_or(0) / TilePixels));
}

std::expected<ObjectKind, StarException> TMXObject::getObjectKind(Json const& tmx, std::optional<Json> const& objectProperties) {
  if (objectProperties) {
    auto stagehand = objectProperties->contains("stagehand");
    if (!stagehand) return std::unexpected(std::move(stagehand.error()));
    if (*stagehand) return ObjectKind::Stagehand;
  }
  auto gid = tmx.contains("gid");
  if (!gid) return std::unexpected(std::move(gid.error()));
  if (*gid) return ObjectKind::Tile;
  auto ellipse = tmx.contains("ellipse");
  if (!ellipse) return std::unexpected(std::move(ellipse.error()));
  if (*ellipse) {
    auto pos = getPos(tmx);
    if (!pos) return std::unexpected(std::move(pos.error()));
    return std::unexpected(tmxObjectError(*pos, "object has unsupported ellipse shape"));
  }
  auto polygon = tmx.contains("polygon");
  if (!polygon) return std::unexpected(std::move(polygon.error()));
  if (*polygon) {
    auto pos = getPos(tmx);
    if (!pos) return std::unexpected(std::move(pos.error()));
    return std::unexpected(tmxObjectError(*pos, "object has unsupported polygon shape"));
  }
  auto polyline = tmx.contains("polyline");
  if (!polyline) return std::unexpected(std::move(polyline.error()));
  return *polyline ? ObjectKind::Polyline : ObjectKind::Rectangle;
}

std::expected<std::optional<TMXObject::TileObjectInfo>, StarException> TMXObject::getTileObjectInfo(Json const& tmx, std::shared_ptr<TMXTilesets> tilesets, TileLayer layer) {
  auto gidValue = tmx.optUInt("gid");
  if (!gidValue) return std::unexpected(std::move(gidValue.error()));
  if (!*gidValue) return std::optional<TileObjectInfo>();
  unsigned flipBits = **gidValue & TileFlip::AllBits;
  unsigned gid = **gidValue & ~TileFlip::AllBits;
  if (flipBits & (TileFlip::Vertical | TileFlip::Diagonal)) {
    auto pos = getPos(tmx);
    if (!pos) return std::unexpected(std::move(pos.error()));
    return std::unexpected(tmxObjectError(*pos, "object contains vertical or diagonal flips, which are not supported"));
  }
  auto tile = tilesets->getTile(gid, layer);
  if (!tile) return std::unexpected(std::move(tile.error()));
  return TileObjectInfo{tile->get().properties, flipBits};
}

std::expected<TileLayer, StarException> TMXObject::getLayer(std::optional<Json> const& groupProperties, std::optional<Json> const& objectProperties) {
  for (auto propertiesRef : {std::cref(objectProperties), std::cref(groupProperties)}) {
    auto const& properties = propertiesRef.get();
    if (properties) {
      auto hasLayer = properties->contains("layer");
      if (!hasLayer) return std::unexpected(std::move(hasLayer.error()));
      if (*hasLayer) {
        auto layer = properties->getString("layer");
        if (!layer) return std::unexpected(std::move(layer.error()));
        if (!Tiled::LayerNames) return std::unexpected(Tiled::LayerNames.error());
        auto value = Tiled::LayerNames->get_left(*layer);
        if (!value) return std::unexpected(std::move(value.error()));
        return value->get();
      }
    }
  }
  return TileLayer::Foreground;
}

std::expected<Vec2I, StarException> TMXObject::getPos(Json const& tmx) {
  auto x = tmx.getInt("x");
  if (!x) return std::unexpected(std::move(x.error()));
  auto y = tmx.getInt("y");
  if (!y) return std::unexpected(std::move(y.error()));
  return Vec2I(*x, *y) / TilePixels;
}

StarException TMXObject::tmxObjectError(Vec2I pos, String const& msg) {
  return StarException::format("At {},{}: {}", pos[0], pos[1], msg);
}

std::expected<std::shared_ptr<TMXObjectGroup>, StarException> TMXObjectGroup::create(Root& root, Json const& tmx, std::shared_ptr<TMXTilesets> tilesets) {
  auto result = std::shared_ptr<TMXObjectGroup>(new TMXObjectGroup);
  auto name = tmx.getString("name");
  if (!name) return std::unexpected(std::move(name.error()));
  result->m_name = std::move(*name);
  auto groupProperties = tmx.opt("properties");
  if (!groupProperties) return std::unexpected(std::move(groupProperties.error()));
  if (*groupProperties) {
    if ((**groupProperties).type() == Json::Type::Array) {
      JsonObject converted;
      auto properties = (**groupProperties).toArray();
      if (!properties) return std::unexpected(std::move(properties.error()));
      for (auto const& p : *properties) {
        auto propertyName = p.getString("name");
        if (!propertyName) return std::unexpected(std::move(propertyName.error()));
        auto value = p.get("value");
        if (!value) return std::unexpected(std::move(value.error()));
        converted.set(*propertyName, *value);
      }
      *groupProperties = Json(std::move(converted));
    } else {
      auto object = (**groupProperties).toObject();
      if (!object) return std::unexpected(std::move(object.error()));
      *groupProperties = Json(std::move(*object));
    }
  }
  auto objects = tmx.getArray("objects");
  if (!objects) return std::unexpected(std::move(objects.error()));
  for (Json const& definition : *objects) {
    auto object = TMXObject::create(root, *groupProperties, definition, tilesets);
    if (!object) return std::unexpected(std::move(object.error()));
    result->m_objects.push_back(std::move(*object));
  }
  return result;
}

std::expected<void, StarException> TMXPartReader::readAsset(String const& asset) {
  auto assets = m_root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto json = (*assets)->json(asset);
  if (!json) return std::unexpected(std::move(json.error()));
  auto map = TMXMap::create(m_root, *json);
  if (!map) return std::unexpected(std::move(map.error()));
  m_maps.push_back(std::make_pair(asset, std::move(*map)));
  return {};
}

Vec2U TMXPartReader::size() const {
  Vec2U size;
  for (auto const& map : m_maps)
    size = Vec2U{map.second->width(), map.second->height()};
  return size;
}

std::expected<void, StarException> TMXPartReader::forEachTile(TileCallback const& callback) const {
  for (auto const& map : m_maps) {
    auto result = map.second->forEachTile(callback);
    if (!result) return std::unexpected(std::move(result.error()));
  }
  return {};
}

std::expected<void, StarException> TMXPartReader::forEachTileAt(Vec2I pos, TileCallback const& callback) const {
  for (auto const& map : m_maps) {
    auto result = map.second->forEachTileAt(pos, callback);
    if (!result) return std::unexpected(std::move(result.error()));
  }
  return {};
}

}
