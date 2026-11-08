#include <cstdio>
import std;
import star.json;
import star.native_runtime;
import star.file;
import star.logging;
import star.string;
import star.list;
import star.format;
import star.output_proxy;
import star.option_parser;
import star.root;
import star.root_loader;

using namespace Star;

void removeCommonPrefix(StringList& a, StringList& b) {
  while (!a.empty() && !b.empty() && a[0] == b[0]) {
    a.erase_at(0);
    b.erase_at(0);
  }
}

[[nodiscard]]
std::expected<String, StarException> createRelativePath(String fromFile, String toFile) {
  auto fromDirectory = File::is_directory(fromFile);
  if (!fromDirectory)
    return std::unexpected(std::move(fromDirectory.error()));
  if (!*fromDirectory)
    fromFile = File::dir_name(fromFile);
  auto fromPath = File::full_path(fromFile);
  if (!fromPath)
    return std::unexpected(std::move(fromPath.error()));
  auto toPath = File::full_path(toFile);
  if (!toPath)
    return std::unexpected(std::move(toPath.error()));
  auto fromParts = fromPath->split_any("/\\");
  if (!fromParts)
    return std::unexpected(std::move(fromParts.error()));
  auto toParts = toPath->split_any("/\\");
  if (!toParts)
    return std::unexpected(std::move(toParts.error()));
  removeCommonPrefix(*fromParts, *toParts);
  StringList relativeParts;
  for (std::size_t i = 0; i < fromParts->size(); ++i)
    relativeParts.push_back("..");
  relativeParts.append_range(*toParts);
  return relativeParts.join("/");
}

[[nodiscard]]
std::expected<std::optional<Json>, StarException> repairTileset(Json tileset, String const& mapPath, String const& tilesetPath) {
  auto external = tileset.contains("source");
  if (!external)
    return std::unexpected(std::move(external.error()));
  if (*external)
    return std::optional<Json>{};
  auto firstGid = tileset.getUInt("firstgid");
  if (!firstGid)
    return std::unexpected(std::move(firstGid.error()));
  auto tilesetName = tileset.getString("name");
  if (!tilesetName)
    return std::unexpected(std::move(tilesetName.error()));
  String filename = File::relative_to(tilesetPath, *tilesetName + ".json");
  auto exists = File::exists(filename);
  if (!exists)
    return std::unexpected(std::move(exists.error()));
  if (!*exists)
    return std::unexpected(StarException::format("Tileset {} does not exist. Can't repair {}", filename, mapPath));
  auto relativePath = createRelativePath(mapPath, filename);
  if (!relativePath)
    return std::unexpected(std::move(relativePath.error()));
  return std::optional<Json>{JsonObject{{"firstgid", *firstGid}, {"source", std::move(*relativePath)}}};
}

[[nodiscard]]
std::expected<std::optional<Json>, StarException> repair(Json mapJson, String const& mapPath, String const& tilesetPath) {
  auto tilesets = mapJson.getArray("tilesets");
  if (!tilesets)
    return std::unexpected(std::move(tilesets.error()));
  bool changed = false;
  for (auto& tileset : *tilesets) {
    auto fixed = repairTileset(tileset, mapPath, tilesetPath);
    if (!fixed)
      return std::unexpected(std::move(fixed.error()));
    if (*fixed) {
      tileset = std::move(**fixed);
      changed = true;
    }
  }
  if (!changed)
    return std::optional<Json>{};
  auto fixed = mapJson.set("tilesets", std::move(*tilesets));
  if (!fixed)
    return std::unexpected(std::move(fixed.error()));
  return std::optional<Json>{std::move(*fixed)};
}

[[nodiscard]]
std::expected<void, StarException> forEachRecursiveFileMatch(Logger& logger, String const& dirName, String const& filenameSuffix, std::function<std::expected<void, StarException>(String const&)> const& func) {
  auto entries = File::directory_entries(dirName);
  if (!entries)
    return std::unexpected(std::move(entries.error()));
  for (auto const& entry : *entries) {
    String entryPath = File::relative_to(dirName, entry.first);
    // Do not descend outside the tree or revisit a parent through a symlink.
    if (entry.second) {
      auto symlink = File::is_symlink(entryPath);
      if (!symlink)
        return std::unexpected(std::move(symlink.error()));
      if (*symlink) {
        auto logged = logger.warn("fix_embedded_tilesets: Not descending into '{}' because it is a symlink", entryPath);
        if (!logged)
          return std::unexpected(std::move(logged.error()));
        continue;
      }
    }
    if (entry.second) {
      auto visited = forEachRecursiveFileMatch(logger, entryPath, filenameSuffix, func);
      if (!visited)
        return std::unexpected(std::move(visited.error()));
    } else {
      auto matches = entry.first.ends_with(filenameSuffix);
      if (!matches)
        return std::unexpected(std::move(matches.error()));
      if (*matches) {
        auto visited = func(entryPath);
        if (!visited)
          return std::unexpected(std::move(visited.error()));
      }
    }
  }
  return {};
}

[[nodiscard]]
std::expected<void, StarException> fixEmbeddedTilesets(Logger& logger, String const& searchRoot, String const& tilesetPath) {
  return forEachRecursiveFileMatch(logger, searchRoot, ".json",
                                   [&](String const& path) -> std::expected<void, StarException> {
                                     auto contents = File::read_file_string(path);
                                     if (!contents)
                                       return std::unexpected(std::move(contents.error()));
                                     auto json = Json::parseJson(*contents);
                                     if (!json)
                                       return std::unexpected(std::move(json.error()));
                                     auto hasTilesets = json->contains("tilesets");
                                     if (!hasTilesets)
                                       return std::unexpected(std::move(hasTilesets.error()));
                                     if (!*hasTilesets)
                                       return {};
                                     auto fixed = repair(std::move(*json), path, tilesetPath);
                                     if (!fixed)
                                       return std::unexpected(std::move(fixed.error()));
                                     if (!*fixed)
                                       return {};
                                     auto text = (**fixed).repr(2, true);
                                     if (!text)
                                       return std::unexpected(std::move(text.error()));
                                     // Finish validation and serialization before replacing the original file.
                                     auto written = File::overwrite_file_with_rename(*text, path);
                                     if (!written)
                                       return std::unexpected(std::move(written.error()));
                                     return logger.info("Repaired {}", path);
                                   });
}

int main(int argc, char* argv[]) {
  auto run = [&]() -> std::expected<int, StarException> {
    auto nativeRuntime = NativeRuntime::create();
    if (!nativeRuntime)
      return std::unexpected(std::move(nativeRuntime.error()));
    auto rootLoader = RootLoader::create({{}, {}, {}, LogLevel::Info, false, {}});
    if (!rootLoader)
      return std::unexpected(std::move(rootLoader.error()));
    rootLoader->setSummary("Replaces embedded tilesets in Tiled JSON files with references to external tilesets. Assumes tilesets are available in the packed assets.");
    rootLoader->addArgument("searchRoot", OptionParser::Required);
    rootLoader->addArgument("tilesetsPath", OptionParser::Required);
    auto initialized = rootLoader->commandInitOrDie(argc, argv);
    if (!initialized)
      return std::unexpected(std::move(initialized.error()));
    auto const& options = initialized->second;
    auto fixed = fixEmbeddedTilesets(initialized->first->logger(), options.arguments[0], options.arguments[1]);
    if (!fixed)
      return std::unexpected(std::move(fixed.error()));
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
