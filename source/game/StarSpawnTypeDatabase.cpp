import star.weighted_pool;
import star.json;
import star.json_extra;
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
import star.time;
import star.string;
import star.set;
import star.format;
import star.version;
import star.optional;

import star.asset_source;
import star.assets;

import star.spawn_type_database;

namespace Star {

SpawnParameters::SpawnParameters(Set<Area> areas, Region region, Time time) : areas(areas), region(region), time(time) {}

SpawnParameters::SpawnParameters()
  : areas{Area::Surface, Area::Ceiling, Area::Air, Area::Liquid, Area::Solid}, region(Region::All), time(Time::All) {}

std::expected<SpawnParameters, StarException> SpawnParameters::create(Json const& config) {
  SpawnParameters result;
  if (config.isNull())
    return result;
  result.areas.clear();
  if (!AreaNames) return std::unexpected(AreaNames.error());
  if (!RegionNames) return std::unexpected(RegionNames.error());
  if (!TimeNames) return std::unexpected(TimeNames.error());
  auto areaName = config.optString("area");
  if (!areaName) return std::unexpected(std::move(areaName.error()));
  if (*areaName) {
    if (**areaName == "all")
      result.areas = {Area::Surface, Area::Ceiling, Area::Air, Area::Liquid, Area::Solid};
    else {
      auto area = AreaNames->getLeft(**areaName);
      if (!area) return std::unexpected(std::move(area.error()));
      result.areas.add(area->get());
    }
  } else {
    auto areaNames = config.optArray("areas");
    if (!areaNames) return std::unexpected(std::move(areaNames.error()));
    if (*areaNames) {
      for (auto const& entry : **areaNames) {
        auto name = entry.toString();
        if (!name) return std::unexpected(std::move(name.error()));
        auto area = AreaNames->getLeft(*name);
        if (!area) return std::unexpected(std::move(area.error()));
        result.areas.add(area->get());
      }
    }
  }
  auto regionName = config.getString("region");
  if (!regionName) return std::unexpected(std::move(regionName.error()));
  auto region = RegionNames->getLeft(*regionName);
  if (!region) return std::unexpected(std::move(region.error()));
  result.region = region->get();
  auto timeName = config.getString("time");
  if (!timeName) return std::unexpected(std::move(timeName.error()));
  auto time = TimeNames->getLeft(*timeName);
  if (!time) return std::unexpected(std::move(time.error()));
  result.time = time->get();
  return result;
}

bool SpawnParameters::compatible(SpawnParameters const& parameters) const {
  return areas.hasIntersection(parameters.areas)
      && (region == Region::All || parameters.region == Region::All || region == parameters.region)
      && (time == Time::All || parameters.time == Time::All || time == parameters.time);
}

std::expected<EnumMap<SpawnParameters::Area>, StarException> const SpawnParameters::AreaNames = EnumMap<Area>::create({
  {Area::Surface, "surface"}, {Area::Ceiling, "ceiling"}, {Area::Air, "air"}, {Area::Liquid, "liquid"}, {Area::Solid, "solid"}});

std::expected<EnumMap<SpawnParameters::Region>, StarException> const SpawnParameters::RegionNames = EnumMap<Region>::create({
  {Region::All, "all"}, {Region::Enclosed, "enclosed"}, {Region::Exposed, "exposed"}});

std::expected<EnumMap<SpawnParameters::Time>, StarException> const SpawnParameters::TimeNames = EnumMap<Time>::create({
  {Time::All, "all"}, {Time::Day, "day"}, {Time::Night, "night"}});

std::expected<SpawnType, StarException> spawnTypeFromJson(Json const& config) {
  SpawnType result;
  auto name = config.getString("name");
  if (!name) return std::unexpected(std::move(name.error()));
  result.typeName = std::move(*name);
  auto dayConfig = config.get("dayLevelAdjustment", JsonArray{0, 0});
  if (!dayConfig) return std::unexpected(std::move(dayConfig.error()));
  auto day = jsonToVec2F(*dayConfig);
  if (!day) return std::unexpected(std::move(day.error()));
  result.dayLevelAdjustment = *day;
  auto nightConfig = config.get("nightLevelAdjustment", JsonArray{0, 0});
  if (!nightConfig) return std::unexpected(std::move(nightConfig.error()));
  auto night = jsonToVec2F(*nightConfig);
  if (!night) return std::unexpected(std::move(night.error()));
  result.nightLevelAdjustment = *night;
  auto monsterType = config.get("monsterType");
  if (!monsterType) return std::unexpected(std::move(monsterType.error()));
  if (monsterType->type() == Json::Type::Array) {
    auto pool = jsonToWeightedPool<String>(*monsterType);
    if (!pool) return std::unexpected(std::move(pool.error()));
    result.monsterType = std::move(*pool);
  } else {
    auto name = monsterType->toString();
    if (!name) return std::unexpected(std::move(name.error()));
    result.monsterType = std::move(*name);
  }
  auto parameters = config.get("monsterParameters", JsonObject{});
  if (!parameters) return std::unexpected(std::move(parameters.error()));
  result.monsterParameters = std::move(*parameters);
  auto sizeConfig = config.get("groupSize", JsonArray{1, 1});
  if (!sizeConfig) return std::unexpected(std::move(sizeConfig.error()));
  auto size = jsonToVec2I(*sizeConfig);
  if (!size) return std::unexpected(std::move(size.error()));
  result.groupSize = *size;
  auto chance = config.getFloat("spawnChance");
  if (!chance) return std::unexpected(std::move(chance.error()));
  result.spawnChance = *chance;
  auto spawnConfig = config.get("spawnParameters", {});
  if (!spawnConfig) return std::unexpected(std::move(spawnConfig.error()));
  auto spawn = SpawnParameters::create(*spawnConfig);
  if (!spawn) return std::unexpected(std::move(spawn.error()));
  result.spawnParameters = std::move(*spawn);
  auto seed = config.getUInt("seedMix", 0);
  if (!seed) return std::unexpected(std::move(seed.error()));
  result.seedMix = *seed;
  return result;
}

SpawnProfile::SpawnProfile() {}

std::expected<SpawnProfile, StarException> SpawnProfile::create(Json const& config) {
  auto typesConfig = config.get("spawnTypes", JsonArray());
  if (!typesConfig) return std::unexpected(std::move(typesConfig.error()));
  auto types = jsonToStringSet(*typesConfig);
  if (!types) return std::unexpected(std::move(types.error()));
  auto parameters = config.get("monsterParameters", {});
  if (!parameters) return std::unexpected(std::move(parameters.error()));
  return SpawnProfile(std::move(*types), std::move(*parameters));
}

SpawnProfile::SpawnProfile(StringSet spawnTypes, Json monsterParameters)
  : spawnTypes(std::move(spawnTypes)), monsterParameters(std::move(monsterParameters)) {}

Json SpawnProfile::toJson() const {
  return JsonObject{
    {"spawnTypes", jsonFromStringSet(spawnTypes)},
    {"monsterParameters", monsterParameters}
  };
}

std::expected<SpawnProfile, StarException> constructSpawnProfile(Assets const& assets, Json const& config, std::uint64_t seed) {
  SpawnProfile result;
  auto commonGroups = assets.json("/spawning.config:spawnGroups");
  if (!commonGroups) return std::unexpected(assets.withAssetContext(std::move(commonGroups.error()), "/spawning.config:spawnGroups", "Loading common spawn groups"));
  auto groups = config.getArray("groups", JsonArray());
  if (!groups) return std::unexpected(std::move(groups.error()));
  for (auto const& group : *groups) {
    auto poolConfig = group.get("pool");
    if (!poolConfig) return std::unexpected(std::move(poolConfig.error()));
    bool commonPool = false;
    if (poolConfig->canConvert(Json::Type::String)) {
      auto name = poolConfig->toString();
      if (!name) return std::unexpected(std::move(name.error()));
      auto common = commonGroups->get(*name);
      if (!common) return std::unexpected(assets.withAssetContext(std::move(common.error()), "/spawning.config:spawnGroups", "Loading common spawn group"));
      poolConfig = std::move(common);
      commonPool = true;
    }
    auto pool = jsonToWeightedPool<String>(*poolConfig);
    if (!pool) {
      if (commonPool)
        return std::unexpected(assets.withAssetContext(std::move(pool.error()), "/spawning.config:spawnGroups", "Loading common spawn group"));
      return std::unexpected(std::move(pool.error()));
    }
    auto count = group.getUInt("select");
    if (!count) return std::unexpected(std::move(count.error()));
    result.spawnTypes.addAll(pool->selectUniques(*count, ++seed));
  }
  auto parameters = config.getObject("monsterParameters", JsonObject());
  if (!parameters) return std::unexpected(std::move(parameters.error()));
  result.monsterParameters = std::move(*parameters);
  return result;
}

std::expected<std::shared_ptr<SpawnTypeDatabase>, StarException> SpawnTypeDatabase::create(std::shared_ptr<Assets const> const& assets) {
  auto result = std::shared_ptr<SpawnTypeDatabase>(new SpawnTypeDatabase());
  auto files = assets->scanExtension("spawntypes");
  if (!files) return std::unexpected(std::move(files.error()));
  auto queued = assets->queueJsons(files->get());
  if (!queued) return std::unexpected(std::move(queued.error()));
  std::uint64_t seedMix = 0;
  for (auto const& path : files->get()) {
    auto const& file = path.string();
    auto readFile = [&]() -> std::expected<void, StarException> {
      auto config = assets->json(file);
      if (!config) return std::unexpected(std::move(config.error()));
      auto entries = config->iterateArray();
      if (!entries) return std::unexpected(std::move(entries.error()));
      for (auto const& entry : *entries) {
        auto type = spawnTypeFromJson(entry);
        if (!type) return std::unexpected(std::move(type.error()));
        if (result->m_spawnTypes.contains(type->typeName))
          return std::unexpected(SpawnTypeDatabaseException::format("Duplicate spawnType named '{}' in config file '{}'", type->typeName, file));
        auto hasSeedMix = entry.contains("seedMix");
        if (!hasSeedMix) return std::unexpected(std::move(hasSeedMix.error()));
        if (!*hasSeedMix)
          type->seedMix = ++seedMix;
        result->m_spawnTypes[type->typeName] = std::move(*type);
      }
      return {};
    };
    auto loaded = readFile();
    if (!loaded)
      return std::unexpected(SpawnTypeDatabaseException(strf("Error reading spawnType config file {}", file), assets->withAssetContext(std::move(loaded.error()), file, "Loading spawn types")));
  }
  return result;
}

std::expected<SpawnType, StarException> SpawnTypeDatabase::spawnType(String const& typeName) const {
  if (auto type = m_spawnTypes.maybe(typeName))
    return std::move(*type);
  return std::unexpected(SpawnTypeDatabaseException::format("No such spawnType '{}'", typeName));
}

}
