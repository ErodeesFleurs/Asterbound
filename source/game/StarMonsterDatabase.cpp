import star.json_extra;
import star.json;
import star.id_map;
import star.game_types;
import star.casting;
import star.strong_typedef;
import star.list;
import star.rpc_promise;
import star.bi_map;
import star.a_star;
import star.ordered_map;
import star.lru_cache;
import star.thread;
import star.time;
import star.random;
import star.ttl_cache;
import star.data_stream;
import star.image_processing;
import star.color;
import star.string;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.data_stream_devices;
import star.lexical_cast;
import star.periodic_function;
import star.audio;
import star.map;
import star.set;
import star.lua;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.format;

import star.utility_lua_bindings;

import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.tile_damage;
import star.interaction_types;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_modification;
import star.lua_root;
import star.force_regions;
import star.world;
import star.physics_entity;
import star.movement_controller;
import star.platformer_astar_types;
import star.anchorable_entity;

import star.game_timers;
import star.actor_movement_controller;
import star.damage_types;
import star.drawable;
import star.entity_rendering_types;
import star.animated_part_set;
import star.animation;
import star.particle;
import star.mixer;
import star.networked_animator;
import star.entity_rendering;
import star.portrait_entity;
import star.damage_bar_entity;
import star.nametag_entity;
import star.aggressive_entity;
import star.scripted_entity;
import star.chat_action;
import star.chatty_entity;
import star.lua_components;
import star.lua_animation_component;
import star.lua_actor_movement_component;
import star.mobile_entity;
import star.actor_entity;
import star.effect_emitter;
import star.monster;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.rebuilder;

import star.monster_database;

import star.net_compatibility;
import star.byte_array;
import star.static_random;
import star.math_common;
namespace Star {
namespace {
template<std::unsigned_integral T>
  requires (std::numeric_limits<T>::digits <= std::numeric_limits<double>::digits)
std::expected<T, StarException> read_unsigned_field(Json const& parameters, String const& key, T fallback) {
  auto field = parameters.find(key);
  if (!field) return std::unexpected(std::move(field.error()));
  if (!*field || (**field).get().is_null()) return fallback;
  auto number = (**field).get().to_double();
  if (!number) return std::unexpected(std::move(number.error()));
  if (!is_finite(*number) || *number < 0 || *number > std::numeric_limits<T>::max() || std::trunc(*number) != *number)
    return std::unexpected(MonsterException(Star::format("Invalid unsigned integer for '{}'", key)));
  return static_cast<T>(*number);
}
}

MonsterDatabase::MonsterDatabase(ConstructionKey, Root& root) : m_root(root), m_monster_cache(root.random()) {}

std::expected<std::shared_ptr<MonsterDatabase>, StarException> MonsterDatabase::create(Root& root) {
  auto database = std::make_shared<MonsterDatabase>(ConstructionKey{}, root);
  auto initialized = database->initialize();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return database;
}

std::expected<void, StarException> MonsterDatabase::initialize() {
  auto rebuilder = Rebuilder::create(m_root, "monster");
  if (!rebuilder)
    return std::unexpected(std::move(rebuilder.error()));
  m_rebuilder = std::move(*rebuilder);
  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));

  auto monster_types = (*assets)->scan_extension("monstertype");
  if (!monster_types)
    return std::unexpected(std::move(monster_types.error()));
  auto monster_parts = (*assets)->scan_extension("monsterpart");
  if (!monster_parts)
    return std::unexpected(std::move(monster_parts.error()));
  auto monster_skills = (*assets)->scan_extension("monsterskill");
  if (!monster_skills)
    return std::unexpected(std::move(monster_skills.error()));
  auto monster_colors = (*assets)->scan_extension("monstercolors");
  if (!monster_colors)
    return std::unexpected(std::move(monster_colors.error()));

  for (auto const& files : {*monster_types, *monster_parts, *monster_skills, *monster_colors}) {
    if (auto queued = (*assets)->queue_jsons(files.get()); !queued)
      return std::unexpected(std::move(queued.error()));
  }

  for (auto const& asset_file : monster_types->get()) {
    auto const& file = asset_file.string();
    auto loaded = [&, self = std::ref(*this)]() -> std::expected<void, StarException> {
      auto config = (*assets)->json(file);
      if (!config)
        return std::unexpected(std::move(config.error()));
      auto type_name = config->get_string("type");
      if (!type_name)
        return std::unexpected(std::move(type_name.error()));
      if (self.get().m_monster_types.contains(*type_name))
        return std::unexpected(MonsterException(Star::format("Repeat monster type name '{}'", *type_name)));

      auto& monster_type = self.get().m_monster_types[*type_name];
      monster_type.type_name = std::move(*type_name);
      auto short_description = config->opt_string("shortdescription");
      if (!short_description)
        return std::unexpected(std::move(short_description.error()));
      monster_type.short_description = std::move(*short_description);
      auto description = config->opt_string("description");
      if (!description)
        return std::unexpected(std::move(description.error()));
      monster_type.description = std::move(*description);

      auto categories = config->get("categories");
      if (!categories)
        return std::unexpected(std::move(categories.error()));
      auto category_names = json_to_string_list(*categories);
      if (!category_names)
        return std::unexpected(std::move(category_names.error()));
      monster_type.categories = std::move(*category_names);
      auto parts = config->get("parts");
      if (!parts)
        return std::unexpected(std::move(parts.error()));
      auto part_types = json_to_string_list(*parts);
      if (!part_types)
        return std::unexpected(std::move(part_types.error()));
      monster_type.part_types = std::move(*part_types);

      auto animation = config->get_string("animation");
      if (!animation)
        return std::unexpected(std::move(animation.error()));
      monster_type.animation_config_path = AssetPath::relative_to(file, *animation);
      auto colors = config->get_string("colors", "default");
      if (!colors)
        return std::unexpected(std::move(colors.error()));
      monster_type.colors = std::move(*colors);
      auto reversed = config->get_bool("reversed", false);
      if (!reversed)
        return std::unexpected(std::move(reversed.error()));
      monster_type.reversed = *reversed;

      auto has_drop_pools = config->contains("dropPools");
      if (!has_drop_pools)
        return std::unexpected(std::move(has_drop_pools.error()));
      if (*has_drop_pools) {
        auto drop_pools = config->get_array("dropPools");
        if (!drop_pools)
          return std::unexpected(std::move(drop_pools.error()));
        monster_type.drop_pools = std::move(*drop_pools);
      }
      auto base_parameters = config->get("baseParameters", {});
      if (!base_parameters)
        return std::unexpected(std::move(base_parameters.error()));
      monster_type.base_parameters = std::move(*base_parameters);

      auto has_part_parameters = config->contains("partParameters");
      if (!has_part_parameters)
        return std::unexpected(std::move(has_part_parameters.error()));
      if (*has_part_parameters) {
        auto part_parameters_file = config->get_string("partParameters");
        if (!part_parameters_file)
          return std::unexpected(std::move(part_parameters_file.error()));
        auto part_parameters_path = AssetPath::relative_to(file, *part_parameters_file);
        auto part_parameters = [&]() -> std::expected<void, StarException> {
          auto source = (*assets)->json(part_parameters_path);
          if (!source)
            return std::unexpected(std::move(source.error()));
          auto description = source->get_object("partParameterDescription");
          if (!description)
            return std::unexpected(std::move(description.error()));
          monster_type.part_parameter_description = std::move(*description);
          auto overrides = source->get_object("partParameters");
          if (!overrides)
            return std::unexpected(std::move(overrides.error()));
          monster_type.part_parameter_overrides = std::move(*overrides);
          return {};
        }();
        if (!part_parameters)
          return std::unexpected((*assets)->with_asset_context(std::move(part_parameters.error()), part_parameters_path,
                                                              "Loading monster part parameters"));
      } else {
        // Older .monstertype files define the merge rules inline.
        auto part_parameter_description = config->get_object("partParameterDescription", {});
        if (!part_parameter_description)
          return std::unexpected(std::move(part_parameter_description.error()));
        monster_type.part_parameter_description = std::move(*part_parameter_description);
      }
      monster_type.compile_part_parameter_rules();
      return {};
    }();
    if (!loaded)
      return std::unexpected(MonsterException(Star::format("Error loading monster type '{}'", file),
          (*assets)->with_asset_context(std::move(loaded.error()), file, "Loading monster type")));
  }

  for (auto const& asset_file : monster_parts->get()) {
    auto const& file = asset_file.string();
    auto loaded = [&, self = std::ref(*this)]() -> std::expected<void, StarException> {
      auto config = (*assets)->json(file);
      if (!config)
        return std::unexpected(std::move(config.error()));
      if (config->is_null())
        return {};

      auto name = config->get_string("name");
      if (!name)
        return std::unexpected(std::move(name.error()));
      auto category = config->get_string("category");
      if (!category)
        return std::unexpected(std::move(category.error()));
      auto type = config->get_string("type");
      if (!type)
        return std::unexpected(std::move(type.error()));
      auto frames = config->get("frames").and_then(&Json::object_ptr);
      if (!frames)
        return std::unexpected(std::move(frames.error()));
      auto parameters = config->get("parameters", JsonObject());
      if (!parameters)
        return std::unexpected(std::move(parameters.error()));

      auto category_group = self.get().m_part_directory.try_emplace(std::move(*category)).first;
      auto& part_pool = category_group->second[std::move(*type)];
      auto position = std::ranges::lower_bound(part_pool, *name, std::less<String>{}, &MonsterPart::name);
      if (position != part_pool.end() && position->name == *name)
        return std::unexpected(MonsterException(Star::format("Repeat monster part name '{}' for category '{}'", *name, category_group->first)));
      part_pool.insert(position, MonsterPart{std::move(*name), AssetPath::directory(file),
                                             std::move(*frames), std::move(*parameters)});
      return {};
    }();
    if (!loaded)
      return std::unexpected(MonsterException(Star::format("Error loading monster part '{}'", file),
          (*assets)->with_asset_context(std::move(loaded.error()), file, "Loading monster part")));
  }

  for (auto const& asset_file : monster_skills->get()) {
    auto const& file = asset_file.string();
    auto loaded = [&, self = std::ref(*this)]() -> std::expected<void, StarException> {
      auto config = (*assets)->json(file);
      if (!config)
        return std::unexpected(std::move(config.error()));
      if (config->is_null())
        return {};

      MonsterSkill skill;
      auto name = config->get_string("name");
      if (!name)
        return std::unexpected(std::move(name.error()));
      skill.name = std::move(*name);
      auto label = config->get_string("label");
      if (!label)
        return std::unexpected(std::move(label.error()));
      skill.label = std::move(*label);
      auto image = config->get_string("image");
      if (!image)
        return std::unexpected(std::move(image.error()));
      skill.image = std::move(*image);
      auto skill_config = config->get("config", JsonObject());
      if (!skill_config)
        return std::unexpected(std::move(skill_config.error()));
      skill.config = std::move(*skill_config);
      auto parameters = config->get("parameters", JsonObject());
      if (!parameters)
        return std::unexpected(std::move(parameters.error()));
      skill.parameters = std::move(*parameters);
      auto animation_parameters = config->get("animationParameters", JsonObject());
      if (!animation_parameters)
        return std::unexpected(std::move(animation_parameters.error()));
      skill.animation_parameters = std::move(*animation_parameters);

      if (self.get().m_skills.contains(skill.name))
        return std::unexpected(MonsterException(Star::format("Repeat monster skill name '{}'", skill.name)));
      self.get().m_skills[skill.name] = std::move(skill);
      return {};
    }();
    if (!loaded)
      return std::unexpected(MonsterException(Star::format("Error loading monster skill '{}'", file),
          (*assets)->with_asset_context(std::move(loaded.error()), file, "Loading monster skill")));
  }

  for (auto const& asset_file : monster_colors->get()) {
    auto const& file = asset_file.string();
    auto loaded = [&, self = std::ref(*this)]() -> std::expected<void, StarException> {
      auto config = (*assets)->json(file);
      if (!config)
        return std::unexpected(std::move(config.error()));
      if (config->is_null())
        return {};

      auto palette_name = config->get_string("name");
      if (!palette_name)
        return std::unexpected(std::move(palette_name.error()));
      if (self.get().m_color_swaps.contains(*palette_name))
        return std::unexpected(MonsterException(Star::format("Duplicate monster colors name '{}'", *palette_name)));

      auto swap_sets = config->get_array("swaps");
      if (!swap_sets)
        return std::unexpected(std::move(swap_sets.error()));
      for (auto const& swap_set : *swap_sets) {
        ColorReplaceMap color_swaps;
        auto swaps = swap_set.iterate_object();
        if (!swaps)
          return std::unexpected(std::move(swaps.error()));
        for (auto const& [key, value] : *swaps) {
          auto from = Color::from_hex(key);
          if (!from)
            return std::unexpected(std::move(from.error()));
          auto text = value.to_string();
          if (!text)
            return std::unexpected(std::move(text.error()));
          auto to = Color::from_hex(*text);
          if (!to)
            return std::unexpected(std::move(to.error()));
          color_swaps[from->to_rgba()] = to->to_rgba();
        }
        self.get().m_color_swaps[*palette_name].push_back(std::move(color_swaps));
      }
      return {};
    }();
    if (!loaded)
      return std::unexpected(MonsterException(Star::format("Error loading monster colors '{}'", file),
          (*assets)->with_asset_context(std::move(loaded.error()), file, "Loading monster colors")));
  }
  return {};
}

void MonsterDatabase::cleanup() {
  std::lock_guard lock(m_cache_mutex);
  m_monster_cache.cleanup();
}

StringList MonsterDatabase::monster_types() const {
  return m_monster_types.keys();
}

std::expected<MonsterVariant, StarException> MonsterDatabase::random_monster(
    String const& type_name, Json const& unique_parameters) const {
  auto seed_config = unique_parameters.opt("seed");
  if (!seed_config)
    return std::unexpected(std::move(seed_config.error()));
  if (!*seed_config)
    return monster_variant(type_name, m_root.random().next_u64(), unique_parameters);

  std::expected<std::uint64_t, StarException> seed;
  if ((*seed_config)->type() == Json::Type::String) {
    auto text = (*seed_config)->to_string();
    if (!text)
      return std::unexpected(std::move(text.error()));
    seed = lexical_cast<std::uint64_t>(*text);
  } else {
    seed = (*seed_config)->to_uint();
  }
  if (!seed)
    return std::unexpected(std::move(seed.error()));
  return monster_variant(type_name, *seed, unique_parameters);
}

std::expected<MonsterVariant, StarException> MonsterDatabase::monster_variant(
    String const& type_name, std::uint64_t seed, Json const& unique_parameters) const {
  std::lock_guard lock(m_cache_mutex);
  auto variant = m_monster_cache.get(std::make_tuple(type_name, seed, unique_parameters),
      [self = std::cref(*this)](std::tuple<String, std::uint64_t, Json> const& key) {
        return self.get().produce_monster(std::get<0>(key), std::get<1>(key), std::get<2>(key));
      });
  if (!variant)
    return std::unexpected(std::move(variant.error()));
  return variant->get();
}

std::expected<ByteArray, StarException> MonsterDatabase::write_monster_variant(MonsterVariant const& variant, NetCompatibilityRules rules) const {
  DataStreamBuffer ds;
  ds.set_stream_compatibility_version(rules);

  if (auto written = ds.write(variant.type); !written)
    return std::unexpected(std::move(written.error()));
  if (auto written = ds.write(variant.seed); !written)
    return std::unexpected(std::move(written.error()));
  if (auto written = ds.write(variant.unique_parameters); !written)
    return std::unexpected(std::move(written.error()));

  return ds.take_data();
}

std::expected<MonsterVariant, StarException> MonsterDatabase::read_monster_variant(ByteArray const& data, NetCompatibilityRules rules) const {
  DataStreamReader ds(data.span());
  ds.set_stream_compatibility_version(rules);

  auto type = ds.read<String>();
  if (!type)
    return std::unexpected(std::move(type.error()));
  auto seed = ds.read<std::uint64_t>();
  if (!seed)
    return std::unexpected(std::move(seed.error()));
  auto unique_parameters = ds.read<Json>();
  if (!unique_parameters)
    return std::unexpected(std::move(unique_parameters.error()));
  return monster_variant(*type, *seed, *unique_parameters);
}

std::expected<Json, StarException> MonsterDatabase::write_monster_variant_to_json(MonsterVariant const& variant) const {
  return JsonObject{
      {"type", variant.type},
      {"seed", variant.seed},
      {"uniqueParameters", variant.unique_parameters},
  };
}

std::expected<MonsterVariant, StarException> MonsterDatabase::read_monster_variant_from_json(Json const& variant) const {
  auto type = variant.get_string("type");
  if (!type)
    return std::unexpected(std::move(type.error()));
  auto seed = variant.get_uint("seed");
  if (!seed)
    return std::unexpected(std::move(seed.error()));
  auto unique_parameters = variant.get_object("uniqueParameters");
  if (!unique_parameters)
    return std::unexpected(std::move(unique_parameters.error()));
  return monster_variant(*type, *seed, std::move(*unique_parameters));
}

std::expected<std::shared_ptr<Monster>, StarException> MonsterDatabase::create_monster(
    MonsterVariant variant, std::optional<float> level, Json unique_parameters) const {
  if (unique_parameters) {
    variant.unique_parameters = json_merge(variant.unique_parameters, unique_parameters);
    variant.parameters = json_merge(variant.parameters, variant.unique_parameters);
    if (auto read = read_common_parameters(variant); !read)
      return std::unexpected(std::move(read.error()));
  }
  return Monster::create(m_root, std::move(variant), level);
}

std::expected<std::shared_ptr<Monster>, StarException> MonsterDatabase::disk_load_monster(Json const& disk_store) const {
  auto attempt = [root = std::ref(m_root)](Json const& store) -> std::expected<std::shared_ptr<Monster>, StarException> {
    return Monster::create(root.get(), store);
  };
  auto monster = attempt(disk_store);
  if (monster)
    return monster;
  StarException last_error = std::move(monster.error());
  auto rebuilt = m_rebuilder->rebuild(disk_store, last_error, [&](Json const& store) -> std::expected<void, StarException> {
    auto replacement = attempt(store);
    if (!replacement) {
      last_error = std::move(replacement.error());
      return std::unexpected(last_error);
    }
    monster = std::move(replacement);
    return {};
  });
  if (!rebuilt)
    return std::unexpected(std::move(rebuilt.error()));
  if (!*rebuilt)
    return std::unexpected(std::move(last_error));
  return monster;
}

std::expected<std::shared_ptr<Monster>, StarException> MonsterDatabase::net_load_monster(ByteArray const& net_store, NetCompatibilityRules rules) const {
  auto variant = read_monster_variant(net_store, rules);
  if (!variant)
    return std::unexpected(std::move(variant.error()));
  return Monster::create(m_root, std::move(*variant));
}

std::expected<List<Drawable>, StarException> MonsterDatabase::monster_portrait(MonsterVariant const& variant) const {
  auto animator = NetworkedAnimator::create(m_root, variant.animator_config);
  if (!animator)
    return std::unexpected(std::move(animator.error()));
  if (auto applied = apply_appearance(variant, **animator); !applied)
    return std::unexpected(std::move(applied.error()));
  auto drawables = (*animator)->drawables();
  if (!drawables)
    return std::unexpected(std::move(drawables.error()));
  Drawable::scale_all(*drawables, tile_pixels);
  return drawables;
}

std::expected<void, StarException> MonsterDatabase::apply_appearance(
    MonsterVariant const& variant, NetworkedAnimator& animator) const {
  for (auto const& [part, image] : variant.animator_part_tags)
    animator.setPartTag(part, "partImage", image);
  animator.setZoom(variant.scale);

  auto colors = variant.parameters.get_string("colors", "default");
  if (!colors)
    return std::unexpected(std::move(colors.error()));
  // Resolve the configured palette even when an explicit swap (including an empty one) wins.
  auto palette = select_color_swap(*colors, variant.seed);
  if (!palette)
    return std::unexpected(std::move(palette.error()));
  auto const& swaps = variant.color_swap ? *variant.color_swap : palette->get();
  if (!swaps.empty())
    animator.setProcessingDirectives(color_replace_operation_to_string(swaps));
  return {};
}

std::expected<std::pair<String, String>, StarException> MonsterDatabase::skill_info(String const& skill_name) const {
  if (auto skill = m_skills.find(skill_name); skill != m_skills.end())
    return std::make_pair(skill->second.label, skill->second.image);
  return std::pair<String, String>{};
}

std::expected<Json, StarException> MonsterDatabase::skill_config_parameter(String const& skill_name, String const& config_parameter_name) const {
  if (auto skill = m_skills.find(skill_name); skill != m_skills.end())
    return skill->second.config.get(config_parameter_name, Json());
  return Json();
}

std::expected<std::reference_wrapper<ColorReplaceMap const>, StarException> MonsterDatabase::select_color_swap(
    String const& set_name, std::uint64_t seed) const {
  auto swaps = m_color_swaps.find_ref(set_name);
  if (!swaps) {
    auto reported = m_root.logger().error("Monster colors '{}' not found!", set_name);
    if (!reported)
      return std::unexpected(std::move(reported.error()));
    auto fallback = m_color_swaps.get("default");
    if (!fallback)
      return std::unexpected(std::move(fallback.error()));
    swaps = *fallback;
  }
  return static_random_from(swaps->get(), seed);
}

std::expected<Json, StarException> MonsterDatabase::monster_config(String const& type_name) const {
  auto type = m_monster_types.get(type_name);
  if (!type)
    return std::unexpected(std::move(type.error()));
  return type->get().to_json();
}

std::expected<MonsterDatabase::PartMergeMethod, StarException> MonsterDatabase::read_part_merge_method(Json const& method_json) {
  auto method = method_json.string_ptr();
  if (!method)
    return std::unexpected(std::move(method.error()));
  constexpr std::array methods{
    std::pair{StringView("add"), PartMergeMethod::Add},
    std::pair{StringView("multiply"), PartMergeMethod::Multiply},
    std::pair{StringView("merge"), PartMergeMethod::Merge},
    std::pair{StringView("override"), PartMergeMethod::Override},
  };
  for (auto const& [name, rule] : methods) {
    auto matches = StringView(**method).equals_ignore_case(name);
    if (!matches)
      return std::unexpected(std::move(matches.error()));
    if (*matches)
      return rule;
  }
  return PartMergeMethod::Unrecognized;
}

void MonsterDatabase::MonsterType::compile_part_parameter_rules() {
  auto descriptions = *part_parameter_description.object_ptr();
  for (auto const& [key, description] : *descriptions) {
    if (!description.is_type(Json::Type::Array)) {
      invalid_part_parameter_default = description;
      return;
    }
    auto fields = *description.array_ptr();
    if (fields->size() < 2) {
      invalid_part_parameter_default = description;
      return;
    }
    part_parameter_defaults[key] = (*fields)[1];
    auto& method = part_parameter_methods[key];
    // A malformed method is not an error unless a selected part supplies this key.
    if ((*fields)[0].is_type(Json::Type::String)) {
      if (auto decoded = read_part_merge_method((*fields)[0]); decoded)
        method = *decoded;
    }
  }
}

namespace {
JsonArray join_parameter_arrays(JsonArray const& left, JsonArray const& right) {
  JsonArray combined;
  combined.reserve(left.size() + right.size());
  combined.append_range(left);
  combined.append_range(right);
  return combined;
}
}

std::expected<Json, StarException> MonsterDatabase::merge_part_parameters(MonsterType const& monster_type, JsonArray const& parameters) {
  if (auto const& invalid = monster_type.invalid_part_parameter_default)
    return std::unexpected(invalid->get(1).error());
  auto merged = monster_type.part_parameter_defaults;
  // Keep growing arrays mutable until the last part, without touching shared defaults.
  StringMap<JsonArray> appended_arrays;

  for (auto const& part_parameters : parameters) {
    auto entries = part_parameters.iterate_object();
    if (!entries)
      return std::unexpected(std::move(entries.error()));
    for (auto const& [key, next] : *entries) {
      auto rule = monster_type.part_parameter_methods.find(key);
      if (rule == monster_type.part_parameter_methods.end())
        return std::unexpected(monster_type.part_parameter_description.get(key).error());
      if (!rule->second) {
        auto description = *monster_type.part_parameter_description.get(key);
        return std::unexpected(read_part_merge_method(*description.get(0)).error());
      }
      auto method = *rule->second;
      auto& current = merged.find(key)->second;
      if (method == PartMergeMethod::Add || method == PartMergeMethod::Multiply) {
        auto left = current.to_double();
        if (!left)
          return std::unexpected(std::move(left.error()));
        auto right = next.to_double();
        if (!right)
          return std::unexpected(std::move(right.error()));
        current = method == PartMergeMethod::Add ? *left + *right : *left * *right;
        continue;
      }

      if (method == PartMergeMethod::Merge) {
        if (next.is_null())
          continue;
        if (current.type() != next.type()) {
          if (current.is_type(Json::Type::Array))
            appended_arrays.erase(key);
          current = next;
        } else if (next.is_type(Json::Type::Array)) {
          auto right = *next.array_ptr();
          if (right->empty())
            continue;
          auto pending = appended_arrays.find(key);
          if (pending != appended_arrays.end()) {
            pending->second.append_range(*right);
          } else if (auto left = *current.array_ptr(); left->empty()) {
            current = next;
          } else {
            appended_arrays.insert(key, join_parameter_arrays(*left, *right));
          }
        } else if (next.type() == Json::Type::Object) {
          auto combined = current.to_object();
          if (!combined)
            return std::unexpected(std::move(combined.error()));
          auto right = next.object_ptr();
          if (!right)
            return std::unexpected(std::move(right.error()));
          combined->merge(**right, true);
          current = std::move(*combined);
        }
        continue;
      }

      if (method == PartMergeMethod::Override && !next.is_null())
        current = next;
    }
  }
  for (auto&& [key, array] : appended_arrays)
    merged.find(key)->second = std::move(array);
  return merged;
}

std::expected<JsonObject, StarException> MonsterDatabase::merge_final_parameters(JsonArray const& parameters) {
  JsonObject merged;
  // Scripts and skill lists append instead of replacing.
  constexpr std::array<std::string_view, 4> append_keys{"scripts", "skills", "specialSkills", "baseSkills"};
  std::array<JsonArray, append_keys.size()> appended_arrays;
  for (auto const& source : parameters) {
    auto entries = source.iterate_object();
    if (!entries)
      return std::unexpected(std::move(entries.error()));
    for (auto const& [key, next] : *entries) {
      auto& value = merged[key];
      auto key_position = std::ranges::find(append_keys, std::string_view(key.utf8()));
      if (key_position == append_keys.end()) {
        value = json_merge(value, next);
        continue;
      }
      if (next.is_null()) {
        if (value.is_null())
          value = JsonArray{};
        continue;
      }
      auto right = next.array_ptr();
      if (!right)
        return std::unexpected(std::move(right.error()));
      if (value.is_null()) {
        value = next;
      } else if (!(*right)->empty()) {
        auto& pending = appended_arrays[key_position - append_keys.begin()];
        if (!pending.empty()) {
          pending.append_range(**right);
        } else if (auto left = *value.array_ptr(); left->empty()) {
          value = next;
        } else {
          pending = join_parameter_arrays(*left, **right);
        }
      }
    }
  }
  for (std::size_t i = 0; i < append_keys.size(); ++i) {
    if (!appended_arrays[i].empty())
      merged.find(String(append_keys[i]))->second = std::move(appended_arrays[i]);
  }
  return merged;
}

std::expected<void, StarException> MonsterDatabase::read_common_parameters(MonsterVariant& variant) {
  auto const& parameters = variant.parameters;
  auto short_description = parameters.opt_string("shortdescription");
  if (!short_description) return std::unexpected(std::move(short_description.error()));
  if (*short_description)
    variant.short_description = std::move(*short_description);
  auto drop_pool = parameters.get("dropPools", variant.drop_pool_config);
  if (!drop_pool) return std::unexpected(std::move(drop_pool.error()));
  variant.drop_pool_config = std::move(*drop_pool);

  auto script_paths = parameters.get("scripts");
  if (!script_paths) return std::unexpected(std::move(script_paths.error()));
  auto scripts = json_to_string_list(*script_paths);
  if (!scripts) return std::unexpected(std::move(scripts.error()));
  variant.scripts = std::move(*scripts);
  auto animation_script_paths = parameters.get_array("animationScripts", {});
  if (!animation_script_paths) return std::unexpected(std::move(animation_script_paths.error()));
  auto animation_scripts = json_to_string_list(std::move(*animation_script_paths));
  if (!animation_scripts) return std::unexpected(std::move(animation_scripts.error()));
  variant.animation_scripts = std::move(*animation_scripts);

  auto animation_custom = parameters.get("animationCustom", JsonObject());
  if (!animation_custom) return std::unexpected(std::move(animation_custom.error()));
  variant.animator_config = json_merge(variant.animator_config, *animation_custom);
  auto initial_script_delta = read_unsigned_field(parameters, "initialScriptDelta", 5u);
  if (!initial_script_delta) return std::unexpected(std::move(initial_script_delta.error()));
  variant.initial_script_delta = *initial_script_delta;
  auto bounds = parameters.get("metaBoundBox");
  if (!bounds) return std::unexpected(std::move(bounds.error()));
  auto meta_bound_box = json_to_rectf(*bounds);
  if (!meta_bound_box) return std::unexpected(std::move(meta_bound_box.error()));
  variant.meta_bound_box = *meta_bound_box;
  auto render_layer_name = parameters.opt_string("renderLayer");
  if (!render_layer_name) return std::unexpected(std::move(render_layer_name.error()));
  variant.render_layer = render_layer_monster;
  if (*render_layer_name) {
    auto layer = parse_render_layer(**render_layer_name);
    if (!layer) return std::unexpected(std::move(layer.error()));
    variant.render_layer = *layer;
  }
  auto scale = parameters.get_float("scale");
  if (!scale) return std::unexpected(std::move(scale.error()));
  variant.scale = *scale;

  auto movement = parameters.get("movementSettings", {});
  if (!movement) return std::unexpected(std::move(movement.error()));
  auto movement_settings = ActorMovementParameters::create(*movement);
  if (!movement_settings) return std::unexpected(std::move(movement_settings.error()));
  variant.movement_settings = std::move(*movement_settings);
  auto walk_multiplier = parameters.get_float("walkMultiplier", 1.0f);
  if (!walk_multiplier) return std::unexpected(std::move(walk_multiplier.error()));
  variant.walk_multiplier = *walk_multiplier;
  auto run_multiplier = parameters.get_float("runMultiplier", 1.0f);
  if (!run_multiplier) return std::unexpected(std::move(run_multiplier.error()));
  variant.run_multiplier = *run_multiplier;
  auto jump_multiplier = parameters.get_float("jumpMultiplier", 1.0f);
  if (!jump_multiplier) return std::unexpected(std::move(jump_multiplier.error()));
  variant.jump_multiplier = *jump_multiplier;
  auto weight_multiplier = parameters.get_float("weightMultiplier", 1.0f);
  if (!weight_multiplier) return std::unexpected(std::move(weight_multiplier.error()));
  variant.weight_multiplier = *weight_multiplier;
  auto health_multiplier = parameters.get_float("healthMultiplier", 1.0f);
  if (!health_multiplier) return std::unexpected(std::move(health_multiplier.error()));
  variant.health_multiplier = *health_multiplier;
  auto touch_damage_multiplier = parameters.get_float("touchDamageMultiplier", 1.0f);
  if (!touch_damage_multiplier) return std::unexpected(std::move(touch_damage_multiplier.error()));
  variant.touch_damage_multiplier = *touch_damage_multiplier;

  auto touch_damage = parameters.get("touchDamage", {});
  if (!touch_damage) return std::unexpected(std::move(touch_damage.error()));
  variant.touch_damage_config = std::move(*touch_damage);
  auto animation_damage_parts = parameters.get_object("animationDamageParts", {});
  if (!animation_damage_parts) return std::unexpected(std::move(animation_damage_parts.error()));
  variant.animation_damage_parts = std::move(*animation_damage_parts);
  auto status_settings = parameters.get("statusSettings");
  if (!status_settings) return std::unexpected(std::move(status_settings.error()));
  variant.status_settings = std::move(*status_settings);
  auto mouth = parameters.get("mouthOffset");
  if (!mouth) return std::unexpected(std::move(mouth.error()));
  auto mouth_offset = json_to_vec2f(*mouth);
  if (!mouth_offset) return std::unexpected(std::move(mouth_offset.error()));
  variant.mouth_offset = *mouth_offset / tile_pixels;
  auto feet = parameters.get("feetOffset");
  if (!feet) return std::unexpected(std::move(feet.error()));
  auto feet_offset = json_to_vec2f(*feet);
  if (!feet_offset) return std::unexpected(std::move(feet_offset.error()));
  variant.feet_offset = *feet_offset / tile_pixels;

  auto power_level_function = parameters.get_string("powerLevelFunction", "monsterLevelPowerMultiplier");
  if (!power_level_function) return std::unexpected(std::move(power_level_function.error()));
  variant.power_level_function = std::move(*power_level_function);
  auto health_level_function = parameters.get_string("healthLevelFunction", "monsterLevelHealthMultiplier");
  if (!health_level_function) return std::unexpected(std::move(health_level_function.error()));
  variant.health_level_function = std::move(*health_level_function);
  auto client_mode_name = parameters.get_string("clientEntityMode", "ClientSlaveOnly");
  if (!client_mode_name) return std::unexpected(std::move(client_mode_name.error()));
  if (!ClientEntityModeNames)
    return std::unexpected(ClientEntityModeNames.error());
  auto client_mode = ClientEntityModeNames->get_left(*client_mode_name);
  if (!client_mode) return std::unexpected(std::move(client_mode.error()));
  variant.client_entity_mode = client_mode->get();
  auto persistent = parameters.get_bool("persistent", false);
  if (!persistent) return std::unexpected(std::move(persistent.error()));
  variant.persistent = *persistent;
  auto damage_team_name = parameters.get_string("damageTeamType", "enemy");
  if (!damage_team_name) return std::unexpected(std::move(damage_team_name.error()));
  auto damage_team_type = enum_value<TeamTypeNames>(*damage_team_name);
  if (!damage_team_type) return std::unexpected(std::move(damage_team_type.error()));
  variant.damage_team_type = *damage_team_type;
  auto damage_team = read_unsigned_field(parameters, "damageTeam", std::uint8_t{2});
  if (!damage_team) return std::unexpected(std::move(damage_team.error()));
  variant.damage_team = *damage_team;

  auto poly = parameters.get("selfDamagePoly", {});
  if (!poly) return std::unexpected(std::move(poly.error()));
  if (*poly) {
    auto self_damage_poly = json_to_polyf(*poly);
    if (!self_damage_poly) return std::unexpected(std::move(self_damage_poly.error()));
    variant.self_damage_poly = std::move(*self_damage_poly);
  } else {
    if (!variant.movement_settings.standing_poly)
      return std::unexpected(MonsterException("Monster has no standing polygon"));
    variant.self_damage_poly = *variant.movement_settings.standing_poly;
  }

  auto portrait_icon = parameters.opt_string("portraitIcon");
  if (!portrait_icon) return std::unexpected(std::move(portrait_icon.error()));
  variant.portrait_icon = std::move(*portrait_icon);
  auto damage_received_aggressive_duration = parameters.get_float("damageReceivedAggressiveDuration", 1.0f);
  if (!damage_received_aggressive_duration) return std::unexpected(std::move(damage_received_aggressive_duration.error()));
  variant.damage_received_aggressive_duration = *damage_received_aggressive_duration;
  auto on_damaged_others_aggressive_duration = parameters.get_float("onDamagedOthersAggressiveDuration", 5.0f);
  if (!on_damaged_others_aggressive_duration) return std::unexpected(std::move(on_damaged_others_aggressive_duration.error()));
  variant.on_damaged_others_aggressive_duration = *on_damaged_others_aggressive_duration;
  auto on_fire_aggressive_duration = parameters.get_float("onFireAggressiveDuration", 5.0f);
  if (!on_fire_aggressive_duration) return std::unexpected(std::move(on_fire_aggressive_duration.error()));
  variant.on_fire_aggressive_duration = *on_fire_aggressive_duration;

  auto color = parameters.get("nametagColor", JsonArray{255, 255, 255});
  if (!color) return std::unexpected(std::move(color.error()));
  auto nametag_color = json_to_vec3b(*color);
  if (!nametag_color) return std::unexpected(std::move(nametag_color.error()));
  variant.nametag_color = *nametag_color;
  auto color_swap = parameters.opt_object("colorSwap");
  if (!color_swap) return std::unexpected(std::move(color_swap.error()));
  if (*color_swap) {
    ColorReplaceMap swaps;
    for (auto const& [key, value] : **color_swap) {
      auto from = Color::from_hex(key);
      if (!from) return std::unexpected(std::move(from.error()));
      auto text = value.to_string();
      if (!text) return std::unexpected(std::move(text.error()));
      auto to = Color::from_hex(*text);
      if (!to) return std::unexpected(std::move(to.error()));
      swaps.insert(from->to_rgba(), to->to_rgba());
    }
    variant.color_swap = std::move(swaps);
  }
  return {};
}

std::expected<MonsterVariant, StarException> MonsterDatabase::produce_monster(
    String const& type_name, std::uint64_t seed, Json const& unique_parameters) const {
  RandomSource random(seed);
  auto type = m_monster_types.get(type_name);
  if (!type)
    return std::unexpected(std::move(type.error()));
  auto const& monster_type = type->get();

  MonsterVariant variant;
  variant.type = type_name;
  variant.short_description = monster_type.short_description;
  variant.description = monster_type.description;
  variant.seed = seed;
  variant.unique_parameters = unique_parameters;

  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto animation = (*assets)->fetch_json(monster_type.animation_config_path);
  if (!animation)
    return std::unexpected((*assets)->with_asset_context(std::move(animation.error()),
        monster_type.animation_config_path, "Loading monster animation"));
  variant.animator_config = std::move(*animation);
  variant.reversed = monster_type.reversed;

  auto category = random.sample_ref(monster_type.categories);
  if (!category)
    return std::unexpected(std::move(category.error()));
  // Part records belong to this immutable directory and are borrowed only during generation.
  SmallList<std::reference_wrapper<MonsterPart const>, 4> parts;
  parts.reserve(monster_type.part_types.size());
  if (!monster_type.part_types.empty()) {
    auto category_parts = m_part_directory.get(category->get());
    if (!category_parts)
      return std::unexpected(std::move(category_parts.error()));
    std::optional<Json> selected_parts;
    for (auto const& part_type : monster_type.part_types) {
      auto found_pool = category_parts->get().get(part_type);
      if (!found_pool)
        return std::unexpected(std::move(found_pool.error()));
      auto const& part_pool = found_pool->get();
      // The draw is consumed even when selectedParts overrides this part.
      auto sampled = random.sample_ref(part_pool);
      if (!sampled)
        return std::unexpected(std::move(sampled.error()));
      if (!selected_parts) {
        auto configured_parts = unique_parameters.get("selectedParts", JsonObject());
        if (!configured_parts)
          return std::unexpected(std::move(configured_parts.error()));
        selected_parts = std::move(*configured_parts);
      }
      auto name = selected_parts->opt_string(part_type);
      if (!name)
        return std::unexpected(std::move(name.error()));
      if (*name) {
        auto selected = std::ranges::lower_bound(part_pool, **name, std::less<String>{}, &MonsterPart::name);
        if (selected == part_pool.end() || selected->name != **name)
          return std::unexpected(map_diagnostic_error("Key '{}' not found in Map::get()",
              "Map::get: key not found while rendering its diagnostic", **name));
        parts.emplace_back(*selected);
      } else {
        parts.push_back(*sampled);
      }
    }
  }

  // Resolve only selected frames, before reading any part parameter overrides.
  for (MonsterPart const& part : parts) {
    for (auto const& [tag, frame] : *part.frames) {
      auto image = frame.to_string();
      if (!image)
        return std::unexpected(std::move(image.error()));
      variant.animator_part_tags[tag] = AssetPath::relative_to(part.path, *image);
    }
  }

  JsonArray part_parameters;
  part_parameters.reserve(parts.size());
  auto const& overrides = monster_type.part_parameter_overrides;
  for (MonsterPart const& part : parts) {
    part_parameters.push_back(part.part_parameters);
    if (overrides.is_null())
      continue;
    auto has_override = overrides.contains(part.name);
    if (!has_override)
      return std::unexpected(std::move(has_override.error()));
    if (*has_override) {
      auto parameters = overrides.get_object(part.name);
      if (!parameters)
        return std::unexpected(std::move(parameters.error()));
      part_parameters.push_back(std::move(*parameters));
    }
  }

  auto merged_parts = merge_part_parameters(monster_type, part_parameters);
  if (!merged_parts)
    return std::unexpected(std::move(merged_parts.error()));
  auto merged = merge_final_parameters({monster_type.base_parameters, std::move(*merged_parts)});
  if (!merged)
    return std::unexpected(std::move(merged.error()));
  variant.parameters = json_merge(std::move(*merged), unique_parameters);
  if (auto chosen = choose_skills(variant, random); !chosen)
    return std::unexpected(std::move(chosen.error()));

  variant.drop_pool_config = monster_type.drop_pools;
  if (auto read = read_common_parameters(variant); !read)
    return std::unexpected(std::move(read.error()));
  if (variant.drop_pool_config.is_type(Json::Type::Array)) {
    auto drop_pools = variant.drop_pool_config.array_ptr();
    if (!drop_pools)
      return std::unexpected(std::move(drop_pools.error()));
    auto drop_pool = random.sample_value(**drop_pools);
    if (!drop_pool)
      return std::unexpected(std::move(drop_pool.error()));
    variant.drop_pool_config = std::move(*drop_pool);
  }
  return variant;
}

std::expected<void, StarException> MonsterDatabase::choose_skills(MonsterVariant& variant, RandomSource& random) const {
  auto const& parameters = variant.parameters;
  auto split = parameters.contains("baseSkills");
  if (!split)
    return std::unexpected(std::move(split.error()));
  if (!*split) {
    split = parameters.contains("specialSkills");
    if (!split)
      return std::unexpected(std::move(split.error()));
  }

  StringList primary_skills;
  StringList special_skills;
  std::uint64_t skill_count;
  if (*split) {
    auto requested = parameters.get_uint("skillCount", 2);
    if (!requested)
      return std::unexpected(std::move(requested.error()));
    skill_count = *requested;
    auto base_list = parameters.get("baseSkills");
    if (!base_list)
      return std::unexpected(std::move(base_list.error()));
    auto base_names = json_to_string_list(*base_list);
    if (!base_names)
      return std::unexpected(std::move(base_names.error()));
    primary_skills = std::move(*base_names);
    auto special_list = parameters.get("specialSkills");
    if (!special_list)
      return std::unexpected(std::move(special_list.error()));
    auto special_names = json_to_string_list(*special_list);
    if (!special_names)
      return std::unexpected(std::move(special_names.error()));
    special_skills = std::move(*special_names);
  } else {
    auto has_skills = parameters.contains("skills");
    if (!has_skills)
      return std::unexpected(std::move(has_skills.error()));
    if (!*has_skills)
      return {};
    auto skill_list = parameters.get("skills");
    if (!skill_list)
      return std::unexpected(std::move(skill_list.error()));
    auto names = json_to_string_list(*skill_list);
    if (!names)
      return std::unexpected(std::move(names.error()));
    primary_skills = std::move(*names);
    auto requested = parameters.get_uint("skillCount", 2);
    if (!requested)
      return std::unexpected(std::move(requested.error()));
    skill_count = std::min<std::size_t>(*requested, primary_skills.size());
  }

  auto primary_count = std::min<std::uint64_t>(skill_count, primary_skills.size());
  auto special_count = std::min<std::uint64_t>(skill_count - primary_count, special_skills.size());
  JsonArray selected_names;
  selected_names.reserve(primary_count + special_count);
  JsonArray all_parameters;
  all_parameters.reserve(1 + primary_count + special_count);
  all_parameters.push_back(parameters);
  auto select = [&](StringList& available, std::size_t count) -> std::expected<void, StarException> {
    while (count-- != 0) {
      auto index = random.uniform_int(available.size() - 1);
      if (!index)
        return std::unexpected(std::move(index.error()));
      // Keep the remaining list ordered: swapping with its tail changes later picks.
      auto name = available.take_at(*index);
      if (!name)
        return std::unexpected(std::move(name.error()));
      if (auto skill = m_skills.find(*name); skill != m_skills.end()) {
        all_parameters.push_back(skill->second.parameters);
        variant.animator_config = json_merge(variant.animator_config, skill->second.animation_parameters);
      }
      selected_names.push_back(std::move(*name));
    }
    return {};
  };
  if (auto selected = select(primary_skills, primary_count); !selected)
    return std::unexpected(std::move(selected.error()));
  if (auto selected = select(special_skills, special_count); !selected)
    return std::unexpected(std::move(selected.error()));

  auto final_parameters = merge_final_parameters(all_parameters);
  if (!final_parameters)
    return std::unexpected(std::move(final_parameters.error()));
  // Replace the final list rather than appending it to the input lists.
  (*final_parameters)["skills"] = std::move(selected_names);
  variant.parameters = std::move(*final_parameters);
  return {};
}

Json MonsterDatabase::MonsterType::to_json() const {
  return JsonObject{
    {"type", type_name},
    {"shortdescription", short_description.value_or(String{})},
    {"description", description.value_or(String{})},
    {"categories", json_from_string_list(categories)},
    {"parts", json_from_string_list(part_types)},
    {"animation", animation_config_path},
    {"colors", colors},
    {"reversed", reversed},
    {"dropPools", drop_pools},
    {"baseParameters", base_parameters},
    {"partParameters", part_parameter_overrides},
    {"partParameterDescription", part_parameter_description}
  };
}
}
