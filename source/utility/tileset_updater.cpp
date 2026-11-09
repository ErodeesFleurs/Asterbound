#include <cstdio>
import std;
import star.format;
import star.string;
import star.io_device;
import star.file;
import star.logging;
import star.json;
import star.game_types;
import star.lexical_cast;
import star.tileset_updater;
import star.list;
import star.map;
import star.set;
import star.image;
import star.tileset_database;

using namespace Star;

String const InvalidTileImage = "../packed/invalid.png";
String const AssetsTilesetDirectory = "tilesets";
String const TileImagesDirectory = "../../../../tiled";
int const Indentation = 2;

String unixFileJoin(String const& dirname, String const& filename) {
  std::string dir = dirname.utf8(), file = filename.utf8();
  while (!dir.empty() && (dir.back() == '\\' || dir.back() == '/'))
    dir.pop_back();
  auto start = file.find_first_not_of("\\/");
  file = start == std::string::npos ? std::string{} : file.substr(start);
  dir += '/';
  dir += file;
  std::replace(dir.begin(), dir.end(), '\\', '/');
  return String(std::move(dir));
}

TileDatabase::TileDatabase(String const& name) : m_name(name) {}

void TileDatabase::defineTile(std::shared_ptr<::Star::Tile> const& tile) {
  m_tiles[tile->name] = tile;
}

std::shared_ptr<::Star::Tile> TileDatabase::getTile(String const& tileName) const {
  return m_tiles.maybe(tileName).value_or(nullptr);
}

String TileDatabase::name() const {
  return m_name;
}

StringSet TileDatabase::tileNames() const {
  return StringSet::from(m_tiles.keys());
}

Tileset::Tileset(Logger& logger, String const& source, String const& name, std::shared_ptr<::Star::TileDatabase> const& database)
  : m_logger(logger), m_source(source), m_name(name), m_tiles(), m_database(database) {}

void Tileset::defineTile(std::shared_ptr<::Star::Tile> const& tile) {
  // Each tileset must be exported from a single database. When a tile switches
  // to another tileset (e.g. because an object has changed category), we allow
  // it to stay in the previous tileset to avoid breaking maps.
  // This means that if we exported a mix of, e.g. materials, liquids and
  // objects (which would cause the assertion failure below) it'd be harder to
  // check if a tile still exists in the database and should be exported
  // despite no longer belonging to the tileset.
  if constexpr (Star::DebugEnabled) {
    Star::star_assert_failed((m_source == tile->source));
  }
  if constexpr (Star::DebugEnabled) {
    Star::star_assert_failed((m_database->name() == tile->database));
  }

  m_tiles.push_back(tile);
}

[[nodiscard]]
std::expected<std::optional<std::pair<String, String>>, StarException> parseAssetSource(String const& source) {
  auto directory = File::is_directory(source);
  if (!directory)
    return std::unexpected(std::move(directory.error()));
  if (!*directory)
    return std::optional<std::pair<String, String>>{};

  std::string path = source.utf8();
  while (!path.empty() && (path.back() == '/' || path.back() == '\\'))
    path.pop_back();
  String sourcePath(path);
  auto separator = path.find_last_of("/\\");
  String sourceName(separator == std::string::npos ? path : path.substr(separator + 1));

  return std::make_optional(std::make_pair(sourceName, sourcePath));
}

String tilesetExportDir(String const& sourcePath, String const& sourceName) {
  return StringList{sourcePath, AssetsTilesetDirectory, sourceName}.join("/");
}

std::expected<void, StarException> Tileset::exportTileset() const {
  auto parsedSource = parseAssetSource(m_source);
  if (!parsedSource)
    return std::unexpected(std::move(parsedSource.error()));
  if (!*parsedSource)
    return {};
  auto const& [sourceName, sourcePath] = **parsedSource;
  String exportDir = tilesetExportDir(sourcePath, sourceName);
  String tilesetPath = unixFileJoin(exportDir, m_name + ".json");
  auto directory = File::create_directories(File::dir_name(tilesetPath));
  if (!directory)
    return std::unexpected(std::move(directory.error()));
  auto logged = m_logger.info("Updating tileset at {}", tilesetPath);
  if (!logged)
    return std::unexpected(std::move(logged.error()));
  auto loaded = getTilesetJson(tilesetPath);
  if (!loaded)
    return std::unexpected(std::move(loaded.error()));
  Json root = std::move(*loaded);
  auto properties = root.getObject("tileproperties", JsonObject{});
  if (!properties)
    return std::unexpected(std::move(properties.error()));
  JsonObject tileProperties = std::move(*properties), tileImages;
  auto indexed = indexExistingTiles(root);
  if (!indexed)
    return std::unexpected(std::move(indexed.error()));
  auto& [existingTiles, next_id] = *indexed;
  auto updated = updateTiles(tileProperties, tileImages, existingTiles, next_id, tilesetPath);
  if (!updated)
    return std::unexpected(std::move(updated.error()));
  auto invalidTiles = StringSet::from(existingTiles.keys()).difference(*updated);
  auto invalidated = invalidateTiles(invalidTiles, existingTiles, tileProperties, tileImages, tilesetPath);
  if (!invalidated)
    return std::unexpected(std::move(invalidated.error()));
  for (std::size_t id = 0; id < next_id; ++id) {
    String idKey = Star::to_string(id);
    if (!tileProperties.contains(idKey))
      tileProperties[idKey] = JsonObject{{"invalid", "true"}};
    if (!tileImages.contains(idKey))
      tileImages[idKey] = imageFileReference(InvalidTileImage);
  }
  auto withImages = root.set("tiles", tileImages);
  if (!withImages)
    return std::unexpected(std::move(withImages.error()));
  auto withProperties = withImages->set("tileproperties", tileProperties);
  if (!withProperties)
    return std::unexpected(std::move(withProperties.error()));
  auto complete = withProperties->set("tilecount", next_id);
  if (!complete)
    return std::unexpected(std::move(complete.error()));
  auto text = complete->printJson(Indentation, true);
  if (!text)
    return std::unexpected(std::move(text.error()));
  auto images = exportTilesetImages(exportDir);
  if (!images)
    return std::unexpected(std::move(images.error()));
  return File::overwrite_file_with_rename(*text, tilesetPath);
}

String Tileset::name() const {
  return m_name;
}

std::shared_ptr<::Star::TileDatabase> Tileset::database() const {
  return m_database;
}

String imageExportDirName(String const& baseExportDir, String const& assetSourceName) {
  String dir = unixFileJoin(baseExportDir, TileImagesDirectory);
  return unixFileJoin(dir, assetSourceName);
}

String Tileset::imageDirName(String const& baseExportDir) const {
  std::string path = m_source.utf8();
  while (!path.empty() && (path.back() == '/' || path.back() == '\\'))
    path.pop_back();
  auto separator = path.find_last_of("/\\");
  String sourceName(separator == std::string::npos ? path : path.substr(separator + 1));
  return imageExportDirName(baseExportDir, sourceName);
}

String Tileset::relativePathBase() const {
  int subdirs = -1;
  bool inComponent = false;
  for (char c : m_name.utf8()) {
    if (c == '/' || c == '\\') {
      inComponent = false;
    } else if (!inComponent) {
      ++subdirs;
      inComponent = true;
    }
  }
  String relativePathBase;
  if (subdirs == 0) {
    relativePathBase = ".";
  } else {
    StringList path;
    for (int i = 0; i < subdirs; ++i)
      path.push_back("..");
    relativePathBase = path.join("/");
  }
  return relativePathBase;
}

Json Tileset::imageFileReference(String const& fileName) const {
  String tileImagePath = unixFileJoin(imageDirName(relativePathBase()), fileName);
  return JsonObject{{"image", tileImagePath}};
}

Json Tileset::tileImageReference(String const& tileName, String const& database) const {
  String tileImageName = unixFileJoin(database, tileName + ".png");
  return imageFileReference(tileImageName);
}

std::expected<void, StarException> Tileset::exportTilesetImages(String const& exportDir) const {
  for (auto const& tile : m_tiles) {
    String imageDir = unixFileJoin(imageDirName(exportDir), tile->database);
    auto directory = File::create_directories(imageDir);
    if (!directory)
      return std::unexpected(std::move(directory.error()));
    String imageName = unixFileJoin(imageDir, tile->name + ".png");
    auto logged = m_logger.info("Updating image {}", imageName);
    if (!logged)
      return std::unexpected(std::move(logged.error()));
    auto outputFile = File::open(imageName, IOMode::Write);
    if (!outputFile)
      return std::unexpected(std::move(outputFile.error()));
    auto written = tile->image->writePng(m_logger, *outputFile);
    if (!written)
      return std::unexpected(std::move(written.error()));
    auto closed = (*outputFile)->close();
    if (!closed)
      return std::unexpected(std::move(closed.error()));
  }
  return {};
}

std::expected<Json, StarException> Tileset::getTilesetJson(String const& tilesetPath) const {
  auto exists = File::exists(tilesetPath);
  if (!exists)
    return std::unexpected(std::move(exists.error()));
  if (*exists) {
    auto text = File::read_file_string(tilesetPath);
    if (!text)
      return std::unexpected(std::move(text.error()));
    return Json::parseJson(*text);
  } else {
    auto logged = m_logger.warn(
        "Tileset {} wasn't already present. Creating it from scratch. Any maps already using this tileset may be "
        "broken.",
        tilesetPath);
    if (!logged)
      return std::unexpected(std::move(logged.error()));
    return JsonObject{{"margin", 0},
        {"name", m_name},
        {"properties", JsonObject{}},
        {"spacing", 0},
        {"tilecount", m_tiles.size()},
        {"tileheight", TilePixels},
        {"tilewidth", TilePixels},
        {"tiles", JsonObject{}},
        {"tileproperties", JsonObject{}}};
  }
}

std::expected<std::pair<StringMap<std::size_t>, std::size_t>, StarException> Tileset::indexExistingTiles(Json tileset) const {
  StringMap<std::size_t> existingTiles;
  std::size_t next_id = 0;
  auto properties = tileset.getObject("tileproperties");
  if (!properties)
    return std::unexpected(std::move(properties.error()));
  for (auto const& entry : *properties) {
    auto id = lexical_cast<std::size_t>(entry.first);
    if (!id)
      return std::unexpected(std::move(id.error()));
    auto containsName = entry.second.contains("//name");
    if (!containsName)
      return std::unexpected(std::move(containsName.error()));
    if (*containsName) {
      auto name = entry.second.getString("//name");
      if (!name)
        return std::unexpected(std::move(name.error()));
      existingTiles[*name] = *id;
      next_id = std::max(*id + 1, next_id);
    }
  }
  return std::make_pair(existingTiles, next_id);
}

std::expected<StringSet, StarException> Tileset::updateTiles(JsonObject& tileProperties, JsonObject& tileImages,
                                                             StringMap<std::size_t> const& existingTiles,
                                                             std::size_t& next_id, String const& tilesetPath) const {
  StringSet updatedTiles;
  for (std::shared_ptr<::Star::Tile> const& tile : m_tiles) {
    Json properties = tile->properties;
    std::size_t id = 0;

    if (auto existing = existingTiles.find_ref(tile->name)) {
      id = existing->get();
    } else {
      if (auto written = Star::print("Adding '{}' to {}\n", tile->name, tilesetPath); !written)
        return std::unexpected(std::move(written.error()));
      id = next_id++;
    }

    tileProperties[Star::to_string(id)] = std::move(properties);
    tileImages[Star::to_string(id)] = tileImageReference(tile->name, tile->database);

    updatedTiles.add(tile->name);
  }
  return updatedTiles;
}

std::expected<void, StarException> Tileset::invalidateTiles(StringSet const& invalidTiles,
    StringMap<std::size_t> const& existingTiles,
    JsonObject& tileProperties,
    JsonObject& tileImages,
    String const& tilesetPath) const {
  for (String tileName : invalidTiles) {
    auto indexed = existingTiles.get(tileName);
    if (!indexed)
      return std::unexpected(std::move(indexed.error()));
    std::size_t id = indexed->get();

    if (std::shared_ptr<::Star::Tile> const& tile = m_database->getTile(tileName)) {
      // Tile has moved category, but we're leaving it in this tileset to avoid
      // breaking existing maps.
      tileProperties[Star::to_string(id)] = tile->properties;
      tileImages[Star::to_string(id)] = tileImageReference(tile->name, tile->database);
    } else {
      auto invalid = tileProperties[Star::to_string(id)].contains("invalid");
      if (!invalid)
        return std::unexpected(std::move(invalid.error()));
      if (!*invalid) {
        if (auto written = Star::print("Removing '{}' from {}\n", tileName, tilesetPath); !written)
          return std::unexpected(std::move(written.error()));
      }
      tileProperties[Star::to_string(id)] = JsonObject{{"//name", tileName}, {"invalid", "true"}};
      tileImages[Star::to_string(id)] = imageFileReference(InvalidTileImage);
    }
  }
  return {};
}

TilesetUpdater::TilesetUpdater(Logger& logger) : m_logger(logger) {}

std::expected<void, StarException> TilesetUpdater::defineAssetSource(String const& source) {
  auto parsedSource = parseAssetSource(source);
  if (!parsedSource)
    return std::unexpected(std::move(parsedSource.error()));
  if (!*parsedSource)
    return {};

  String sourceName;
  String sourcePath;
  std::tie(sourceName, sourcePath) = **parsedSource;
  String tilesetDir = tilesetExportDir(sourcePath, sourceName);
  String imageDir = imageExportDirName(tilesetDir, sourceName);

  auto logged = m_logger.info("Scanning {} for images...", imageDir);
  if (!logged)
    return std::unexpected(std::move(logged.error()));
  auto directory = File::is_directory(imageDir);
  if (!directory)
    return std::unexpected(std::move(directory.error()));
  if (!*directory)
    return {};

  auto entries = File::directory_entries(imageDir);
  if (!entries)
    return std::unexpected(std::move(entries.error()));
  for (std::pair<String, bool> const& entry : *entries) {
    // Never follow a symlinked directory: it can point outside of the asset
    // source, or at one of its own parents.
    auto symlink = File::is_symlink(unixFileJoin(imageDir, entry.first));
    if (!symlink)
      return std::unexpected(std::move(symlink.error()));
    if (*symlink) {
      auto warning = m_logger.warn("TilesetUpdater: Not scanning '{}' because it is a symlink", entry.first);
      if (!warning)
        return std::unexpected(std::move(warning.error()));
      continue;
    }

    if (entry.second) {
      String databaseName = entry.first;
      String databasePath = unixFileJoin(imageDir, databaseName);
      auto loggedDatabase = m_logger.info("Scanning database {}...", databaseName);
      if (!loggedDatabase)
        return std::unexpected(std::move(loggedDatabase.error()));
      auto images = File::directory_entries(databasePath);
      if (!images)
        return std::unexpected(std::move(images.error()));
      for (std::pair<String, bool> const& image : *images) {
        if (image.second || !image.first.utf8().ends_with(".png"))
          return std::unexpected(StarException::format("Unexpected entry in tile image database: {}", image.first));
        String tileName(image.first.utf8().substr(0, image.first.utf8().size() - 4));
        m_preexistingImages[sourceName][databaseName].add(tileName);
      }
    }
  }
  return {};
}

void TilesetUpdater::defineTile(std::shared_ptr<::Star::Tile> const& tile) {
  getDatabase(tile)->defineTile(tile);
  getTileset(tile)->defineTile(tile);
}

std::expected<void, StarException> TilesetUpdater::exportTilesets() {
  for (auto const& tilesets : m_tilesets) {
    auto parsedAssetSource = parseAssetSource(tilesets.first);
    if (!parsedAssetSource)
      return std::unexpected(std::move(parsedAssetSource.error()));
    if (!*parsedAssetSource) {
      auto logged = m_logger.info("Not updating tilesets in {} because it is packed", tilesets.first);
      if (!logged)
        return std::unexpected(std::move(logged.error()));
      continue;
    }
    String sourceName;
    String sourcePath;
    std::tie(sourceName, sourcePath) = **parsedAssetSource;

    String tilesetDir = tilesetExportDir(sourcePath, sourceName);
    String imageDir = imageExportDirName(tilesetDir, sourceName);

    for (auto const& tileset : tilesets.second.values()) {
      auto exported = tileset->exportTileset();
      if (!exported)
        return std::unexpected(std::move(exported.error()));
    }

    for (auto const& database : m_databases[tilesets.first].values()) {
      String databaseImagePath = unixFileJoin(imageDir, database->name());
      StringSet unusedImages = m_preexistingImages[sourceName][database->name()].difference(database->tileNames());
      for (String tileName : unusedImages) {
        String tileImagePath = unixFileJoin(databaseImagePath, tileName + ".png");
        auto file = File::is_regular_file(tileImagePath);
        if (!file)
          return std::unexpected(std::move(file.error()));
        if (!*file)
          return std::unexpected(StarException::format("Expected unused tile image file: {}", tileImagePath));
        if (auto written = Star::print("Removing unused tile image tiled/{}/{}/{}.png\n", sourceName, database->name(), tileName); !written)
          return std::unexpected(std::move(written.error()));
        auto removed = File::remove(tileImagePath);
        if (!removed)
          return std::unexpected(std::move(removed.error()));
      }
      m_preexistingImages[sourceName][database->name()] = database->tileNames();
    }
  }
  return {};
}

std::shared_ptr<::Star::TileDatabase> const& TilesetUpdater::getDatabase(std::shared_ptr<::Star::Tile> const& tile) {
  auto& databases = m_databases[tile->source];
  if (!databases.contains(tile->database))
    databases[tile->database] = std::make_shared<TileDatabase>(tile->database);
  return databases[tile->database];
}

std::shared_ptr<::Star::Tileset> const& TilesetUpdater::getTileset(std::shared_ptr<::Star::Tile> const& tile) {
  std::shared_ptr<::Star::TileDatabase> database = getDatabase(tile);
  auto& tilesets = m_tilesets[tile->source];
  if (!tilesets.contains(tile->tileset))
    tilesets[tile->tileset] = std::make_shared<Tileset>(m_logger, tile->source, tile->tileset, database);
  return tilesets[tile->tileset];
}
