import star.json;
import star.json_extra;
import star.strong_typedef;
import star.data_stream;
import star.id_map;
import star.ordered_map;
import star.bi_map;
import star.thread;
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
import star.string;
import star.format;
import star.optional;

import star.status_types;
import star.asset_source;
import star.assets;

import star.status_effect_database;

namespace Star {

std::expected<std::shared_ptr<StatusEffectDatabase>, StarException> StatusEffectDatabase::create(std::shared_ptr<Assets const> assets) {
  auto database = std::shared_ptr<StatusEffectDatabase>(new StatusEffectDatabase());
  auto files = assets->scan_extension("statuseffect");
  if (!files) return std::unexpected(std::move(files.error()));
  auto queued = assets->queue_jsons(files->get());
  if (!queued) return std::unexpected(std::move(queued.error()));
  for (auto const& asset : files->get()) {
    auto const& file = asset.string();
    auto loaded = [&]() -> std::expected<void, StarException> {
      auto config = assets->json(file);
      if (!config) return std::unexpected(std::move(config.error()));
      auto effect = database->parseUniqueEffect(*config, file);
      if (!effect) return std::unexpected(std::move(effect.error()));
      auto name = effect->name;
      if (!database->m_uniqueEffects.try_emplace(std::move(name), std::move(*effect)).second)
        return std::unexpected(StatusEffectDatabaseException::format(
            "Duplicate stat effect named '{}', config file '{}'", effect->name, file));
      return {};
    }();
    if (!loaded)
      return std::unexpected(assets->with_asset_context(std::move(loaded.error()), file, "Loading status effect"));
  }
  return database;
}

std::expected<std::reference_wrapper<UniqueStatusEffectConfig const>, StarException>
StatusEffectDatabase::unique_effect_config(UniqueStatusEffect const& effect) const & {
  if (auto definition = m_uniqueEffects.find_ref(effect))
    return *definition;
  return std::unexpected(StatusEffectDatabaseException::format("No such unique stat effect '{}'", effect));
}

std::expected<UniqueStatusEffectConfig, StarException> StatusEffectDatabase::parseUniqueEffect(Json const& config, String const& path) const {
  auto parsed = [&]() -> std::expected<UniqueStatusEffectConfig, StarException> {
    UniqueStatusEffectConfig effect;
    auto name = config.get_string("name");
    if (!name) return std::unexpected(std::move(name.error()));
    effect.name = std::move(*name);
    auto blockingStat = config.opt_string("blockingStat");
    if (!blockingStat) return std::unexpected(std::move(blockingStat.error()));
    effect.blockingStat = std::move(*blockingStat);
    auto effectConfig = config.get("effectConfig", JsonObject());
    if (!effectConfig) return std::unexpected(std::move(effectConfig.error()));
    effect.effectConfig = std::move(*effectConfig);
    auto defaultDuration = config.get_float("defaultDuration", 0.0f);
    if (!defaultDuration) return std::unexpected(std::move(defaultDuration.error()));
    effect.defaultDuration = *defaultDuration;
    auto scriptsConfig = config.get("scripts", JsonArray{});
    if (!scriptsConfig) return std::unexpected(std::move(scriptsConfig.error()));
    auto scripts = json_to_string_list(*scriptsConfig);
    if (!scripts) return std::unexpected(std::move(scripts.error()));
    for (auto const& script : *scripts)
      effect.scripts.push_back(AssetPath::relative_to(path, script));
    auto scriptDelta = config.get_uint("scriptDelta", 1);
    if (!scriptDelta) return std::unexpected(std::move(scriptDelta.error()));
    effect.scriptDelta = *scriptDelta;
    auto animationConfig = config.opt_string("animationConfig");
    if (!animationConfig) return std::unexpected(std::move(animationConfig.error()));
    if (*animationConfig)
      effect.animationConfig = AssetPath::relative_to(path, **animationConfig);
    auto label = config.get_string("label", "");
    if (!label) return std::unexpected(std::move(label.error()));
    effect.label = std::move(*label);
    auto description = config.get_string("description", "");
    if (!description) return std::unexpected(std::move(description.error()));
    effect.description = std::move(*description);
    auto icon = config.opt_string("icon");
    if (!icon) return std::unexpected(std::move(icon.error()));
    if (*icon)
      effect.icon = AssetPath::relative_to(path, **icon);
    return effect;
  }();
  if (!parsed)
    return std::unexpected(StatusEffectDatabaseException("Error reading StatusEffect config", ExceptionDiagnostic(parsed.error())));
  return parsed;
}

JsonObject UniqueStatusEffectConfig::to_json() const {
  return {
    {"name", name},
    {"blockingStat", blockingStat ? Json(*blockingStat) : Json()},
    {"effectConfig", effectConfig},
    {"defaultDuration", defaultDuration},
    {"scripts", json_from_string_list(scripts)},
    {"scriptDelta", scriptDelta},
    {"animationConfig", animationConfig ? Json(*animationConfig) : Json()},
    {"label", label},
    {"description", description},
    {"icon", icon ? Json(*icon) : Json()}
  };
}

}// namespace Star
