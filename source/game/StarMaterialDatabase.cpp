import std;
import star.asset_path;
import star.assets;
import star.bi_map;
import star.collision_block;
import star.color;
import star.format;
import star.game_types;
import star.image_metadata_database;
import star.item_descriptor;
import star.json;
import star.json_extra;
import star.liquid_types;
import star.list;
import star.logging;
import star.map;
import star.material_database;
import star.material_render_profile;
import star.material_types;
import star.math_common;
import star.particle_database;
import star.random;
import star.string;
import star.tile_damage;

namespace Star {

namespace {

constexpr char meta_material_path[] = "/metamaterials.config";

template <std::unsigned_integral Value>
std::expected<Value, StarException> checked_unsigned(std::int64_t value, std::string_view key) {
  if (!std::in_range<Value>(value))
    return std::unexpected(MaterialException::format(
        "{} value {} is out of range [0, {}]", key, value, std::numeric_limits<Value>::max()));
  return static_cast<Value>(value);
}

template <std::unsigned_integral Value>
std::expected<Value, StarException> checked_unsigned(
    std::expected<std::int64_t, StarException> value, std::string_view key) {
  if (!value)
    return std::unexpected(std::move(value.error()));
  return checked_unsigned<Value>(*value, key);
}

std::expected<float, StarException> read_consume_liquid(Json const& entry) {
  auto amount = entry.get_double("consumeLiquid", 0.0);
  if (!amount)
    return std::unexpected(std::move(amount.error()));
  if (!is_finite(*amount) || *amount < 0.0 || *amount > std::numeric_limits<float>::max())
    return std::unexpected(MaterialException("consumeLiquid must be a finite, nonnegative float"));
  return static_cast<float>(*amount);
}

template <typename Interaction>
concept catalog_liquid_interaction =
    std::same_as<Interaction, LiquidMaterialInteraction> || std::same_as<Interaction, LiquidModInteraction>;

template <std::unsigned_integral Id, catalog_liquid_interaction Interaction>
std::expected<void, StarException> load_liquid_interactions(Json const& config, Id id,
    String const& transform_key, Id transform_default,
    HashMap<std::pair<LiquidId, Id>, Interaction>& interactions) {
  auto node = config.find("liquidInteractions");
  if (!node)
    return std::unexpected(std::move(node.error()));
  if (!*node || (*node)->get().is_null())
    return {};
  auto entries = (*node)->get().iterate_array();
  if (!entries)
    return std::unexpected(std::move(entries.error()));
  for (auto const& entry : *entries) {
    auto liquid = checked_unsigned<LiquidId>(entry.get_int("liquidId"), "liquidId");
    if (!liquid)
      return std::unexpected(std::move(liquid.error()));
    auto consume = read_consume_liquid(entry);
    if (!consume)
      return std::unexpected(std::move(consume.error()));
    auto transform = checked_unsigned<Id>(entry.get_int(transform_key, transform_default), transform_key.utf8());
    if (!transform)
      return std::unexpected(std::move(transform.error()));
    auto top_only = entry.get_bool("topOnly", false);
    if (!top_only)
      return std::unexpected(std::move(top_only.error()));

    // Forward references and sentinels are valid. The last entry for a liquid wins.
    interactions.insert_or_assign({*liquid, id}, Interaction{*consume, *transform, *top_only});
  }
  return {};
}

String meta_declaration(std::size_t source_entry) {
  if (source_entry == 0)
    return "the engine";
  return Star::format("'{}' entry #{}", meta_material_path, source_entry);
}

StarException duplicate_asset(Assets const& assets, StarException error,
    String const& original_path, std::string_view operation) {
  auto original = assets.with_asset_context(
      MaterialException("Original declaration", false), original_path, operation);
  return std::move(error).with_related(original, "Original declaration");
}

TileDamageParameters const& default_damage_parameters() {
  static TileDamageParameters const parameters;
  return parameters;
}

}

std::expected<void, StarException> MaterialDatabase::AssetInfo::load_descriptions() {
  auto drop = asset.config.get_string("itemDrop", "");
  if (!drop) return std::unexpected(std::move(drop.error()));
  item_drop = std::move(*drop);
  auto text = asset.config.get_string("description", "");
  if (!text) return std::unexpected(std::move(text.error()));
  description = std::move(*text);
  auto short_text = asset.config.get_string("shortdescription", "");
  if (!short_text) return std::unexpected(std::move(short_text.error()));
  short_description = std::move(*short_text);
  return {};
}

std::expected<String, StarException> MaterialDatabase::AssetInfo::description_for(String const& species) const {
  auto text = asset.config.opt_string(Star::format("{}Description", species));
  if (!text) return std::unexpected(std::move(text.error()));
  if (*text) return std::move(**text);
  return description;
}

std::expected<Color, StarException> MaterialDatabase::AssetInfo::load_media(ParticleDatabase const& particles) {
  auto color_config = asset.config.get("particleColor", JsonArray{0, 0, 0, 255});
  if (!color_config) return std::unexpected(std::move(color_config.error()));
  auto color = json_to_color(*color_config);
  if (!color) return std::unexpected(std::move(color.error()));

  auto particle = asset.config.opt_string("miningParticle");
  if (!particle) return std::unexpected(std::move(particle.error()));
  if (*particle) {
    auto value = particles.config(**particle);
    if (!value) return std::unexpected(std::move(value.error()));
    mining_particle = std::move(*value);
  }

  auto sounds = asset.config.find("miningSounds");
  if (!sounds) return std::unexpected(std::move(sounds.error()));
  if (*sounds && !(*sounds)->get().is_null()) {
    auto names = json_to_string_list((*sounds)->get());
    if (!names) return std::unexpected(std::move(names.error()));
    for (auto& name : *names)
      name = AssetPath::relative_to(asset.path, name);
    mining_sounds = std::move(*names);
  }

  auto footstep = asset.config.opt_string("footstepSound");
  if (!footstep) return std::unexpected(std::move(footstep.error()));
  if (*footstep)
    footstep_sound = AssetPath::relative_to(asset.path, **footstep);
  return *color;
}

std::expected<void, StarException> MaterialDatabase::AssetInfo::load_render_and_damage(
    Assets const& assets, ImageMetadataDatabase const& image_metadata,
    String const& template_base, String const& harvest_key) {
  auto render_config = asset.config.find("renderTemplate");
  if (!render_config) return std::unexpected(std::move(render_config.error()));
  if (*render_config && !(*render_config)->get().is_null()) {
    auto render_template = assets.fetch_json((*render_config)->get(), template_base);
    if (!render_template) return std::unexpected(std::move(render_template.error()));
    auto parameters = asset.config.get("renderParameters");
    if (!parameters) return std::unexpected(std::move(parameters.error()));
    auto profile = parse_material_render_profile(image_metadata, json_merge(*render_template, *parameters), asset.path);
    if (!profile) return std::unexpected(std::move(profile.error()));
    render_profile.emplace(std::move(*profile));
  }

  auto damage_config = asset.config.get("damageTable", "/tiles/defaultDamage.config");
  if (!damage_config) return std::unexpected(std::move(damage_config.error()));
  auto damage = assets.fetch_json(*damage_config);
  if (!damage) return std::unexpected(std::move(damage.error()));
  auto health = asset.config.opt_float("health");
  if (!health) return std::unexpected(std::move(health.error()));
  auto harvest = asset.config.opt_int(harvest_key);
  if (!harvest) return std::unexpected(std::move(harvest.error()));
  std::optional<unsigned> harvest_level;
  if (*harvest) {
    auto level = checked_unsigned<unsigned>(**harvest, harvest_key.utf8());
    if (!level) return std::unexpected(std::move(level.error()));
    harvest_level = *level;
  }
  auto value = TileDamageParameters::create(assets, *damage, *health, harvest_level);
  if (!value) return std::unexpected(std::move(value.error()));
  damage_parameters = std::move(*value);
  return {};
}

std::expected<std::shared_ptr<MaterialDatabase>, StarException> MaterialDatabase::create(
    Assets const& assets, ParticleDatabase const& particles, ImageMetadataDatabase const& image_metadata,
    Logger& logger, RandomService& random) {
  auto result = std::make_shared<MaterialDatabase>(ConstructionKey{}, random);
  auto loaded = result->load(assets, particles, image_metadata, logger);
  if (!loaded) return std::unexpected(std::move(loaded.error()));
  return result;
}

std::expected<void, StarException> MaterialDatabase::load_meta_materials(Assets const& assets, Logger& logger) {
  set_meta_material(empty_material_id, {"metamaterial:empty", CollisionKind::None, false});
  set_meta_material(null_material_id, {"metamaterial:null", CollisionKind::Block, true});
  set_meta_material(structure_material_id, {"metamaterial:structure", CollisionKind::Block, true});
  set_meta_material(biome_material_id, {"metamaterial:biome", CollisionKind::Block, true});
  set_meta_material(biome1_material_id, {"metamaterial:biome1", CollisionKind::Block, true});
  set_meta_material(biome2_material_id, {"metamaterial:biome2", CollisionKind::Block, true});
  set_meta_material(biome3_material_id, {"metamaterial:biome3", CollisionKind::Block, true});
  set_meta_material(biome4_material_id, {"metamaterial:biome4", CollisionKind::Block, true});
  set_meta_material(biome5_material_id, {"metamaterial:biome5", CollisionKind::Block, true});
  set_meta_material(boundary_material_id, {"metamaterial:boundary", CollisionKind::Slippery, true});
  set_meta_material(object_solid_material_id, {"metamaterial:objectsolid", CollisionKind::Block, true});
  set_meta_material(object_platform_material_id, {"metamaterial:objectplatform", CollisionKind::Platform, false});

  auto meta_config = assets.json(meta_material_path);
  if (!meta_config) return std::unexpected(std::move(meta_config.error()));
  auto metas = meta_config->iterate_array();
  if (!metas) return std::unexpected(std::move(metas.error()));
  std::size_t source_entry = 0;
  for (auto const& meta : *metas) {
    ++source_entry;
    auto name = meta.get_string("name");
    if (!name) return std::unexpected(std::move(name.error()));
    auto material_name = "metamaterial:" + *name;
    if (auto previous = m_meta_material_index.maybe(material_name)) {
      auto const& original = *m_meta_materials[meta_material_slot(*previous)];
      auto logged = logger.info("Metamaterial '{}' in {} has duplicate material name, first declared in {}",
          material_name, meta_declaration(source_entry), meta_declaration(original.source_entry));
      if (!logged) return std::unexpected(std::move(logged.error()));
      continue;
    }
    auto raw_id = meta.get_double("materialId");
    if (!raw_id) return std::unexpected(std::move(raw_id.error()));
    if (!is_finite(*raw_id) || *raw_id < first_meta_material_id || *raw_id >= first_engine_meta_material_id) {
      auto logged = logger.info("Material id {} for metamaterial '{}' in {} does not fall within the valid range!",
          *raw_id, material_name, meta_declaration(source_entry));
      if (!logged) return std::unexpected(std::move(logged.error()));
      continue;
    }
    auto id = static_cast<MaterialId>(*raw_id);
    if (contains_meta_material(id)) {
      auto const& original = *m_meta_materials[meta_material_slot(id)];
      auto logged = logger.info("Material id {} for metamaterial '{}' in {} conflicts with '{}' first declared in {}",
          id, material_name, meta_declaration(source_entry), original.name, meta_declaration(original.source_entry));
      if (!logged) return std::unexpected(std::move(logged.error()));
      continue;
    }
    auto collision_name = meta.get_string("collisionKind");
    if (!collision_name) return std::unexpected(std::move(collision_name.error()));
    auto collision = enum_value<CollisionKindNames>(*collision_name);
    if (!collision) return std::unexpected(std::move(collision.error()));
    auto blocks = meta.get_bool("blocksLiquidFlow", isSolidColliding(*collision));
    if (!blocks) return std::unexpected(std::move(blocks.error()));
    set_meta_material(id, {std::move(material_name), *collision, *blocks, source_entry});
  }
  return {};
}

std::expected<void, StarException> MaterialDatabase::load(
    Assets const& assets, ParticleDatabase const& particles, ImageMetadataDatabase const& image_metadata, Logger& logger) {
  auto meta_mods = BiMap<String, ModId>::create(
      {{"metamod:none", no_mod_id}, {"metamod:biome", biome_mod_id}, {"metamod:undergroundbiome", underground_biome_mod_id}});
  if (!meta_mods) return std::unexpected(std::move(meta_mods.error()));
  m_meta_mod_index = std::move(*meta_mods);
  auto meta_materials = load_meta_materials(assets, logger);
  if (!meta_materials)
    return std::unexpected(assets.with_asset_context(std::move(meta_materials.error()), meta_material_path, "Loading metamaterials"));

  auto materials = assets.scan_extension("material");
  if (!materials) return std::unexpected(std::move(materials.error()));
  auto mods = assets.scan_extension("matmod");
  if (!mods) return std::unexpected(std::move(mods.error()));
  auto queued_materials = assets.queue_jsons(materials->get());
  if (!queued_materials) return std::unexpected(std::move(queued_materials.error()));
  auto queued_mods = assets.queue_jsons(mods->get());
  if (!queued_mods) return std::unexpected(std::move(queued_mods.error()));

  for (auto const& path : materials->get()) {
    auto const& file = path.string();
    auto read_file = [&]() -> std::expected<void, StarException> {
      auto node = assets.json(file);
      if (!node) return std::unexpected(std::move(node.error()));
      auto id = checked_unsigned<MaterialId>(node->get_int("materialId"), "materialId");
      if (!id) return std::unexpected(std::move(id.error()));
      auto name = node->get_string("materialName");
      if (!name) return std::unexpected(std::move(name.error()));

      auto material = std::make_unique<MaterialInfo>();
      material->name = std::move(*name);
      material->asset = {file, std::move(*node)};
      auto const& config = material->asset.config;

      auto descriptions = material->load_descriptions();
      if (!descriptions) return std::unexpected(std::move(descriptions.error()));
      auto category = config.get_string("category");
      if (!category) return std::unexpected(std::move(category.error()));
      material->category = std::move(*category);
      auto color = material->load_media(particles);
      if (!color) return std::unexpected(std::move(color.error()));
      material->particle_color = *color;

      auto tillable_mod = checked_unsigned<ModId>(config.get_int("tillableMod", no_mod_id), "tillableMod");
      if (!tillable_mod) return std::unexpected(std::move(tillable_mod.error()));
      material->tillable_mod = *tillable_mod;
      auto soil = config.get_bool("soil", false);
      if (!soil) return std::unexpected(std::move(soil.error()));
      material->soil = *soil;
      auto falling = config.get_bool("falling", false);
      if (!falling) return std::unexpected(std::move(falling.error()));
      auto cascading = config.get_bool("cascading", false);
      if (!cascading) return std::unexpected(std::move(cascading.error()));
      if (*cascading)
        material->falling_behavior = FallingBehavior::Cascading;
      else if (*falling)
        material->falling_behavior = FallingBehavior::Falling;

      // Materials resolve the render template relative to the declaring file.
      auto render_and_damage = material->load_render_and_damage(assets, image_metadata, file, "requiredHarvestLevel");
      if (!render_and_damage) return std::unexpected(std::move(render_and_damage.error()));
      auto collision_name = config.get_string("collisionKind", "block");
      if (!collision_name) return std::unexpected(std::move(collision_name.error()));
      auto collision = enum_value<CollisionKindNames>(*collision_name);
      if (!collision) return std::unexpected(std::move(collision.error()));
      material->collision_kind = *collision;
      auto foreground_only = config.get_bool("foregroundOnly", false);
      if (!foreground_only) return std::unexpected(std::move(foreground_only.error()));
      material->foreground_only = *foreground_only;
      auto supports_mods = config.get_bool("supportsMods",
          material->falling_behavior == FallingBehavior::Stationary && *collision == CollisionKind::Block);
      if (!supports_mods) return std::unexpected(std::move(supports_mods.error()));
      material->supports_mods = *supports_mods;
      auto blocks = config.get_bool("blocksLiquidFlow", isSolidColliding(*collision));
      if (!blocks) return std::unexpected(std::move(blocks.error()));
      material->blocks_liquid_flow = *blocks;

      auto registered = register_material(assets, *id, std::move(material));
      if (!registered) return std::unexpected(std::move(registered.error()));
      return load_liquid_interactions(config, *id, "transformMaterialId", null_material_id, m_liquid_material_interactions);
    };
    auto loaded = read_file();
    if (!loaded)
      return std::unexpected(assets.with_asset_context(
          MaterialException(Star::format("Error loading material file {}", file), loaded.error()), file, "Loading material"));
  }

  for (auto const& path : mods->get()) {
    auto const& file = path.string();
    auto read_file = [&]() -> std::expected<void, StarException> {
      auto node = assets.json(file);
      if (!node) return std::unexpected(std::move(node.error()));
      auto id = checked_unsigned<ModId>(node->get_int("modId"), "modId");
      if (!id) return std::unexpected(std::move(id.error()));
      auto name = node->get_string("modName");
      if (!name) return std::unexpected(std::move(name.error()));

      auto mod = std::make_unique<ModInfo>();
      mod->name = std::move(*name);
      mod->asset = {file, std::move(*node)};
      auto const& config = mod->asset.config;

      auto descriptions = mod->load_descriptions();
      if (!descriptions) return std::unexpected(std::move(descriptions.error()));
      auto color = mod->load_media(particles);
      if (!color) return std::unexpected(std::move(color.error()));
      auto tilled = config.get_bool("tilled", false);
      if (!tilled) return std::unexpected(std::move(tilled.error()));
      mod->tilled = *tilled;
      auto breaks_with_tile = config.get_bool("breaksWithTile", false);
      if (!breaks_with_tile) return std::unexpected(std::move(breaks_with_tile.error()));
      mod->breaks_with_tile = *breaks_with_tile;

      // Matmods resolve the render template from the asset root, not the declaring file.
      auto render_and_damage = mod->load_render_and_damage(assets, image_metadata, "/", "harvestLevel");
      if (!render_and_damage) return std::unexpected(std::move(render_and_damage.error()));
      auto registered = register_mod(assets, *id, std::move(mod));
      if (!registered) return std::unexpected(std::move(registered.error()));
      return load_liquid_interactions(config, *id, "transformModId", no_mod_id, m_liquid_mod_interactions);
    };
    auto loaded = read_file();
    if (!loaded)
      return std::unexpected(assets.with_asset_context(
          MaterialException(Star::format("Error loading mod file {}", file), loaded.error()), file, "Loading material mod"));
  }

  String const footstep_path = "/client.config:defaultFootstepSound";
  auto footstep_config = assets.json(footstep_path);
  if (!footstep_config)
    return std::unexpected(assets.with_asset_context(std::move(footstep_config.error()), footstep_path, "Loading default footstep sound"));
  auto footstep = footstep_config->to_string();
  if (!footstep)
    return std::unexpected(assets.with_asset_context(std::move(footstep.error()), footstep_path, "Loading default footstep sound"));
  m_default_footstep_sound = std::move(*footstep);
  return {};
}

StringList MaterialDatabase::material_names() const {
  StringList names;
  names.reserve(m_material_index.size() + m_meta_material_index.size());
  for (auto const& [name, id] : m_material_index)
    names.push_back(name);
  for (auto const& [name, id] : m_meta_material_index)
    names.push_back(name);
  return names;
}

bool MaterialDatabase::is_meta_material_name(String const& name) const {
  return m_meta_material_index.contains(name);
}

bool MaterialDatabase::is_material_name(String const& name) const {
  return m_material_index.contains(name) || m_meta_material_index.contains(name);
}

bool MaterialDatabase::is_valid_material_id(MaterialId id) const {
  return is_real_material(id) ? contains_material(id) : contains_meta_material(id);
}

std::expected<MaterialId, StarException> MaterialDatabase::material_id(String const& name) const {
  if (auto id = m_meta_material_index.maybe(name)) return *id;
  auto id = m_material_index.get(name);
  if (!id) return std::unexpected(std::move(id.error()));
  return id->get();
}

std::expected<String, StarException> MaterialDatabase::material_name(MaterialId id) const {
  if (is_real_material(id)) {
    auto info = get_material_info(id);
    if (!info) return std::unexpected(std::move(info.error()));
    return info->get().name;
  }
  auto info = get_meta_material_info(id);
  if (!info) return std::unexpected(std::move(info.error()));
  return info->get().name;
}

std::expected<std::optional<std::reference_wrapper<MaterialAsset const>>, StarException> MaterialDatabase::material_asset(MaterialId id) const {
  if (!is_real_material(id)) return std::nullopt;
  auto info = get_material_info(id);
  if (!info) return std::unexpected(std::move(info.error()));
  return std::cref(info->get().asset);
}

std::expected<String, StarException> MaterialDatabase::material_description(MaterialId id, String const& species) const {
  auto info = get_material_info(id);
  if (!info) return std::unexpected(std::move(info.error()));
  return info->get().description_for(species);
}

std::expected<String, StarException> MaterialDatabase::material_description(MaterialId id) const {
  auto info = get_material_info(id);
  if (!info) return std::unexpected(std::move(info.error()));
  return info->get().description;
}

std::expected<String, StarException> MaterialDatabase::material_short_description(MaterialId id) const {
  auto info = get_material_info(id);
  if (!info) return std::unexpected(std::move(info.error()));
  return info->get().short_description;
}

std::expected<String, StarException> MaterialDatabase::material_category(MaterialId id) const {
  auto info = get_material_info(id);
  if (!info) return std::unexpected(std::move(info.error()));
  return info->get().category;
}

StringList MaterialDatabase::mod_names() const {
  StringList names;
  names.reserve(m_mod_index.size() + m_meta_mod_index.size());
  for (auto const& [name, id] : m_mod_index)
    names.push_back(name);
  for (auto [name, id] : m_meta_mod_index)
    names.push_back(name);
  return names;
}

bool MaterialDatabase::is_mod_name(String const& name) const {
  return m_mod_index.contains(name) || m_meta_mod_index.contains_left(name);
}

bool MaterialDatabase::is_valid_mod_id(ModId id) const {
  return is_real_mod(id) ? contains_mod(id) : m_meta_mod_index.contains_right(id);
}

std::expected<ModId, StarException> MaterialDatabase::mod_id(String const& name) const {
  if (auto id = m_meta_mod_index.maybe_right(name)) return *id;
  auto id = m_mod_index.get(name);
  if (!id) return std::unexpected(std::move(id.error()));
  return id->get();
}

std::expected<std::reference_wrapper<String const>, StarException> MaterialDatabase::mod_name(ModId id) const {
  if (!is_real_mod(id))
    return m_meta_mod_index.get_left(id);
  auto info = get_mod_info(id);
  if (!info) return std::unexpected(std::move(info.error()));
  return std::cref(info->get().name);
}

std::expected<std::optional<std::reference_wrapper<MaterialAsset const>>, StarException> MaterialDatabase::mod_asset(ModId id) const {
  if (!is_real_mod(id)) return std::nullopt;
  auto info = get_mod_info(id);
  if (!info) return std::unexpected(std::move(info.error()));
  return std::cref(info->get().asset);
}

std::expected<String, StarException> MaterialDatabase::mod_description(ModId id, String const& species) const {
  auto info = get_mod_info(id);
  if (!info) return std::unexpected(std::move(info.error()));
  return info->get().description_for(species);
}

std::expected<String, StarException> MaterialDatabase::mod_description(ModId id) const {
  auto info = get_mod_info(id);
  if (!info) return std::unexpected(std::move(info.error()));
  return info->get().description;
}

std::expected<String, StarException> MaterialDatabase::mod_short_description(ModId id) const {
  auto info = get_mod_info(id);
  if (!info) return std::unexpected(std::move(info.error()));
  return info->get().short_description;
}

String MaterialDatabase::default_footstep_sound() const {
  return m_default_footstep_sound;
}

std::expected<std::reference_wrapper<TileDamageParameters const>, StarException> MaterialDatabase::material_damage_parameters(MaterialId id) const {
  if (!is_real_material(id)) return std::cref(default_damage_parameters());
  auto info = get_material_info(id);
  if (!info) return std::unexpected(std::move(info.error()));
  return std::cref(info->get().damage_parameters);
}

std::expected<std::reference_wrapper<TileDamageParameters const>, StarException> MaterialDatabase::mod_damage_parameters(ModId id) const {
  if (!is_real_mod(id)) return std::cref(default_damage_parameters());
  auto info = get_mod_info(id);
  if (!info) return std::unexpected(std::move(info.error()));
  return std::cref(info->get().damage_parameters);
}

std::expected<bool, StarException> MaterialDatabase::mod_breaks_with_tile(ModId id) const {
  if (!is_real_mod(id)) return false;
  auto info = get_mod_info(id);
  if (!info) return std::unexpected(std::move(info.error()));
  return info->get().breaks_with_tile;
}

std::expected<CollisionKind, StarException> MaterialDatabase::material_collision_kind(MaterialId id) const {
  if (is_real_material(id)) {
    auto info = get_material_info(id);
    if (!info) return std::unexpected(std::move(info.error()));
    return info->get().collision_kind;
  }
  if (contains_meta_material(id))
    return m_meta_materials[meta_material_slot(id)]->collision_kind;
  return CollisionKind::Block;
}

std::expected<bool, StarException> MaterialDatabase::can_place_in_layer(MaterialId id, TileLayer layer) const {
  if (layer != TileLayer::Background) return true;
  auto info = get_material_info(id);
  if (!info) return std::unexpected(std::move(info.error()));
  return !info->get().foreground_only;
}

std::expected<ItemDescriptor, StarException> MaterialDatabase::material_item_drop(MaterialId id) const {
  if (is_real_material(id)) {
    auto info = get_material_info(id);
    if (!info) return std::unexpected(std::move(info.error()));
    auto const& drop = info->get().item_drop;
    if (!drop.empty()) return ItemDescriptor::create(drop, 1, JsonObject());
  }
  return ItemDescriptor();
}

std::expected<ItemDescriptor, StarException> MaterialDatabase::mod_item_drop(ModId id) const {
  if (is_real_mod(id)) {
    auto info = get_mod_info(id);
    if (!info) return std::unexpected(std::move(info.error()));
    auto const& drop = info->get().item_drop;
    if (!drop.empty()) return ItemDescriptor::create(drop, 1);
  }
  return ItemDescriptor();
}

std::expected<MaterialColorVariant, StarException> MaterialDatabase::material_color_variants(MaterialId id) const {
  if (is_real_material(id)) {
    auto info = get_material_info(id);
    if (!info) return std::unexpected(std::move(info.error()));
    if (auto const& profile = info->get().render_profile) return profile->color_variants;
  }
  return 0;
}

std::expected<MaterialColorVariant, StarException> MaterialDatabase::mod_color_variants(ModId id) const {
  if (is_real_mod(id)) {
    auto info = get_mod_info(id);
    if (!info) return std::unexpected(std::move(info.error()));
    if (auto const& profile = info->get().render_profile) return profile->color_variants;
  }
  return 0;
}

std::expected<bool, StarException> MaterialDatabase::is_multi_color(MaterialId id) const {
  auto variants = material_color_variants(id);
  if (!variants) return std::unexpected(std::move(variants.error()));
  return *variants > 0;
}

std::expected<std::shared_ptr<ParticleConfig>, StarException> MaterialDatabase::mining_particle(MaterialId material_id, ModId mod_id) const {
  if (is_real_mod(mod_id)) {
    auto info = get_mod_info(mod_id);
    if (!info) return std::unexpected(std::move(info.error()));
    if (auto const& particle = info->get().mining_particle) return particle;
  }
  if (is_real_material(material_id)) {
    auto info = get_material_info(material_id);
    if (!info) return std::unexpected(std::move(info.error()));
    if (auto const& particle = info->get().mining_particle) return particle;
  }
  return std::shared_ptr<ParticleConfig>();
}

std::expected<String, StarException> MaterialDatabase::mining_sound(MaterialId material_id, ModId mod_id) const {
  if (is_real_mod(mod_id)) {
    auto info = get_mod_info(mod_id);
    if (!info) return std::unexpected(std::move(info.error()));
    auto const& sounds = info->get().mining_sounds;
    if (!sounds.empty()) return m_random.sample_value(sounds);
  }
  if (is_real_material(material_id)) {
    auto info = get_material_info(material_id);
    if (!info) return std::unexpected(std::move(info.error()));
    auto const& sounds = info->get().mining_sounds;
    if (!sounds.empty()) return m_random.sample_value(sounds);
  }
  return String();
}

std::expected<String, StarException> MaterialDatabase::footstep_sound(MaterialId material_id, ModId mod_id) const {
  if (is_real_mod(mod_id)) {
    auto info = get_mod_info(mod_id);
    if (!info) return std::unexpected(std::move(info.error()));
    auto const& sound = info->get().footstep_sound;
    if (!sound.empty()) return sound;
  }
  if (is_real_material(material_id)) {
    auto info = get_material_info(material_id);
    if (!info) return std::unexpected(std::move(info.error()));
    auto const& sound = info->get().footstep_sound;
    if (!sound.empty()) return sound;
  }
  return m_default_footstep_sound;
}

std::expected<Color, StarException> MaterialDatabase::material_particle_color(MaterialId id, MaterialHue hue_shift) const {
  auto info = get_material_info(id);
  if (!info) return std::unexpected(std::move(info.error()));
  auto color = info->get().particle_color;
  color.set_hue(positive_mod(color.hue() + material_hue_to_degrees(hue_shift) / 360.0f, 1.0f));
  return color;
}

std::expected<bool, StarException> MaterialDatabase::is_tilled_mod(ModId id) const {
  if (!is_real_mod(id)) return false;
  auto info = get_mod_info(id);
  if (!info) return std::unexpected(std::move(info.error()));
  return info->get().tilled;
}

std::expected<bool, StarException> MaterialDatabase::is_soil(MaterialId id) const {
  if (!is_real_material(id)) return false;
  auto info = get_material_info(id);
  if (!info) return std::unexpected(std::move(info.error()));
  return info->get().soil;
}

std::expected<ModId, StarException> MaterialDatabase::tilled_mod_for(MaterialId id) const {
  if (!is_real_material(id)) return no_mod_id;
  auto info = get_material_info(id);
  if (!info) return std::unexpected(std::move(info.error()));
  return info->get().tillable_mod;
}

std::expected<FallingBehavior, StarException> MaterialDatabase::falling_behavior(MaterialId id) const {
  if (!is_real_material(id)) return FallingBehavior::Stationary;
  auto info = get_material_info(id);
  if (!info) return std::unexpected(std::move(info.error()));
  return info->get().falling_behavior;
}

std::expected<bool, StarException> MaterialDatabase::supports_mod(MaterialId material_id, ModId mod_id) const {
  if (mod_id == no_mod_id) return true;
  if (!is_real_material(material_id) || !is_real_mod(mod_id)) return false;
  auto info = get_material_info(material_id);
  if (!info) return std::unexpected(std::move(info.error()));
  return info->get().supports_mods;
}

std::size_t MaterialDatabase::meta_material_slot(MaterialId id) {
  return id - first_meta_material_id;
}

bool MaterialDatabase::contains_meta_material(MaterialId id) const {
  auto slot = meta_material_slot(id);
  return slot < m_meta_materials.size() && m_meta_materials[slot];
}

void MaterialDatabase::set_meta_material(MaterialId id, MetaMaterialInfo info) {
  auto slot = meta_material_slot(id);
  if (slot >= m_meta_materials.size())
    m_meta_materials.resize(slot + 1);
  m_meta_material_index.try_emplace(info.name, id);
  m_meta_materials[slot] = std::make_unique<MetaMaterialInfo const>(std::move(info));
}

std::expected<void, StarException> MaterialDatabase::register_material(Assets const& assets, MaterialId id, std::unique_ptr<MaterialInfo> info) {
  auto const& path = info->asset.path;
  if (!is_real_material(id))
    return std::unexpected(MaterialException::format("materialId {} in '{}' is reserved for metamaterials", id, path));
  if (contains_material(id)) {
    auto const& original = m_materials[id]->asset.path;
    return std::unexpected(duplicate_asset(assets, MaterialException::format(
        "Duplicate material id {} in '{}', first declared in '{}'", id, path, original), original, "Loading material"));
  }
  if (auto meta_id = m_meta_material_index.maybe(info->name)) {
    auto const& meta = *m_meta_materials[meta_material_slot(*meta_id)];
    auto error = MaterialException::format(
        "Duplicate material name '{}' in '{}', first declared in {}", info->name, path, meta_declaration(meta.source_entry));
    if (meta.source_entry == 0) return std::unexpected(std::move(error));
    return std::unexpected(duplicate_asset(assets, std::move(error), meta_material_path, "Loading metamaterials"));
  }
  auto [entry, inserted] = m_material_index.try_emplace(info->name, id);
  if (!inserted) {
    auto const& original = m_materials[entry->second]->asset.path;
    return std::unexpected(duplicate_asset(assets, MaterialException::format(
        "Duplicate material name '{}' in '{}', first declared in '{}'", info->name, path, original), original, "Loading material"));
  }
  if (id >= m_materials.size())
    m_materials.resize(id + 1);
  m_materials[id] = std::move(info);
  return {};
}

std::expected<void, StarException> MaterialDatabase::register_mod(Assets const& assets, ModId id, std::unique_ptr<ModInfo> info) {
  auto const& path = info->asset.path;
  if (!is_real_mod(id))
    return std::unexpected(MaterialException::format("modId {} in '{}' is reserved for metamods", id, path));
  if (contains_mod(id)) {
    auto const& original = m_mods[id]->asset.path;
    return std::unexpected(duplicate_asset(assets, MaterialException::format(
        "Duplicate mod id {} in '{}', first declared in '{}'", id, path, original), original, "Loading material mod"));
  }
  if (m_meta_mod_index.contains_left(info->name))
    return std::unexpected(MaterialException::format(
        "Duplicate mod name '{}' in '{}', first declared in the engine", info->name, path));
  auto [entry, inserted] = m_mod_index.try_emplace(info->name, id);
  if (!inserted) {
    auto const& original = m_mods[entry->second]->asset.path;
    return std::unexpected(duplicate_asset(assets, MaterialException::format(
        "Duplicate mod name '{}' in '{}', first declared in '{}'", info->name, path, original), original, "Loading material mod"));
  }
  if (id >= m_mods.size())
    m_mods.resize(id + 1);
  m_mods[id] = std::move(info);
  return {};
}

std::expected<std::reference_wrapper<MaterialDatabase::MetaMaterialInfo const>, StarException> MaterialDatabase::get_meta_material_info(MaterialId id) const {
  if (!contains_meta_material(id)) return std::unexpected(MaterialException::format("No such metamaterial id: {}\n", id));
  return std::cref(*m_meta_materials[meta_material_slot(id)]);
}

std::expected<std::reference_wrapper<MaterialDatabase::MaterialInfo const>, StarException> MaterialDatabase::get_material_info(MaterialId id) const {
  if (!contains_material(id)) return std::unexpected(MaterialException::format("No such material id: {}\n", id));
  return std::cref(*m_materials[id]);
}

std::expected<std::reference_wrapper<MaterialDatabase::ModInfo const>, StarException> MaterialDatabase::get_mod_info(ModId id) const {
  if (!contains_mod(id)) return std::unexpected(MaterialException::format("No such mod id: {}\n", id));
  return std::cref(*m_mods[id]);
}

}
