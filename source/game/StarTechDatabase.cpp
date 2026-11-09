import star.json;
import star.json_extra;
import star.game_types;
import star.ordered_map;
import star.bi_map;
import star.thread;
import star.io_device;
import star.list;
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
import star.string;
import star.format;
import star.version;

import star.asset_source;
import star.assets;

import star.tech_database;

namespace Star {

extern std::expected<EnumMap<TechType>, StarException> const TechTypeNames = EnumMap<TechType>::create({
  {TechType::Head, "Head"},
  {TechType::Body, "Body"},
  {TechType::Legs, "Legs"}
});

std::expected<std::shared_ptr<TechDatabase>, StarException> TechDatabase::create(std::shared_ptr<Assets const> assets) {
  auto result = std::shared_ptr<TechDatabase>(new TechDatabase());
  auto files = assets->scanExtension("tech");
  if (!files)
    return std::unexpected(std::move(files.error()));
  auto queued = assets->queueJsons(files->get());
  if (!queued)
    return std::unexpected(std::move(queued.error()));
  for (auto const& assetFile : files->get()) {
    auto const& file = assetFile.string();
    auto loaded = [&]() -> std::expected<void, StarException> {
      auto config = assets->json(file);
      if (!config)
        return std::unexpected(std::move(config.error()));
      auto tech = result->parseTech(*config, file);
      if (!tech)
        return std::unexpected(std::move(tech.error()));
      if (result->m_tech.contains(tech->name))
        return std::unexpected(TechDatabaseException::format("Duplicate tech named '{}', config file '{}'", tech->name, file));
      result->m_tech[tech->name] = std::move(*tech);
      return {};
    }();
    if (!loaded)
      return std::unexpected(assets->withAssetContext(std::move(loaded.error()), file, "Loading tech"));
  }
  return result;
}

bool TechDatabase::contains(String const& techName) const {
  return m_tech.contains(techName);
}

std::expected<TechConfig, StarException> TechDatabase::tech(String const& techName) const {
  if (auto tech = m_tech.find_ref(techName))
    return tech->get();
  return std::unexpected(TechDatabaseException::format("No such tech '{}'", techName));
}

std::expected<TechConfig, StarException> TechDatabase::parseTech(Json const& config, String const& path) const {
  auto parsed = [&]() -> std::expected<TechConfig, StarException> {
    if (!TechTypeNames)
      return std::unexpected(TechTypeNames.error());
    TechConfig tech;
    auto name = config.getString("name");
    if (!name)
      return std::unexpected(std::move(name.error()));
    tech.name = std::move(*name);
    tech.path = path;
    tech.parameters = config;
    auto typeName = config.getString("type");
    if (!typeName)
      return std::unexpected(std::move(typeName.error()));
    auto type = TechTypeNames->find_left_ref(*typeName);
    if (!type)
      return std::unexpected(std::move(type.error()));
    if (!*type)
      return std::unexpected(TechDatabaseException::format("Unknown tech type '{}'", *typeName));
    tech.type = (*type)->get();
    auto scripts = config.get("scripts").and_then([](Json const& value) { return value.iterateArray(); });
    if (!scripts)
      return std::unexpected(std::move(scripts.error()));
    for (auto const& script : *scripts) {
      auto scriptName = script.toString();
      if (!scriptName)
        return std::unexpected(std::move(scriptName.error()));
      tech.scripts.push_back(AssetPath::relative_to(path, *scriptName));
    }
    auto animator = config.optString("animator");
    if (!animator)
      return std::unexpected(std::move(animator.error()));
    if (*animator)
      tech.animationConfig = AssetPath::relative_to(path, **animator);
    auto description = config.getString("description");
    if (!description)
      return std::unexpected(std::move(description.error()));
    tech.description = std::move(*description);
    auto shortDescription = config.getString("shortDescription");
    if (!shortDescription)
      return std::unexpected(std::move(shortDescription.error()));
    tech.shortDescription = std::move(*shortDescription);
    auto rarityName = config.getString("rarity");
    if (!rarityName)
      return std::unexpected(std::move(rarityName.error()));
    auto rarity = maybeEnumValue<RarityNames>(*rarityName);
    if (!rarity)
      return std::unexpected(std::move(rarity.error()));
    if (!*rarity)
      return std::unexpected(TechDatabaseException::format("Unknown tech rarity '{}'", *rarityName));
    tech.rarity = **rarity;
    auto icon = config.getString("icon");
    if (!icon)
      return std::unexpected(std::move(icon.error()));
    tech.icon = AssetPath::relative_to(path, *icon);
    return tech;
  }();
  if (!parsed)
    return std::unexpected(TechDatabaseException(Star::format("Error reading tech config {}", path), parsed.error()));
  return parsed;
}

}
