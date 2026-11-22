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
import star.string;
import star.format;
import star.output_proxy;

import star.asset_source;
import star.assets;
import star.root;

import star.tenant_database;

namespace Star {

bool Tenant::criteriaSatisfied(StringMap<unsigned> const& colonyTags) const {
  // Check whether colonyTags is a supermultiset of colonyTagCriteria
  for (auto const& entry : colonyTagCriteria) {
    if (colonyTags.value(entry.first, 0) < entry.second)
      return false;
  }
  return true;
}

TenantDatabase::TenantDatabase(Root& root) : m_root(root), m_tenantCache(root.random()) {}

std::expected<std::shared_ptr<TenantDatabase>, StarException> TenantDatabase::create(Root& root) {
  auto result = std::shared_ptr<TenantDatabase>(new TenantDatabase(root));
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto files = (*assets)->scan_extension("tenant");
  if (!files)
    return std::unexpected(std::move(files.error()));
  auto queued = (*assets)->queue_jsons(files->get());
  if (!queued)
    return std::unexpected(std::move(queued.error()));
  for (auto const& assetFile : files->get()) {
    auto const& file = assetFile.string();
    auto name = [&]() -> std::expected<String, StarException> {
      auto config = (*assets)->json(file);
      if (!config)
        return std::unexpected(std::move(config.error()));
      return config->get_string("name");
    }();
    if (!name) {
      auto logged = root.logger().error_exception(
        Star::format("Error loading tenant file {}", file),
        ExceptionDiagnostic((*assets)->with_asset_context(std::move(name.error()), file, "Loading tenant")));
      if (!logged)
        return std::unexpected(std::move(logged.error()));
      continue;
    }
    if (result->m_paths.contains(*name)) {
      auto logged = root.logger().error("Tenant {} defined twice, second time from {}", *name, file);
      if (!logged)
        return std::unexpected(std::move(logged.error()));
    } else {
      result->m_paths[*name] = file;
    }
  }
  return result;
}

void TenantDatabase::cleanup() {
  std::lock_guard locker(m_cacheMutex);
  m_tenantCache.cleanup();
}

std::expected<std::shared_ptr<Tenant>, StarException> TenantDatabase::getTenant(String const& name) const {
  std::lock_guard locker(m_cacheMutex);
  auto result = m_tenantCache.get(
    name, [&database = *this](String const& name) -> std::expected<std::shared_ptr<Tenant>, StarException> {
      auto path = database.m_paths.find_ref(name);
      if (!path)
        return std::unexpected(TenantException::format("No such tenant named '{}'", name));
      auto assets = database.m_root.assets();
      if (!assets)
        return std::unexpected(std::move(assets.error()));
      return readTenant(**assets, path->get());
    });
  if (!result)
    return std::unexpected(std::move(result.error()));
  return result->get();
}

std::expected<List<std::shared_ptr<Tenant>>, StarException> TenantDatabase::getMatchingTenants(StringMap<unsigned> const& colonyTags) const {
  List<std::shared_ptr<Tenant>> matchingTenants;
  for (auto const& entry : m_paths) {
    auto tenant = getTenant(entry.first);
    if (!tenant)
      return std::unexpected(std::move(tenant.error()));
    if ((*tenant)->criteriaSatisfied(colonyTags))
      matchingTenants.push_back(std::move(*tenant));
  }
  return matchingTenants;
}

std::expected<std::shared_ptr<Tenant>, StarException> TenantDatabase::readTenant(Assets const& assets, String const& path) {
  auto parsed = [&]() -> std::expected<std::shared_ptr<Tenant>, StarException> {
    auto config = assets.json(path);
    if (!config)
      return std::unexpected(std::move(config.error()));
    auto name = config->get_string("name");
    if (!name)
      return std::unexpected(std::move(name.error()));
    auto priority = config->get_float("priority");
    if (!priority)
      return std::unexpected(std::move(priority.error()));
    auto criteriaConfig =
      config->get("colonyTagCriteria").and_then([](Json const& value) { return value.iterate_object(); });
    if (!criteriaConfig)
      return std::unexpected(std::move(criteriaConfig.error()));
    StringMap<unsigned> criteria;
    for (auto const& entry : *criteriaConfig) {
      auto count = entry.second.to_uint();
      if (!count)
        return std::unexpected(std::move(count.error()));
      criteria[entry.first] = *count;
    }
    auto tenantsConfig = config->get("tenants").and_then([](Json const& value) { return value.iterate_array(); });
    if (!tenantsConfig)
      return std::unexpected(std::move(tenantsConfig.error()));
    List<TenantSpawnable> tenants;
    for (auto const& json : *tenantsConfig) {
      auto spawn = json.get_string("spawn");
      if (!spawn)
        return std::unexpected(std::move(spawn.error()));
      auto type = json.get_string("type");
      if (!type)
        return std::unexpected(std::move(type.error()));
      auto level = json.opt_float("level");
      if (!level)
        return std::unexpected(std::move(level.error()));
      auto overrides = json.opt("overrides");
      if (!overrides)
        return std::unexpected(std::move(overrides.error()));
      if (*spawn == "monster") {
        tenants.push_back(TenantMonsterSpawnable{*type, *level, *overrides});
      } else {
        if (*spawn != "npc")
          return std::unexpected(TenantException::format("Unknown tenant spawn type '{}'", *spawn));
        auto speciesConfig = json.get("species");
        if (!speciesConfig)
          return std::unexpected(std::move(speciesConfig.error()));
        List<String> species;
        if (speciesConfig->is_type(Json::Type::Array)) {
          auto speciesArray = speciesConfig->iterate_array();
          if (!speciesArray)
            return std::unexpected(std::move(speciesArray.error()));
          for (auto const& entry : *speciesArray) {
            auto value = entry.to_string();
            if (!value)
              return std::unexpected(std::move(value.error()));
            species.push_back(std::move(*value));
          }
        } else {
          auto value = speciesConfig->to_string();
          if (!value)
            return std::unexpected(std::move(value.error()));
          species.push_back(std::move(*value));
        }
        tenants.push_back(TenantNpcSpawnable{std::move(species), *type, *level, *overrides});
      }
    }
    auto rentConfig = config->opt("rent");
    if (!rentConfig)
      return std::unexpected(std::move(rentConfig.error()));
    std::optional<TenantRent> rent;
    if (*rentConfig) {
      auto periodConfig = (**rentConfig).get("periodRange");
      if (!periodConfig)
        return std::unexpected(std::move(periodConfig.error()));
      auto period = json_to_vec2f(*periodConfig);
      if (!period)
        return std::unexpected(std::move(period.error()));
      auto pool = (**rentConfig).get_string("pool");
      if (!pool)
        return std::unexpected(std::move(pool.error()));
      rent = TenantRent{*period, *pool};
    }
    return std::make_shared<Tenant>(Tenant{*name, *priority, std::move(criteria), std::move(tenants), std::move(rent), *config});
  }();
  if (!parsed)
    return std::unexpected(TenantException(Star::format("Error loading tenant '{}'", path), assets.with_asset_context(std::move(parsed.error()), path, "Loading tenant")));
  return parsed;
}

}
