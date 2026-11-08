import star.json;
import star.json_extra;
import star.color;
import star.bi_map;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.string;
import star.format;
import star.version;

import star.animation;
import star.particle;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;

import star.particle_database;

namespace Star {

std::expected<std::shared_ptr<ParticleConfig>, StarException> ParticleConfig::create(Root& root, Json const& config) {
  auto result = std::shared_ptr<ParticleConfig>(new ParticleConfig(root));
  auto kind = config.getString("kind");
  if (!kind)
    return std::unexpected(std::move(kind.error()));
  result->m_kind = std::move(*kind);
  auto definition = config.queryObject("definition");
  if (!definition)
    return std::unexpected(std::move(definition.error()));
  auto particle = Particle::create(root, *definition);
  if (!particle)
    return std::unexpected(std::move(particle.error()));
  result->m_particle = std::move(*particle);
  auto varianceDefinition = config.queryObject("definition.variance", {});
  if (!varianceDefinition)
    return std::unexpected(std::move(varianceDefinition.error()));
  auto variance = Particle::create(root, *varianceDefinition);
  if (!variance)
    return std::unexpected(std::move(variance.error()));
  result->m_variance = std::move(*variance);
  return result;
}

String const& ParticleConfig::kind() {
  return m_kind;
}

Particle ParticleConfig::instance() {
  auto particle = m_particle;
  particle.applyVariance(m_root, m_variance);
  return particle;
}

std::expected<std::shared_ptr<ParticleDatabase>, StarException> ParticleDatabase::create(Root& root) {
  auto result = std::shared_ptr<ParticleDatabase>(new ParticleDatabase(root));
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto files = (*assets)->scanExtension("particle");
  if (!files)
    return std::unexpected(std::move(files.error()));
  auto queued = (*assets)->queueJsons(files->get());
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
      if (result->m_configs.contains((*config)->kind()))
        return std::unexpected(StarException(strf("Duplicate particle asset kind Name {}. configfile {}", (*config)->kind(), file.string())));
      result->m_configs[(*config)->kind()] = std::move(*config);
      return {};
    }();
    if (!loaded)
      return std::unexpected((*assets)->withAssetContext(std::move(loaded.error()), file.string(), "Loading particle"));
  }
  return result;
}

std::expected<std::shared_ptr<ParticleConfig>, StarException> ParticleDatabase::config(String const& kind) const {
  auto key = kind.to_lower();
  if (!key)
    return std::unexpected(std::move(key.error()));
  if (auto config = m_configs.find_ref(*key))
    return config->get();
  return std::unexpected(StarException(strf("Unknown particle definition with kind {}.", kind)));
}

std::expected<ParticleVariantCreator, StarException> ParticleDatabase::particleCreator(Json const& kindOrConfig, String const& relativePath) const {
  if (kindOrConfig.isType(Json::Type::String)) {
    auto name = kindOrConfig.toString();
    if (!name)
      return std::unexpected(std::move(name.error()));
    auto particleConfig = config(*name);
    if (!particleConfig)
      return std::unexpected(std::move(particleConfig.error()));
    return ParticleVariantCreator([config = std::move(*particleConfig)]() { return config->instance(); });
  }
  auto definition = kindOrConfig.toObject();
  if (!definition)
    return std::unexpected(std::move(definition.error()));
  auto particle = Particle::create(m_root, *definition, relativePath);
  if (!particle)
    return std::unexpected(std::move(particle.error()));
  auto varianceDefinition = kindOrConfig.getObject("variance", {});
  if (!varianceDefinition)
    return std::unexpected(std::move(varianceDefinition.error()));
  auto variance = Particle::create(m_root, *varianceDefinition, relativePath);
  if (!variance)
    return std::unexpected(std::move(variance.error()));
  return makeParticleVariantCreator(m_root, std::move(*particle), std::move(*variance));
}

std::expected<Particle, StarException> ParticleDatabase::particle(Json const& kindOrConfig, String const& relativePath) const {
  auto creator = particleCreator(kindOrConfig, relativePath);
  if (!creator)
    return std::unexpected(std::move(creator.error()));
  return (*creator)();
}

}
