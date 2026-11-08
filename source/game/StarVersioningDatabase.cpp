import star.json;
import star.json_extra;
import star.ordered_map;
import star.either;
import star.game_types;
import star.color;
import star.bi_map;
import star.random;
import star.weighted_pool;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.data_stream_extra;
import star.data_stream_devices;
import star.format;
import star.output_proxy;
import star.lexical_cast;
import star.file;
import star.logging;
import star.io_device;
import star.ref_ptr;
import star.parametric_function;
import star.multi_array_interpolator;
import star.multi_table;
import star.listener;
import std;
import star.optional;
import star.string;
import star.lua;
import star.lua_root;
import star.version;
import star.celestial_coordinate;
import star.sky_types;
import star.animation;
import star.particle;
import star.weather_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.celestial_types;
import star.utility_lua_bindings;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.stored_functions;
import star.versioning_database;
import star.root_lua_bindings;
import star.npc_database;
import star.celestial_database;
import star.byte_array;
import star.algorithm;

namespace Star {
std::string_view const VersionedJson::Magic = "SBVJ01";
std::size_t const VersionedJson::MagicStringSize = 6;
VersionNumber const VersionedJson::SubVersioning = 1;

std::expected<VersionedJson, StarException> VersionedJson::read_file(String const& filename) {
  auto file = File::open(filename, IOMode::Read);
  if (!file) return std::unexpected(std::move(file.error()));
  DataStreamIODevice ds(**file);
  auto magic = ds.read_bytes(MagicStringSize);
  if (!magic) return std::unexpected(std::move(magic.error()));
  if (*magic != ByteArray(std::span<char const>(Magic)))
    return std::unexpected(IOException::format("Wrong magic bytes at start of versioned json file, expected '{}'", Magic));
  auto result = ds.read<VersionedJson>();
  if (!result) return std::unexpected(std::move(result.error()));
  auto subVersions = readSubVersioning(ds, *result);
  if (!subVersions) return std::unexpected(std::move(subVersions.error()));
  return std::move(*result);
}
std::expected<void, StarException> VersionedJson::write_file(VersionedJson const& versionedJson,
                                                             String const& filename) {
  DataStreamBuffer ds;
  auto magic = ds.writeData(std::span<char const>(Magic));
  if (!magic) return std::unexpected(std::move(magic.error()));
  auto written = ds.write(versionedJson);
  if (!written) return std::unexpected(std::move(written.error()));
  auto subVersions = writeSubVersioning(ds, versionedJson);
  if (!subVersions) return std::unexpected(std::move(subVersions.error()));
  return File::overwrite_file_with_rename(ds.takeData(), filename);
}
std::expected<void, StarException> VersionedJson::writeSubVersioning(DataStream& ds, VersionedJson const& versionedJson) {
  auto version = ds.write(SubVersioning);
  if (!version) return std::unexpected(std::move(version.error()));
  JsonObject subVersions;
  for (auto const& pair : versionedJson.subVersions) subVersions.set(pair.first, pair.second);
  return ds.write(JsonObject{{"subVersions", subVersions}});
}
std::expected<void, StarException> VersionedJson::readSubVersioning(DataStream& ds, VersionedJson& versionedJson) {
  auto end = ds.at_end();
  if (!end) return std::unexpected(std::move(end.error()));
  if (*end) return {};
  VersionNumber extraVersioning = 0;
  auto version = ds.read(extraVersioning);
  // The historical reader tolerates an absent or unreadable optional trailer
  // version, but does not tolerate malformed metadata once version 1 is read.
  if (!version) return {};
  if (extraVersioning == 1) {
    auto source = ds.read<JsonObject>();
    if (!source) return std::unexpected(std::move(source.error()));
    auto subVersions = Json(std::move(*source)).getObject("subVersions");
    if (!subVersions) return std::unexpected(std::move(subVersions.error()));
    for (auto const& pair : *subVersions) {
      auto value = pair.second.toUInt();
      if (!value) return std::unexpected(std::move(value.error()));
      versionedJson.subVersions[pair.first] = *value;
    }
  }
  return {};
}
Json VersionedJson::toJson() const {
  JsonObject subVersionsOut;
  for (auto const& pair : subVersions) subVersionsOut.set(pair.first, pair.second);
  return JsonObject{{"id", identifier}, {"version", version}, {"content", content}, {"subVersions", subVersionsOut}};
}
std::expected<VersionedJson, StarException> VersionedJson::fromJson(Json const& source) {
  // Legacy '__' fields remain readable; modern fields take precedence.
  auto id = source.optString("id");
  if (!id) return std::unexpected(std::move(id.error()));
  auto legacyId = source.optString("__id");
  if (!legacyId) return std::unexpected(std::move(legacyId.error()));
  if (!*id) *id = std::move(*legacyId);
  auto version = source.optUInt("version");
  if (!version) return std::unexpected(std::move(version.error()));
  auto legacyVersion = source.optUInt("__version");
  if (!legacyVersion) return std::unexpected(std::move(legacyVersion.error()));
  if (!*version) *version = std::move(*legacyVersion);
  auto content = source.opt("content");
  if (!content) return std::unexpected(std::move(content.error()));
  auto legacyContent = source.opt("__content");
  if (!legacyContent) return std::unexpected(std::move(legacyContent.error()));
  if (!*content) *content = std::move(*legacyContent);
  StringMap<VersionNumber> subVersions;
  auto metadata = source.getObject("subVersions", JsonObject{});
  if (!metadata) return std::unexpected(std::move(metadata.error()));
  for (auto const& pair : *metadata) {
    auto value = pair.second.toUInt();
    if (!value) return std::unexpected(std::move(value.error()));
    subVersions[pair.first] = *value;
  }
  if (!*id || !*version || !*content)
    return std::unexpected(VersionedJsonException("VersionedJson requires an identifier, version, and content"));
  return VersionedJson{std::move(**id), static_cast<VersionNumber>(**version), std::move(**content), std::move(subVersions)};
}
bool VersionedJson::empty() const { return content.isNull(); }
std::expected<void, StarException> VersionedJson::expectIdentifier(String const& expectedIdentifier) const {
  if (identifier != expectedIdentifier)
    return std::unexpected(VersionedJsonException::format("VersionedJson identifier mismatch, expected '{}' but got '{}'", expectedIdentifier, identifier));
  return {};
}
std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, VersionedJson& versionedJson) {
  auto id = ds.read(versionedJson.identifier);
  if (!id) return std::unexpected(std::move(id.error()));
  // Keep the legacy optional-version wire representation.
  auto version = ds.read<std::optional<VersionNumber>>();
  if (!version) return std::unexpected(std::move(version.error()));
  versionedJson.version = version->value_or(VersionNumber{});
  auto content = ds.read(versionedJson.content);
  if (!content) return std::unexpected(std::move(content.error()));
  // Older writers smuggled sub-version metadata into the payload.
  if (versionedJson.content.isType(Json::Type::Object)) {
    auto hasSubVersions = versionedJson.content.contains("subVersions");
    if (!hasSubVersions) return std::unexpected(std::move(hasSubVersions.error()));
    if (*hasSubVersions) {
      auto metadata = versionedJson.content.getObject("subVersions", JsonObject{});
      if (!metadata) return std::unexpected(std::move(metadata.error()));
      for (auto const& pair : *metadata) {
        auto value = pair.second.toUInt();
        if (!value) return std::unexpected(std::move(value.error()));
        versionedJson.subVersions[pair.first] = *value;
      }
      auto stripped = versionedJson.content.eraseKey("subVersions");
      if (!stripped) return std::unexpected(std::move(stripped.error()));
      versionedJson.content = std::move(*stripped);
    }
  }
  return std::ref(ds);
}
std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, VersionedJson const& versionedJson) {
  auto id = ds.write(versionedJson.identifier);
  if (!id) return std::unexpected(std::move(id.error()));
  auto version = ds.write(std::optional<VersionNumber>(versionedJson.version));
  if (!version) return std::unexpected(std::move(version.error()));
  auto content = ds.write(versionedJson.content);
  if (!content) return std::unexpected(std::move(content.error()));
  return std::ref(ds);
}

std::expected<std::shared_ptr<VersioningDatabase>, StarException> VersioningDatabase::create(Root& root) {
  auto result = std::shared_ptr<VersioningDatabase>(new VersioningDatabase(root));
  auto initialized = result->init();
  if (!initialized) return std::unexpected(std::move(initialized.error()));
  return result;
}
std::expected<void, StarException> VersioningDatabase::init() {
  auto luaRoot = LuaRoot::create(m_root);
  if (!luaRoot) return std::unexpected(std::move(luaRoot.error()));
  m_luaRoot = std::move(*luaRoot);
  auto assets = m_root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto configured = [&]() -> std::expected<void, StarException> {
    auto versions = (*assets)->json("/versioning.config");
    if (!versions) return std::unexpected(std::move(versions.error()));
    auto entries = versions->iterateObject();
    if (!entries) return std::unexpected(std::move(entries.error()));
    for (auto const& pair : *entries) {
      auto version = pair.second.toUInt();
      if (!version) return std::unexpected(std::move(version.error()));
      m_currentVersions[pair.first] = *version;
    }
    return {};
  }();
  if (!configured)
    return std::unexpected((*assets)->withAssetContext(std::move(configured.error()), "/versioning.config", "Loading versioning configuration"));
  auto subConfigured = [&]() -> std::expected<void, StarException> {
    auto subVersions = (*assets)->json("/versioning/subVersioning.config");
    if (!subVersions) return std::unexpected(std::move(subVersions.error()));
    auto subEntries = subVersions->iterateObject();
    if (!subEntries) return std::unexpected(std::move(subEntries.error()));
    for (auto const& pair : *subEntries) {
      auto identifiers = pair.second.iterateObject();
      if (!identifiers) return std::unexpected(std::move(identifiers.error()));
      for (auto const& entry : *identifiers) {
        auto version = entry.second.toUInt();
        if (!version) return std::unexpected(std::move(version.error()));
        m_currentSubVersions[pair.first][entry.first] = *version;
      }
    }
    return {};
  }();
  if (!subConfigured)
    return std::unexpected((*assets)->withAssetContext(std::move(subConfigured.error()), "/versioning/subVersioning.config", "Loading sub-versioning configuration"));
  auto scripts = (*assets)->scan("/versioning/", ".lua");
  if (!scripts) return std::unexpected(std::move(scripts.error()));
  for (auto const& script : *scripts) {
    auto parse = [&]() -> std::expected<void, StarException> {
      auto parts = File::base_name(script).split_any("_.");
      if (!parts)
        return std::unexpected(std::move(parts.error()));
      if (parts->size() != 4 && parts->size() != 6)
        return std::unexpected(VersioningDatabaseException::format("Script file '{}' filename not of the form <identifier>_<fromversion>_<toversion>.lua or <identifier>_<atVersion>_<subIdentifier>_<fromSubVersion>_<toSubVersion>.lua", script));
      auto identifier = (*parts)[0].to_lower();
      if (!identifier)
        return std::unexpected(std::move(identifier.error()));
      auto firstVersion = lexicalCast<VersionNumber>((*parts)[1]);
      if (!firstVersion)
        return std::unexpected(std::move(firstVersion.error()));
      if (parts->size() == 4) {
        auto toVersion = lexicalCast<VersionNumber>((*parts)[2]);
        if (!toVersion)
          return std::unexpected(std::move(toVersion.error()));
        m_versionUpdateScripts[*identifier].push_back({script, *firstVersion, *toVersion});
      } else {
        auto subIdentifier = (*parts)[2].to_lower();
        if (!subIdentifier)
          return std::unexpected(std::move(subIdentifier.error()));
        auto fromVersion = lexicalCast<VersionNumber>((*parts)[3]);
        if (!fromVersion)
          return std::unexpected(std::move(fromVersion.error()));
        auto toVersion = lexicalCast<VersionNumber>((*parts)[4]);
        if (!toVersion)
          return std::unexpected(std::move(toVersion.error()));
        m_subVersionUpdateScripts[*identifier][*firstVersion][*subIdentifier].push_back(
          {script, *fromVersion, *toVersion});
      }
      return {};
    }();
    if (!parse)
      return std::unexpected(VersioningDatabaseException(Star::format("Error parsing version information from versioning script '{}'", script), (*assets)->withAssetContext(std::move(parse.error()), script, "Parsing versioning script filename")));
  }
  // Preserve the existing stable selection order for matching upgrades.
  auto order = [](VersionUpdateScript const& lhs, VersionUpdateScript const& rhs) {
    if (lhs.fromVersion != rhs.fromVersion) return lhs.fromVersion < rhs.fromVersion;
    return lhs.toVersion < rhs.toVersion;
  };
  for (auto&& pair : m_versionUpdateScripts)
    pair.second.sort(order);
  for (auto&& pair : m_subVersionUpdateScripts)
    for (auto&& version : pair.second)
      for (auto&& scripts : version.second)
        scripts.second.sort(order);
  return {};
}
std::expected<VersionedJson, StarException> VersioningDatabase::makeCurrentVersionedJson(String const& identifier, Json const& content) const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  auto version = m_currentVersions.get(identifier);
  if (!version) return std::unexpected(std::move(version.error()));
  return VersionedJson{identifier, version->get(), content, m_currentSubVersions.value(identifier)};
}
std::expected<bool, StarException> VersioningDatabase::versionedJsonCurrent(VersionedJson const& json) const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  auto version = m_currentVersions.get(json.identifier);
  if (!version) return std::unexpected(std::move(version.error()));
  return json.version == version->get() && json.subVersions == m_currentSubVersions.value(json.identifier);
}
std::expected<VersionedJson, StarException> VersioningDatabase::updateVersionedJson(VersionedJson const& json) const {
  return updateVersionedJson(json, std::nullopt);
}
std::expected<VersionedJson, StarException> VersioningDatabase::updateVersionedJson(VersionedJson const& versionedJson, std::optional<std::reference_wrapper<ItemDatabase const>> itemCatalog) const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  auto celestialDatabase = CelestialMasterDatabase::create(m_root);
  if (!celestialDatabase) return std::unexpected(std::move(celestialDatabase.error()));
  VersionedJson result = versionedJson;
  auto targetVersion = m_currentVersions.maybe(versionedJson.identifier);
  if (!targetVersion)
    return std::unexpected(VersioningDatabaseException::format("Versioned JSON has an unregistered identifier '{}'", versionedJson.identifier));
  LuaCallbacks celestialCallbacks;
  auto registered = celestialCallbacks.registerCallback("parameters", [&celestialDatabase](Json const& coordinate) -> std::expected<Json, StarException> {
    auto coord = CelestialCoordinate::create(coordinate);
    if (!coord) return std::unexpected(std::move(coord.error()));
    auto parameters = (*celestialDatabase)->parameters(*coord);
    if (!parameters) return std::unexpected(std::move(parameters.error()));
    if (!*parameters)
      return std::unexpected(VersioningDatabaseException("Missing celestial parameters in versioning script"));
    return (*parameters)->diskStore();
  });
  if (!registered) return std::unexpected(std::move(registered.error()));
  auto invokeUpdate = [&](String const& script, Json const& content) -> std::expected<Json, StarException> {
    auto context = m_luaRoot->createContext();
    if (!context) return std::unexpected(std::move(context.error()));
    auto previousRoot = context->get("root");
    if (!previousRoot) return std::unexpected(std::move(previousRoot.error()));
    auto previousVersioning = context->get("versioning");
    if (!previousVersioning) return std::unexpected(std::move(previousVersioning.error()));
    auto invoked = [&]() -> std::expected<Json, StarException> {
      auto removed = context->remove("root");
      if (!removed) return std::unexpected(std::move(removed.error()));
      removed = context->remove("versioning");
      if (!removed) return std::unexpected(std::move(removed.error()));
      auto callbacks = LuaBindings::makeRootCallbacks(m_root, itemCatalog);
      if (!callbacks) return std::unexpected(std::move(callbacks.error()));
      auto installed = context->setCallbacks("root", *callbacks);
      if (!installed) return std::unexpected(std::move(installed.error()));
      auto utility = LuaBindings::makeUtilityCallbacks(m_root.random(), m_root.logger(), m_root.logMap());
      if (!utility) return std::unexpected(std::move(utility.error()));
      installed = context->setCallbacks("sb", *utility);
      if (!installed) return std::unexpected(std::move(installed.error()));
      installed = context->setCallbacks("celestial", celestialCallbacks);
      if (!installed) return std::unexpected(std::move(installed.error()));
      auto versioning = makeVersioningCallbacks(itemCatalog);
      if (!versioning) return std::unexpected(std::move(versioning.error()));
      installed = context->setCallbacks("versioning", *versioning);
      if (!installed) return std::unexpected(std::move(installed.error()));
      auto assets = m_root.assets();
      if (!assets) return std::unexpected(std::move(assets.error()));
      auto bytes = (*assets)->bytes(script);
      if (!bytes) return std::unexpected(std::move(bytes.error()));
      auto loaded = context->load(**bytes, script);
      if (!loaded) return std::unexpected(std::move(loaded.error()));
      return context->invokePath<Json>("update", content);
    }();
    // Both restorations run even if script execution or one restoration fails.
    auto restoredRoot = context->set("root", *previousRoot);
    auto restoredVersioning = context->set("versioning", *previousVersioning);
    if (!restoredRoot) return std::unexpected(std::move(restoredRoot.error()));
    if (!restoredVersioning) return std::unexpected(std::move(restoredVersioning.error()));
    return invoked;
  };
  auto updated = [&]() -> std::expected<void, StarException> {
    auto identifier = versionedJson.identifier.to_lower();
    if (!identifier) return std::unexpected(std::move(identifier.error()));
    for (auto const& script : m_versionUpdateScripts.value(*identifier)) {
      for (auto const& subScripts : m_subVersionUpdateScripts.value(*identifier).value(result.version)) {
        auto targetSubVersion = m_currentSubVersions.value(result.identifier).value(subScripts.first);
        for (auto const& subScript : subScripts.second) {
          if (result.subVersions.value(subScripts.first) >= targetSubVersion) break;
          if (subScript.fromVersion == result.subVersions.value(subScripts.first)) {
            auto content = invokeUpdate(subScript.script, result.content);
            if (!content) return std::unexpected(std::move(content.error()));
            result.content = std::move(*content);
            if (!result.content)
              return std::unexpected(VersioningDatabaseException::format("Could not bring versionedJson with identifier '{}' and version {} forward to current version of {}, conversion script of sub identifier '{}' from {} to {} returned null (un-upgradeable)", versionedJson.identifier, result.version, Star::output_optional(targetVersion), subScripts.first, subScript.fromVersion, subScript.toVersion));
            auto logged = m_root.logger().debug("Brought versionedJson '{}' sub identifier '{}' from version {} to {}", versionedJson.identifier, subScripts.first, result.subVersions.value(subScripts.first), subScript.toVersion);
            if (!logged) return std::unexpected(std::move(logged.error()));
            result.subVersions[subScripts.first] = subScript.toVersion;
          }
        }
      }
      if (result.version >= *targetVersion) break;
      if (script.fromVersion == result.version) {
        auto content = invokeUpdate(script.script, result.content);
        if (!content) return std::unexpected(std::move(content.error()));
        result.content = std::move(*content);
        if (!result.content)
          return std::unexpected(VersioningDatabaseException::format("Could not bring versionedJson with identifier '{}' and version {} forward to current version of {}, conversion script from {} to {} returned null (un-upgradeable)", versionedJson.identifier, result.version, Star::output_optional(targetVersion), script.fromVersion, script.toVersion));
        auto logged = m_root.logger().debug("Brought versionedJson '{}' from version {} to {}", versionedJson.identifier, result.version, script.toVersion);
        if (!logged) return std::unexpected(std::move(logged.error()));
        result.version = script.toVersion;
      }
    }
    return {};
  }();
  if (!updated)
    return std::unexpected(VersioningDatabaseException(Star::format("Could not bring versionedJson with identifier '{}' and version {} forward to current version of {}", versionedJson.identifier, result.version, Star::output_optional(targetVersion)), std::move(updated.error())));
  if (result.version > *targetVersion)
    return std::unexpected(VersioningDatabaseException::format("VersionedJson with identifier '{}' and version {} is newer than current version of {}, cannot load", versionedJson.identifier, result.version, Star::output_optional(targetVersion)));
  if (result.version != *targetVersion)
    return std::unexpected(VersioningDatabaseException::format("Could not bring VersionedJson with identifier '{}' and version {} forward to current version of {}, best version was {}", versionedJson.identifier, result.version, Star::output_optional(targetVersion), result.version));
  return result;
}
std::expected<Json, StarException> VersioningDatabase::loadVersionedJson(VersionedJson const& json, String const& expectedIdentifier) const {
  return loadVersionedJson(json, expectedIdentifier, std::nullopt);
}
std::expected<Json, StarException> VersioningDatabase::loadVersionedJson(VersionedJson const& json, String const& expectedIdentifier, std::optional<std::reference_wrapper<ItemDatabase const>> itemCatalog) const {
  auto identifier = json.expectIdentifier(expectedIdentifier);
  if (!identifier) return std::unexpected(std::move(identifier.error()));
  auto current = versionedJsonCurrent(json);
  if (!current) return std::unexpected(std::move(current.error()));
  if (*current) return json.content;
  auto updated = updateVersionedJson(json, itemCatalog);
  if (!updated) return std::unexpected(std::move(updated.error()));
  return std::move(updated->content);
}
std::expected<LuaCallbacks, StarException> VersioningDatabase::makeVersioningCallbacks(std::optional<std::reference_wrapper<ItemDatabase const>> itemCatalog) const {
  LuaCallbacks callbacks;
  auto registered = callbacks.registerCallback(
    "loadVersionedJson",
    [&database = *this, itemCatalog](String const& storagePath) -> std::expected<Json, StarException> {
      auto loaded = [&]() -> std::expected<Json, StarException> {
        auto filePath = File::full_path(database.m_root.toStoragePath(storagePath));
        if (!filePath)
          return std::unexpected(std::move(filePath.error()));
        auto basePath = File::full_path(database.m_root.toStoragePath("."));
        if (!basePath)
          return std::unexpected(std::move(basePath.error()));
        auto withinStorage = filePath->starts_with(*basePath);
        if (!withinStorage)
          return std::unexpected(std::move(withinStorage.error()));
        if (!*withinStorage)
          return std::unexpected(
            VersioningDatabaseException("Cannot load external VersionedJson outside of the Root storage path"));
        auto file = VersionedJson::read_file(*filePath);
        if (!file)
          return std::unexpected(std::move(file.error()));
        auto updated = database.updateVersionedJson(*file, itemCatalog);
        if (!updated)
          return std::unexpected(std::move(updated.error()));
        return std::move(updated->content);
      }();
      if (!loaded && loaded.error().is<IOException>()) {
        auto logged =
          database.m_root.logger().debug("Unable to load versioned JSON file {} in versioning script: {}", storagePath,
                                         Star::output_exception(ExceptionDiagnostic(loaded.error()), false));
        if (!logged)
          return std::unexpected(std::move(logged.error()));
        return Json();
      }
      return loaded;
    });
  if (!registered) return std::unexpected(std::move(registered.error()));
  return callbacks;
}
}
