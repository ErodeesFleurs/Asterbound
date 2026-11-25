import std;
import star.assets;
import star.cellular_liquid;
import star.color;
import star.either;
import star.format;
import star.item_descriptor;
import star.json;
import star.json_extra;
import star.liquid_types;
import star.liquids_database;
import star.material_database;
import star.string;
import star.vector;

namespace Star {

namespace {

template <std::unsigned_integral Number>
std::expected<Number, StarException> checked_unsigned(
    std::expected<std::uint64_t, StarException> value, std::string_view field) {
  if (!value)
    return std::unexpected(std::move(value.error()));
  if (!std::in_range<Number>(*value))
    return std::unexpected(LiquidException::format(
      "{} value {} is out of range [0, {}]", field, *value, std::numeric_limits<Number>::max()));
  return static_cast<Number>(*value);
}

std::expected<void, StarException> load_interactions(
    Json const& config, MaterialDatabase const& material_database, LiquidSettings& settings) {
  auto interactions_node = config.find("interactions");
  if (!interactions_node)
    return std::unexpected(std::move(interactions_node.error()));
  if (!*interactions_node || (*interactions_node)->get().is_null())
    return {};
  auto interactions = (*interactions_node)->get().iterate_array();
  if (!interactions)
    return std::unexpected(std::move(interactions.error()));

  // Targets and results may refer to liquids registered later. Each target's last result wins.
  for (auto const& interaction : *interactions) {
    auto other_id = checked_unsigned<LiquidId>(interaction.get_uint("liquid"), "liquid");
    if (!other_id)
      return std::unexpected(std::move(other_id.error()));
    auto material_result = interaction.opt_string("materialResult");
    if (!material_result)
      return std::unexpected(std::move(material_result.error()));
    if (*material_result) {
      auto material = material_database.materialId(**material_result);
      if (!material)
        return std::unexpected(std::move(material.error()));
      settings.interactions.insert_or_assign(*other_id, make_left(*material));
    } else {
      auto liquid_result = interaction.opt_uint("liquidResult");
      if (!liquid_result)
        return std::unexpected(std::move(liquid_result.error()));
      if (!*liquid_result)
        return std::unexpected(LiquidException::format(
          "Neither materialResult nor liquidResult specified in liquid interaction of liquid {}", settings.id));
      auto result_id = checked_unsigned<LiquidId>(**liquid_result, "liquidResult");
      if (!result_id)
        return std::unexpected(std::move(result_id.error()));
      settings.interactions.insert_or_assign(*other_id, make_right(*result_id));
    }
  }
  return {};
}

StarException duplicate_liquid(Assets const& assets, StarException error, String const& original_path) {
  auto original = assets.with_asset_context(
    LiquidException("Original liquid declaration", false), original_path, "Loading liquid");
  return std::move(error).with_related(original, "Original declaration");
}

}

std::expected<std::shared_ptr<LiquidsDatabase>, StarException> LiquidsDatabase::create(
    Assets const& assets, MaterialDatabase const& material_database) {
  struct MakeShared final : LiquidsDatabase {};
  auto result = std::make_shared<MakeShared>();
  auto load_engine_parameters = [&]() -> std::expected<void, StarException> {
    auto config = assets.json("/liquids.config");
    if (!config) return std::unexpected(std::move(config.error()));
    auto engine = config->get("liquidEngineParameters");
    if (!engine) return std::unexpected(std::move(engine.error()));
    auto& parameters = result->m_liquid_engine_parameters;
    const std::pair<std::string_view, std::reference_wrapper<float>> fields[] = {
      {"lateralMoveFactor", parameters.lateral_move_factor},
      {"spreadOverfillUpFactor", parameters.spread_overfill_up_factor},
      {"spreadOverfillLateralFactor", parameters.spread_overfill_lateral_factor},
      {"spreadOverfillDownFactor", parameters.spread_overfill_down_factor},
      {"pressureEqualizeFactor", parameters.pressure_equalize_factor},
      {"pressureMoveFactor", parameters.pressure_move_factor},
      {"maximumPressureLevelImbalance", parameters.maximum_pressure_level_imbalance},
      {"minimumLivenPressureChange", parameters.minimum_liven_pressure_change},
      {"minimumLivenLevelChange", parameters.minimum_liven_level_change},
      {"minimumLiquidLevel", parameters.minimum_liquid_level},
      {"interactTransformationLevel", parameters.interact_transformation_level}};
    for (auto const& [name, member] : fields) {
      auto value = engine->get_float(name);
      if (!value) return std::unexpected(std::move(value.error()));
      member.get() = *value;
    }
    auto background_drain = config->get_float("backgroundDrain");
    if (!background_drain) return std::unexpected(std::move(background_drain.error()));
    result->m_background_drain = *background_drain;
    return {};
  };
  auto engine_parameters = load_engine_parameters();
  if (!engine_parameters)
    return std::unexpected(assets.with_asset_context(
      std::move(engine_parameters.error()), "/liquids.config", "Loading liquid engine parameters"));
  result->m_liquid_names.try_emplace("empty", EmptyLiquidId);
  auto liquids = assets.scan_extension("liquid");
  if (!liquids) return std::unexpected(std::move(liquids.error()));
  auto queued = assets.queue_jsons(liquids->get());
  if (!queued) return std::unexpected(std::move(queued.error()));
  result->m_settings.reserve(std::min(liquids->get().size(), result->m_setting_slots.size() - 1));

  for (auto const& path : liquids->get()) {
    auto const& file = path.string();
    auto read_file = [&]() -> std::expected<void, StarException> {
      auto config = assets.json(file);
      if (!config)
        return std::unexpected(std::move(config.error()));
      auto id = checked_unsigned<LiquidId>(config->get_uint("liquidId"), "liquidId");
      if (!id)
        return std::unexpected(std::move(id.error()));
      auto name = config->get_string("name");
      if (!name)
        return std::unexpected(std::move(name.error()));
      auto description = config->get_string("description", "");
      if (!description)
        return std::unexpected(std::move(description.error()));
      auto tick_delta = checked_unsigned<unsigned>(config->get_uint("tickDelta"), "tickDelta");
      if (!tick_delta)
        return std::unexpected(std::move(tick_delta.error()));

      auto color = config->get("color").and_then(json_to_vec4b);
      if (!color)
        return std::unexpected(std::move(color.error()));
      Vec3F radiant_light_level;
      auto light_config = config->opt("radiantLight");
      if (!light_config)
        return std::unexpected(std::move(light_config.error()));
      if (*light_config) {
        auto light = json_to_vec3b(**light_config);
        if (!light)
          return std::unexpected(std::move(light.error()));
        radiant_light_level = Color::rgb(*light).to_rgb_f();
      }
      auto item_drop = config->get("itemDrop", {}).and_then(
        [](Json const& spec) { return ItemDescriptor::create(spec); });
      if (!item_drop)
        return std::unexpected(std::move(item_drop.error()));
      auto status_effects = config->get_array("statusEffects", {});
      if (!status_effects)
        return std::unexpected(std::move(status_effects.error()));

      auto& entry = result->m_settings.emplace_back();
      entry.id = *id;
      auto interactions = load_interactions(*config, material_database, entry);
      if (!interactions)
        return std::unexpected(std::move(interactions.error()));

      if (*id == EmptyLiquidId)
        return std::unexpected(LiquidException::format(
          "Reserved liquidId {} in '{}': the built-in 'empty' liquid already uses it", *id, file));
      if (auto previous = result->liquid_settings(*id)) {
        auto const& original_path = previous->get().path;
        return std::unexpected(duplicate_liquid(assets, LiquidException::format(
          "Duplicate liquidId {} in '{}', first declared in '{}'", *id, file, original_path), original_path));
      }
      auto [name_entry, inserted] = result->m_liquid_names.try_emplace(*name, *id);
      if (!inserted) {
        if (name_entry->second == EmptyLiquidId)
          return std::unexpected(LiquidException::format(
            "Reserved liquid name '{}' in '{}': the built-in 'empty' liquid already uses it", *name, file));
        auto const& original_path = result->liquid_settings(name_entry->second)->get().path;
        return std::unexpected(duplicate_liquid(assets, LiquidException::format(
          "Duplicate liquid name '{}' in '{}', first declared in '{}'", *name, file, original_path), original_path));
      }
      entry.name = std::move(*name);
      entry.description = std::move(*description);
      entry.tick_delta = *tick_delta;
      entry.liquid_color = *color;
      entry.radiant_light_level = radiant_light_level;
      entry.item_drop = std::move(*item_drop);
      entry.status_effects = std::move(*status_effects);
      entry.path = file;
      entry.config = std::move(*config);
      result->m_setting_slots[*id] = static_cast<LiquidId>(result->m_settings.size());
      return {};
    };
    auto loaded = read_file();
    if (!loaded)
      return std::unexpected(assets.with_asset_context(
        LiquidException(Star::format("Error loading liquid file {}", file), std::move(loaded.error())),
        file, "Loading liquid"));
  }

  std::ranges::sort(result->m_settings, {}, [](LiquidSettings const& entry) { return entry.id; });
  LiquidId slot = 0;
  for (auto const& entry : result->m_settings)
    result->m_setting_slots[entry.id] = ++slot;
  return result;
}

LiquidCellEngineParameters LiquidsDatabase::liquid_engine_parameters() const {
  return m_liquid_engine_parameters;
}

StringList LiquidsDatabase::liquid_names() const {
  return m_liquid_names.keys();
}

std::expected<LiquidId, StarException> LiquidsDatabase::liquid_id(String const& name) const {
  auto id = m_liquid_names.get(name);
  if (!id) return std::unexpected(std::move(id.error()));
  return id->get();
}

std::expected<String, StarException> LiquidsDatabase::liquid_name(LiquidId liquid_id) const {
  if (liquid_id == EmptyLiquidId) return "empty";
  if (auto settings = liquid_settings(liquid_id)) return settings->get().name;
  return std::unexpected(LiquidException::format("invalid liquid id {}", liquid_id));
}

std::expected<String, StarException> LiquidsDatabase::liquid_description(LiquidId liquid_id, String const& species) const {
  if (liquid_id == EmptyLiquidId) return String();
  if (auto settings = liquid_settings(liquid_id)) {
    auto const& entry = settings->get();
    auto description = entry.config.opt_string(Star::format("{}Description", species));
    if (!description) return std::unexpected(std::move(description.error()));
    if (*description) return std::move(**description);
    return entry.description;
  }
  return std::unexpected(LiquidException::format("invalid liquid id {}", liquid_id));
}

std::expected<String, StarException> LiquidsDatabase::liquid_description(LiquidId liquid_id) const {
  if (liquid_id == EmptyLiquidId) return String();
  if (auto settings = liquid_settings(liquid_id)) return settings->get().description;
  return std::unexpected(LiquidException::format("invalid liquid id {}", liquid_id));
}

std::optional<LiquidInteractionResult> LiquidsDatabase::interact(LiquidId target, LiquidId other) const {
  if (auto settings = liquid_settings(target))
    return settings->get().interactions.maybe(other);
  return {};
}

}
