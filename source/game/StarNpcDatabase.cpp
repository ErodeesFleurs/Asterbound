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
import star.version;
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

NpcDatabase::NpcDatabase(Root& root) : m_root(root) {}

std::expected<std::shared_ptr<NpcDatabase>, StarException> NpcDatabase::create(Root& root) {
  auto database = std::shared_ptr<NpcDatabase>(new NpcDatabase(root));
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

  auto result2 = assets->scanExtension("npctype");
if (!result2) return std::unexpected(std::move(result2.error()));
auto& files = result2->get();
  auto result3 = assets->queueJsons(files);
if (!result3) return std::unexpected(std::move(result3.error()));
  for (auto const& assetFile : files) {
    auto const& file = assetFile.string();
    auto loaded = [&, &types = m_npcTypes]() -> std::expected<void, StarException> {
      auto result4 = assets->json(file);
if (!result4) return std::unexpected(std::move(result4.error()));
auto config = std::move(*result4);
      auto result5 = config.getString("type");
if (!result5) return std::unexpected(std::move(result5.error()));
String typeName = std::move(*result5);

      if (types.contains(typeName))
        return std::unexpected(NpcException(strf("Repeat npc type name '{}'", typeName)));

      types[typeName] = config;

      return {};
    }();
    if (!loaded)
      return std::unexpected(NpcException(strf("Error loading npc type '{}'", file), assets->withAssetContext(std::move(loaded.error()), file, "Loading NPC type")));
  }
  return {};
}

std::expected<NpcVariant, StarException> NpcDatabase::generateNpcVariant(String const& species, String const& typeName, float level, std::optional<std::reference_wrapper<ItemDatabase const>> itemCatalog) const {
  return generateNpcVariant(species, typeName, level, m_root.random().randu64(), {}, itemCatalog);
}

std::expected<NpcVariant, StarException> NpcDatabase::generateNpcVariant(
    String const& species, String const& typeName, float level, std::uint64_t seed, Json const& overrides, std::optional<std::reference_wrapper<ItemDatabase const>> itemCatalog) const {
  NpcVariant variant;
  variant.species = species;
  variant.typeName = typeName;
  variant.seed = seed;
  variant.overrides = overrides;

  RandomSource randSource(seed);

  auto result6 = buildConfig(typeName, overrides);
if (!result6) return std::unexpected(std::move(result6.error()));
auto config = std::move(*result6);

  auto result7 = config.optString("description");
if (!result7) return std::unexpected(std::move(result7.error()));
variant.description = std::move(*result7);

  auto result8 = config.getArray("levelVariance", {0, 0});
if (!result8) return std::unexpected(std::move(result8.error()));
auto result9 = jsonToVec2F(std::move(*result8));
if (!result9) return std::unexpected(std::move(result9.error()));
auto levelVariance = std::move(*result9);
  auto result10 = randSource.randf(level + levelVariance[0], level + levelVariance[1]);
if (!result10) return std::unexpected(std::move(result10.error()));
variant.level = std::max(std::move(*result10), 0.0f);

  auto result11 = config.get("scripts");
if (!result11) return std::unexpected(std::move(result11.error()));
auto result12 = jsonToStringList(std::move(*result11));
if (!result12) return std::unexpected(std::move(result12.error()));
variant.scripts = std::move(*result12);
  auto result13 = config.getUInt("initialScriptDelta", 5);
if (!result13) return std::unexpected(std::move(result13.error()));
variant.initialScriptDelta = std::move(*result13);
  auto result14 = config.get("scriptConfig");
if (!result14) return std::unexpected(std::move(result14.error()));
variant.scriptConfig = std::move(*result14);

  auto result15 = m_root.speciesDatabase();
if (!result15) return std::unexpected(std::move(result15.error()));
auto speciesDatabase = std::move(*result15);
  auto result16 = speciesDatabase->species(species);
if (!result16) return std::unexpected(std::move(result16.error()));
auto speciesDefinition = std::move(*result16);
  auto result17 = speciesDatabase->generateHumanoid(species, seed, {}, itemCatalog);
if (!result17) return std::unexpected(std::move(result17.error()));
auto result = std::move(*result17);
  HumanoidIdentity identity = result.identity;

  auto result18 = config.getObject("humanoidParameters", JsonObject());
if (!result18) return std::unexpected(std::move(result18.error()));
auto result19 = jsonMerge(result.humanoidParameters, std::move(*result18)).toObject();
if (!result19) return std::unexpected(std::move(result19.error()));
variant.humanoidParameters = std::move(*result19);

  {
  auto result20 = config.contains("npcname");
  if (!result20) return std::unexpected(std::move(result20.error()));
  if (std::move(*result20))
      {
    auto result21 = config.getString("npcname");
    if (!result21) return std::unexpected(std::move(result21.error()));
    identity.name = std::move(*result21);
  }
    else {
    auto result22 = config.contains("nameGen");
    if (!result22) return std::unexpected(std::move(result22.error()));
    if (std::move(*result22)) {
        auto result23 = m_root.nameGenerator();
    if (!result23) return std::unexpected(std::move(result23.error()));
    auto result24 = config.get("nameGen");
    if (!result24) return std::unexpected(std::move(result24.error()));
    auto result25 = jsonToStringList(std::move(*result24));
    if (!result25) return std::unexpected(std::move(result25.error()));
    auto result26 =
      std::move(*result23)->generateName(std::move(*result25)[static_cast<int>(identity.gender)], randSource);
    if (!result26) return std::unexpected(std::move(result26.error()));
    identity.name = std::move(*result26);
      }
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
    auto result28 = identity.toJson();
    if (!result28) return std::unexpected(std::move(result28.error()));
    auto result29 = config.get("identity");
    if (!result29) return std::unexpected(std::move(result29.error()));
    auto result30 = HumanoidIdentity::create(jsonMerge(std::move(*result28), std::move(*result29)));
    if (!result30) return std::unexpected(std::move(result30.error()));
    identity = std::move(*result30);
  }
}

  auto result31 = config.contains("humanoidConfig");
if (!result31) return std::unexpected(std::move(result31.error()));
variant.uniqueHumanoidConfig = std::move(*result31);
  if (variant.uniqueHumanoidConfig){
    auto result32 = m_root.assets();
if (!result32) return std::unexpected(std::move(result32.error()));
auto result33 = config.getString("humanoidConfig");
if (!result33) return std::unexpected(std::move(result33.error()));
    auto humanoidLoaded = [&]() -> std::expected<void, StarException> {
auto result34 = (*result32)->json(*result33);
if (!result34) return std::unexpected(std::move(result34.error()));
variant.humanoidConfig = std::move(*result34);
    auto result35 = speciesDatabase->humanoidConfig(identity, variant.humanoidParameters, variant.humanoidConfig, itemCatalog);
if (!result35) return std::unexpected(std::move(result35.error()));
auto usedHumanoidConfig = std::move(*result35);
    // this only needs to be done if the npc has a unique humanoid config, otherwise the output from generateHumanoid should be fine
    auto result36 = usedHumanoidConfig.getArray("personalities");
if (!result36) return std::unexpected(std::move(result36.error()));
auto result37 = randSource.randFrom(*result36);
if (!result37) return std::unexpected(std::move(result37.error()));
auto result38 = parsePersonalityArray(result37->get());
if (!result38) return std::unexpected(std::move(result38.error()));
identity.personality = std::move(*result38);
    return {};
    }();
    if (!humanoidLoaded)
      return std::unexpected((*result32)->withAssetContext(std::move(humanoidLoaded.error()), *result33, "Loading NPC humanoid configuration"));
  } else {
    auto result39 = speciesDefinition->humanoidConfig();
if (!result39) return std::unexpected(std::move(result39.error()));
variant.humanoidConfig = std::move(*result39);
  }

  // and where it ususally merges the identity
  {
  auto result40 = config.contains("identity");
  if (!result40) return std::unexpected(std::move(result40.error()));
  if (std::move(*result40))
      {
    auto result41 = identity.toJson();
    if (!result41) return std::unexpected(std::move(result41.error()));
    auto result42 = config.get("identity");
    if (!result42) return std::unexpected(std::move(result42.error()));
    auto result43 = HumanoidIdentity::create(jsonMerge(std::move(*result41), std::move(*result42)));
    if (!result43) return std::unexpected(std::move(result43.error()));
    identity = std::move(*result43);
  }
}

  variant.humanoidIdentity = identity;

  auto result44 = config.get("movementParameters", {});
if (!result44) return std::unexpected(std::move(result44.error()));
variant.movementParameters = std::move(*result44);
  auto result45 = config.get("statusControllerSettings");
if (!result45) return std::unexpected(std::move(result45.error()));
variant.statusControllerSettings = std::move(*result45);

  auto result46 = m_root.functionDatabase();
if (!result46) return std::unexpected(std::move(result46.error()));
auto functionDatabase = std::move(*result46);
  auto result47 = functionDatabase->function("npcLevelPowerMultiplierModifier");
if (!result47) return std::unexpected(std::move(result47.error()));
auto result48 = std::move(*result47)->evaluate(variant.level);
if (!result48) return std::unexpected(std::move(result48.error()));
float powerMultiplierModifier = std::move(*result48);
  auto result49 = functionDatabase->function("npcLevelProtectionMultiplier");
if (!result49) return std::unexpected(std::move(result49.error()));
auto result50 = std::move(*result49)->evaluate(variant.level);
if (!result50) return std::unexpected(std::move(result50.error()));
float protectionMultiplier = std::move(*result50);
  auto result51 = functionDatabase->function("npcLevelHealthMultiplier");
if (!result51) return std::unexpected(std::move(result51.error()));
auto result52 = std::move(*result51)->evaluate(variant.level);
if (!result52) return std::unexpected(std::move(result52.error()));
float maxHealthMultiplier = std::move(*result52);
  auto result53 = functionDatabase->function("npcLevelEnergyMultiplier");
if (!result53) return std::unexpected(std::move(result53.error()));
auto result54 = std::move(*result53)->evaluate(variant.level);
if (!result54) return std::unexpected(std::move(result54.error()));
float maxEnergyMultiplier = std::move(*result54);

  auto result55 = config.get("innateStatusEffects", JsonArray());
if (!result55) return std::unexpected(std::move(result55.error()));
auto result56 = jsonToList<PersistentStatusEffect>(std::move(*result55), jsonToPersistentStatusEffect);
if (!result56) return std::unexpected(std::move(result56.error()));
variant.innateStatusEffects = std::move(*result56);
  variant.innateStatusEffects.append(StatModifier(StatValueModifier{"powerMultiplier", powerMultiplierModifier}));
  variant.innateStatusEffects.append(StatModifier(StatBaseMultiplier{"protection", protectionMultiplier}));
  variant.innateStatusEffects.append(StatModifier(StatBaseMultiplier{"maxHealth", maxHealthMultiplier}));
  variant.innateStatusEffects.append(StatModifier(StatBaseMultiplier{"maxEnergy", maxEnergyMultiplier}));

  auto result57 = config.get("touchDamage", {});
if (!result57) return std::unexpected(std::move(result57.error()));
variant.touchDamageConfig = std::move(*result57);

  auto result58 = config.get("items", {});
if (!result58) return std::unexpected(std::move(result58.error()));
auto itemsConfig = std::move(*result58);
  if (!itemsConfig.isNull()) {
    auto result59 = itemsConfig.get("override", {});
if (!result59) return std::unexpected(std::move(result59.error()));
auto speciesItemsConfig = std::move(*result59);
    if (speciesItemsConfig.isNull())
      {
  auto result60 = itemsConfig.get(species, {});
  if (!result60) return std::unexpected(std::move(result60.error()));
  speciesItemsConfig = std::move(*result60);
}
    if (speciesItemsConfig.isNull())
      {
  auto result61 = itemsConfig.get("default", {});
  if (!result61) return std::unexpected(std::move(result61.error()));
  speciesItemsConfig = std::move(*result61);
}

    if (!speciesItemsConfig.isNull()) {
      Json highestLevelItemsConfig;
      {
        auto result62 = speciesItemsConfig.iterateArray();
        if (!result62)
          return std::unexpected(std::move(result62.error()));
        for (auto const& levelItemsConfig : *result62) {
          {
    auto result63 = levelItemsConfig.getFloat(0);
    if (!result63) return std::unexpected(std::move(result63.error()));
    if (variant.level >= std::move(*result63)) {
              auto result64 = levelItemsConfig.get(1);
    if (!result64) return std::unexpected(std::move(result64.error()));
    highestLevelItemsConfig = std::move(*result64);
            }
  }
        }
}

      if (!highestLevelItemsConfig.isNull()) {
        int randomColorIndex = -1;
        auto result65 = config.getBool("matchColorIndices", false);
if (!result65) return std::unexpected(std::move(result65.error()));
bool matchColorIndices = std::move(*result65);

        {
          auto result66 = highestLevelItemsConfig.arrayPtr();
          if (!result66)
            return std::unexpected(std::move(result66.error()));
          auto result67 = randSource.randFrom(**result66);
          if (!result67)
            return std::unexpected(std::move(result67.error()));
          auto result68 = result67->get().toObject();
          if (!result68)
            return std::unexpected(std::move(result68.error()));
          for (auto const& itemSlotConfig : std::move(*result68)) {
            auto result69 = itemSlotConfig.second.arrayPtr();
            if (!result69)
              return std::unexpected(std::move(result69.error()));
            auto result70 = randSource.randFrom(**result69);
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
    auto colorIndex = std::move(*result73);
                if (colorIndex.isType(Json::Type::Array)) {
                  if (!matchColorIndices || randomColorIndex == -1)
                    {
                    auto result74 = colorIndex.arrayPtr();
                    if (!result74)
                      return std::unexpected(std::move(result74.error()));
                    auto result75 = randSource.randFrom(**result74);
                    if (!result75)
                      return std::unexpected(std::move(result75.error()));
                    auto result76 = result75->get().toInt();
                    if (!result76)
                      return std::unexpected(std::move(result76.error()));
                    randomColorIndex = std::move(*result76);
    }

                  auto result77 = item.applyParameters({{"colorIndex", randomColorIndex}});
    if (!result77) return std::unexpected(std::move(result77.error()));
    item = std::move(*result77);
                }
              }
  }

            variant.items[itemSlotConfig.first] = std::move(item);
          }
}
      }
    }
  }

  auto result78 = config.getBool("disableWornArmor", true);
if (!result78) return std::unexpected(std::move(result78.error()));
variant.disableWornArmor = std::move(*result78);

  auto result79 = config.get("dropPools", JsonArray{});
if (!result79) return std::unexpected(std::move(result79.error()));
auto result80 = jsonToStringList(std::move(*result79));
if (!result80) return std::unexpected(std::move(result80.error()));
variant.dropPools = std::move(*result80);

  auto result81 = config.getBool("persistent", false);
if (!result81) return std::unexpected(std::move(result81.error()));
variant.persistent = std::move(*result81);
  auto result82 = config.getBool("keepAlive", false);
if (!result82) return std::unexpected(std::move(result82.error()));
variant.keepAlive = std::move(*result82);

  auto result83 = config.getUInt("damageTeam", 0);
if (!result83) return std::unexpected(std::move(result83.error()));
variant.damageTeam = std::move(*result83);
  auto result84 = config.getString("damageTeamType", "enemy");
if (!result84) return std::unexpected(std::move(result84.error()));
auto damageTeamType = enumValue<TeamTypeNames>(*result84);
if (!damageTeamType)
  return std::unexpected(std::move(damageTeamType.error()));
variant.damageTeamType = *damageTeamType;

auto result86 = config.get("nametagColor", JsonArray{255, 255, 255});
if (!result86) return std::unexpected(std::move(result86.error()));
auto result87 = jsonToVec3B(std::move(*result86));
if (!result87) return std::unexpected(std::move(result87.error()));
variant.nametagColor = std::move(*result87);

  auto result88 = config.get("splashConfig");
if (!result88) return std::unexpected(std::move(result88.error()));
auto result89 = EntitySplashConfig::create(m_root, std::move(*result88));
if (!result89) return std::unexpected(std::move(result89.error()));
variant.splashConfig = std::move(*result89);

  return variant;
}

std::expected<ByteArray, StarException> NpcDatabase::writeNpcVariant(NpcVariant const& variant, NetCompatibilityRules rules) const {
  DataStreamBuffer ds;
  ds.setStreamCompatibilityVersion(rules);

  auto result90 = ds.write(variant.species);
if (!result90) return std::unexpected(std::move(result90.error()));

  auto result91 = ds.write(variant.typeName);
if (!result91) return std::unexpected(std::move(result91.error()));

  auto result92 = ds.write(variant.level);
if (!result92) return std::unexpected(std::move(result92.error()));

  auto result93 = ds.write(variant.seed);
if (!result93) return std::unexpected(std::move(result93.error()));

  auto result94 = ds.write(variant.overrides);
if (!result94) return std::unexpected(std::move(result94.error()));


  auto result95 = ds.write(variant.initialScriptDelta);
if (!result95) return std::unexpected(std::move(result95.error()));

  auto result96 = ds.write(variant.humanoidIdentity);
if (!result96) return std::unexpected(std::move(result96.error()));

  if (rules.version() >= 11 ) {
    auto result97 = ds.write(variant.humanoidParameters);
if (!result97) return std::unexpected(std::move(result97.error()));

    auto result98 = ds.write(variant.description);
if (!result98) return std::unexpected(std::move(result98.error()));

  }

  auto result99 = ds.writeMapContainer(variant.items);
if (!result99) return std::unexpected(std::move(result99.error()));


  auto result100 = ds.write(variant.persistent);
if (!result100) return std::unexpected(std::move(result100.error()));

  auto result101 = ds.write(variant.keepAlive);
if (!result101) return std::unexpected(std::move(result101.error()));

  auto result102 = ds.write(variant.damageTeam);
if (!result102) return std::unexpected(std::move(result102.error()));

  auto result103 = ds.write(variant.damageTeamType);
if (!result103) return std::unexpected(std::move(result103.error()));


  return ds.takeData();
}

std::expected<NpcVariant, StarException> NpcDatabase::readNpcVariant(ByteArray const& data, NetCompatibilityRules rules) const {
  DataStreamExternalBuffer ds(data.span());
  ds.setStreamCompatibilityVersion(rules);

  NpcVariant variant;

  auto result104 = ds.read(variant.species);
if (!result104) return std::unexpected(std::move(result104.error()));

  auto result105 = ds.read(variant.typeName);
if (!result105) return std::unexpected(std::move(result105.error()));

  auto result106 = ds.read(variant.level);
if (!result106) return std::unexpected(std::move(result106.error()));

  auto result107 = ds.read(variant.seed);
if (!result107) return std::unexpected(std::move(result107.error()));

  auto result108 = ds.read(variant.overrides);
if (!result108) return std::unexpected(std::move(result108.error()));


  auto result109 = buildConfig(variant.typeName, variant.overrides);
if (!result109) return std::unexpected(std::move(result109.error()));
auto config = std::move(*result109);


  auto result110 = config.get("scripts");
if (!result110) return std::unexpected(std::move(result110.error()));
auto result111 = jsonToStringList(std::move(*result110));
if (!result111) return std::unexpected(std::move(result111.error()));
variant.scripts = std::move(*result111);
  auto result112 = config.get("scriptConfig");
if (!result112) return std::unexpected(std::move(result112.error()));
variant.scriptConfig = std::move(*result112);

  auto result113 = ds.read(variant.initialScriptDelta);
if (!result113) return std::unexpected(std::move(result113.error()));

  auto result114 = ds.read(variant.humanoidIdentity);
if (!result114) return std::unexpected(std::move(result114.error()));

  if (rules.version() >= 11){
    auto result115 = ds.read(variant.humanoidParameters);
if (!result115) return std::unexpected(std::move(result115.error()));

    auto result116 = ds.read(variant.description);
if (!result116) return std::unexpected(std::move(result116.error()));

  } else {
    auto result117 = config.getObject("humanoidParameters", JsonObject());
if (!result117) return std::unexpected(std::move(result117.error()));
variant.humanoidParameters = std::move(*result117);
    auto result118 = config.optString("description");
if (!result118) return std::unexpected(std::move(result118.error()));
variant.description = std::move(*result118);
  }

  auto result119 = m_root.speciesDatabase();
if (!result119) return std::unexpected(std::move(result119.error()));
auto speciesDatabase = std::move(*result119);
  auto result120 = speciesDatabase->species(variant.species);
if (!result120) return std::unexpected(std::move(result120.error()));
auto speciesDefinition = std::move(*result120);
  auto result121 = config.contains("humanoidConfig");
if (!result121) return std::unexpected(std::move(result121.error()));
variant.uniqueHumanoidConfig = std::move(*result121);
  if (variant.uniqueHumanoidConfig)
    {
  auto result122 = m_root.assets();
  if (!result122) return std::unexpected(std::move(result122.error()));
  auto result123 = config.getString("humanoidConfig");
  if (!result123) return std::unexpected(std::move(result123.error()));
  auto result124 = (*result122)->json(*result123);
  if (!result124) return std::unexpected((*result122)->withAssetContext(std::move(result124.error()), *result123, "Loading NPC humanoid configuration"));
  variant.humanoidConfig = std::move(*result124);
}
  else
    {
  auto result125 = speciesDefinition->humanoidConfig();
  if (!result125) return std::unexpected(std::move(result125.error()));
  variant.humanoidConfig = std::move(*result125);
}

  auto result126 = config.get("movementParameters", {});
if (!result126) return std::unexpected(std::move(result126.error()));
variant.movementParameters = std::move(*result126);
  auto result127 = config.get("statusControllerSettings");
if (!result127) return std::unexpected(std::move(result127.error()));
variant.statusControllerSettings = std::move(*result127);

  auto result128 = m_root.functionDatabase();
if (!result128) return std::unexpected(std::move(result128.error()));
auto functionDatabase = std::move(*result128);
  auto result129 = functionDatabase->function("npcLevelPowerMultiplierModifier");
if (!result129) return std::unexpected(std::move(result129.error()));
auto result130 = std::move(*result129)->evaluate(variant.level);
if (!result130) return std::unexpected(std::move(result130.error()));
float powerMultiplierModifier =
      std::move(*result130);
  auto result131 = functionDatabase->function("npcLevelProtectionMultiplier");
if (!result131) return std::unexpected(std::move(result131.error()));
auto result132 = std::move(*result131)->evaluate(variant.level);
if (!result132) return std::unexpected(std::move(result132.error()));
float protectionMultiplier = std::move(*result132);
  auto result133 = functionDatabase->function("npcLevelHealthMultiplier");
if (!result133) return std::unexpected(std::move(result133.error()));
auto result134 = std::move(*result133)->evaluate(variant.level);
if (!result134) return std::unexpected(std::move(result134.error()));
float maxHealthMultiplier = std::move(*result134);
  auto result135 = functionDatabase->function("npcLevelEnergyMultiplier");
if (!result135) return std::unexpected(std::move(result135.error()));
auto result136 = std::move(*result135)->evaluate(variant.level);
if (!result136) return std::unexpected(std::move(result136.error()));
float maxEnergyMultiplier = std::move(*result136);

  auto result137 = config.get("innateStatusEffects", JsonArray());
if (!result137) return std::unexpected(std::move(result137.error()));
auto result138 = jsonToList<PersistentStatusEffect>(std::move(*result137), jsonToPersistentStatusEffect);
if (!result138) return std::unexpected(std::move(result138.error()));
variant.innateStatusEffects = std::move(*result138);

  variant.innateStatusEffects.append(StatModifier(StatValueModifier{"powerMultiplier", powerMultiplierModifier}));
  variant.innateStatusEffects.append(StatModifier(StatBaseMultiplier{"protection", protectionMultiplier}));
  variant.innateStatusEffects.append(StatModifier(StatBaseMultiplier{"maxHealth", maxHealthMultiplier}));
  variant.innateStatusEffects.append(StatModifier(StatBaseMultiplier{"maxEnergy", maxEnergyMultiplier}));

  auto result139 = config.get("touchDamage", {});
if (!result139) return std::unexpected(std::move(result139.error()));
variant.touchDamageConfig = std::move(*result139);

  auto result140 = ds.readMapContainer(variant.items);
if (!result140) return std::unexpected(std::move(result140.error()));


  auto result141 = config.getBool("disableWornArmor", true);
if (!result141) return std::unexpected(std::move(result141.error()));
variant.disableWornArmor = std::move(*result141);
  auto result142 = config.get("dropPools", JsonArray{});
if (!result142) return std::unexpected(std::move(result142.error()));
auto result143 = jsonToStringList(std::move(*result142));
if (!result143) return std::unexpected(std::move(result143.error()));
variant.dropPools = std::move(*result143);

  auto result144 = ds.read(variant.persistent);
if (!result144) return std::unexpected(std::move(result144.error()));

  auto result145 = ds.read(variant.keepAlive);
if (!result145) return std::unexpected(std::move(result145.error()));

  auto result146 = ds.read(variant.damageTeam);
if (!result146) return std::unexpected(std::move(result146.error()));

  auto result147 = ds.read(variant.damageTeamType);
if (!result147) return std::unexpected(std::move(result147.error()));


  auto result148 = config.get("nametagColor", JsonArray{255, 255, 255});
if (!result148) return std::unexpected(std::move(result148.error()));
auto result149 = jsonToVec3B(std::move(*result148));
if (!result149) return std::unexpected(std::move(result149.error()));
variant.nametagColor = std::move(*result149);

  auto result150 = config.get("splashConfig");
if (!result150) return std::unexpected(std::move(result150.error()));
auto result151 = EntitySplashConfig::create(m_root, std::move(*result150));
if (!result151) return std::unexpected(std::move(result151.error()));
variant.splashConfig = std::move(*result151);

  return variant;
}

std::expected<Json, StarException> NpcDatabase::writeNpcVariantToJson(NpcVariant const& variant) const {
  auto result152 = m_root.versioningDatabase();
if (!result152) return std::unexpected(std::move(result152.error()));
auto versioningDatabase = std::move(*result152);
  auto result153 = variant.humanoidIdentity.toJson();
if (!result153) return std::unexpected(std::move(result153.error()));
auto result154 = jsonFromMapV<StringMap<ItemDescriptor>>(variant.items, [&versioningDatabase](ItemDescriptor const& descriptor) {
      return descriptor.diskStore(*versioningDatabase);
    });
if (!result154) return std::unexpected(std::move(result154.error()));
auto damageTeamName = enumName<TeamTypeNames>(variant.damageTeamType);
if (!damageTeamName)
  return std::unexpected(std::move(damageTeamName.error()));
JsonObject store{{"species", variant.species},
                 {"typeName", variant.typeName},
                 {"level", variant.level},
                 {"seed", variant.seed},
                 {"overrides", variant.overrides},
                 {"initialScriptDelta", variant.initialScriptDelta},
                 {"humanoidIdentity", std::move(*result153)},
                 {"items", std::move(*result154)},
                 {"persistent", variant.persistent},
                 {"keepAlive", variant.keepAlive},
                 {"damageTeam", variant.damageTeam},
                 {"damageTeamType", String(*damageTeamName)},
                 {"humanoidParameters", variant.humanoidParameters}};
if (variant.description.has_value())
  store.set("description", variant.description.value_or(String{}));
return store;
}

std::expected<NpcVariant, StarException> NpcDatabase::readNpcVariantFromJson(Json const& data) const {
  NpcVariant variant;

  auto result156 = data.getString("species");
if (!result156) return std::unexpected(std::move(result156.error()));
variant.species = std::move(*result156);
  auto result157 = data.getString("typeName");
if (!result157) return std::unexpected(std::move(result157.error()));
variant.typeName = std::move(*result157);
  auto result158 = data.getFloat("level");
if (!result158) return std::unexpected(std::move(result158.error()));
variant.level = std::move(*result158);
  auto result159 = data.getUInt("seed");
if (!result159) return std::unexpected(std::move(result159.error()));
variant.seed = std::move(*result159);
  auto result160 = data.get("overrides");
if (!result160) return std::unexpected(std::move(result160.error()));
variant.overrides = std::move(*result160);

  auto result161 = buildConfig(variant.typeName, variant.overrides);
if (!result161) return std::unexpected(std::move(result161.error()));
auto config = std::move(*result161);

  auto result162 = data.optString("description");
if (!result162) return std::unexpected(std::move(result162.error()));
variant.description = std::move(*result162);

  auto result163 = config.get("scripts");
if (!result163) return std::unexpected(std::move(result163.error()));
auto result164 = jsonToStringList(std::move(*result163));
if (!result164) return std::unexpected(std::move(result164.error()));
variant.scripts = std::move(*result164);
  auto result165 = config.get("scriptConfig");
if (!result165) return std::unexpected(std::move(result165.error()));
variant.scriptConfig = std::move(*result165);

  auto result166 = data.getInt("initialScriptDelta");
if (!result166) return std::unexpected(std::move(result166.error()));
variant.initialScriptDelta = std::move(*result166);
  auto result167 = data.get("humanoidIdentity");
if (!result167) return std::unexpected(std::move(result167.error()));
auto result168 = HumanoidIdentity::create(std::move(*result167));
if (!result168) return std::unexpected(std::move(result168.error()));
variant.humanoidIdentity = std::move(*result168);
  auto result169 = data.getObject("humanoidParameters", JsonObject());
if (!result169) return std::unexpected(std::move(result169.error()));
variant.humanoidParameters = std::move(*result169);

  auto result170 = m_root.speciesDatabase();
if (!result170) return std::unexpected(std::move(result170.error()));
auto speciesDatabase = std::move(*result170);
  auto result171 = speciesDatabase->species(variant.species);
if (!result171) return std::unexpected(std::move(result171.error()));
auto speciesDefinition = std::move(*result171);
  auto result172 = config.contains("humanoidConfig");
if (!result172) return std::unexpected(std::move(result172.error()));
variant.uniqueHumanoidConfig = std::move(*result172);
  if (variant.uniqueHumanoidConfig)
    {
  auto result173 = m_root.assets();
  if (!result173) return std::unexpected(std::move(result173.error()));
  auto result174 = config.getString("humanoidConfig");
  if (!result174) return std::unexpected(std::move(result174.error()));
  auto result175 = (*result173)->json(*result174);
  if (!result175) return std::unexpected((*result173)->withAssetContext(std::move(result175.error()), *result174, "Loading NPC humanoid configuration"));
  variant.humanoidConfig = std::move(*result175);
}
  else
    {
  auto result176 = speciesDefinition->humanoidConfig();
  if (!result176) return std::unexpected(std::move(result176.error()));
  variant.humanoidConfig = std::move(*result176);
}


  auto result177 = config.get("movementParameters", {});
if (!result177) return std::unexpected(std::move(result177.error()));
variant.movementParameters = std::move(*result177);
  auto result178 = config.get("statusControllerSettings", {});
if (!result178) return std::unexpected(std::move(result178.error()));
variant.statusControllerSettings = std::move(*result178);

  auto result179 = m_root.functionDatabase();
if (!result179) return std::unexpected(std::move(result179.error()));
auto functionDatabase = std::move(*result179);
  auto result180 = functionDatabase->function("npcLevelPowerMultiplierModifier");
if (!result180) return std::unexpected(std::move(result180.error()));
auto result181 = std::move(*result180)->evaluate(variant.level);
if (!result181) return std::unexpected(std::move(result181.error()));
float powerMultiplierModifier =
      std::move(*result181);
  auto result182 = functionDatabase->function("npcLevelProtectionMultiplier");
if (!result182) return std::unexpected(std::move(result182.error()));
auto result183 = std::move(*result182)->evaluate(variant.level);
if (!result183) return std::unexpected(std::move(result183.error()));
float protectionMultiplier = std::move(*result183);
  auto result184 = functionDatabase->function("npcLevelHealthMultiplier");
if (!result184) return std::unexpected(std::move(result184.error()));
auto result185 = std::move(*result184)->evaluate(variant.level);
if (!result185) return std::unexpected(std::move(result185.error()));
float maxHealthMultiplier = std::move(*result185);
  auto result186 = functionDatabase->function("npcLevelEnergyMultiplier");
if (!result186) return std::unexpected(std::move(result186.error()));
auto result187 = std::move(*result186)->evaluate(variant.level);
if (!result187) return std::unexpected(std::move(result187.error()));
float maxEnergyMultiplier = std::move(*result187);

  auto result188 = config.get("innateStatusEffects", JsonArray());
if (!result188) return std::unexpected(std::move(result188.error()));
auto result189 = jsonToList<PersistentStatusEffect>(std::move(*result188), jsonToPersistentStatusEffect);
if (!result189) return std::unexpected(std::move(result189.error()));
variant.innateStatusEffects = std::move(*result189);

  variant.innateStatusEffects.append(StatModifier(StatValueModifier{"powerMultiplier", powerMultiplierModifier}));
  variant.innateStatusEffects.append(StatModifier(StatBaseMultiplier{"protection", protectionMultiplier}));
  variant.innateStatusEffects.append(StatModifier(StatBaseMultiplier{"maxHealth", maxHealthMultiplier}));
  variant.innateStatusEffects.append(StatModifier(StatBaseMultiplier{"maxEnergy", maxEnergyMultiplier}));

  auto result190 = config.get("touchDamage", {});
if (!result190) return std::unexpected(std::move(result190.error()));
variant.touchDamageConfig = std::move(*result190);

  auto result191 = m_root.versioningDatabase();
if (!result191) return std::unexpected(std::move(result191.error()));
auto versioningDatabase = std::move(*result191);
  auto result192 = data.get("items");
if (!result192) return std::unexpected(std::move(result192.error()));
auto result193 = jsonToMapV<StringMap<ItemDescriptor>>(std::move(*result192), [&versioningDatabase](Json const& descriptor) {
    return ItemDescriptor::loadStore(*versioningDatabase, descriptor);
  });
if (!result193) return std::unexpected(std::move(result193.error()));
variant.items = std::move(*result193);

  auto result194 = config.getBool("disableWornArmor", true);
if (!result194) return std::unexpected(std::move(result194.error()));
variant.disableWornArmor = std::move(*result194);
  auto result195 = config.get("dropPools", JsonArray{});
if (!result195) return std::unexpected(std::move(result195.error()));
auto result196 = jsonToStringList(std::move(*result195));
if (!result196) return std::unexpected(std::move(result196.error()));
variant.dropPools = std::move(*result196);

  auto result197 = data.getBool("persistent");
if (!result197) return std::unexpected(std::move(result197.error()));
variant.persistent = std::move(*result197);
  auto result198 = data.getBool("keepAlive");
if (!result198) return std::unexpected(std::move(result198.error()));
variant.keepAlive = std::move(*result198);
  auto result199 = data.getUInt("damageTeam");
if (!result199) return std::unexpected(std::move(result199.error()));
variant.damageTeam = std::move(*result199);
  auto result200 = data.getString("damageTeamType");
if (!result200) return std::unexpected(std::move(result200.error()));
auto damageTeamType = enumValue<TeamTypeNames>(*result200);
if (!damageTeamType)
  return std::unexpected(std::move(damageTeamType.error()));
variant.damageTeamType = *damageTeamType;

auto result202 = config.get("nametagColor", JsonArray{255, 255, 255});
if (!result202) return std::unexpected(std::move(result202.error()));
auto result203 = jsonToVec3B(std::move(*result202));
if (!result203) return std::unexpected(std::move(result203.error()));
variant.nametagColor = std::move(*result203);

  auto result204 = config.get("splashConfig");
if (!result204) return std::unexpected(std::move(result204.error()));
auto result205 = EntitySplashConfig::create(m_root, std::move(*result204));
if (!result205) return std::unexpected(std::move(result205.error()));
variant.splashConfig = std::move(*result205);

  return variant;
}

std::expected<std::shared_ptr<Npc>, StarException> NpcDatabase::createNpc(NpcVariant const& npcVariant) const {
  return Npc::create(m_root, npcVariant);
}

std::expected<std::shared_ptr<Npc>, StarException> NpcDatabase::diskLoadNpc(Json const& diskStore) const {
  auto attempt = [&, &database = *this](Json const& store) -> std::expected<std::shared_ptr<Npc>, StarException> {
    auto encoded = store.get("npcVariant");
    if (!encoded) return std::unexpected(std::move(encoded.error()));
    auto variant = database.readNpcVariantFromJson(*encoded);
    if (!variant) return std::unexpected(std::move(variant.error()));
    return Npc::create(database.m_root, *variant, store);
  };
  auto npc = attempt(diskStore);
  if (npc)
    return npc;
  StarException lastError = std::move(npc.error());
  auto rebuilt = m_rebuilder->rebuild(diskStore, lastError, [&](Json const& store) -> std::expected<void, StarException> {
    auto replacement = attempt(store);
    if (!replacement) {
      lastError = std::move(replacement.error());
      return std::unexpected(lastError);
    }
    npc = std::move(replacement);
    return {};
  });
  if (!rebuilt)
    return std::unexpected(std::move(rebuilt.error()));
  if (!*rebuilt)
    return std::unexpected(std::move(lastError));
  return npc;
}

std::expected<std::shared_ptr<Npc>, StarException> NpcDatabase::netLoadNpc(ByteArray const& netStore, NetCompatibilityRules rules) const {
  auto result206 = readNpcVariant(netStore, rules);
if (!result206) return std::unexpected(std::move(result206.error()));
return Npc::create(m_root, std::move(*result206));
}

std::expected<List<Drawable>, StarException> NpcDatabase::npcPortrait(NpcVariant const& npcVariant, PortraitMode mode, std::optional<std::reference_wrapper<ItemDatabase const>> itemCatalog) const {
  auto result207 = Humanoid::create(m_root, npcVariant.humanoidIdentity, npcVariant.humanoidParameters, npcVariant.uniqueHumanoidConfig ? npcVariant.humanoidConfig : Json(), itemCatalog);
if (!result207) return std::unexpected(std::move(result207.error()));
auto humanoid = std::move(*result207);

  std::shared_ptr<ItemDatabase const> itemDatabase;
  if (!itemCatalog) {
    auto result208 = m_root.itemDatabase();
if (!result208) return std::unexpected(std::move(result208.error()));
itemDatabase = std::move(*result208);
    itemCatalog = std::cref(*itemDatabase);
  }

  auto makeItem = [&npcVariant,
                   itemCatalog](ItemDescriptor itemDescriptor) -> std::expected<std::shared_ptr<Item>, StarException> {
    return itemCatalog->get().item(std::move(itemDescriptor), npcVariant.level, npcVariant.seed);
  };

  auto result209 = ArmorWearer::create(m_root);
if (!result209) return std::unexpected(std::move(result209.error()));
auto armor = std::move(*result209);
for (auto const& item : npcVariant.items) {
  {
    if (!EquipmentSlotNames)
      return std::unexpected(EquipmentSlotNames.error());
    auto result210 = EquipmentSlotNames->maybe_left(item.first);
    if (!result210)
      return std::unexpected(std::move(result210.error()));
    if (auto equipmentSlot = std::move(*result210)) {
      auto result211 = makeItem(item.second);
      if (!result211)
        return std::unexpected(std::move(result211.error()));
      armor->setItem(static_cast<std::uint8_t>(*equipmentSlot), as<ArmorItem>(std::move(*result211)));
    }
  }
}

  auto result212 = armor->setupHumanoid(*humanoid, false);
if (!result212) return std::unexpected(std::move(result212.error()));


  return humanoid->renderPortrait(mode);
}

std::expected<Json, StarException> NpcDatabase::buildConfig(String const& typeName, Json const& overrides) const {
  auto result213 = m_npcTypes.get(typeName);
if (!result213) return std::unexpected(std::move(result213.error()));
auto const& baseConfig = result213->get();
  auto result214 = mergeConfigValues(baseConfig, overrides);
if (!result214) return std::unexpected(std::move(result214.error()));
auto config = std::move(*result214);

  auto result215 = baseConfig.getString("baseType", "");
if (!result215) return std::unexpected(std::move(result215.error()));
String baseTypeName = std::move(*result215);
  if (baseTypeName.empty()) {
    return config;
  } else {
    return buildConfig(baseTypeName, config);
  }
}

std::expected<Json, StarException> NpcDatabase::mergeConfigValues(Json const& base, Json const& merger) const {
  if (base.type() == Json::Type::Object && merger.type() == Json::Type::Object) {
    auto result216 = base.toObject();
if (!result216) return std::unexpected(std::move(result216.error()));
auto map = std::move(*result216);
    {
  auto result217 = merger.iterateObject();
  if (!result217) return std::unexpected(std::move(result217.error()));
  for (auto const& entry : std::move(*result217)) {
        if (!map.insert(entry.first, entry.second).second) {
          auto result218 = mergeConfigValues(map[entry.first], entry.second);
  if (!result218) return std::unexpected(std::move(result218.error()));
  map[entry.first] = std::move(*result218);
        }
      }
}
    return map;
  } else if (merger.isNull()) {
    return base;
  } else {
    return merger;
  }
}

}
