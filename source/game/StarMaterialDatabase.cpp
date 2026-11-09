import star.json;
import star.json_extra;
import star.color;
import star.list;
import star.bi_map;
import star.game_types;
import star.liquid_types;
import star.material_types;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.random;
import star.format;
import star.ordered_map;
import star.io_device;
import star.asset_path;
import star.ref_ptr;
import star.listener;
import std;
import star.string;
import star.version;
import star.logging;

import star.collision_block;
import star.tile_damage;
import star.material_render_profile;
import star.item_descriptor;
import star.asset_source;
import star.assets;

import star.material_database;
import star.particle_database;
import star.image_metadata_database;

import star.math_common;
import star.algorithm;
namespace Star {

std::expected<std::shared_ptr<MaterialDatabase>, StarException> MaterialDatabase::create(std::shared_ptr<Assets const> const& assets, std::shared_ptr<ParticleDatabase const> const& pdb, ImageMetadataDatabase const& imageMetadata, Logger& logger, RandomService& random) {
  auto result = std::shared_ptr<MaterialDatabase>(new MaterialDatabase(random));
  auto loaded = result->load(assets, pdb, imageMetadata, logger);
  if (!loaded) return std::unexpected(std::move(loaded.error()));
  return result;
}

std::expected<void, StarException> MaterialDatabase::load(std::shared_ptr<Assets const> const& assets, std::shared_ptr<ParticleDatabase const> const& pdb, ImageMetadataDatabase const& imageMetadata, Logger& logger) {
  {
    auto value = BiMap<String, ModId>::create({{"metamod:none", NoModId}, {"metamod:biome", BiomeModId}, {"metamod:undergroundbiome", UndergroundBiomeModId}});
    if (!value) return std::unexpected(std::move(value.error()));
    m_metaModIndex = std::move(*value);
  }
  setMetaMaterial(EmptyMaterialId, MetaMaterialInfo{"metamaterial:empty", EmptyMaterialId, CollisionKind::None, false});
  setMetaMaterial(NullMaterialId, MetaMaterialInfo{"metamaterial:null", NullMaterialId, CollisionKind::Block, true});
  setMetaMaterial(StructureMaterialId, MetaMaterialInfo{"metamaterial:structure", StructureMaterialId, CollisionKind::Block, true});
  setMetaMaterial(BiomeMaterialId, MetaMaterialInfo{"metamaterial:biome", BiomeMaterialId, CollisionKind::Block, true});
  setMetaMaterial(Biome1MaterialId, MetaMaterialInfo{"metamaterial:biome1", Biome1MaterialId, CollisionKind::Block, true});
  setMetaMaterial(Biome2MaterialId, MetaMaterialInfo{"metamaterial:biome2", Biome2MaterialId, CollisionKind::Block, true});
  setMetaMaterial(Biome3MaterialId, MetaMaterialInfo{"metamaterial:biome3", Biome3MaterialId, CollisionKind::Block, true});
  setMetaMaterial(Biome4MaterialId, MetaMaterialInfo{"metamaterial:biome4", Biome4MaterialId, CollisionKind::Block, true});
  setMetaMaterial(Biome5MaterialId, MetaMaterialInfo{"metamaterial:biome5", Biome5MaterialId, CollisionKind::Block, true});
  setMetaMaterial(BoundaryMaterialId, MetaMaterialInfo{"metamaterial:boundary", BoundaryMaterialId, CollisionKind::Slippery, true});
  setMetaMaterial(ObjectSolidMaterialId, MetaMaterialInfo{"metamaterial:objectsolid", ObjectSolidMaterialId, CollisionKind::Block, true});
  setMetaMaterial(ObjectPlatformMaterialId, MetaMaterialInfo{"metamaterial:objectplatform", ObjectPlatformMaterialId, CollisionKind::Platform, false});
  auto loadMetaMaterials = [&]() -> std::expected<void, StarException> {
    auto metaConfig = assets->json("/metamaterials.config");
    if (!metaConfig) return std::unexpected(std::move(metaConfig.error()));
    auto metas = metaConfig->iterateArray();
    if (!metas) return std::unexpected(std::move(metas.error()));
    for (auto const& meta : *metas) {
      auto name = meta.getString("name");
      if (!name) return std::unexpected(std::move(name.error()));
      auto matName = "metamaterial:" + *name;
      if (isMaterialName(matName)) {
        auto logged = logger.info("Metamaterial '{}' has duplicate material name!", matName);
        if (!logged) return std::unexpected(std::move(logged.error()));
        continue;
      }
      auto id = meta.getUInt("materialId");
      if (!id) return std::unexpected(std::move(id.error()));
      MaterialId matId = *id;
      if (isRealMaterial(matId) || matId >= FirstEngineMetaMaterialId) {
        auto logged = logger.info("Material id {} for metamaterial '{}' does not fall within the valid range!", matId, matName);
        if (!logged) return std::unexpected(std::move(logged.error()));
        continue;
      } else if (containsMetaMaterial(matId)) {
        auto logged = logger.info("Material id {} for metamaterial '{}' conflicts with another metamaterial id!", matId, matName);
        if (!logged) return std::unexpected(std::move(logged.error()));
        continue;
      }
      auto collisionName = meta.getString("collisionKind");
      if (!collisionName) return std::unexpected(std::move(collisionName.error()));
      auto collision = enumValue<CollisionKindNames>(*collisionName);
      if (!collision) return std::unexpected(std::move(collision.error()));
      auto blocks = meta.getBool("blocksLiquidFlow", isSolidColliding(*collision));
      if (!blocks) return std::unexpected(std::move(blocks.error()));
      setMetaMaterial(matId, MetaMaterialInfo{matName, matId, *collision, *blocks});
    }
    return {};
  };
  auto metaMaterials = loadMetaMaterials();
  if (!metaMaterials)
    return std::unexpected(assets->withAssetContext(std::move(metaMaterials.error()), "/metamaterials.config", "Loading metamaterials"));
  auto materials = assets->scanExtension("material");
  if (!materials) return std::unexpected(std::move(materials.error()));
  auto mods = assets->scanExtension("matmod");
  if (!mods) return std::unexpected(std::move(mods.error()));
  auto queuedMaterials = assets->queueJsons(materials->get());
  if (!queuedMaterials) return std::unexpected(std::move(queuedMaterials.error()));
  auto queuedMods = assets->queueJsons(mods->get());
  if (!queuedMods) return std::unexpected(std::move(queuedMods.error()));
  for (auto const& path : materials->get()) {
    auto const& file = path.string();
    auto read_file = [&, &database = *this]() -> std::expected<void, StarException> {
      auto config = assets->json(file);
      if (!config)
        return std::unexpected(std::move(config.error()));
      MaterialInfo material;
      auto id = config->getInt("materialId");
      if (!id)
        return std::unexpected(std::move(id.error()));
      material.id = *id;
      {
        auto value = config->getString("materialName");
        if (!value)
          return std::unexpected(std::move(value.error()));
        material.name = std::move(*value);
      }
      material.path = file;
      material.config = *config;
      {
        auto value = config->getString("itemDrop", "");
        if (!value)
          return std::unexpected(std::move(value.error()));
        material.itemDrop = std::move(*value);
      }
      JsonObject descriptions;
      auto members = config->iterateObject();
      if (!members)
        return std::unexpected(std::move(members.error()));
      for (auto const& entry : *members) {
        auto description = entry.first.ends_with("Description");
        if (!description)
          return std::unexpected(std::move(description.error()));
        if (*description)
          descriptions[entry.first] = entry.second;
      }
      {
        auto value = config->getString("description", "");
        if (!value)
          return std::unexpected(std::move(value.error()));
        descriptions["description"] = std::move(*value);
      }
      {
        auto value = config->getString("shortdescription", "");
        if (!value)
          return std::unexpected(std::move(value.error()));
        descriptions["shortdescription"] = std::move(*value);
      }
      material.descriptions = std::move(descriptions);
      {
        auto value = config->getString("category");
        if (!value)
          return std::unexpected(std::move(value.error()));
        material.category = std::move(*value);
      }
      auto colorConfig = config->get("particleColor", JsonArray{0, 0, 0, 255});
      if (!colorConfig)
        return std::unexpected(std::move(colorConfig.error()));
      {
        auto value = jsonToColor(*colorConfig);
        if (!value)
          return std::unexpected(std::move(value.error()));
        material.particleColor = std::move(*value);
      }
      auto particle = config->optString("miningParticle");
      if (!particle)
        return std::unexpected(std::move(particle.error()));
      if (*particle) {
        {
          auto value = pdb->config(**particle);
          if (!value)
            return std::unexpected(std::move(value.error()));
          material.miningParticle = std::move(*value);
        }
      }
      auto sounds = config->opt("miningSounds");
      if (!sounds)
        return std::unexpected(std::move(sounds.error()));
      if (*sounds) {
        auto names = jsonToStringList(**sounds);
        if (!names)
          return std::unexpected(std::move(names.error()));
        for (auto const& name : *names)
          material.miningSounds.push_back(AssetPath::relative_to(file, name));
      }
      auto footstep = config->optString("footstepSound");
      if (!footstep)
        return std::unexpected(std::move(footstep.error()));
      if (*footstep)
        material.footstepSound = AssetPath::relative_to(file, **footstep);
      {
        auto value = config->getInt("tillableMod", NoModId);
        if (!value)
          return std::unexpected(std::move(value.error()));
        material.tillableMod = std::move(*value);
      }
      {
        auto value = config->getBool("soil", false);
        if (!value)
          return std::unexpected(std::move(value.error()));
        material.soil = std::move(*value);
      }
      {
        auto value = config->getBool("falling", false);
        if (!value)
          return std::unexpected(std::move(value.error()));
        material.falling = std::move(*value);
      }
      {
        auto value = config->getBool("cascading", false);
        if (!value)
          return std::unexpected(std::move(value.error()));
        material.cascading = std::move(*value);
      }
      auto renderConfig = config->opt("renderTemplate");
      if (!renderConfig)
        return std::unexpected(std::move(renderConfig.error()));
      if (*renderConfig) {
        auto renderTemplate = assets->fetchJson(**renderConfig, file);
        if (!renderTemplate)
          return std::unexpected(std::move(renderTemplate.error()));
        auto parameters = config->get("renderParameters");
        if (!parameters)
          return std::unexpected(std::move(parameters.error()));
        auto merged = jsonMerge(*renderTemplate, *parameters);
        auto profile = parseMaterialRenderProfile(imageMetadata, merged, file);
        if (!profile)
          return std::unexpected(std::move(profile.error()));
        material.materialRenderProfile = std::make_shared<MaterialRenderProfile>(std::move(*profile));
      }
      auto damageConfig = config->get("damageTable", "/tiles/defaultDamage.config");
      if (!damageConfig)
        return std::unexpected(std::move(damageConfig.error()));
      auto damage = assets->fetchJson(*damageConfig);
      if (!damage)
        return std::unexpected(std::move(damage.error()));
      auto health = config->optFloat("health");
      if (!health)
        return std::unexpected(std::move(health.error()));
      auto harvest = config->optUInt("requiredHarvestLevel");
      if (!harvest)
        return std::unexpected(std::move(harvest.error()));
      std::optional<unsigned> harvestLevel;
      if (*harvest)
        harvestLevel = **harvest;
      {
        auto value = TileDamageParameters::create(*assets, *damage, *health, harvestLevel);
        if (!value)
          return std::unexpected(std::move(value.error()));
        material.damageParameters = std::move(*value);
      }
      auto collisionName = config->getString("collisionKind", "block");
      if (!collisionName)
        return std::unexpected(std::move(collisionName.error()));
      auto collision = enumValue<CollisionKindNames>(*collisionName);
      if (!collision)
        return std::unexpected(std::move(collision.error()));
      material.collisionKind = *collision;
      {
        auto value = config->getBool("foregroundOnly", false);
        if (!value)
          return std::unexpected(std::move(value.error()));
        material.foregroundOnly = std::move(*value);
      }
      {
        auto value = config->getBool("supportsMods", !(material.falling || material.cascading || material.collisionKind != CollisionKind::Block));
        if (!value)
          return std::unexpected(std::move(value.error()));
        material.supportsMods = std::move(*value);
      }
      {
        auto value = config->getBool("blocksLiquidFlow", isSolidColliding(material.collisionKind));
        if (!value)
          return std::unexpected(std::move(value.error()));
        material.blocksLiquidFlow = std::move(*value);
      }
      if (material.id != *id || !isRealMaterial(material.id))
        return std::unexpected(MaterialException(Star::format("Material id {} does not fall in the valid range\n", *id)));
      if (database.containsMaterial(material.id))
        return std::unexpected(MaterialException(Star::format("Duplicate material id {} found for material {}", material.id, material.name)));
      if (database.isMaterialName(material.name))
        return std::unexpected(MaterialException(Star::format("Duplicate material name '{}' found", material.name)));
      database.setMaterial(material.id, material);

      auto interactions = config->getArray("liquidInteractions", {});
      if (!interactions)
        return std::unexpected(std::move(interactions.error()));
      for (auto const& entry : *interactions) {
        auto liquid = entry.getUInt("liquidId");
        if (!liquid)
          return std::unexpected(std::move(liquid.error()));
        LiquidMaterialInteraction interaction;
        {
          auto value = entry.getFloat("consumeLiquid", 0.0f);
          if (!value)
            return std::unexpected(std::move(value.error()));
          interaction.consumeLiquid = std::move(*value);
        }
        {
          auto value = entry.getUInt("transformMaterialId", NullMaterialId);
          if (!value)
            return std::unexpected(std::move(value.error()));
          interaction.transformTo = std::move(*value);
        }
        {
          auto value = entry.getBool("topOnly", false);
          if (!value)
            return std::unexpected(std::move(value.error()));
          interaction.topOnly = std::move(*value);
        }
        database.m_liquidMaterialInteractions[{static_cast<LiquidId>(*liquid), material.id}] = interaction;
      }
      return {};
    };
    auto loaded = read_file();
    if (!loaded) return std::unexpected(assets->withAssetContext(MaterialException(Star::format("Error loading material file {}", file), loaded.error()), file, "Loading material"));
  }
  for (auto const& path : mods->get()) {
    auto const& file = path.string();
    auto read_file = [&, &database = *this]() -> std::expected<void, StarException> {
      auto config = assets->json(file);
      if (!config)
        return std::unexpected(std::move(config.error()));
      ModInfo mod;
      auto id = config->getInt("modId");
      if (!id)
        return std::unexpected(std::move(id.error()));
      mod.id = *id;
      {
        auto value = config->getString("modName");
        if (!value)
          return std::unexpected(std::move(value.error()));
        mod.name = std::move(*value);
      }
      mod.path = file;
      mod.config = *config;
      {
        auto value = config->getString("itemDrop", "");
        if (!value)
          return std::unexpected(std::move(value.error()));
        mod.itemDrop = std::move(*value);
      }
      JsonObject descriptions;
      auto members = config->iterateObject();
      if (!members)
        return std::unexpected(std::move(members.error()));
      for (auto const& entry : *members) {
        auto description = entry.first.ends_with("Description");
        if (!description)
          return std::unexpected(std::move(description.error()));
        if (*description)
          descriptions[entry.first] = entry.second;
      }
      {
        auto value = config->getString("description", "");
        if (!value)
          return std::unexpected(std::move(value.error()));
        descriptions["description"] = std::move(*value);
      }

      mod.descriptions = std::move(descriptions);

      auto colorConfig = config->get("particleColor", JsonArray{0, 0, 0, 255});
      if (!colorConfig)
        return std::unexpected(std::move(colorConfig.error()));
      {
        auto value = jsonToColor(*colorConfig);
        if (!value)
          return std::unexpected(std::move(value.error()));
        mod.particleColor = std::move(*value);
      }
      auto particle = config->optString("miningParticle");
      if (!particle)
        return std::unexpected(std::move(particle.error()));
      if (*particle) {
        {
          auto value = pdb->config(**particle);
          if (!value)
            return std::unexpected(std::move(value.error()));
          mod.miningParticle = std::move(*value);
        }
      }
      auto sounds = config->opt("miningSounds");
      if (!sounds)
        return std::unexpected(std::move(sounds.error()));
      if (*sounds) {
        auto names = jsonToStringList(**sounds);
        if (!names)
          return std::unexpected(std::move(names.error()));
        for (auto const& name : *names)
          mod.miningSounds.push_back(AssetPath::relative_to(file, name));
      }
      auto footstep = config->optString("footstepSound");
      if (!footstep)
        return std::unexpected(std::move(footstep.error()));
      if (*footstep)
        mod.footstepSound = AssetPath::relative_to(file, **footstep);
      {
        auto value = config->getBool("tilled", false);
        if (!value)
          return std::unexpected(std::move(value.error()));
        mod.tilled = std::move(*value);
      }
      {
        auto value = config->getBool("breaksWithTile", false);
        if (!value)
          return std::unexpected(std::move(value.error()));
        mod.breaksWithTile = std::move(*value);
      }
      auto renderConfig = config->opt("renderTemplate");
      if (!renderConfig)
        return std::unexpected(std::move(renderConfig.error()));
      if (*renderConfig) {
        auto renderTemplate = assets->fetchJson(**renderConfig);
        if (!renderTemplate)
          return std::unexpected(std::move(renderTemplate.error()));
        auto parameters = config->get("renderParameters");
        if (!parameters)
          return std::unexpected(std::move(parameters.error()));
        auto merged = jsonMerge(*renderTemplate, *parameters);
        auto profile = parseMaterialRenderProfile(imageMetadata, merged, file);
        if (!profile)
          return std::unexpected(std::move(profile.error()));
        mod.modRenderProfile = std::make_shared<MaterialRenderProfile>(std::move(*profile));
      }
      auto damageConfig = config->get("damageTable", "/tiles/defaultDamage.config");
      if (!damageConfig)
        return std::unexpected(std::move(damageConfig.error()));
      auto damage = assets->fetchJson(*damageConfig);
      if (!damage)
        return std::unexpected(std::move(damage.error()));
      auto health = config->optFloat("health");
      if (!health)
        return std::unexpected(std::move(health.error()));
      auto harvest = config->optUInt("harvestLevel");
      if (!harvest)
        return std::unexpected(std::move(harvest.error()));
      std::optional<unsigned> harvestLevel;
      if (*harvest)
        harvestLevel = **harvest;
      {
        auto value = TileDamageParameters::create(*assets, *damage, *health, harvestLevel);
        if (!value)
          return std::unexpected(std::move(value.error()));
        mod.damageParameters = std::move(*value);
      }

      if (mod.id != *id || !isRealMod(mod.id))
        return std::unexpected(MaterialException(Star::format("Mod id {} does not fall in the valid range\n", mod.id)));
      if (database.containsMod(mod.id))
        return std::unexpected(MaterialException(Star::format("Duplicate mod id {} found for mod {}", mod.id, mod.name)));
      if (database.m_modIndex.contains(mod.name) || database.m_metaModIndex.contains_left(mod.name))
        return std::unexpected(MaterialException(Star::format("Duplicate mod name '{}' found", mod.name)));
      database.setMod(mod.id, mod);
      database.m_modIndex[mod.name] = mod.id;
      auto interactions = config->getArray("liquidInteractions", {});
      if (!interactions)
        return std::unexpected(std::move(interactions.error()));
      for (auto const& entry : *interactions) {
        auto liquid = entry.getUInt("liquidId");
        if (!liquid)
          return std::unexpected(std::move(liquid.error()));
        LiquidModInteraction interaction;
        {
          auto value = entry.getFloat("consumeLiquid", 0.0f);
          if (!value)
            return std::unexpected(std::move(value.error()));
          interaction.consumeLiquid = std::move(*value);
        }
        {
          auto value = entry.getUInt("transformModId", NoModId);
          if (!value)
            return std::unexpected(std::move(value.error()));
          interaction.transformTo = std::move(*value);
        }
        {
          auto value = entry.getBool("topOnly", false);
          if (!value)
            return std::unexpected(std::move(value.error()));
          interaction.topOnly = std::move(*value);
        }
        database.m_liquidModInteractions[{static_cast<LiquidId>(*liquid), mod.id}] = interaction;
      }
      return {};
    };
    auto loaded = read_file();
    if (!loaded) return std::unexpected(assets->withAssetContext(MaterialException(Star::format("Error loading mod file {}", file), loaded.error()), file, "Loading material mod"));
  }
  auto footstepConfig = assets->json("/client.config:defaultFootstepSound");
  if (!footstepConfig) return std::unexpected(assets->withAssetContext(std::move(footstepConfig.error()), "/client.config:defaultFootstepSound", "Loading default footstep sound"));
  {
    auto value = footstepConfig->toString();
    if (!value) return std::unexpected(assets->withAssetContext(std::move(value.error()), "/client.config:defaultFootstepSound", "Loading default footstep sound"));
    m_defaultFootstepSound = std::move(*value);
  }
  return {};
}

StringList MaterialDatabase::materialNames() const {
  StringList names = m_materialIndex.keys();
  names.append_range(m_metaMaterialIndex.keys());
  return names;
}

bool MaterialDatabase::isMetaMaterialName(String const& name) const {
  return m_metaMaterialIndex.contains(name);
}

bool MaterialDatabase::isMaterialName(String const& name) const {
  return m_materialIndex.contains(name) || m_metaMaterialIndex.contains(name);
}

bool MaterialDatabase::isValidMaterialId(MaterialId material) const {
  if (isRealMaterial(material))
    return containsMaterial(material);
  else
    return containsMetaMaterial(material);
}

std::expected<MaterialId, StarException> MaterialDatabase::materialId(String const& name) const {
  if (auto id = m_metaMaterialIndex.maybe(name)) return *id;
  auto id = m_materialIndex.get(name);
  if (!id) return std::unexpected(std::move(id.error()));
  return id->get();
}

std::expected<String, StarException> MaterialDatabase::materialName(MaterialId id) const {
  if (isRealMaterial(id)) {
    auto info = getMaterialInfo(id);
    if (!info) return std::unexpected(std::move(info.error()));
    return info->get().name;
  }
  auto info = getMetaMaterialInfo(id);
  if (!info) return std::unexpected(std::move(info.error()));
  return info->get().name;
}

std::expected<std::optional<String>, StarException> MaterialDatabase::materialPath(MaterialId id) const {
  if (!isRealMaterial(id)) return std::optional<String>();
  auto info = getMaterialInfo(id);
  if (!info) return std::unexpected(std::move(info.error()));
  return info->get().path;
}

std::expected<std::optional<Json>, StarException> MaterialDatabase::materialConfig(MaterialId id) const {
  if (!isRealMaterial(id)) return std::optional<Json>();
  auto info = getMaterialInfo(id);
  if (!info) return std::unexpected(std::move(info.error()));
  return info->get().config;
}

std::expected<String, StarException> MaterialDatabase::materialDescription(MaterialId id, String const& species) const {
  auto info = getMaterialInfo(id);
  if (!info) return std::unexpected(std::move(info.error()));
  auto description = info->get().descriptions.getString("description");
  if (!description) return std::unexpected(std::move(description.error()));
  return info->get().descriptions.getString(Star::format("{}Description", species), *description);
}

std::expected<String, StarException> MaterialDatabase::materialDescription(MaterialId id) const {
  auto info = getMaterialInfo(id);
  if (!info) return std::unexpected(std::move(info.error()));
  return info->get().descriptions.getString("description");
}

std::expected<String, StarException> MaterialDatabase::materialShortDescription(MaterialId id) const {
  auto info = getMaterialInfo(id);
  if (!info) return std::unexpected(std::move(info.error()));
  return info->get().descriptions.getString("shortdescription");
}

std::expected<String, StarException> MaterialDatabase::materialCategory(MaterialId id) const {
  auto info = getMaterialInfo(id);
  if (!info) return std::unexpected(std::move(info.error()));
  return info->get().category;
}

StringList MaterialDatabase::modNames() const {
  StringList modNames = m_modIndex.keys();
  modNames.append_range(m_metaModIndex.left_values());
  return modNames;
}

bool MaterialDatabase::isModName(String const& name) const {
  return m_modIndex.contains(name);
}

bool MaterialDatabase::isValidModId(ModId mod) const {
  if (isRealMod(mod))
    return mod < m_mods.size() && static_cast<bool>(m_mods[mod]);
  else
    return m_metaModIndex.contains_right(mod);
}

std::expected<ModId, StarException> MaterialDatabase::modId(String const& name) const {
  if (auto id = m_metaModIndex.maybe_right(name))
    return *id;
  auto id = m_modIndex.get(name);
  if (!id) return std::unexpected(std::move(id.error()));
  return id->get();
}

std::expected<std::reference_wrapper<String const>, StarException> MaterialDatabase::modName(ModId id) const {
  if (!isRealMod(id))
    return m_metaModIndex.get_left(id);
  auto info = getModInfo(id);
  if (!info) return std::unexpected(std::move(info.error()));
  return std::cref(info->get().name);
}

std::expected<std::optional<String>, StarException> MaterialDatabase::modPath(ModId id) const {
  if (!isRealMod(id)) return std::optional<String>();
  auto info = getModInfo(id);
  if (!info) return std::unexpected(std::move(info.error()));
  return info->get().path;
}

std::expected<std::optional<Json>, StarException> MaterialDatabase::modConfig(ModId id) const {
  if (!isRealMod(id)) return std::optional<Json>();
  auto info = getModInfo(id);
  if (!info) return std::unexpected(std::move(info.error()));
  return info->get().config;
}

std::expected<String, StarException> MaterialDatabase::modDescription(ModId id, String const& species) const {
  auto info = getModInfo(id);
  if (!info) return std::unexpected(std::move(info.error()));
  auto description = info->get().descriptions.getString("description");
  if (!description) return std::unexpected(std::move(description.error()));
  return info->get().descriptions.getString(Star::format("{}Description", species), *description);
}

std::expected<String, StarException> MaterialDatabase::modDescription(ModId id) const {
  auto info = getModInfo(id);
  if (!info) return std::unexpected(std::move(info.error()));
  return info->get().descriptions.getString("description");
}

std::expected<String, StarException> MaterialDatabase::modShortDescription(ModId id) const {
  auto info = getModInfo(id);
  if (!info) return std::unexpected(std::move(info.error()));
  return info->get().descriptions.getString("shortdescription");
}

String MaterialDatabase::defaultFootstepSound() const {
  return m_defaultFootstepSound;
}

std::expected<TileDamageParameters, StarException> MaterialDatabase::materialDamageParameters(MaterialId id) const {
  if (!isRealMaterial(id)) return TileDamageParameters();
  auto info = getMaterialInfo(id);
  if (!info) return std::unexpected(std::move(info.error()));
  return info->get().damageParameters;
}

std::expected<TileDamageParameters, StarException> MaterialDatabase::modDamageParameters(ModId id) const {
  if (!isRealMod(id)) return TileDamageParameters();
  auto info = getModInfo(id);
  if (!info) return std::unexpected(std::move(info.error()));
  return info->get().damageParameters;
}

std::expected<bool, StarException> MaterialDatabase::modBreaksWithTile(ModId id) const {
  if (!isRealMod(id)) return false;
  auto info = getModInfo(id);
  if (!info) return std::unexpected(std::move(info.error()));
  return info->get().breaksWithTile;
}

std::expected<CollisionKind, StarException> MaterialDatabase::materialCollisionKind(MaterialId id) const {
  if (isRealMaterial(id)) {
    auto info = getMaterialInfo(id);
    if (!info) return std::unexpected(std::move(info.error()));
    return info->get().collisionKind;
  }
  if (containsMetaMaterial(id)) {
    auto info = getMetaMaterialInfo(id);
    if (!info) return std::unexpected(std::move(info.error()));
    return info->get().collisionKind;
  }
  return CollisionKind::Block;
}

std::expected<bool, StarException> MaterialDatabase::canPlaceInLayer(MaterialId id, TileLayer layer) const {
  if (layer != TileLayer::Background) return true;
  auto info = getMaterialInfo(id);
  if (!info) return std::unexpected(std::move(info.error()));
  return !info->get().foregroundOnly;
}

std::expected<ItemDescriptor, StarException> MaterialDatabase::materialItemDrop(MaterialId id) const {
  if (isRealMaterial(id)) {
    auto info = getMaterialInfo(id);
    if (!info) return std::unexpected(std::move(info.error()));
    if (!info->get().itemDrop.empty()) return ItemDescriptor::create(info->get().itemDrop, 1, JsonObject());
  }
  return ItemDescriptor();
}

std::expected<ItemDescriptor, StarException> MaterialDatabase::modItemDrop(ModId id) const {
  if (isRealMod(id)) {
    auto info = getModInfo(id);
    if (!info) return std::unexpected(std::move(info.error()));
    if (!info->get().itemDrop.empty()) return ItemDescriptor::create(info->get().itemDrop, 1);
  }
  return ItemDescriptor();
}

std::expected<MaterialColorVariant, StarException> MaterialDatabase::materialColorVariants(MaterialId id) const {
  if (isRealMaterial(id)) {
    auto info = getMaterialInfo(id);
    if (!info) return std::unexpected(std::move(info.error()));
    if (info->get().materialRenderProfile) return info->get().materialRenderProfile->colorVariants;
  }
  return 0;
}

std::expected<MaterialColorVariant, StarException> MaterialDatabase::modColorVariants(ModId id) const {
  if (isRealMod(id)) {
    auto info = getModInfo(id);
    if (!info) return std::unexpected(std::move(info.error()));
    if (info->get().modRenderProfile) return info->get().modRenderProfile->colorVariants;
  }
  return 0;
}

std::expected<bool, StarException> MaterialDatabase::isMultiColor(MaterialId id) const {
  if (isRealMaterial(id)) {
    auto info = getMaterialInfo(id);
    if (!info) return std::unexpected(std::move(info.error()));
    if (info->get().materialRenderProfile) return info->get().materialRenderProfile->colorVariants > 0;
  }
  return false;
}

std::expected<std::shared_ptr<ParticleConfig>, StarException> MaterialDatabase::miningParticle(MaterialId materialId, ModId modId) const {
  if (isRealMod(modId)) {
    auto info = getModInfo(modId);
    if (!info) return std::unexpected(std::move(info.error()));
    if (info->get().miningParticle) return info->get().miningParticle;
  }
  if (isRealMaterial(materialId)) {
    auto info = getMaterialInfo(materialId);
    if (!info) return std::unexpected(std::move(info.error()));
    if (info->get().miningParticle) return info->get().miningParticle;
  }
  return std::shared_ptr<ParticleConfig>();
}

std::expected<String, StarException> MaterialDatabase::miningSound(MaterialId materialId, ModId modId) const {
  if (isRealMod(modId)) {
    auto info = getModInfo(modId);
    if (!info) return std::unexpected(std::move(info.error()));
    if (!info->get().miningSounds.empty()) return m_random.randValueFrom(info->get().miningSounds);
  }
  if (isRealMaterial(materialId)) {
    auto info = getMaterialInfo(materialId);
    if (!info) return std::unexpected(std::move(info.error()));
    if (!info->get().miningSounds.empty()) return m_random.randValueFrom(info->get().miningSounds);
  }
  return String();
}

std::expected<String, StarException> MaterialDatabase::footstepSound(MaterialId materialId, ModId modId) const {
  if (isRealMod(modId)) {
    auto info = getModInfo(modId);
    if (!info) return std::unexpected(std::move(info.error()));
    if (!info->get().footstepSound.empty()) return info->get().footstepSound;
  }
  if (isRealMaterial(materialId)) {
    auto info = getMaterialInfo(materialId);
    if (!info) return std::unexpected(std::move(info.error()));
    if (!info->get().footstepSound.empty()) return info->get().footstepSound;
  }
  return m_defaultFootstepSound;
}

std::expected<Color, StarException> MaterialDatabase::materialParticleColor(MaterialId id, MaterialHue hueShift) const {
  auto info = getMaterialInfo(id);
  if (!info) return std::unexpected(std::move(info.error()));
  auto color = info->get().particleColor;
  color.setHue(pfmod(color.hue() + materialHueToDegrees(hueShift) / 360.0f, 1.0f));
  return color;
}

std::expected<bool, StarException> MaterialDatabase::isTilledMod(ModId id) const {
  if (!isRealMod(id)) return false;
  auto info = getModInfo(id);
  if (!info) return std::unexpected(std::move(info.error()));
  return info->get().tilled;
}

std::expected<bool, StarException> MaterialDatabase::isSoil(MaterialId id) const {
  if (!isRealMaterial(id)) return false;
  auto info = getMaterialInfo(id);
  if (!info) return std::unexpected(std::move(info.error()));
  return info->get().soil;
}

std::expected<ModId, StarException> MaterialDatabase::tilledModFor(MaterialId id) const {
  if (!isRealMaterial(id)) return NoModId;
  auto info = getMaterialInfo(id);
  if (!info) return std::unexpected(std::move(info.error()));
  return info->get().tillableMod;
}

std::expected<bool, StarException> MaterialDatabase::isFallingMaterial(MaterialId id) const {
  if (!isRealMaterial(id)) return false;
  auto info = getMaterialInfo(id);
  if (!info) return std::unexpected(std::move(info.error()));
  return info->get().falling;
}

std::expected<bool, StarException> MaterialDatabase::isCascadingFallingMaterial(MaterialId id) const {
  if (!isRealMaterial(id)) return false;
  auto info = getMaterialInfo(id);
  if (!info) return std::unexpected(std::move(info.error()));
  return info->get().cascading;
}

std::expected<bool, StarException> MaterialDatabase::supportsMod(MaterialId materialId, ModId modId) const {
  if (modId == NoModId) return true;
  if (!isRealMaterial(materialId) || !isRealMod(modId)) return false;
  auto info = getMaterialInfo(materialId);
  if (!info) return std::unexpected(std::move(info.error()));
  return info->get().supportsMods;
}

MaterialDatabase::MetaMaterialInfo::MetaMaterialInfo(String name, MaterialId id, CollisionKind collisionKind,
                                                     bool blocksLiquidFlow)
    : name(std::move(name)), id(id), collisionKind(collisionKind), blocksLiquidFlow(blocksLiquidFlow) {}

MaterialDatabase::MaterialInfo::MaterialInfo() : id(NullMaterialId), tillableMod(NoModId), falling(), cascading() {}

MaterialDatabase::ModInfo::ModInfo() : id(NoModId), tilled(), breaksWithTile() {}

std::size_t MaterialDatabase::metaMaterialIndex(MaterialId materialId) const {
  return materialId - FirstMetaMaterialId;
}

bool MaterialDatabase::containsMetaMaterial(MaterialId materialId) const {
  auto i = metaMaterialIndex(materialId);
  return m_metaMaterials.size() > i && m_metaMaterials[i];
}

void MaterialDatabase::setMetaMaterial(MaterialId materialId, MetaMaterialInfo info) {
  auto i = metaMaterialIndex(materialId);
  if (i >= m_metaMaterials.size())
    m_metaMaterials.resize(i + 1);
  m_metaMaterials[i] = std::make_shared<MetaMaterialInfo>(info);
  m_metaMaterialIndex[info.name] = materialId;
}

bool MaterialDatabase::containsMaterial(MaterialId materialId) const {
  return m_materials.size() > materialId && m_materials[materialId];
}

void MaterialDatabase::setMaterial(MaterialId materialId, MaterialInfo info) {
  if (materialId >= m_materials.size())
    m_materials.resize(materialId + 1);
  m_materials[materialId] = std::make_shared<MaterialInfo>(info);
  m_materialIndex[info.name] = materialId;
}

bool MaterialDatabase::containsMod(ModId modId) const {
  return m_mods.size() > modId && m_mods[modId];
}

void MaterialDatabase::setMod(ModId modId, ModInfo info) {
  if (modId >= m_mods.size())
    m_mods.resize(modId + 1);
  m_mods[modId] = std::make_shared<ModInfo>(info);
}

std::expected<std::reference_wrapper<MaterialDatabase::MetaMaterialInfo const>, StarException> MaterialDatabase::getMetaMaterialInfo(MaterialId id) const {
  if (!containsMetaMaterial(id)) return std::unexpected(MaterialException(Star::format("No such metamaterial id: {}\n", id)));
  return std::cref(*m_metaMaterials[metaMaterialIndex(id)]);
}

std::expected<std::reference_wrapper<MaterialDatabase::MaterialInfo const>, StarException> MaterialDatabase::getMaterialInfo(MaterialId id) const {
  if (!containsMaterial(id)) return std::unexpected(MaterialException(Star::format("No such material id: {}\n", id)));
  return std::cref(*m_materials[id]);
}

std::expected<std::reference_wrapper<MaterialDatabase::ModInfo const>, StarException> MaterialDatabase::getModInfo(ModId id) const {
  if (!containsMod(id)) return std::unexpected(MaterialException(Star::format("No such mod id: {}\n", id)));
  return std::cref(*m_mods[id]);
}

}
