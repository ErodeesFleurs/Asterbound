import std;
import star.string;
import star.list;
import star.json;
import star.json_extra;
import star.format;
import star.asset_path;
import star.tile_damage;
import star.assets;
import star.root;
import star.plant;
import star.plant_database;

namespace Star {
namespace {

template <typename Variant>
concept plant_variant = std::same_as<Variant, TreeVariant> || std::same_as<Variant, GrassVariant>
    || std::same_as<Variant, BushVariant>;

template <plant_variant Variant>
std::expected<void, StarException> read_stored_traits(Json const& store, Variant& variant) {
  auto descriptions = store.get("descriptions");
  if (!descriptions) return std::unexpected(std::move(descriptions.error()));
  auto ceiling = store.get_bool("ceiling");
  if (!ceiling) return std::unexpected(std::move(ceiling.error()));
  auto ephemeral = store.get_bool("ephemeral");
  if (!ephemeral) return std::unexpected(std::move(ephemeral.error()));
  variant.descriptions = std::move(*descriptions);
  variant.ceiling = *ceiling;
  variant.ephemeral = *ephemeral;
  return {};
}

std::expected<TileDamageParameters, StarException> read_stored_damage(Assets const& assets, Json const& store) {
  return store.get("tileDamageParameters").and_then(
      [&](Json const& damage) { return TileDamageParameters::create(assets, damage); });
}

std::expected<Json, StarException> resolve_descriptions(Json const& settings, String const& name,
    std::optional<std::reference_wrapper<String const>> suffix = std::nullopt) {
  auto entries = settings.iterate_object();
  if (!entries) return std::unexpected(std::move(entries.error()));
  JsonObject descriptions;
  for (auto const& [key, value] : *entries) {
    auto localized = key.ends_with("Description");
    if (!localized) return std::unexpected(std::move(localized.error()));
    if (*localized)
      descriptions.insert(key, value);
  }
  auto text = settings.opt_string("description");
  if (!text) return std::unexpected(std::move(text.error()));
  descriptions.insert("description", *text ? std::move(**text) : (suffix ? name + " with " + suffix->get() : name));
  return Json(std::move(descriptions));
}

std::expected<TileDamageParameters, StarException> resolve_damage(Root& root, Json const& settings, Json const& default_table) {
  auto assets = root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto table = settings.find("damageTable");
  if (!table) return std::unexpected(std::move(table.error()));
  auto health = settings.get_float("health", 1.0f);
  if (!health) return std::unexpected(std::move(health.error()));
  return TileDamageParameters::create(**assets, *table ? (*table)->get() : default_table, *health);
}

Json const& empty_plant_config() {
  static Json const empty(JsonObject{});
  return empty;
}

std::expected<Json, StarException> resolve_drop_config(Json const& settings) {
  auto drop = settings.find("dropConfig");
  if (!drop) return std::unexpected(std::move(drop.error()));
  if (*drop)
    return (*drop)->get();
  return empty_plant_config();
}

}

std::expected<TreeVariant, StarException> TreeVariant::create(Assets const& assets, Json const& store) {
  auto stem_name = store.get_string("stemName");
  if (!stem_name) return std::unexpected(std::move(stem_name.error()));
  auto foliage_name = store.get_string("foliageName");
  if (!foliage_name) return std::unexpected(std::move(foliage_name.error()));
  auto stem_directory = store.get_string("stemDirectory");
  if (!stem_directory) return std::unexpected(std::move(stem_directory.error()));
  auto stem_settings = store.get("stemSettings");
  if (!stem_settings) return std::unexpected(std::move(stem_settings.error()));
  auto stem_hue_shift = store.get_float("stemHueShift");
  if (!stem_hue_shift) return std::unexpected(std::move(stem_hue_shift.error()));
  auto foliage_directory = store.get_string("foliageDirectory");
  if (!foliage_directory) return std::unexpected(std::move(foliage_directory.error()));
  auto foliage_settings = store.get("foliageSettings");
  if (!foliage_settings) return std::unexpected(std::move(foliage_settings.error()));
  auto foliage_hue_shift = store.get_float("foliageHueShift");
  if (!foliage_hue_shift) return std::unexpected(std::move(foliage_hue_shift.error()));
  TreeVariant variant{
    .stem_name = std::move(*stem_name),
    .foliage_name = std::move(*foliage_name),
    .stem_directory = std::move(*stem_directory),
    .stem_settings = std::move(*stem_settings),
    .stem_hue_shift = *stem_hue_shift,
    .foliage_directory = std::move(*foliage_directory),
    .foliage_settings = std::move(*foliage_settings),
    .foliage_hue_shift = *foliage_hue_shift,
  };
  if (auto traits = read_stored_traits(store, variant); !traits)
    return std::unexpected(std::move(traits.error()));
  auto stem_drop_config = store.get("stemDropConfig");
  if (!stem_drop_config) return std::unexpected(std::move(stem_drop_config.error()));
  auto foliage_drop_config = store.get("foliageDropConfig");
  if (!foliage_drop_config) return std::unexpected(std::move(foliage_drop_config.error()));
  auto damage = read_stored_damage(assets, store);
  if (!damage) return std::unexpected(std::move(damage.error()));
  variant.stem_drop_config = std::move(*stem_drop_config);
  variant.foliage_drop_config = std::move(*foliage_drop_config);
  variant.tile_damage_parameters = std::move(*damage);
  return variant;
}

std::expected<Json, StarException> TreeVariant::to_json() const {
  auto damage = tile_damage_parameters.to_json();
  if (!damage) return std::unexpected(std::move(damage.error()));
  return JsonObject{
      {"stemName", stem_name},
      {"foliageName", foliage_name},
      {"stemDirectory", stem_directory},
      {"stemSettings", stem_settings},
      {"stemHueShift", stem_hue_shift},
      {"foliageDirectory", foliage_directory},
      {"foliageSettings", foliage_settings},
      {"foliageHueShift", foliage_hue_shift},
      {"descriptions", descriptions},
      {"ceiling", ceiling},
      {"ephemeral", ephemeral},
      {"stemDropConfig", stem_drop_config},
      {"foliageDropConfig", foliage_drop_config},
      {"tileDamageParameters", std::move(*damage)},
  };
}

std::expected<GrassVariant, StarException> GrassVariant::create(Assets const& assets, Json const& store) {
  auto name = store.get_string("name");
  if (!name) return std::unexpected(std::move(name.error()));
  auto directory = store.get_string("directory");
  if (!directory) return std::unexpected(std::move(directory.error()));
  auto images = store.get("images").and_then(json_to_string_list);
  if (!images) return std::unexpected(std::move(images.error()));
  auto hue_shift = store.get_float("hueShift");
  if (!hue_shift) return std::unexpected(std::move(hue_shift.error()));
  GrassVariant variant{
    .name = std::move(*name),
    .directory = std::move(*directory),
    .images = std::move(*images),
    .hue_shift = *hue_shift,
  };
  if (auto traits = read_stored_traits(store, variant); !traits)
    return std::unexpected(std::move(traits.error()));
  auto damage = read_stored_damage(assets, store);
  if (!damage) return std::unexpected(std::move(damage.error()));
  variant.tile_damage_parameters = std::move(*damage);
  return variant;
}

std::expected<Json, StarException> GrassVariant::to_json() const {
  auto damage = tile_damage_parameters.to_json();
  if (!damage) return std::unexpected(std::move(damage.error()));
  return JsonObject{{"name", name},
      {"directory", directory},
      {"images", json_from_string_list(images)},
      {"hueShift", hue_shift},
      {"descriptions", descriptions},
      {"ceiling", ceiling},
      {"ephemeral", ephemeral},
      {"tileDamageParameters", std::move(*damage)}};
}

std::expected<BushVariant, StarException> BushVariant::create(Assets const& assets, Json const& store) {
  auto bush_name = store.get_string("bushName");
  if (!bush_name) return std::unexpected(std::move(bush_name.error()));
  auto mod_name = store.get_string("modName");
  if (!mod_name) return std::unexpected(std::move(mod_name.error()));
  auto directory = store.get_string("directory");
  if (!directory) return std::unexpected(std::move(directory.error()));
  auto shape_array = store.get("shapes").and_then(&Json::array_ptr);
  if (!shape_array) return std::unexpected(std::move(shape_array.error()));
  List<BushShape> shapes;
  shapes.reserve((*shape_array)->size());
  for (auto const& shape : **shape_array) {
    auto image = shape.get_string(0);
    if (!image) return std::unexpected(std::move(image.error()));
    auto mods = shape.get(1).and_then(json_to_string_list);
    if (!mods) return std::unexpected(std::move(mods.error()));
    shapes.push_back({std::move(*image), std::move(*mods)});
  }
  auto base_hue_shift = store.get_float("baseHueShift");
  if (!base_hue_shift) return std::unexpected(std::move(base_hue_shift.error()));
  auto mod_hue_shift = store.get_float("modHueShift");
  if (!mod_hue_shift) return std::unexpected(std::move(mod_hue_shift.error()));
  BushVariant variant{
    .bush_name = std::move(*bush_name),
    .mod_name = std::move(*mod_name),
    .directory = std::move(*directory),
    .shapes = std::move(shapes),
    .base_hue_shift = *base_hue_shift,
    .mod_hue_shift = *mod_hue_shift,
  };
  if (auto traits = read_stored_traits(store, variant); !traits)
    return std::unexpected(std::move(traits.error()));
  auto damage = read_stored_damage(assets, store);
  if (!damage) return std::unexpected(std::move(damage.error()));
  variant.tile_damage_parameters = std::move(*damage);
  return variant;
}

std::expected<Json, StarException> BushVariant::to_json() const {
  auto damage = tile_damage_parameters.to_json();
  if (!damage) return std::unexpected(std::move(damage.error()));
  return JsonObject{{"bushName", bush_name},
      {"modName", mod_name},
      {"directory", directory},
      {"shapes", shapes.transformed([](BushShape const& shape) -> Json {
          return JsonArray{shape.image, json_from_string_list(shape.mods)};
        })},
      {"baseHueShift", base_hue_shift},
      {"modHueShift", mod_hue_shift},
      {"descriptions", descriptions},
      {"ceiling", ceiling},
      {"ephemeral", ephemeral},
      {"tileDamageParameters", std::move(*damage)}};
}

std::expected<std::shared_ptr<PlantDatabase>, StarException> PlantDatabase::create(Root& root) {
  struct MakeShared final : PlantDatabase {
    explicit MakeShared(Root& root) : PlantDatabase(root) {}
  };
  auto database = std::make_shared<MakeShared>(root);
  auto assets = root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto load = [&]() -> std::expected<void, StarException> {
    auto stems = (*assets)->scan_extension("modularstem");
    if (!stems) return std::unexpected(std::move(stems.error()));
    auto foliages = (*assets)->scan_extension("modularfoliage");
    if (!foliages) return std::unexpected(std::move(foliages.error()));
    auto grasses = (*assets)->scan_extension("grass");
    if (!grasses) return std::unexpected(std::move(grasses.error()));
    auto bushes = (*assets)->scan_extension("bush");
    if (!bushes) return std::unexpected(std::move(bushes.error()));
    for (auto const& files : {*stems, *foliages, *grasses, *bushes})
      if (auto queued = (*assets)->queue_jsons(files.get()); !queued) return std::unexpected(std::move(queued.error()));
    auto read = [&](CaseInsensitiveStringSet const& files, ConfigMap& configs) -> std::expected<void, StarException> {
      for (auto const& scanned_file : files) {
        String const& file = scanned_file.string();
        auto loaded = [&]() -> std::expected<void, StarException> {
          auto config = (*assets)->json(file);
          if (!config) return std::unexpected(std::move(config.error()));
          auto name = config->get_string("name");
          if (!name) return std::unexpected(std::move(name.error()));
          // The first path in the asset scan order wins a duplicate name.
          // Settings are immutable and can share the asset's JSON storage.
          configs.insert(std::move(*name), Config{AssetPath::directory(file), std::move(*config)});
          return {};
        }();
        if (!loaded)
          return std::unexpected((*assets)->with_asset_context(std::move(loaded.error()), file, "Loading plant configuration"));
      }
      return {};
    };
    if (auto result = read(stems->get(), database->m_tree_stem_configs); !result) return result;
    if (auto result = read(foliages->get(), database->m_tree_foliage_configs); !result) return result;
    if (auto result = read(grasses->get(), database->m_grass_configs); !result) return result;
    return read(bushes->get(), database->m_bush_configs);
  }();
  if (!load) return std::unexpected(PlantDatabaseException("Error loading plant database", load.error()));
  return database;
}

std::expected<StringList, StarException> PlantDatabase::tree_stem_names(bool ceiling) const {
  StringList names;
  for (auto const& [name, config] : m_tree_stem_configs) {
    auto is_ceiling = config.settings.get_bool("ceiling", false);
    if (!is_ceiling) return std::unexpected(std::move(is_ceiling.error()));
    if (*is_ceiling == ceiling)
      names.push_back(name);
  }
  return names;
}

StringList PlantDatabase::tree_foliage_names() const {
  return m_tree_foliage_configs.keys();
}

std::expected<String, StarException> PlantDatabase::tree_stem_shape(String const& stem_name) const {
  auto config = m_tree_stem_configs.get(stem_name);
  if (!config) return std::unexpected(std::move(config.error()));
  return config->get().settings.get_string("shape");
}

std::expected<String, StarException> PlantDatabase::tree_foliage_shape(String const& foliage_name) const {
  auto config = m_tree_foliage_configs.get(foliage_name);
  if (!config) return std::unexpected(std::move(config.error()));
  return config->get().settings.get_string("shape");
}

std::optional<String> PlantDatabase::tree_stem_directory(String const& stem_name) const {
  auto stem = m_tree_stem_configs.find(stem_name);
  if (stem != m_tree_stem_configs.end())
    return stem->second.directory;
  return std::nullopt;
}

std::optional<String> PlantDatabase::tree_foliage_directory(String const& foliage_name) const {
  auto foliage = m_tree_foliage_configs.find(foliage_name);
  if (foliage != m_tree_foliage_configs.end())
    return foliage->second.directory;
  return std::nullopt;
}

std::expected<TreeVariant, StarException> PlantDatabase::build_tree_variant(
    String const& stem_name, float stem_hue_shift, String const& foliage_name, float foliage_hue_shift) const {
  if (auto stem = m_tree_stem_configs.find(stem_name); stem != m_tree_stem_configs.end()) {
    if (auto foliage = m_tree_foliage_configs.find(foliage_name); foliage != m_tree_foliage_configs.end())
      return resolve_tree_variant(stem_name, stem->second, stem_hue_shift,
          FoliageSelection{foliage_name, foliage->second, foliage_hue_shift});
  }
  return std::unexpected(PlantDatabaseException::format(
      "stemName '{}' or foliageName '{}' not found in plant database", stem_name, foliage_name));
}

std::expected<TreeVariant, StarException> PlantDatabase::build_tree_variant(String const& stem_name, float stem_hue_shift) const {
  auto stem = m_tree_stem_configs.find(stem_name);
  if (stem == m_tree_stem_configs.end())
    return std::unexpected(PlantDatabaseException::format("stemName '{}' not found in plant database", stem_name));
  return resolve_tree_variant(stem_name, stem->second, stem_hue_shift, std::nullopt);
}

std::expected<TreeVariant, StarException> PlantDatabase::resolve_tree_variant(
    String const& stem_name, Config const& stem_config, float stem_hue_shift,
    std::optional<FoliageSelection> foliage) const {
  auto ceiling = stem_config.settings.get_bool("ceiling", false);
  if (!ceiling) return std::unexpected(std::move(ceiling.error()));
  auto stem_drop_config = resolve_drop_config(stem_config.settings);
  if (!stem_drop_config) return std::unexpected(std::move(stem_drop_config.error()));

  TreeVariant variant;
  std::optional<std::reference_wrapper<String const>> foliage_name;
  if (foliage) {
    auto const& [name, config, hue_shift] = *foliage;
    auto foliage_drop_config = resolve_drop_config(config.settings);
    if (!foliage_drop_config) return std::unexpected(std::move(foliage_drop_config.error()));
    foliage_name = std::cref(name);
    variant.foliage_name = name;
    variant.foliage_directory = config.directory;
    variant.foliage_settings = config.settings;
    variant.foliage_hue_shift = hue_shift;
    variant.foliage_drop_config = std::move(*foliage_drop_config);
  } else {
    variant.foliage_settings = empty_plant_config();
    variant.foliage_drop_config = empty_plant_config();
  }

  auto descriptions = resolve_descriptions(stem_config.settings, stem_name, foliage_name);
  if (!descriptions) return std::unexpected(std::move(descriptions.error()));
  // The two legacy asset fields are independent; only a tree with foliage uses allowsBlockPlacement.
  auto ephemeral = foliage ? stem_config.settings.get_bool("allowsBlockPlacement", false)
                           : stem_config.settings.get_bool("ephemeral", false);
  if (!ephemeral) return std::unexpected(std::move(ephemeral.error()));
  static Json const default_damage_table("/plants/treeDamage.config");
  auto damage = resolve_damage(m_root, stem_config.settings, default_damage_table);
  if (!damage) return std::unexpected(std::move(damage.error()));

  variant.stem_name = stem_name;
  variant.stem_directory = stem_config.directory;
  variant.stem_settings = stem_config.settings;
  variant.stem_hue_shift = stem_hue_shift;
  variant.ceiling = *ceiling;
  variant.stem_drop_config = std::move(*stem_drop_config);
  variant.descriptions = std::move(*descriptions);
  variant.ephemeral = *ephemeral;
  variant.tile_damage_parameters = std::move(*damage);
  return variant;
}

std::expected<StringList, StarException> PlantDatabase::grass_names(bool ceiling) const {
  StringList names;
  for (auto const& [name, config] : m_grass_configs) {
    auto is_ceiling = config.settings.get_bool("ceiling", false);
    if (!is_ceiling) return std::unexpected(std::move(is_ceiling.error()));
    if (*is_ceiling == ceiling)
      names.push_back(name);
  }
  return names;
}

std::expected<GrassVariant, StarException> PlantDatabase::build_grass_variant(String const& name, float hue_shift) const {
  auto entry = m_grass_configs.find(name);
  if (entry == m_grass_configs.end())
    return std::unexpected(PlantDatabaseException::format("grass '{}' not found in plant database", name));
  auto const& config = entry->second;
  auto images = config.settings.get("images").and_then(json_to_string_list);
  if (!images) return std::unexpected(std::move(images.error()));
  auto ceiling = config.settings.get_bool("ceiling", false);
  if (!ceiling) return std::unexpected(std::move(ceiling.error()));
  auto descriptions = resolve_descriptions(config.settings, name);
  if (!descriptions) return std::unexpected(std::move(descriptions.error()));
  auto ephemeral = config.settings.get_bool("ephemeral", true);
  if (!ephemeral) return std::unexpected(std::move(ephemeral.error()));
  static Json const default_damage_table("/plants/grassDamage.config");
  auto damage = resolve_damage(m_root, config.settings, default_damage_table);
  if (!damage) return std::unexpected(std::move(damage.error()));
  return GrassVariant{
    .name = name,
    .directory = config.directory,
    .images = std::move(*images),
    .hue_shift = hue_shift,
    .descriptions = std::move(*descriptions),
    .ceiling = *ceiling,
    .ephemeral = *ephemeral,
    .tile_damage_parameters = std::move(*damage),
  };
}

std::expected<StringList, StarException> PlantDatabase::bush_names(bool ceiling) const {
  StringList names;
  for (auto const& [name, config] : m_bush_configs) {
    auto is_ceiling = config.settings.get_bool("ceiling");
    if (!is_ceiling) return std::unexpected(std::move(is_ceiling.error()));
    if (*is_ceiling == ceiling)
      names.push_back(name);
  }
  return names;
}

std::expected<StringList, StarException> PlantDatabase::bush_mods(String const& bush_name) const {
  auto config = m_bush_configs.get(bush_name);
  if (!config) return std::unexpected(std::move(config.error()));
  auto mods = config->get().settings.find("mods");
  if (!mods) return std::unexpected(std::move(mods.error()));
  if (!*mods || (*mods)->get().is_null()) return StringList{};
  return json_to_string_list((*mods)->get());
}

std::expected<BushVariant, StarException> PlantDatabase::build_bush_variant(
    String const& bush_name, float base_hue_shift, String const& mod_name, float mod_hue_shift) const {
  auto entry = m_bush_configs.find(bush_name);
  if (entry == m_bush_configs.end())
    return std::unexpected(PlantDatabaseException::format("bush '{}' not found in plant database", bush_name));
  auto const& config = entry->second;
  auto shape_array = config.settings.get("shapes").and_then(&Json::array_ptr);
  if (!shape_array) return std::unexpected(std::move(shape_array.error()));
  List<BushVariant::BushShape> shapes;
  shapes.reserve((*shape_array)->size());
  for (auto const& shape : **shape_array) {
    auto fields = shape.object_ptr();
    if (!fields) return std::unexpected(std::move(fields.error()));
    auto base = (*fields)->get("base");
    if (!base) return std::unexpected(std::move(base.error()));
    auto image = base->get().to_string();
    if (!image) return std::unexpected(std::move(image.error()));
    StringList mods;
    if (!mod_name.empty()) {
      auto mod_table = (*fields)->get("mods");
      if (!mod_table) return std::unexpected(std::move(mod_table.error()));
      auto selection = mod_table->get().find(mod_name);
      if (!selection) return std::unexpected(std::move(selection.error()));
      if (*selection) {
        auto parsed = json_to_string_list((*selection)->get());
        if (!parsed) return std::unexpected(std::move(parsed.error()));
        mods = std::move(*parsed);
      }
    }
    shapes.push_back({std::move(*image), std::move(mods)});
  }
  auto ceiling = config.settings.get_bool("ceiling", false);
  if (!ceiling) return std::unexpected(std::move(ceiling.error()));
  auto descriptions = resolve_descriptions(config.settings, bush_name, std::cref(mod_name));
  if (!descriptions) return std::unexpected(std::move(descriptions.error()));
  auto ephemeral = config.settings.get_bool("ephemeral", true);
  if (!ephemeral) return std::unexpected(std::move(ephemeral.error()));
  static Json const default_damage_table("/plants/bushDamage.config");
  auto damage = resolve_damage(m_root, config.settings, default_damage_table);
  if (!damage) return std::unexpected(std::move(damage.error()));
  return BushVariant{
    .bush_name = bush_name,
    .mod_name = mod_name,
    .directory = config.directory,
    .shapes = std::move(shapes),
    .base_hue_shift = base_hue_shift,
    .mod_hue_shift = mod_hue_shift,
    .descriptions = std::move(*descriptions),
    .ceiling = *ceiling,
    .ephemeral = *ephemeral,
    .tile_damage_parameters = std::move(*damage),
  };
}

std::expected<std::shared_ptr<Plant>, StarException> PlantDatabase::create_plant(TreeVariant const& variant, std::uint64_t seed) const {
  auto plant = Plant::create(m_root, variant, seed);
  if (!plant)
    return std::unexpected(PlantDatabaseException(Star::format(
        "Error constructing plant from tree variant stem: {} foliage: {}", variant.stem_name, variant.foliage_name), plant.error()));
  return plant;
}

std::expected<std::shared_ptr<Plant>, StarException> PlantDatabase::create_plant(GrassVariant const& variant, std::uint64_t seed) const {
  auto plant = Plant::create(m_root, variant, seed);
  if (!plant)
    return std::unexpected(PlantDatabaseException(
        Star::format("Error constructing plant from grass variant name: {}", variant.name), plant.error()));
  return plant;
}

std::expected<std::shared_ptr<Plant>, StarException> PlantDatabase::create_plant(BushVariant const& variant, std::uint64_t seed) const {
  auto plant = Plant::create(m_root, variant, seed);
  if (!plant)
    return std::unexpected(PlantDatabaseException(Star::format(
        "Error constructing plant from bush variant name: {} mod: {}", variant.bush_name, variant.mod_name), plant.error()));
  return plant;
}

Json PlantDatabase::Config::to_json() const {
  return JsonObject{
    {"config", settings},
    {"path", directory}
  };
}

std::expected<Json, StarException> PlantDatabase::tree_foliage_config(String const& foliage_name) const {
  auto config = m_tree_foliage_configs.get(foliage_name);
  if (!config) return std::unexpected(std::move(config.error()));
  return config->get().to_json();
}

std::expected<Json, StarException> PlantDatabase::tree_stem_config(String const& stem_name) const {
  auto config = m_tree_stem_configs.get(stem_name);
  if (!config) return std::unexpected(std::move(config.error()));
  return config->get().to_json();
}

}
