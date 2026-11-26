import star.inventory_types;
import star.json_extra;
import star.json;
import star.data_stream_extra;
import star.string;
import star.random;
import star.periodic_function;
import star.ordered_map;
import star.audio;
import star.list;
import star.map;
import star.set;
import star.data_stream;
import star.game_types;
import star.strong_typedef;
import star.id_map;
import star.color;
import star.bi_map;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.data_stream_devices;
import star.byte_array;
import star.encode;
import star.casting;
import star.rpc_promise;
import star.a_star;
import star.lua;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.format;
import star.parametric_function;
import star.multi_array_interpolator;
import star.multi_table;

import star.drawable;
import star.animated_part_set;
import star.mixer;
import star.light_source;
import star.networked_animator;
import star.humanoid;
import star.damage_types;
import star.status_types;
import star.animation;
import star.particle;
import star.item_descriptor;
import star.world_geometry;
import star.damage;
import star.entity;
import star.tile_damage;
import star.interaction_types;
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
import star.portrait_entity;
import star.damage_bar_entity;
import star.nametag_entity;
import star.scripted_entity;
import star.chat_action;
import star.chatty_entity;
import star.emote_entity;
import star.entity_rendering_types;
import star.lounging_entities;
import star.mobile_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.lua_components;
import star.lua_actor_movement_component;
import star.lua_animation_component;
import star.effect_emitter;
import star.npc;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.item;
import star.status_effect_item;
import star.effect_source_item;
import star.previewable_item;
import star.tool_user_item;
import star.fireable_item;
import star.swingable_item;
import star.armors;
import star.utility_lua_bindings;
import star.name_generator;
import star.stored_functions;
import star.rebuilder;

import star.npc_database;
import star.item_database;
import star.species_database;
import star.versioning_database;

import star.armor_wearer;
import star.entity_splash;

import star.net_compatibility;
namespace Star {

namespace {

std::expected<void, StarException> read_script_config(NpcVariant& variant, Json const& config, bool generating = false) {
  auto scripts = config.get("scripts").and_then(json_to_string_list);
  if (!scripts) return std::unexpected(std::move(scripts.error()));
  if (generating) {
    auto initial_script_delta = config.get_uint("initialScriptDelta", 5);
    if (!initial_script_delta) return std::unexpected(std::move(initial_script_delta.error()));
    variant.initial_script_delta = *initial_script_delta;
  }
  auto script_config = config.get("scriptConfig");
  if (!script_config) return std::unexpected(std::move(script_config.error()));
  variant.scripts = std::move(*scripts);
  variant.script_config = std::move(*script_config);
  return {};
}

std::expected<void, StarException> read_movement_and_status(Root& root, NpcVariant& variant, Json const& config, bool disk = false) {
  auto movement = config.get("movementParameters", {});
  if (!movement) return std::unexpected(std::move(movement.error()));
  auto status = disk ? config.get("statusControllerSettings", {}) : config.get("statusControllerSettings");
  if (!status) return std::unexpected(std::move(status.error()));
  auto functions = root.functionDatabase();
  if (!functions) return std::unexpected(std::move(functions.error()));
  auto multiplier = [&](String const& name) -> std::expected<float, StarException> {
    auto function = (*functions)->function(name);
    if (!function) return std::unexpected(std::move(function.error()));
    return (*function)->evaluate(variant.level);
  };
  auto power = multiplier("npcLevelPowerMultiplierModifier");
  if (!power) return std::unexpected(std::move(power.error()));
  auto protection = multiplier("npcLevelProtectionMultiplier");
  if (!protection) return std::unexpected(std::move(protection.error()));
  auto health = multiplier("npcLevelHealthMultiplier");
  if (!health) return std::unexpected(std::move(health.error()));
  auto energy = multiplier("npcLevelEnergyMultiplier");
  if (!energy) return std::unexpected(std::move(energy.error()));
  auto effects = config.get("innateStatusEffects", JsonArray()).and_then([](Json const& effects) {
    return json_to_list<PersistentStatusEffect>(effects, jsonToPersistentStatusEffect);
  });
  if (!effects) return std::unexpected(std::move(effects.error()));
  auto touch_damage = config.get("touchDamage", {});
  if (!touch_damage) return std::unexpected(std::move(touch_damage.error()));

  effects->push_back(StatModifier(StatValueModifier{"powerMultiplier", *power}));
  effects->push_back(StatModifier(StatBaseMultiplier{"protection", *protection}));
  effects->push_back(StatModifier(StatBaseMultiplier{"maxHealth", *health}));
  effects->push_back(StatModifier(StatBaseMultiplier{"maxEnergy", *energy}));
  variant.movement_parameters = std::move(*movement);
  variant.status_controller_settings = std::move(*status);
  variant.innate_status_effects = std::move(*effects);
  variant.touch_damage_config = std::move(*touch_damage);
  return {};
}

std::expected<void, StarException> read_appearance(NpcVariant& variant, Json const& config) {
  auto disable_worn_armor = config.get_bool("disableWornArmor", true);
  if (!disable_worn_armor) return std::unexpected(std::move(disable_worn_armor.error()));
  auto drop_pools = config.get("dropPools", JsonArray{}).and_then(json_to_string_list);
  if (!drop_pools) return std::unexpected(std::move(drop_pools.error()));
  variant.disable_worn_armor = *disable_worn_armor;
  variant.drop_pools = std::move(*drop_pools);
  return {};
}

std::expected<void, StarException> read_display(Root& root, NpcVariant& variant, Json const& config) {
  auto color = config.get("nametagColor", JsonArray{255, 255, 255}).and_then(json_to_vec3b);
  if (!color) return std::unexpected(std::move(color.error()));
  auto splash_config = config.get("splashConfig");
  if (!splash_config) return std::unexpected(std::move(splash_config.error()));
  auto splash = EntitySplashConfig::create(root, std::move(*splash_config));
  if (!splash) return std::unexpected(std::move(splash.error()));
  variant.nametag_color = *color;
  variant.splash_config = std::move(*splash);
  return {};
}

std::expected<void, StarException> read_humanoid_config(Root& root, NpcVariant& variant, Json const& config) {
  auto species_database = root.speciesDatabase();
  if (!species_database) return std::unexpected(std::move(species_database.error()));
  auto species = (*species_database)->species(variant.species);
  if (!species) return std::unexpected(std::move(species.error()));
  auto unique = config.contains("humanoidConfig");
  if (!unique) return std::unexpected(std::move(unique.error()));
  auto humanoid_config = [&]() -> std::expected<Json, StarException> {
    if (!*unique)
      return (*species)->humanoid_config();
    auto assets = root.assets();
    if (!assets) return std::unexpected(std::move(assets.error()));
    auto path = config.get_string("humanoidConfig");
    if (!path) return std::unexpected(std::move(path.error()));
    auto value = (*assets)->json(*path);
    if (!value)
      return std::unexpected((*assets)->with_asset_context(std::move(value.error()), *path, "Loading NPC humanoid configuration"));
    return value;
  }();
  if (!humanoid_config) return std::unexpected(std::move(humanoid_config.error()));
  variant.unique_humanoid_config = *unique;
  variant.humanoid_config = std::move(*humanoid_config);
  return {};
}

}

NpcDatabase::NpcDatabase(ConstructionKey, Root& root) : m_root(root) {}

std::expected<std::shared_ptr<NpcDatabase>, StarException> NpcDatabase::create(Root& root) {
  auto database = std::make_shared<NpcDatabase>(ConstructionKey{}, root);
  auto initialized = database->initialize();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return database;
}

std::expected<void, StarException> NpcDatabase::initialize() {
  auto rebuilder = Rebuilder::create(m_root, "npc");
  if (!rebuilder)
    return std::unexpected(std::move(rebuilder.error()));
  m_rebuilder = std::move(*rebuilder);
  auto result1 = m_root.assets();
if (!result1) return std::unexpected(std::move(result1.error()));
auto assets = std::move(*result1);

  auto result2 = assets->scan_extension("npctype");
if (!result2) return std::unexpected(std::move(result2.error()));
auto& files = result2->get();
  auto result3 = assets->queue_jsons(files);
if (!result3) return std::unexpected(std::move(result3.error()));
  for (auto const& asset_file : files) {
    auto const& file = asset_file.string();
    auto loaded = [&, &types = m_npc_types]() -> std::expected<void, StarException> {
      auto result4 = assets->json(file);
if (!result4) return std::unexpected(std::move(result4.error()));
auto config = std::move(*result4);
      auto result5 = config.get_string("type");
if (!result5) return std::unexpected(std::move(result5.error()));
String type_name = std::move(*result5);

      if (types.contains(type_name))
        return std::unexpected(NpcException(Star::format("Repeat npc type name '{}'", type_name)));

      auto base_type = config.get_string("baseType", "");
      types.emplace(std::move(type_name), TypeConfig{std::move(config), std::move(base_type)});

      return {};
    }();
    if (!loaded)
      return std::unexpected(NpcException(Star::format("Error loading npc type '{}'", file), assets->with_asset_context(std::move(loaded.error()), file, "Loading NPC type")));
  }
  return {};
}

std::expected<NpcVariant, StarException> NpcDatabase::generate_npc_variant(String const& species, String const& type_name, float level, std::optional<std::reference_wrapper<ItemDatabase const>> item_catalog) const {
  return generate_npc_variant(species, type_name, level, m_root.random().next_u64(), {}, item_catalog);
}

std::expected<NpcVariant, StarException> NpcDatabase::generate_npc_variant(
    String const& species, String const& type_name, float level, std::uint64_t seed, Json const& overrides, std::optional<std::reference_wrapper<ItemDatabase const>> item_catalog) const {
  NpcVariant variant;
  variant.species = species;
  variant.type_name = type_name;
  variant.seed = seed;
  variant.overrides = overrides;

  RandomSource rand_source(seed);

  auto result6 = build_config(type_name, overrides);
if (!result6) return std::unexpected(std::move(result6.error()));
auto config = std::move(*result6);

  auto result7 = config.opt_string("description");
if (!result7) return std::unexpected(std::move(result7.error()));
variant.description = std::move(*result7);

  auto result8 = config.get_array("levelVariance", {0, 0});
if (!result8) return std::unexpected(std::move(result8.error()));
auto result9 = json_to_vec2f(std::move(*result8));
if (!result9) return std::unexpected(std::move(result9.error()));
auto level_variance = std::move(*result9);
  auto result10 = rand_source.uniform_float(level + level_variance[0], level + level_variance[1]);
if (!result10) return std::unexpected(std::move(result10.error()));
variant.level = std::max(std::move(*result10), 0.0f);

  auto scripts = read_script_config(variant, config, true);
  if (!scripts) return std::unexpected(std::move(scripts.error()));

  auto result15 = m_root.speciesDatabase();
if (!result15) return std::unexpected(std::move(result15.error()));
auto species_database = std::move(*result15);
  auto result16 = species_database->species(species);
if (!result16) return std::unexpected(std::move(result16.error()));
auto species_definition = std::move(*result16);
  auto result17 = species_database->generate_humanoid(species, seed, {}, item_catalog);
if (!result17) return std::unexpected(std::move(result17.error()));
auto result = std::move(*result17);
  HumanoidIdentity identity = result.identity;

  auto result18 = config.get_object("humanoidParameters", JsonObject());
if (!result18) return std::unexpected(std::move(result18.error()));
auto result19 = json_merge_object(result.humanoid_parameters, *result18);
if (!result19) return std::unexpected(std::move(result19.error()));
variant.humanoid_parameters = std::move(*result19);

  auto has_explicit_name = config.contains("npcname");
  if (!has_explicit_name) return std::unexpected(std::move(has_explicit_name.error()));
  if (*has_explicit_name) {
    auto name = config.get_string("npcname");
    if (!name) return std::unexpected(std::move(name.error()));
    identity.name = std::move(*name);
  } else {
    auto has_name_gen = config.contains("nameGen");
    if (!has_name_gen) return std::unexpected(std::move(has_name_gen.error()));
    if (*has_name_gen) {
      auto generator = m_root.name_generator();
      if (!generator) return std::unexpected(std::move(generator.error()));
      auto rules = config.get("nameGen");
      if (!rules) return std::unexpected(std::move(rules.error()));
      auto rules_by_gender = json_to_string_list(*rules);
      if (!rules_by_gender) return std::unexpected(std::move(rules_by_gender.error()));
      auto gender_index = static_cast<std::size_t>(identity.gender);
      if (gender_index >= rules_by_gender->size())
        return std::unexpected(NpcException::format("NPC type '{}' nameGen has no entry for gender index {}", type_name, gender_index));
      auto name = (*generator)->generate_name((*rules_by_gender)[gender_index], rand_source);
      if (!name) return std::unexpected(std::move(name.error()));
      identity.name = std::move(*name);
    }
  }
  // we're going to kinda end up doing this twice just to make sure it ends up generating with the right personality array
  // its dumb that personality is the only identity value that isn't in the customization screen but comes from the humanoid config
  // therefore we have to do this little dance here for the npcs that get unique humanoid configs, which is silly since if they
  // got a unique humanoid config they probably would have had a predefined personality anyway, but whatever
  {
  auto result27 = config.contains("identity");
  if (!result27) return std::unexpected(std::move(result27.error()));
  if (std::move(*result27))
      {
    auto result28 = identity.to_json();
    if (!result28) return std::unexpected(std::move(result28.error()));
    auto result29 = config.get("identity");
    if (!result29) return std::unexpected(std::move(result29.error()));
    auto result30 = HumanoidIdentity::create(json_merge(std::move(*result28), std::move(*result29)));
    if (!result30) return std::unexpected(std::move(result30.error()));
    identity = std::move(*result30);
  }
}

  auto result31 = config.contains("humanoidConfig");
if (!result31) return std::unexpected(std::move(result31.error()));
variant.unique_humanoid_config = std::move(*result31);
  if (variant.unique_humanoid_config){
    auto result32 = m_root.assets();
if (!result32) return std::unexpected(std::move(result32.error()));
auto result33 = config.get_string("humanoidConfig");
if (!result33) return std::unexpected(std::move(result33.error()));
    auto humanoid_loaded = [&]() -> std::expected<void, StarException> {
auto result34 = (*result32)->json(*result33);
if (!result34) return std::unexpected(std::move(result34.error()));
variant.humanoid_config = std::move(*result34);
    auto result35 = species_database->humanoid_config(identity, variant.humanoid_parameters, variant.humanoid_config, item_catalog);
if (!result35) return std::unexpected(std::move(result35.error()));
auto used_humanoid_config = std::move(*result35);
    // this only needs to be done if the npc has a unique humanoid config, otherwise the output from generateHumanoid should be fine
    auto result36 = used_humanoid_config.get_array("personalities");
if (!result36) return std::unexpected(std::move(result36.error()));
auto result37 = rand_source.sample_ref(*result36);
if (!result37) return std::unexpected(std::move(result37.error()));
auto result38 = parse_personality_array(result37->get());
if (!result38) return std::unexpected(std::move(result38.error()));
identity.personality = std::move(*result38);
    return {};
    }();
    if (!humanoid_loaded)
      return std::unexpected((*result32)->with_asset_context(std::move(humanoid_loaded.error()), *result33, "Loading NPC humanoid configuration"));
  } else {
    auto result39 = species_definition->humanoid_config();
if (!result39) return std::unexpected(std::move(result39.error()));
variant.humanoid_config = std::move(*result39);
  }

  // and where it ususally merges the identity
  {
  auto result40 = config.contains("identity");
  if (!result40) return std::unexpected(std::move(result40.error()));
  if (std::move(*result40))
      {
    auto result41 = identity.to_json();
    if (!result41) return std::unexpected(std::move(result41.error()));
    auto result42 = config.get("identity");
    if (!result42) return std::unexpected(std::move(result42.error()));
    auto result43 = HumanoidIdentity::create(json_merge(std::move(*result41), std::move(*result42)));
    if (!result43) return std::unexpected(std::move(result43.error()));
    identity = std::move(*result43);
  }
}

  variant.humanoid_identity = std::move(identity);

  auto movement_and_status = read_movement_and_status(m_root, variant, config);
  if (!movement_and_status) return std::unexpected(std::move(movement_and_status.error()));

  auto result58 = config.get("items", {});
if (!result58) return std::unexpected(std::move(result58.error()));
auto items_config = std::move(*result58);
  if (!items_config.is_null()) {
    auto result59 = items_config.get("override", {});
if (!result59) return std::unexpected(std::move(result59.error()));
auto species_items_config = std::move(*result59);
    if (species_items_config.is_null())
      {
  auto result60 = items_config.get(species, {});
  if (!result60) return std::unexpected(std::move(result60.error()));
  species_items_config = std::move(*result60);
}
    if (species_items_config.is_null())
      {
  auto result61 = items_config.get("default", {});
  if (!result61) return std::unexpected(std::move(result61.error()));
  species_items_config = std::move(*result61);
}

    if (!species_items_config.is_null()) {
      Json highest_level_items_config;
      {
        auto result62 = species_items_config.iterate_array();
        if (!result62)
          return std::unexpected(std::move(result62.error()));
        for (auto const& level_items_config : *result62) {
          {
    auto result63 = level_items_config.get_float(0);
    if (!result63) return std::unexpected(std::move(result63.error()));
    if (variant.level >= std::move(*result63)) {
              auto result64 = level_items_config.get(1);
    if (!result64) return std::unexpected(std::move(result64.error()));
    highest_level_items_config = std::move(*result64);
            }
  }
        }
}

      if (!highest_level_items_config.is_null()) {
        int random_color_index = -1;
        auto result65 = config.get_bool("matchColorIndices", false);
if (!result65) return std::unexpected(std::move(result65.error()));
bool match_color_indices = std::move(*result65);

        {
          auto result66 = highest_level_items_config.array_ptr();
          if (!result66)
            return std::unexpected(std::move(result66.error()));
          auto result67 = rand_source.sample_ref(**result66);
          if (!result67)
            return std::unexpected(std::move(result67.error()));
          auto result68 = result67->get().iterate_object();
          if (!result68)
            return std::unexpected(std::move(result68.error()));
          for (auto const& item_slot_config : *result68) {
            auto result69 = item_slot_config.second.array_ptr();
            if (!result69)
              return std::unexpected(std::move(result69.error()));
            auto result70 = rand_source.sample_ref(**result69);
            if (!result70)
              return std::unexpected(std::move(result70.error()));
            auto result71 = ItemDescriptor::create(result70->get());
            if (!result71)
              return std::unexpected(std::move(result71.error()));
            ItemDescriptor item = std::move(*result71);

            // Randomize color index if colorIndex is an array
            {
    auto result72 = item.parameters().contains("colorIndex");
    if (!result72) return std::unexpected(std::move(result72.error()));
    if (std::move(*result72)) {
                auto result73 = item.parameters().get("colorIndex");
    if (!result73) return std::unexpected(std::move(result73.error()));
    auto color_index = std::move(*result73);
                if (color_index.is_type(Json::Type::Array)) {
                  if (!match_color_indices || random_color_index == -1)
                    {
                    auto result74 = color_index.array_ptr();
                    if (!result74)
                      return std::unexpected(std::move(result74.error()));
                    auto result75 = rand_source.sample_ref(**result74);
                    if (!result75)
                      return std::unexpected(std::move(result75.error()));
                    auto result76 = result75->get().to_int();
                    if (!result76)
                      return std::unexpected(std::move(result76.error()));
                    random_color_index = std::move(*result76);
    }

                  auto result77 = item.apply_parameters({{"colorIndex", random_color_index}});
    if (!result77) return std::unexpected(std::move(result77.error()));
    item = std::move(*result77);
                }
              }
  }

            variant.items[item_slot_config.first] = std::move(item);
          }
}
      }
    }
  }

  auto appearance = read_appearance(variant, config);
  if (!appearance) return std::unexpected(std::move(appearance.error()));

  auto result81 = config.get_bool("persistent", false);
if (!result81) return std::unexpected(std::move(result81.error()));
variant.persistent = std::move(*result81);
  auto result82 = config.get_bool("keepAlive", false);
if (!result82) return std::unexpected(std::move(result82.error()));
variant.keep_alive = std::move(*result82);

  auto result83 = config.get_uint("damageTeam", 0);
if (!result83) return std::unexpected(std::move(result83.error()));
variant.damage_team = std::move(*result83);
  auto result84 = config.get_string("damageTeamType", "enemy");
if (!result84) return std::unexpected(std::move(result84.error()));
auto damage_team_type = enum_value<TeamTypeNames>(*result84);
if (!damage_team_type)
  return std::unexpected(std::move(damage_team_type.error()));
variant.damage_team_type = *damage_team_type;

  auto display = read_display(m_root, variant, config);
  if (!display) return std::unexpected(std::move(display.error()));

  return variant;
}

std::expected<ByteArray, StarException> NpcDatabase::write_npc_variant(NpcVariant const& variant, NetCompatibilityRules rules) const {
  DataStreamBuffer ds;
  ds.set_stream_compatibility_version(rules);

  auto result90 = ds.write(variant.species);
if (!result90) return std::unexpected(std::move(result90.error()));

  auto result91 = ds.write(variant.type_name);
if (!result91) return std::unexpected(std::move(result91.error()));

  auto result92 = ds.write(variant.level);
if (!result92) return std::unexpected(std::move(result92.error()));

  auto result93 = ds.write(variant.seed);
if (!result93) return std::unexpected(std::move(result93.error()));

  auto result94 = ds.write(variant.overrides);
if (!result94) return std::unexpected(std::move(result94.error()));


  auto result95 = ds.write(variant.initial_script_delta);
if (!result95) return std::unexpected(std::move(result95.error()));

  auto result96 = ds.write(variant.humanoid_identity);
if (!result96) return std::unexpected(std::move(result96.error()));

  if (rules.version() >= 11 ) {
    auto result97 = ds.write(variant.humanoid_parameters);
if (!result97) return std::unexpected(std::move(result97.error()));

    auto result98 = ds.write(variant.description);
if (!result98) return std::unexpected(std::move(result98.error()));

  }

  auto result99 = ds.write_map_container(variant.items);
  if (!result99)
    return std::unexpected(std::move(result99.error()));

  auto result100 = ds.write(variant.persistent);
if (!result100) return std::unexpected(std::move(result100.error()));

  auto result101 = ds.write(variant.keep_alive);
if (!result101) return std::unexpected(std::move(result101.error()));

  auto result102 = ds.write(variant.damage_team);
if (!result102) return std::unexpected(std::move(result102.error()));

  auto result103 = ds.write(variant.damage_team_type);
if (!result103) return std::unexpected(std::move(result103.error()));

return ds.take_data();
}

std::expected<NpcVariant, StarException> NpcDatabase::read_npc_variant(ByteArray const& data, NetCompatibilityRules rules) const {
  DataStreamReader ds(data.span());
  ds.set_stream_compatibility_version(rules);

  NpcVariant variant;

  auto result104 = ds.read(variant.species);
if (!result104) return std::unexpected(std::move(result104.error()));

  auto result105 = ds.read(variant.type_name);
if (!result105) return std::unexpected(std::move(result105.error()));

  auto result106 = ds.read(variant.level);
if (!result106) return std::unexpected(std::move(result106.error()));

  auto result107 = ds.read(variant.seed);
if (!result107) return std::unexpected(std::move(result107.error()));

  auto result108 = ds.read(variant.overrides);
if (!result108) return std::unexpected(std::move(result108.error()));


  auto result109 = build_config(variant.type_name, variant.overrides);
if (!result109) return std::unexpected(std::move(result109.error()));
auto config = std::move(*result109);


  auto scripts = read_script_config(variant, config);
  if (!scripts) return std::unexpected(std::move(scripts.error()));

  auto result113 = ds.read(variant.initial_script_delta);
if (!result113) return std::unexpected(std::move(result113.error()));

  auto result114 = ds.read(variant.humanoid_identity);
if (!result114) return std::unexpected(std::move(result114.error()));

  if (rules.version() >= 11){
    auto result115 = ds.read(variant.humanoid_parameters);
if (!result115) return std::unexpected(std::move(result115.error()));

    auto result116 = ds.read(variant.description);
if (!result116) return std::unexpected(std::move(result116.error()));

  } else {
    auto result117 = config.get_object("humanoidParameters", JsonObject());
if (!result117) return std::unexpected(std::move(result117.error()));
variant.humanoid_parameters = std::move(*result117);
    auto result118 = config.opt_string("description");
if (!result118) return std::unexpected(std::move(result118.error()));
variant.description = std::move(*result118);
  }

  auto humanoid_config = read_humanoid_config(m_root, variant, config);
  if (!humanoid_config) return std::unexpected(std::move(humanoid_config.error()));

  auto movement_and_status = read_movement_and_status(m_root, variant, config);
  if (!movement_and_status) return std::unexpected(std::move(movement_and_status.error()));

auto result140 = ds.read_map_container(variant.items);
if (!result140) return std::unexpected(std::move(result140.error()));


  auto appearance = read_appearance(variant, config);
  if (!appearance) return std::unexpected(std::move(appearance.error()));

  auto result144 = ds.read(variant.persistent);
if (!result144) return std::unexpected(std::move(result144.error()));

  auto result145 = ds.read(variant.keep_alive);
if (!result145) return std::unexpected(std::move(result145.error()));

  auto result146 = ds.read(variant.damage_team);
if (!result146) return std::unexpected(std::move(result146.error()));

  auto result147 = ds.read(variant.damage_team_type);
if (!result147) return std::unexpected(std::move(result147.error()));


  auto display = read_display(m_root, variant, config);
  if (!display) return std::unexpected(std::move(display.error()));

  return variant;
}

std::expected<Json, StarException> NpcDatabase::write_npc_variant_to_json(NpcVariant const& variant) const {
  auto result152 = m_root.versioningDatabase();
if (!result152) return std::unexpected(std::move(result152.error()));
auto versioning_database = std::move(*result152);
  auto result153 = variant.humanoid_identity.to_json();
if (!result153) return std::unexpected(std::move(result153.error()));
auto result154 = json_from_map_v<StringMap<ItemDescriptor>>(variant.items, [&versioning_database](ItemDescriptor const& descriptor) {
      return descriptor.disk_store(*versioning_database);
    });
if (!result154) return std::unexpected(std::move(result154.error()));
auto damage_team_name = enum_name<TeamTypeNames>(variant.damage_team_type);
if (!damage_team_name)
  return std::unexpected(std::move(damage_team_name.error()));
JsonObject store{{"species", variant.species},
                 {"typeName", variant.type_name},
                 {"level", variant.level},
                 {"seed", variant.seed},
                 {"overrides", variant.overrides},
                 {"initialScriptDelta", variant.initial_script_delta},
                 {"humanoidIdentity", std::move(*result153)},
                 {"items", std::move(*result154)},
                 {"persistent", variant.persistent},
                 {"keepAlive", variant.keep_alive},
                 {"damageTeam", variant.damage_team},
                 {"damageTeamType", String(*damage_team_name)},
                 {"humanoidParameters", variant.humanoid_parameters}};
if (variant.description.has_value())
  store.set("description", variant.description.value_or(String{}));
return store;
}

std::expected<NpcVariant, StarException> NpcDatabase::read_npc_variant_from_json(Json const& data) const {
  NpcVariant variant;

  auto result156 = data.get_string("species");
if (!result156) return std::unexpected(std::move(result156.error()));
variant.species = std::move(*result156);
  auto result157 = data.get_string("typeName");
if (!result157) return std::unexpected(std::move(result157.error()));
variant.type_name = std::move(*result157);
  auto result158 = data.get_float("level");
if (!result158) return std::unexpected(std::move(result158.error()));
variant.level = std::move(*result158);
  auto result159 = data.get_uint("seed");
if (!result159) return std::unexpected(std::move(result159.error()));
variant.seed = std::move(*result159);
  auto result160 = data.get("overrides");
if (!result160) return std::unexpected(std::move(result160.error()));
variant.overrides = std::move(*result160);

  auto result161 = build_config(variant.type_name, variant.overrides);
if (!result161) return std::unexpected(std::move(result161.error()));
auto config = std::move(*result161);

  auto result162 = data.opt_string("description");
if (!result162) return std::unexpected(std::move(result162.error()));
variant.description = std::move(*result162);

  auto scripts = read_script_config(variant, config);
  if (!scripts) return std::unexpected(std::move(scripts.error()));

  auto result166 = data.get_int("initialScriptDelta");
if (!result166) return std::unexpected(std::move(result166.error()));
variant.initial_script_delta = std::move(*result166);
  auto result167 = data.get("humanoidIdentity");
if (!result167) return std::unexpected(std::move(result167.error()));
auto result168 = HumanoidIdentity::create(std::move(*result167));
if (!result168) return std::unexpected(std::move(result168.error()));
variant.humanoid_identity = std::move(*result168);
  auto result169 = data.get_object("humanoidParameters", JsonObject());
if (!result169) return std::unexpected(std::move(result169.error()));
variant.humanoid_parameters = std::move(*result169);

  auto humanoid_config = read_humanoid_config(m_root, variant, config);
  if (!humanoid_config) return std::unexpected(std::move(humanoid_config.error()));


  auto movement_and_status = read_movement_and_status(m_root, variant, config, true);
  if (!movement_and_status) return std::unexpected(std::move(movement_and_status.error()));

  auto result191 = m_root.versioningDatabase();
if (!result191) return std::unexpected(std::move(result191.error()));
auto versioning_database = std::move(*result191);
  auto result192 = data.get("items");
if (!result192) return std::unexpected(std::move(result192.error()));
auto result193 = json_to_map_v<StringMap<ItemDescriptor>>(std::move(*result192), [&versioning_database](Json const& descriptor) {
    return ItemDescriptor::load_store(*versioning_database, descriptor);
  });
if (!result193) return std::unexpected(std::move(result193.error()));
variant.items = std::move(*result193);

  auto appearance = read_appearance(variant, config);
  if (!appearance) return std::unexpected(std::move(appearance.error()));

  auto result197 = data.get_bool("persistent");
if (!result197) return std::unexpected(std::move(result197.error()));
variant.persistent = std::move(*result197);
  auto result198 = data.get_bool("keepAlive");
if (!result198) return std::unexpected(std::move(result198.error()));
variant.keep_alive = std::move(*result198);
  auto result199 = data.get_uint("damageTeam");
if (!result199) return std::unexpected(std::move(result199.error()));
variant.damage_team = std::move(*result199);
  auto result200 = data.get_string("damageTeamType");
if (!result200) return std::unexpected(std::move(result200.error()));
auto damage_team_type = enum_value<TeamTypeNames>(*result200);
if (!damage_team_type)
  return std::unexpected(std::move(damage_team_type.error()));
variant.damage_team_type = *damage_team_type;

  auto display = read_display(m_root, variant, config);
  if (!display) return std::unexpected(std::move(display.error()));

  return variant;
}

std::expected<std::shared_ptr<Npc>, StarException> NpcDatabase::create_npc(NpcVariant variant) const {
  return Npc::create(m_root, std::move(variant));
}

std::expected<std::shared_ptr<Npc>, StarException> NpcDatabase::disk_load_npc(Json const& disk_store) const {
  auto attempt = [&, &database = *this](Json const& store) -> std::expected<std::shared_ptr<Npc>, StarException> {
    auto encoded = store.get("npcVariant");
    if (!encoded) return std::unexpected(std::move(encoded.error()));
    auto variant = database.read_npc_variant_from_json(*encoded);
    if (!variant) return std::unexpected(std::move(variant.error()));
    return Npc::create(database.m_root, std::move(*variant), store);
  };
  auto npc = attempt(disk_store);
  if (npc)
    return npc;
  StarException last_error = std::move(npc.error());
  auto rebuilt = m_rebuilder->rebuild(disk_store, last_error, [&](Json const& store) -> std::expected<void, StarException> {
    auto replacement = attempt(store);
    if (!replacement) {
      last_error = std::move(replacement.error());
      return std::unexpected(last_error);
    }
    npc = std::move(replacement);
    return {};
  });
  if (!rebuilt)
    return std::unexpected(std::move(rebuilt.error()));
  if (!*rebuilt)
    return std::unexpected(std::move(last_error));
  return npc;
}

std::expected<std::shared_ptr<Npc>, StarException> NpcDatabase::net_load_npc(ByteArray const& net_store, NetCompatibilityRules rules) const {
  auto result206 = read_npc_variant(net_store, rules);
if (!result206) return std::unexpected(std::move(result206.error()));
return Npc::create(m_root, std::move(*result206));
}

std::expected<List<Drawable>, StarException> NpcDatabase::npc_portrait(NpcVariant const& npc_variant, PortraitMode mode, std::optional<std::reference_wrapper<ItemDatabase const>> item_catalog) const {
  auto result207 = Humanoid::create(m_root, npc_variant.humanoid_identity, npc_variant.humanoid_parameters, npc_variant.unique_humanoid_config ? npc_variant.humanoid_config : Json(), item_catalog);
if (!result207) return std::unexpected(std::move(result207.error()));
auto humanoid = std::move(*result207);

  std::shared_ptr<ItemDatabase const> item_database;
  if (!item_catalog) {
    auto result208 = m_root.itemDatabase();
if (!result208) return std::unexpected(std::move(result208.error()));
item_database = std::move(*result208);
    item_catalog = std::cref(*item_database);
  }

  auto make_item = [&npc_variant,
                   item_catalog](ItemDescriptor item_descriptor) -> std::expected<std::shared_ptr<Item>, StarException> {
    return item_catalog->get().item(std::move(item_descriptor), npc_variant.level, npc_variant.seed);
  };

  auto result209 = ArmorWearer::create(m_root);
if (!result209) return std::unexpected(std::move(result209.error()));
auto armor = std::move(*result209);
for (auto const& item : npc_variant.items) {
  {
    auto result210 = maybe_enum_value<equipment_slot_names>(item.first);
    if (!result210)
      return std::unexpected(std::move(result210.error()));
    if (auto equipment_slot = std::move(*result210)) {
      auto result211 = make_item(item.second);
      if (!result211)
        return std::unexpected(std::move(result211.error()));
      armor->set_item(static_cast<std::uint8_t>(*equipment_slot), as<ArmorItem>(std::move(*result211)));
    }
  }
}

  auto result212 = armor->setup_humanoid(*humanoid, false);
if (!result212) return std::unexpected(std::move(result212.error()));


  return humanoid->render_portrait(mode);
}

std::expected<Json, StarException> NpcDatabase::build_config(String const& type_name, Json const& overrides) const {
  // Inheritance is right-associated: each parent receives the already merged
  // child as its override. Flattening first changes object/scalar transitions.
  Json config = overrides;
  auto current = std::cref(type_name);
  auto checkpoint = current;
  std::size_t stride = 1;
  std::size_t distance = 0;
  for (;;) {
    auto type = m_npc_types.get(current.get());
    if (!type) return std::unexpected(std::move(type.error()));
    auto const& [base, parent] = type->get();
    config = json_merge(base, config);
    if (!parent) return std::unexpected(parent.error());
    if (parent->empty()) return config;

    // Brent's cycle detection avoids a visited allocation on every lookup.
    current = std::cref(*parent);
    if (current.get() == checkpoint.get())
      return std::unexpected(NpcException::format("NPC type '{}' has a baseType inheritance cycle at '{}'", type_name, current.get()));
    if (++distance == stride) {
      checkpoint = current;
      stride *= 2;
      distance = 0;
    }
  }
}

}
