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
import star.vector;
import star.option_parser;
import star.root;
import star.root_loader;
import star.tileset_database;
import star.dungeon_generator;
import star.dungeon_tmx_part;

using namespace Star;
using namespace Star::Dungeon;

using TileName = String;
using TileProperty = std::pair<String, String>;
using MatchCriteria = std::variant<TileName, TileProperty>;
struct SearchParameters {
  MatchCriteria criteria;
};
using MatchReporter = std::function<std::expected<void, StarException>(String const&, Vec2I const&)>;
String const MapFilenameSuffix = ".json";

[[nodiscard]]
std::expected<std::optional<String>, StarException> matchTile(SearchParameters const& search, Tiled::Tile const& tile) {
  auto const& properties = tile.properties;
  if (std::holds_alternative<TileName>(search.criteria)) {
    auto const& namePattern = std::get<TileName>(search.criteria);
    auto tileName = properties.opt<String>("//name");
    if (!tileName)
      return std::unexpected(std::move(tileName.error()));
    if (*tileName) {
      auto matches = (**tileName).regex_match(namePattern);
      if (!matches)
        return std::unexpected(std::move(matches.error()));
      if (*matches)
        return std::move(*tileName);
    }
  } else if (std::holds_alternative<TileProperty>(search.criteria)) {
    auto const& property = std::get<TileProperty>(search.criteria);
    auto value = properties.opt<String>(property.first);
    if (!value)
      return std::unexpected(std::move(value.error()));
    if (*value) {
      auto matches = (**value).regex_match(property.second);
      if (!matches)
        return std::unexpected(std::move(matches.error()));
      if (*matches) {
        auto name = properties.opt<String>("//name");
        if (!name)
          return std::unexpected(std::move(name.error()));
        return std::optional<String>{name->value_or("?")};
      }
    }
  }
  return std::optional<String>{};
}

[[nodiscard]]
std::expected<void, StarException> grepTileLayer(SearchParameters const& search, TMXMap const& map, TMXTileLayer const& tileLayer, MatchReporter const& callback) {
  auto visited = tileLayer.forEachTile(map, [&](Vec2I pos, Tile const& tile) -> std::expected<bool, StarException> {
    auto name = matchTile(search, static_cast<Tiled::Tile const&>(tile));
    if (!name)
      return std::unexpected(std::move(name.error()));
    if (*name) {
      auto reported = callback(**name, pos);
      if (!reported)
        return std::unexpected(std::move(reported.error()));
    }
    return false;
  });
  if (!visited)
    return std::unexpected(std::move(visited.error()));
  return {};
}

[[nodiscard]]
std::expected<void, StarException> grepObjectGroup(SearchParameters const& search, TMXObjectGroup const& group, MatchReporter const& callback) {
  for (auto const& object : group.objects()) {
    auto name = matchTile(search, object->tile());
    if (!name)
      return std::unexpected(std::move(name.error()));
    if (*name) {
      auto reported = callback(**name, object->pos());
      if (!reported)
        return std::unexpected(std::move(reported.error()));
    }
  }
  return {};
}

[[nodiscard]]
std::expected<void, StarException> grepMap(Root& root, SearchParameters const& search, String const& file) {
  auto contents = File::read_file_string(file);
  if (!contents)
    return std::unexpected(std::move(contents.error()));
  auto json = Json::parseJson(*contents);
  if (!json)
    return std::unexpected(std::move(json.error()));
  auto map = TMXMap::create(root, *json);
  if (!map)
    return std::unexpected(std::move(map.error()));
  for (auto const& layer : (*map)->tileLayers()) {
    auto searched = grepTileLayer(search, **map, *layer, [&](String const& name, Vec2I const& pos) {
      return Star::print("{}: {}: {} @ {}\n", file, layer->name(), name, pos);
    });
    if (!searched)
      return std::unexpected(std::move(searched.error()));
  }
  for (auto const& group : (*map)->objectGroups()) {
    auto searched = grepObjectGroup(search, *group, [&](String const& name, Vec2I const& pos) {
      return Star::print("{}: {}: {} @ {}\n", file, group->name(), name, pos);
    });
    if (!searched)
      return std::unexpected(std::move(searched.error()));
  }
  return {};
}

[[nodiscard]]
std::expected<void, StarException> grepDirectory(Root& root, SearchParameters const& search, String const& directory) {
  auto entries = File::directory_entries(directory);
  if (!entries)
    return std::unexpected(std::move(entries.error()));
  for (auto const& entry : *entries) {
    String path = File::relative_to(directory, entry.first);
    // A symlinked directory may escape the tree or revisit a parent.
    if (entry.second) {
      auto symlink = File::is_symlink(path);
      if (!symlink)
        return std::unexpected(std::move(symlink.error()));
      if (*symlink) {
        auto logged = root.logger().warn("map_grep: Not descending into '{}' because it is a symlink", path);
        if (!logged)
          return std::unexpected(std::move(logged.error()));
        continue;
      }
    }
    if (entry.second) {
      auto searched = grepDirectory(root, search, path);
      if (!searched)
        return std::unexpected(std::move(searched.error()));
    } else {
      auto matches = entry.first.ends_with(MapFilenameSuffix);
      if (!matches)
        return std::unexpected(std::move(matches.error()));
      if (*matches) {
        auto searched = grepMap(root, search, path);
        if (!searched)
          return std::unexpected(std::move(searched.error()));
      }
    }
  }
  return {};
}

[[nodiscard]]
std::expected<void, StarException> grepPath(Root& root, SearchParameters const& search, String const& path) {
  auto file = File::is_regular_file(path);
  if (!file)
    return std::unexpected(std::move(file.error()));
  if (*file)
    return grepMap(root, search, path);
  auto directory = File::is_directory(path);
  if (!directory)
    return std::unexpected(std::move(directory.error()));
  if (*directory)
    return grepDirectory(root, search, path);
  return {};
}

[[nodiscard]]
std::expected<MatchCriteria, StarException> parseMatchCriteria(String const& text) {
  auto property = text.contains("=");
  if (!property)
    return std::unexpected(std::move(property.error()));
  if (*property) {
    auto parts = text.split('=', 1);
    if (!parts)
      return std::unexpected(std::move(parts.error()));
    return MatchCriteria{TileProperty{(*parts)[0], (*parts)[1]}};
  }
  return MatchCriteria{TileName(text)};
}

int main(int argc, char* argv[]) {
  auto run = [&]() -> std::expected<int, StarException> {
    auto nativeRuntime = NativeRuntime::create();
    if (!nativeRuntime)
      return std::unexpected(std::move(nativeRuntime.error()));
    auto rootLoader = RootLoader::create({{}, {}, {}, LogLevel::Warn, false, {}});
    if (!rootLoader)
      return std::unexpected(std::move(rootLoader.error()));
    rootLoader->setSummary("Search Tiled map files for specific materials or objects.");
    rootLoader->addArgument("MaterialId|ObjectName|Property=Value", OptionParser::Required);
    rootLoader->addArgument("JsonMapFile", OptionParser::Multiple);
    auto initialized = rootLoader->commandInitOrDie(argc, argv);
    if (!initialized)
      return std::unexpected(std::move(initialized.error()));
    auto const& files = initialized->second.arguments;
    auto criteria = parseMatchCriteria(files[0]);
    if (!criteria)
      return std::unexpected(std::move(criteria.error()));
    SearchParameters search{std::move(*criteria)};
    for (std::size_t i = 1; i < files.size(); ++i) {
      auto searched = grepPath(*initialized->first, search, files[i]);
      if (!searched)
        return std::unexpected(std::move(searched.error()));
    }
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
