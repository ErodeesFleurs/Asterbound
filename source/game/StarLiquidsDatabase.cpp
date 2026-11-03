import star.json;
import star.json_extra;
import star.list;
import star.bi_map;
import star.either;
import star.color;
import star.game_types;
import star.liquid_types;
import star.material_types;
import star.map;
import star.ordered_set;
import star.random;
import star.data_stream;
import star.lexical_cast;
import star.ordered_map;
import star.thread;
import star.io_device;
import star.image_processing;
import star.hash;
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

import star.collision_block;
import star.cellular_liquid;
import star.item_descriptor;
import star.asset_source;
import star.assets;

import star.liquids_database;
import star.material_database;


import star.algorithm;
namespace Star {

LiquidSettings::LiquidSettings() : id(EmptyLiquidId) {}

std::expected<std::shared_ptr<LiquidsDatabase>, StarException> LiquidsDatabase::create(std::shared_ptr<Assets const> const& assets, std::shared_ptr<MaterialDatabase const> const& materialDatabase) {
  auto result = std::shared_ptr<LiquidsDatabase>(new LiquidsDatabase());
  auto config = assets->json("/liquids.config");
  if (!config) return std::unexpected(std::move(config.error()));
  auto engine = config->get("liquidEngineParameters");
  if (!engine) return std::unexpected(std::move(engine.error()));
  constexpr std::pair<std::string_view, float LiquidCellEngineParameters::*> fields[] = {
    {"lateralMoveFactor", &LiquidCellEngineParameters::lateralMoveFactor},
    {"spreadOverfillUpFactor", &LiquidCellEngineParameters::spreadOverfillUpFactor},
    {"spreadOverfillLateralFactor", &LiquidCellEngineParameters::spreadOverfillLateralFactor},
    {"spreadOverfillDownFactor", &LiquidCellEngineParameters::spreadOverfillDownFactor},
    {"pressureEqualizeFactor", &LiquidCellEngineParameters::pressureEqualizeFactor},
    {"pressureMoveFactor", &LiquidCellEngineParameters::pressureMoveFactor},
    {"maximumPressureLevelImbalance", &LiquidCellEngineParameters::maximumPressureLevelImbalance},
    {"minimumLivenPressureChange", &LiquidCellEngineParameters::minimumLivenPressureChange},
    {"minimumLivenLevelChange", &LiquidCellEngineParameters::minimumLivenLevelChange},
    {"minimumLiquidLevel", &LiquidCellEngineParameters::minimumLiquidLevel},
    {"interactTransformationLevel", &LiquidCellEngineParameters::interactTransformationLevel}};
  for (auto const& [name, member] : fields) {
    auto value = engine->getFloat(name);
    if (!value) return std::unexpected(std::move(value.error()));
    result->m_liquidEngineParameters.*member = *value;
  }
  {
    auto value = config->getFloat("backgroundDrain");
    if (!value) return std::unexpected(std::move(value.error()));
    result->m_backgroundDrain = std::move(*value);
  }
  result->m_liquidNames["empty"] = EmptyLiquidId;
  auto liquids = assets->scanExtension("liquid");
  if (!liquids) return std::unexpected(std::move(liquids.error()));
  auto queued = assets->queueJsons(liquids->get());
  if (!queued) return std::unexpected(std::move(queued.error()));
  for (auto const& path : liquids->get()) {
    auto const& file = path.string();
    auto readFile = [&]() -> std::expected<void, StarException> {
      auto config = assets->json(file);
      if (!config) return std::unexpected(std::move(config.error()));
      auto id = config->getUInt("liquidId");
      if (!id) return std::unexpected(std::move(id.error()));
      if (*id != (LiquidId)*id)
        return std::unexpected(LiquidException(strf("Liquid id {} does not fall in the valid range, wrapped to {}\n", *id, (LiquidId)*id)));
      auto entry = std::make_shared<LiquidSettings>();
      entry->id = (LiquidId)*id;
      {
        auto value = config->getString("name");
        if (!value) return std::unexpected(std::move(value.error()));
        entry->name = std::move(*value);
      }
      entry->path = file;
      entry->config = *config;
      JsonObject descriptions;
      auto members = config->iterateObject();
      if (!members) return std::unexpected(std::move(members.error()));
      for (auto const& pair : *members) {
        auto description = pair.first.endsWith("Description");
        if (!description) return std::unexpected(std::move(description.error()));
        if (*description) descriptions[pair.first] = pair.second;
      }
      {
        auto value = config->getString("description", "");
        if (!value) return std::unexpected(std::move(value.error()));
        descriptions["description"] = std::move(*value);
      }
      entry->descriptions = std::move(descriptions);
      {
        auto value = config->getUInt("tickDelta");
        if (!value) return std::unexpected(std::move(value.error()));
        entry->tickDelta = std::move(*value);
      }
      auto colorConfig = config->get("color");
      if (!colorConfig) return std::unexpected(std::move(colorConfig.error()));
      {
        auto value = jsonToVec4B(*colorConfig);
        if (!value) return std::unexpected(std::move(value.error()));
        entry->liquidColor = std::move(*value);
      }
      auto lightConfig = config->opt("radiantLight");
      if (!lightConfig) return std::unexpected(std::move(lightConfig.error()));
      if (*lightConfig) {
        auto light = jsonToVec3B(**lightConfig);
        if (!light) return std::unexpected(std::move(light.error()));
        entry->radiantLightLevel = Color::rgb(*light).toRgbF();
      }
      auto dropConfig = config->get("itemDrop", {});
      if (!dropConfig) return std::unexpected(std::move(dropConfig.error()));
      {
        auto value = ItemDescriptor::create(*dropConfig);
        if (!value) return std::unexpected(std::move(value.error()));
        entry->itemDrop = std::move(*value);
      }
      {
        auto value = config->getArray("statusEffects", {});
        if (!value) return std::unexpected(std::move(value.error()));
        entry->statusEffects = std::move(*value);
      }
      auto interactions = config->getArray("interactions", {});
      if (!interactions) return std::unexpected(std::move(interactions.error()));
      for (auto const& interaction : *interactions) {
        auto liquid = interaction.getUInt("liquid");
        if (!liquid) return std::unexpected(std::move(liquid.error()));
        LiquidInteractionResult interactionResult;
        auto materialResult = interaction.optString("materialResult");
        if (!materialResult) return std::unexpected(std::move(materialResult.error()));
        if (*materialResult) {
          auto material = materialDatabase->materialId(**materialResult);
          if (!material) return std::unexpected(std::move(material.error()));
          interactionResult = makeLeft<MaterialId>(*material);
        } else {
          auto liquidResult = interaction.optUInt("liquidResult");
          if (!liquidResult) return std::unexpected(std::move(liquidResult.error()));
          if (!*liquidResult)
            return std::unexpected(LiquidException::format("Neither resultMaterial or resultLiquid specified in liquid interaction of liquid {}", entry->id));
          interactionResult = makeRight<LiquidId>(**liquidResult);
        }
        entry->interactions[(LiquidId)*liquid] = std::move(interactionResult);
      }
      result->m_settings.set(entry->id, entry);
      auto added = result->m_liquidNames.add(entry->name, entry->id);
      if (!added) return std::unexpected(std::move(added.error()));
      return {};
    };
    auto loaded = readFile();
    if (!loaded) return std::unexpected(LiquidException(strf("Error loading liquid file {}", file), loaded.error()));
  }
  return result;
}

LiquidCellEngineParameters LiquidsDatabase::liquidEngineParameters() const {
  return m_liquidEngineParameters;
}

StringList LiquidsDatabase::liquidNames() const {
  return m_liquidNames.keys();
}

List<std::shared_ptr<LiquidSettings const>> LiquidsDatabase::allLiquidSettings() const {
  return filtered<List<std::shared_ptr<LiquidSettings const>>>(m_settings, std::mem_fn(&std::shared_ptr<LiquidSettings const>::operator bool));
}

std::expected<LiquidId, StarException> LiquidsDatabase::liquidId(String const& str) const {
  auto id = m_liquidNames.get(str);
  if (!id) return std::unexpected(std::move(id.error()));
  return id->get();
}

std::expected<String, StarException> LiquidsDatabase::liquidName(LiquidId liquidId) const {
  if (liquidId == EmptyLiquidId) return "empty";
  if (auto settings = liquidSettings(liquidId)) return settings->name;
  return std::unexpected(LiquidException::format("invalid liquid id {}", liquidId));
}

std::expected<String, StarException> LiquidsDatabase::liquidDescription(LiquidId liquidId, String const& species) const {
  if (liquidId == EmptyLiquidId) return String();
  if (auto settings = liquidSettings(liquidId)) {
    auto description = settings->descriptions.getString("description");
    if (!description) return std::unexpected(std::move(description.error()));
    return settings->descriptions.getString(strf("{}Description", species), *description);
  }
  return std::unexpected(LiquidException::format("invalid liquid id {}", liquidId));
}

std::expected<String, StarException> LiquidsDatabase::liquidDescription(LiquidId liquidId) const {
  if (liquidId == EmptyLiquidId) return String();
  if (auto settings = liquidSettings(liquidId)) return settings->descriptions.getString("description");
  return std::unexpected(LiquidException::format("invalid liquid id {}", liquidId));
}

std::expected<std::optional<String>, StarException> LiquidsDatabase::liquidPath(LiquidId liquidId) const {
  if (liquidId == EmptyLiquidId) return std::optional<String>();
  if (auto settings = liquidSettings(liquidId)) return settings->path;
  return std::optional<String>();
}

std::expected<std::optional<Json>, StarException> LiquidsDatabase::liquidConfig(LiquidId liquidId) const {
  if (liquidId == EmptyLiquidId) return std::optional<Json>();
  if (auto settings = liquidSettings(liquidId)) return settings->config;
  return std::optional<Json>();
}

std::optional<LiquidInteractionResult> LiquidsDatabase::interact(LiquidId target, LiquidId other) const {
  if (auto settings = liquidSettings(target))
    return settings->interactions.value(other);
  return {};
}

}
