import star.json;
import star.data_stream;
import star.game_types;
import star.random;
import star.ordered_map;
import star.map;
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
import star.version;

import star.damage_types;
import star.asset_source;
import star.assets;

import star.damage_database;

namespace Star {

std::expected<std::shared_ptr<DamageDatabase>, StarException> DamageDatabase::create(std::shared_ptr<Assets const> assets) {
  auto database = std::shared_ptr<DamageDatabase>(new DamageDatabase);
  auto loadElementalTypes = [&]() -> std::expected<void, StarException> {
    auto elementalConfig = assets->json("/damage/elementaltypes.config");
    if (!elementalConfig)
      return std::unexpected(std::move(elementalConfig.error()));
    auto elementalEntries = elementalConfig->iterateObject();
    if (!elementalEntries)
      return std::unexpected(std::move(elementalEntries.error()));
    for (auto const& p : *elementalEntries) {
      ElementalType type;
      auto resistance = p.second.getString("resistanceStat");
      if (!resistance)
        return std::unexpected(std::move(resistance.error()));
      type.resistanceStat = std::move(*resistance);
      auto particles =
        p.second.get("damageNumberParticles").and_then([](Json const& value) { return value.iterateObject(); });
      if (!particles)
        return std::unexpected(std::move(particles.error()));
      for (auto const& particle : *particles) {
        auto hit = enumValue<HitTypeNames>(particle.first);
        if (!hit)
          return std::unexpected(std::move(hit.error()));
        auto name = particle.second.toString();
        if (!name)
          return std::unexpected(std::move(name.error()));
        type.damageNumberParticles.set(*hit, std::move(*name));
      }
      database->m_elementalTypes.set(p.first, std::move(type));
    }
    return {};
  };
  auto elementalTypes = loadElementalTypes();
  if (!elementalTypes)
    return std::unexpected(assets->withAssetContext(std::move(elementalTypes.error()), "/damage/elementaltypes.config", "Loading elemental types"));

  auto files = assets->scanExtension("damage");
  if (!files)
    return std::unexpected(std::move(files.error()));
  auto queued = assets->queueJsons(files->get());
  if (!queued)
    return std::unexpected(std::move(queued.error()));
  for (auto const& file : files->get()) {
    auto const& path = file.string();
    auto readFile = [&]() -> std::expected<void, StarException> {
      auto config = assets->json(path);
      if (!config)
        return std::unexpected(std::move(config.error()));
      auto name = config->getString("kind");
      if (!name)
        return std::unexpected(std::move(name.error()));
      auto key = CaseInsensitiveString::create(*name);
      if (!key)
        return std::unexpected(std::move(key.error()));
      if (database->m_damageKinds.contains(*key))
        return std::unexpected(StarException(strf("Duplicate damage kind Name {}. configfile {}", *name, path)));

      DamageKind kind;
      kind.name = *name;
      auto effects = config->opt("effects");
      if (!effects)
        return std::unexpected(std::move(effects.error()));
      if (*effects) {
        auto effectEntries = (**effects).iterateObject();
        if (!effectEntries)
          return std::unexpected(std::move(effectEntries.error()));
        for (auto const& effect : *effectEntries) {
          HashMap<HitType, DamageEffect> materialEffects;
          auto hits = effect.second.iterateObject();
          if (!hits)
            return std::unexpected(std::move(hits.error()));
          for (auto const& hit : *hits) {
            auto sounds = hit.second.get("sounds", JsonArray());
            if (!sounds)
              return std::unexpected(std::move(sounds.error()));
            auto particles = hit.second.get("particles", JsonArray());
            if (!particles)
              return std::unexpected(std::move(particles.error()));
            auto hitType = enumValue<HitTypeNames>(hit.first);
            if (!hitType)
              return std::unexpected(std::move(hitType.error()));
            materialEffects.set(*hitType, DamageEffect{std::move(*sounds), std::move(*particles)});
          }
          kind.effects.set(effect.first, std::move(materialEffects));
        }
      }
      auto elementalType = config->getString("elementalType", "default");
      if (!elementalType)
        return std::unexpected(std::move(elementalType.error()));
      kind.elementalType = std::move(*elementalType);
      if (!database->m_elementalTypes.contains(kind.elementalType))
        return std::unexpected(StarException(strf("Undefined elemental type {} in damage kind {}", kind.elementalType, *name)));
      database->m_damageKinds.set(std::move(*key), std::move(kind));
      return {};
    };
    auto loaded = readFile();
    if (!loaded)
      return std::unexpected(assets->withAssetContext(std::move(loaded.error()), path, "Loading damage kind"));
  }
  return database;
}

std::expected<std::reference_wrapper<DamageKind const>, StarException>
DamageDatabase::damageKind(String const& kind) const {
  auto name = kind.empty() ? std::string_view("default") : std::string_view(kind.utf8());
  auto key = CaseInsensitiveStringView::create(name);
  if (!key)
    return std::unexpected(std::move(key.error()));
  if (auto found = m_damageKinds.find_ref(*key))
    return *found;
  return std::unexpected(StarException(strf("Unknown damage definition with kind '{}'.", name)));
}

std::expected<std::reference_wrapper<ElementalType const>, StarException> DamageDatabase::elementalType(String const& name) const {
  if (auto found = m_elementalTypes.find_ref(name))
    return *found;
  return std::unexpected(StarException(strf("Unknown elemental type with name '{}'.", name)));
}

}
