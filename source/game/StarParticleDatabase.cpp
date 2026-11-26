import std;
import star.json;
import star.string;
import star.format;

import star.particle;
import star.assets;
import star.root;

import star.particle_database;

namespace Star {

namespace {

enum class NullVariance { Reject, UseDefault };

struct ParticleDefinition {
  Particle particle;
  Particle variance;
};

std::expected<ParticleDefinition, StarException> parse_particle_definition(
    Root& root, Json const& definition, String const& relative_path, NullVariance null_variance) {
  if (auto object = definition.object_ptr(); !object)
    return std::unexpected(std::move(object.error()));
  auto particle = Particle::create(root, definition, relative_path);
  if (!particle)
    return std::unexpected(std::move(particle.error()));
  auto variance_config = definition.find("variance");
  if (!variance_config)
    return std::unexpected(std::move(variance_config.error()));
  auto has_variance = variance_config->has_value() &&
      (null_variance == NullVariance::Reject || !(*variance_config)->get().is_null());
  static Json const empty_variance = JsonObject{};
  auto const& variance_definition = has_variance ? (*variance_config)->get() : empty_variance;
  if (auto object = variance_definition.object_ptr(); !object)
    return std::unexpected(std::move(object.error()));
  auto variance = Particle::create(root, variance_definition, relative_path);
  if (!variance)
    return std::unexpected(std::move(variance.error()));
  return ParticleDefinition{std::move(*particle), std::move(*variance)};
}

}

std::expected<std::shared_ptr<ParticleConfig>, StarException> ParticleConfig::create(Root& root, Json const& config) {
  auto kind = config.get_string("kind");
  if (!kind)
    return std::unexpected(std::move(kind.error()));
  auto definition = config.query("definition");
  if (!definition)
    return std::unexpected(std::move(definition.error()));
  auto parsed = parse_particle_definition(root, *definition, "", NullVariance::UseDefault);
  if (!parsed)
    return std::unexpected(std::move(parsed.error()));
  return std::make_shared<ParticleConfig>(ConstructionKey{}, root, std::move(*kind),
      std::move(parsed->particle), std::move(parsed->variance));
}

String const& ParticleConfig::kind() const {
  return m_kind;
}

Particle ParticleConfig::instance() const {
  auto particle = m_particle;
  particle.apply_variance(m_root, m_variance);
  return particle;
}

std::expected<std::shared_ptr<ParticleDatabase>, StarException> ParticleDatabase::create(Root& root) {
  auto result = std::make_shared<ParticleDatabase>(ConstructionKey{}, root);
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto files = (*assets)->scan_extension("particle");
  if (!files)
    return std::unexpected(std::move(files.error()));
  auto queued = (*assets)->queue_jsons(files->get());
  if (!queued)
    return std::unexpected(std::move(queued.error()));
  for (auto const& file : files->get()) {
    auto loaded = [&]() -> std::expected<void, StarException> {
      auto json = (*assets)->json(file.string());
      if (!json)
        return std::unexpected(std::move(json.error()));
      auto config = ParticleConfig::create(root, *json);
      if (!config)
        return std::unexpected(std::move(config.error()));
      auto key = (*config)->kind().to_lower();
      if (!key)
        return std::unexpected(std::move(key.error()));
      if (!result->m_configs.try_emplace(std::move(*key), std::move(*config)).second)
        return std::unexpected(StarException(Star::format("Duplicate particle asset kind Name {}. configfile {}", (*config)->kind(), file.string())));
      return {};
    }();
    if (!loaded)
      return std::unexpected((*assets)->with_asset_context(std::move(loaded.error()), file.string(), "Loading particle"));
  }
  return result;
}

std::expected<std::shared_ptr<ParticleConfig>, StarException> ParticleDatabase::config(String const& kind) const {
  auto found = find_config(kind);
  if (!found)
    return std::unexpected(std::move(found.error()));
  return found->get();
}

std::expected<std::reference_wrapper<std::shared_ptr<ParticleConfig> const>, StarException>
ParticleDatabase::find_config(String const& kind) const {
  auto key = kind.to_lower();
  if (!key)
    return std::unexpected(std::move(key.error()));
  if (auto config = m_configs.find_ref(*key))
    return *config;
  return std::unexpected(StarException(Star::format("Unknown particle definition with kind {}.", kind)));
}

std::expected<ParticleVariantCreator, StarException> ParticleDatabase::particle_creator(Json const& kind_or_config, String const& relative_path) const {
  if (kind_or_config.is_type(Json::Type::String)) {
    auto name = kind_or_config.string_ptr();
    if (!name)
      return std::unexpected(std::move(name.error()));
    auto registered = find_config(**name);
    if (!registered)
      return std::unexpected(std::move(registered.error()));
    return ParticleVariantCreator([config = registered->get()] { return config->instance(); });
  }
  auto definition = parse_particle_definition(m_root, kind_or_config, relative_path, NullVariance::Reject);
  if (!definition)
    return std::unexpected(std::move(definition.error()));
  return make_particle_variant_creator(m_root, std::move(definition->particle), std::move(definition->variance));
}

std::expected<Particle, StarException> ParticleDatabase::particle(Json const& kind_or_config, String const& relative_path) const {
  if (kind_or_config.is_type(Json::Type::String)) {
    auto name = kind_or_config.string_ptr();
    if (!name)
      return std::unexpected(std::move(name.error()));
    auto registered = find_config(**name);
    if (!registered)
      return std::unexpected(std::move(registered.error()));
    return registered->get()->instance();
  }
  auto definition = parse_particle_definition(m_root, kind_or_config, relative_path, NullVariance::Reject);
  if (!definition)
    return std::unexpected(std::move(definition.error()));
  definition->particle.apply_variance(m_root, definition->variance);
  return std::move(definition->particle);
}

}
