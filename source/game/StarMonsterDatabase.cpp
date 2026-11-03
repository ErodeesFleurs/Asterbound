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
import star.version;

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
namespace Star {

MonsterDatabase::MonsterDatabase(Root& root) : m_root(root), m_monsterCache(root.random()) {}

std::expected<std::shared_ptr<MonsterDatabase>, StarException> MonsterDatabase::create(Root& root) {
  auto database = std::shared_ptr<MonsterDatabase>(new MonsterDatabase(root));
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
  auto result1 = m_root.assets();
if (!result1) return std::unexpected(std::move(result1.error()));
auto assets = std::move(*result1);

  auto result2 = assets->scanExtension("monstertype");
if (!result2) return std::unexpected(std::move(result2.error()));
auto& monsterTypes = result2->get();
  auto result3 = assets->scanExtension("monsterpart");
if (!result3) return std::unexpected(std::move(result3.error()));
auto& monsterParts = result3->get();
  auto result4 = assets->scanExtension("monsterskill");
if (!result4) return std::unexpected(std::move(result4.error()));
auto& monsterSkills = result4->get();
  auto result5 = assets->scanExtension("monstercolors");
if (!result5) return std::unexpected(std::move(result5.error()));
auto& monsterColors = result5->get();

  auto result6 = assets->queueJsons(monsterTypes);
if (!result6) return std::unexpected(std::move(result6.error()));
  auto result7 = assets->queueJsons(monsterParts);
if (!result7) return std::unexpected(std::move(result7.error()));
  auto result8 = assets->queueJsons(monsterSkills);
if (!result8) return std::unexpected(std::move(result8.error()));
  auto result9 = assets->queueJsons(monsterColors);
if (!result9) return std::unexpected(std::move(result9.error()));

  for (auto const& assetFile : monsterTypes) {
    auto const& file = assetFile.string();
    auto loaded = [&, self = std::ref(*this)]() -> std::expected<void, StarException> {
      auto result10 = assets->json(file);
if (!result10) return std::unexpected(std::move(result10.error()));
auto config = std::move(*result10);
      auto result11 = config.getString("type");
if (!result11) return std::unexpected(std::move(result11.error()));
String typeName = std::move(*result11);

      if (self.get().m_monsterTypes.contains(typeName))
        return std::unexpected(MonsterException(strf("Repeat monster type name '{}'", typeName)));

      MonsterType& monsterType = self.get().m_monsterTypes[typeName];

      monsterType.typeName = typeName;
      auto result12 = config.optString("shortdescription");
if (!result12) return std::unexpected(std::move(result12.error()));
monsterType.shortDescription = std::move(*result12);
      auto result13 = config.optString("description");
if (!result13) return std::unexpected(std::move(result13.error()));
monsterType.description = std::move(*result13);

      auto result14 = config.get("categories");
if (!result14) return std::unexpected(std::move(result14.error()));
auto result15 = jsonToStringList(std::move(*result14));
if (!result15) return std::unexpected(std::move(result15.error()));
monsterType.categories = std::move(*result15);
      auto result16 = config.get("parts");
if (!result16) return std::unexpected(std::move(result16.error()));
auto result17 = jsonToStringList(std::move(*result16));
if (!result17) return std::unexpected(std::move(result17.error()));
monsterType.partTypes = std::move(*result17);

      auto result18 = config.getString("animation");
if (!result18) return std::unexpected(std::move(result18.error()));
monsterType.animationConfigPath = AssetPath::relativeTo(file, std::move(*result18));
      auto result19 = config.getString("colors", "default");
if (!result19) return std::unexpected(std::move(result19.error()));
monsterType.colors = std::move(*result19);

      auto result20 = config.getBool("reversed", false);
if (!result20) return std::unexpected(std::move(result20.error()));
monsterType.reversed = std::move(*result20);

      {
  auto result21 = config.contains("dropPools");
  if (!result21) return std::unexpected(std::move(result21.error()));
  if (std::move(*result21))
          {
    auto result22 = config.getArray("dropPools");
    if (!result22) return std::unexpected(std::move(result22.error()));
    monsterType.dropPools = std::move(*result22);
  }
}

      auto result23 = config.get("baseParameters", {});
if (!result23) return std::unexpected(std::move(result23.error()));
monsterType.baseParameters = std::move(*result23);

      // for updated monsters, use the partParameterDescription from the
      // .partparams file
      {
  auto result24 = config.contains("partParameters");
  if (!result24) return std::unexpected(std::move(result24.error()));
  if (std::move(*result24)) {
          auto result25 = config.getString("partParameters");
  if (!result25) return std::unexpected(std::move(result25.error()));
          auto partParametersPath = AssetPath::relativeTo(file, std::move(*result25));
          auto partParameters = [&]() -> std::expected<void, StarException> {
            auto source = assets->json(partParametersPath);
            if (!source) return std::unexpected(std::move(source.error()));
            auto description = source->getObject("partParameterDescription");
            if (!description) return std::unexpected(std::move(description.error()));
            monsterType.partParameterDescription = std::move(*description);
            auto overrides = source->getObject("partParameters");
            if (!overrides) return std::unexpected(std::move(overrides.error()));
            monsterType.partParameterOverrides = std::move(*overrides);
            return {};
          }();
          if (!partParameters)
            return std::unexpected(assets->withAssetContext(std::move(partParameters.error()), partParametersPath, "Loading monster part parameters"));
        } else {
          // outdated monsters still have partParameterDescription defined
          // directly in the
          // .monstertype file
          auto result29 = config.getObject("partParameterDescription", {});
  if (!result29) return std::unexpected(std::move(result29.error()));
  monsterType.partParameterDescription = std::move(*result29);
        }
}

      return {};
    }();
    if (!loaded)
      return std::unexpected(MonsterException(strf("Error loading monster type '{}'", file), assets->withAssetContext(std::move(loaded.error()), file, "Loading monster type")));
  }

  for (auto const& assetFile : monsterParts) {
    auto const& file = assetFile.string();
    auto loaded = [&, self = std::ref(*this)]() -> std::expected<void, StarException> {
      auto result30 = assets->json(file);
if (!result30) return std::unexpected(std::move(result30.error()));
auto config = std::move(*result30);
      if (config.isNull())
        return {};

      MonsterPart part;
      auto result31 = config.getString("name");
if (!result31) return std::unexpected(std::move(result31.error()));
part.name = std::move(*result31);
      auto result32 = config.getString("category");
if (!result32) return std::unexpected(std::move(result32.error()));
part.category = std::move(*result32);
      auto result33 = config.getString("type");
if (!result33) return std::unexpected(std::move(result33.error()));
part.type = std::move(*result33);
      part.path = AssetPath::directory(file);
      auto result34 = config.getObject("frames");
if (!result34) return std::unexpected(std::move(result34.error()));
part.frames = std::move(*result34);
      auto result35 = config.get("parameters", JsonObject());
if (!result35) return std::unexpected(std::move(result35.error()));
part.partParameters = std::move(*result35);

      auto& partMap = self.get().m_partDirectory[part.category][part.type];

      if (partMap.contains(part.name))
        return std::unexpected(MonsterException(strf("Repeat monster part name '{}' for category '{}'", part.name, part.category)));
      else
        partMap[part.name] = part;
      return {};
    }();
    if (!loaded)
      return std::unexpected(MonsterException(strf("Error loading monster part '{}'", file), assets->withAssetContext(std::move(loaded.error()), file, "Loading monster part")));
  }

  for (auto const& assetFile : monsterSkills) {
    auto const& file = assetFile.string();
    auto loaded = [&, self = std::ref(*this)]() -> std::expected<void, StarException> {
      auto result36 = assets->json(file);
if (!result36) return std::unexpected(std::move(result36.error()));
auto config = std::move(*result36);
      if (config.isNull())
        return {};

      MonsterSkill skill;
      auto result37 = config.getString("name");
if (!result37) return std::unexpected(std::move(result37.error()));
skill.name = std::move(*result37);
      auto result38 = config.getString("label");
if (!result38) return std::unexpected(std::move(result38.error()));
skill.label = std::move(*result38);
      auto result39 = config.getString("image");
if (!result39) return std::unexpected(std::move(result39.error()));
skill.image = std::move(*result39);

      auto result40 = config.get("config", JsonObject());
if (!result40) return std::unexpected(std::move(result40.error()));
skill.config = std::move(*result40);
      auto result41 = config.get("parameters", JsonObject());
if (!result41) return std::unexpected(std::move(result41.error()));
skill.parameters = std::move(*result41);
      auto result42 = config.get("animationParameters", JsonObject());
if (!result42) return std::unexpected(std::move(result42.error()));
skill.animationParameters = std::move(*result42);

      if (self.get().m_skills.contains(skill.name))
        return std::unexpected(MonsterException(strf("Repeat monster skill name '{}'", skill.name)));
      else
        self.get().m_skills[skill.name] = skill;
      return {};
    }();
    if (!loaded)
      return std::unexpected(MonsterException(strf("Error loading monster skill '{}'", file), assets->withAssetContext(std::move(loaded.error()), file, "Loading monster skill")));
  }

  for (auto const& assetFile : monsterColors) {
    auto const& file = assetFile.string();
    auto loaded = [&, self = std::ref(*this)]() -> std::expected<void, StarException> {
      auto result43 = assets->json(file);
if (!result43) return std::unexpected(std::move(result43.error()));
auto config = std::move(*result43);
      if (config.isNull())
        return {};

      auto result44 = config.getString("name");
if (!result44) return std::unexpected(std::move(result44.error()));
auto paletteName = std::move(*result44);
      if (self.get().m_colorSwaps.contains(paletteName))
        return std::unexpected(MonsterException(strf("Duplicate monster colors name '{}'", paletteName)));

      ColorReplaceMap colorSwaps;
      {
  auto result45 = config.getArray("swaps");
  if (!result45) return std::unexpected(std::move(result45.error()));
  for (auto const& swapSet : std::move(*result45)) {
          ColorReplaceMap colorSwaps;
          {
    auto result46 = swapSet.iterateObject();
    if (!result46) return std::unexpected(std::move(result46.error()));
    for (auto const& swap : std::move(*result46)) {
              auto result47 = Color::fromHex(swap.first);
    if (!result47) return std::unexpected(std::move(result47.error()));
    auto result48 = swap.second.toString();
    if (!result48) return std::unexpected(std::move(result48.error()));
    auto result49 = Color::fromHex(std::move(*result48));
    if (!result49) return std::unexpected(std::move(result49.error()));
    colorSwaps[std::move(*result47).toRgba()] = std::move(*result49).toRgba();
            }
  }
          self.get().m_colorSwaps[paletteName].append(colorSwaps);
        }
}
      return {};
    }();
    if (!loaded)
      return std::unexpected(MonsterException(strf("Error loading monster colors '{}'", file), assets->withAssetContext(std::move(loaded.error()), file, "Loading monster colors")));
  }
  return {};
}

void MonsterDatabase::cleanup() {
  std::lock_guard locker(m_cacheMutex);
  m_monsterCache.cleanup();
}

StringList MonsterDatabase::monsterTypes() const {
  return m_monsterTypes.keys();
}

std::expected<MonsterVariant, StarException> MonsterDatabase::randomMonster(String const& typeName, Json const& uniqueParameters) const {
  std::size_t seed;
  {
  auto result50 = uniqueParameters.opt("seed");
  if (!result50) return std::unexpected(std::move(result50.error()));
  if (auto seedConfig = std::move(*result50)) {
      if (seedConfig->type() == Json::Type::String) {
        auto result51 = seedConfig->toString();
  if (!result51) return std::unexpected(std::move(result51.error()));
  auto result52 = lexicalCast<std::uint64_t>(std::move(*result51));
  if (!result52) return std::unexpected(std::move(result52.error()));
  seed = std::move(*result52);
      } else {
        auto result53 = seedConfig->toUInt();
  if (!result53) return std::unexpected(std::move(result53.error()));
  seed = std::move(*result53);
      }
    } else {
      seed = m_root.random().randu64();
    }
}

  return monsterVariant(typeName, seed, uniqueParameters);
}

std::expected<MonsterVariant, StarException> MonsterDatabase::monsterVariant(String const& typeName, std::uint64_t seed, Json const& uniqueParameters) const {
  std::lock_guard locker(m_cacheMutex);
  auto variant = m_monsterCache.get(std::make_tuple(typeName, seed, uniqueParameters),
      [self = std::cref(*this)](std::tuple<String, std::uint64_t, Json> const& key) {
        return self.get().produceMonster(std::get<0>(key), std::get<1>(key), std::get<2>(key));
      });
  if (!variant)
    return std::unexpected(std::move(variant.error()));
  return variant->get();
}

std::expected<ByteArray, StarException> MonsterDatabase::writeMonsterVariant(MonsterVariant const& variant, NetCompatibilityRules rules) const {
  DataStreamBuffer ds;
  ds.setStreamCompatibilityVersion(rules);

  auto result58 = ds.write(variant.type);
if (!result58) return std::unexpected(std::move(result58.error()));

  auto result59 = ds.write(variant.seed);
if (!result59) return std::unexpected(std::move(result59.error()));

  auto result60 = ds.write(variant.uniqueParameters);
if (!result60) return std::unexpected(std::move(result60.error()));


  return ds.data();
}

std::expected<MonsterVariant, StarException> MonsterDatabase::readMonsterVariant(ByteArray const& data, NetCompatibilityRules rules) const {
  DataStreamBuffer ds(data);
  ds.setStreamCompatibilityVersion(rules);

  auto result61 = ds.read<String>();
if (!result61) return std::unexpected(std::move(result61.error()));
String type = std::move(*result61);
  auto result62 = ds.read<std::uint64_t>();
if (!result62) return std::unexpected(std::move(result62.error()));
std::uint64_t seed = std::move(*result62);
  auto result63 = ds.read<Json>();
if (!result63) return std::unexpected(std::move(result63.error()));
Json uniqueParameters = std::move(*result63);

  return monsterVariant(type, seed, uniqueParameters);
}

std::expected<Json, StarException> MonsterDatabase::writeMonsterVariantToJson(MonsterVariant const& mVar) const {
  return JsonObject{
      {"type", mVar.type},
      {"seed", mVar.seed},
      {"uniqueParameters", mVar.uniqueParameters},
  };
}

std::expected<MonsterVariant, StarException> MonsterDatabase::readMonsterVariantFromJson(Json const& variant) const {
  auto result64 = variant.getString("type");
if (!result64) return std::unexpected(std::move(result64.error()));
auto result65 = variant.getUInt("seed");
if (!result65) return std::unexpected(std::move(result65.error()));
auto result66 = variant.getObject("uniqueParameters");
if (!result66) return std::unexpected(std::move(result66.error()));
return monsterVariant(std::move(*result64), std::move(*result65), std::move(*result66));
}

std::expected<std::shared_ptr<Monster>, StarException> MonsterDatabase::createMonster(
    MonsterVariant monsterVariant, std::optional<float> level, Json uniqueParameters) const {
  if (uniqueParameters) {
    monsterVariant.uniqueParameters = jsonMerge(monsterVariant.uniqueParameters, uniqueParameters);
    monsterVariant.parameters = jsonMerge(monsterVariant.parameters, monsterVariant.uniqueParameters);
    auto result67 = readCommonParameters(monsterVariant);
if (!result67) return std::unexpected(std::move(result67.error()));

  }
  return Monster::create(m_root, monsterVariant, level);
}

std::expected<std::shared_ptr<Monster>, StarException> MonsterDatabase::diskLoadMonster(Json const& diskStore) const {
  auto attempt = [root = std::ref(m_root)](Json const& store) -> std::expected<std::shared_ptr<Monster>, StarException> {
    return Monster::create(root.get(), store);
  };
  auto monster = attempt(diskStore);
  if (monster)
    return monster;
  StarException lastError = std::move(monster.error());
  auto rebuilt = m_rebuilder->rebuild(diskStore, lastError, [&](Json const& store) -> std::expected<void, StarException> {
    auto replacement = attempt(store);
    if (!replacement) {
      lastError = std::move(replacement.error());
      return std::unexpected(lastError);
    }
    monster = std::move(replacement);
    return {};
  });
  if (!rebuilt)
    return std::unexpected(std::move(rebuilt.error()));
  if (!*rebuilt)
    return std::unexpected(std::move(lastError));
  return monster;
}

std::expected<std::shared_ptr<Monster>, StarException> MonsterDatabase::netLoadMonster(ByteArray const& netStore, NetCompatibilityRules rules) const {
  auto result68 = readMonsterVariant(netStore, rules);
if (!result68) return std::unexpected(std::move(result68.error()));
return Monster::create(m_root, std::move(*result68));
}

std::expected<List<Drawable>, StarException> MonsterDatabase::monsterPortrait(MonsterVariant const& variant) const {
  auto result69 = NetworkedAnimator::create(m_root, variant.animatorConfig);
if (!result69) return std::unexpected(std::move(result69.error()));
auto animator = std::move(*result69);
  for (auto const& pair : variant.animatorPartTags)
    animator->setPartTag(pair.first, "partImage", pair.second);
  animator->setZoom(variant.animatorZoom);
  auto result70 = variant.parameters.getString("colors", "default");
if (!result70) return std::unexpected(std::move(result70.error()));
auto result71 = this->colorSwap(std::move(*result70), variant.seed);
if (!result71) return std::unexpected(std::move(result71.error()));
auto colorSwap = variant.colorSwap.value_or(std::move(*result71));
  if (!colorSwap.empty())
    animator->setProcessingDirectives(imageOperationToString(ColorReplaceImageOperation{colorSwap}));
  auto result72 = animator->drawables();
if (!result72) return std::unexpected(std::move(result72.error()));
auto drawables = std::move(*result72);
  Drawable::scaleAll(drawables, TilePixels);
  return drawables;
}

std::expected<std::pair<String, String>, StarException> MonsterDatabase::skillInfo(String const& skillName) const {
  if (m_skills.contains(skillName)) {
    auto result73 = m_skills.get(skillName);
if (!result73) return std::unexpected(std::move(result73.error()));
auto& skill = result73->get();
    return std::make_pair(skill.label, skill.image);
  } else {
    return std::pair<String, String>{};
  }
}

std::expected<Json, StarException> MonsterDatabase::skillConfigParameter(String const& skillName, String const& configParameterName) const {
  if (m_skills.contains(skillName)) {
    auto result74 = m_skills.get(skillName);
if (!result74) return std::unexpected(std::move(result74.error()));
auto& skill = result74->get();
    return skill.config.get(configParameterName, Json());
  } else {
    return Json();
  }
}

std::expected<ColorReplaceMap, StarException> MonsterDatabase::colorSwap(String const& setName, std::uint64_t seed) const {
  if (m_colorSwaps.contains(setName))
    {
  auto result75 = m_colorSwaps.get(setName);
  if (!result75) return std::unexpected(std::move(result75.error()));
  auto result76 = staticRandomFrom(result75->get(), seed);
  if (!result76) return std::unexpected(std::move(result76.error()));
  return result76->get();
}
  else {
    auto result77 = m_root.logger().error("Monster colors '{}' not found!", setName);
if (!result77) return std::unexpected(std::move(result77.error()));
    auto result78 = m_colorSwaps.get("default");
if (!result78) return std::unexpected(std::move(result78.error()));
auto result79 = staticRandomFrom(result78->get(), seed);
if (!result79) return std::unexpected(std::move(result79.error()));
return result79->get();
  }
}

std::expected<Json, StarException> MonsterDatabase::monsterConfig(String const& typeName) const {
  auto result80 = m_monsterTypes.get(typeName);
if (!result80) return std::unexpected(std::move(result80.error()));
return result80->get().toJson();
}

std::expected<Json, StarException> MonsterDatabase::mergePartParameters(Json const& partParameterDescription, JsonArray const& parameters) {
  JsonObject mergedParameters;

  // First assign all the defaults.
  {
  auto result81 = partParameterDescription.iterateObject();
  if (!result81) return std::unexpected(std::move(result81.error()));
  for (auto const& pair : std::move(*result81))
      {
    auto result82 = pair.second.get(1);
    if (!result82) return std::unexpected(std::move(result82.error()));
    mergedParameters[pair.first] = std::move(*result82);
  }
}

  // Then go through parameter list and merge based on the merge rules.
  for (auto const& applyParams : parameters) {
    {
  auto result83 = applyParams.iterateObject();
  if (!result83) return std::unexpected(std::move(result83.error()));
  for (auto const& pair : std::move(*result83)) {
        auto result84 = partParameterDescription.get(pair.first);
  if (!result84) return std::unexpected(std::move(result84.error()));
  auto result85 = std::move(*result84).getString(0);
  if (!result85) return std::unexpected(std::move(result85.error()));
  String mergeMethod = std::move(*result85);
        auto result86 = mergedParameters.get(pair.first);
  if (!result86) return std::unexpected(std::move(result86.error()));
  Json value = result86->get();

        {
    auto result87 = mergeMethod.equalsIgnoreCase("add");
    if (!result87) return std::unexpected(std::move(result87.error()));
    if (std::move(*result87)) {
            auto result88 = value.toDouble();
    if (!result88) return std::unexpected(std::move(result88.error()));
    auto result89 = pair.second.toDouble();
    if (!result89) return std::unexpected(std::move(result89.error()));
    value = std::move(*result88) + std::move(*result89);
          } else {
      auto result90 = mergeMethod.equalsIgnoreCase("multiply");
      if (!result90) return std::unexpected(std::move(result90.error()));
      if (std::move(*result90)) {
              auto result91 = value.toDouble();
      if (!result91) return std::unexpected(std::move(result91.error()));
      auto result92 = pair.second.toDouble();
      if (!result92) return std::unexpected(std::move(result92.error()));
      value = std::move(*result91) * std::move(*result92);
            } else {
        auto result93 = mergeMethod.equalsIgnoreCase("merge");
        if (!result93) return std::unexpected(std::move(result93.error()));
        if (std::move(*result93)) {
                // "merge" means to either merge maps, or *append* lists together
                if (!pair.second.isNull()) {
                  if (value.isNull()) {
                    value = pair.second;
                  } else if (value.type() != pair.second.type()) {
                    value = pair.second;
                  } else {
                    if (pair.second.type() == Json::Type::Array) {
                      auto result94 = value.toArray();
        if (!result94) return std::unexpected(std::move(result94.error()));
        auto array = std::move(*result94);
                      auto result95 = pair.second.toArray();
        if (!result95) return std::unexpected(std::move(result95.error()));
        array.appendAll(std::move(*result95));
                      value = std::move(array);
                    } else if (pair.second.type() == Json::Type::Object) {
                      auto result96 = value.toObject();
        if (!result96) return std::unexpected(std::move(result96.error()));
        auto obj = std::move(*result96);
                      auto result97 = pair.second.toObject();
        if (!result97) return std::unexpected(std::move(result97.error()));
        obj.merge(std::move(*result97), true);
                      value = std::move(obj);
                    }
                  }
                }
              } else {
          auto result98 = mergeMethod.equalsIgnoreCase("override");
          if (!result98) return std::unexpected(std::move(result98.error()));
          if (std::move(*result98) && !pair.second.isNull()) {
                  value = pair.second;
                }
        }
      }
    }
  }

        mergedParameters[pair.first] = value;
      }
}
  }

  return mergedParameters;
}

std::expected<Json, StarException> MonsterDatabase::mergeFinalParameters(JsonArray const& parameters) {
  JsonObject mergedParameters;

  for (auto const& applyParams : parameters) {
    {
  auto result99 = applyParams.iterateObject();
  if (!result99) return std::unexpected(std::move(result99.error()));
  for (auto const& pair : std::move(*result99)) {
        Json value = mergedParameters.value(pair.first);

        // Hard-coded merge for scripts and skills parameters, otherwise merge.
        if (pair.first == "scripts" || pair.first == "skills" || pair.first == "specialSkills"
            || pair.first == "baseSkills") {
          auto result100 = value.optArray();
  if (!result100) return std::unexpected(std::move(result100.error()));
  auto array = std::move(*result100).value_or(JsonArray{});
          auto result101 = pair.second.optArray();
  if (!result101) return std::unexpected(std::move(result101.error()));
  array.appendAll(std::move(*result101).value_or(JsonArray{}));
          value = std::move(array);
        } else {
          value = jsonMerge(value, pair.second);
        }

        mergedParameters[pair.first] = value;
      }
}
  }

  return mergedParameters;
}

std::expected<void, StarException> MonsterDatabase::readCommonParameters(MonsterVariant& variant) {
  auto result102 = variant.parameters.optString("shortdescription");
if (!result102) return std::unexpected(std::move(result102.error()));
auto shortDescription = std::move(*result102);
  variant.shortDescription = shortDescription ? std::move(shortDescription) : variant.shortDescription;
  auto result103 = variant.parameters.get("dropPools", variant.dropPoolConfig);
if (!result103) return std::unexpected(std::move(result103.error()));
variant.dropPoolConfig = std::move(*result103);
  auto result104 = variant.parameters.get("scripts");
if (!result104) return std::unexpected(std::move(result104.error()));
auto result105 = jsonToStringList(std::move(*result104));
if (!result105) return std::unexpected(std::move(result105.error()));
variant.scripts = std::move(*result105);
  auto result106 = variant.parameters.getArray("animationScripts", {});
if (!result106) return std::unexpected(std::move(result106.error()));
auto result107 = jsonToStringList(std::move(*result106));
if (!result107) return std::unexpected(std::move(result107.error()));
variant.animationScripts = std::move(*result107);
  auto result108 = variant.parameters.get("animationCustom", JsonObject());
if (!result108) return std::unexpected(std::move(result108.error()));
variant.animatorConfig = jsonMerge(variant.animatorConfig, std::move(*result108));
  auto result109 = variant.parameters.getUInt("initialScriptDelta", 5);
if (!result109) return std::unexpected(std::move(result109.error()));
variant.initialScriptDelta = std::move(*result109);
  auto result110 = variant.parameters.get("metaBoundBox");
if (!result110) return std::unexpected(std::move(result110.error()));
auto result111 = jsonToRectF(std::move(*result110));
if (!result111) return std::unexpected(std::move(result111.error()));
variant.metaBoundBox = std::move(*result111);
  auto renderLayerName = variant.parameters.optString("renderLayer");
  if (!renderLayerName) return std::unexpected(std::move(renderLayerName.error()));
  variant.renderLayer = RenderLayerMonster;
  if (*renderLayerName) {
    auto layer = parseRenderLayer(**renderLayerName);
    if (!layer) return std::unexpected(std::move(layer.error()));
    variant.renderLayer = *layer;
  }
  auto result112 = variant.parameters.getFloat("scale");
if (!result112) return std::unexpected(std::move(result112.error()));
variant.scale = std::move(*result112);
  auto result113 = variant.parameters.get("movementSettings", {});
if (!result113) return std::unexpected(std::move(result113.error()));
auto result114 = ActorMovementParameters::create(std::move(*result113));
if (!result114) return std::unexpected(std::move(result114.error()));
variant.movementSettings = std::move(*result114);
  auto result115 = variant.parameters.getFloat("walkMultiplier", 1.0f);
if (!result115) return std::unexpected(std::move(result115.error()));
variant.walkMultiplier = std::move(*result115);
  auto result116 = variant.parameters.getFloat("runMultiplier", 1.0f);
if (!result116) return std::unexpected(std::move(result116.error()));
variant.runMultiplier = std::move(*result116);
  auto result117 = variant.parameters.getFloat("jumpMultiplier", 1.0f);
if (!result117) return std::unexpected(std::move(result117.error()));
variant.jumpMultiplier = std::move(*result117);
  auto result118 = variant.parameters.getFloat("weightMultiplier", 1.0f);
if (!result118) return std::unexpected(std::move(result118.error()));
variant.weightMultiplier = std::move(*result118);
  auto result119 = variant.parameters.getFloat("healthMultiplier", 1.0f);
if (!result119) return std::unexpected(std::move(result119.error()));
variant.healthMultiplier = std::move(*result119);
  auto result120 = variant.parameters.getFloat("touchDamageMultiplier", 1.0f);
if (!result120) return std::unexpected(std::move(result120.error()));
variant.touchDamageMultiplier = std::move(*result120);
  auto result121 = variant.parameters.get("touchDamage", {});
if (!result121) return std::unexpected(std::move(result121.error()));
variant.touchDamageConfig = std::move(*result121);
  auto result122 = variant.parameters.getObject("animationDamageParts", {});
if (!result122) return std::unexpected(std::move(result122.error()));
variant.animationDamageParts = std::move(*result122);
  auto result123 = variant.parameters.get("statusSettings");
if (!result123) return std::unexpected(std::move(result123.error()));
variant.statusSettings = std::move(*result123);
  auto result124 = variant.parameters.get("mouthOffset");
if (!result124) return std::unexpected(std::move(result124.error()));
auto result125 = jsonToVec2F(std::move(*result124));
if (!result125) return std::unexpected(std::move(result125.error()));
variant.mouthOffset = std::move(*result125) / TilePixels;
  auto result126 = variant.parameters.get("feetOffset");
if (!result126) return std::unexpected(std::move(result126.error()));
auto result127 = jsonToVec2F(std::move(*result126));
if (!result127) return std::unexpected(std::move(result127.error()));
variant.feetOffset = std::move(*result127) / TilePixels;
  auto result128 = variant.parameters.getString("powerLevelFunction", "monsterLevelPowerMultiplier");
if (!result128) return std::unexpected(std::move(result128.error()));
variant.powerLevelFunction = std::move(*result128);
  auto result129 = variant.parameters.getString("healthLevelFunction", "monsterLevelHealthMultiplier");
if (!result129) return std::unexpected(std::move(result129.error()));
variant.healthLevelFunction = std::move(*result129);
  auto result130 = variant.parameters.getString("clientEntityMode", "ClientSlaveOnly");
if (!result130) return std::unexpected(std::move(result130.error()));
if (!ClientEntityModeNames) return std::unexpected(ClientEntityModeNames.error());
auto result131 = ClientEntityModeNames->getLeft(std::move(*result130));
if (!result131) return std::unexpected(std::move(result131.error()));
variant.clientEntityMode = result131->get();
  auto result132 = variant.parameters.getBool("persistent", false);
if (!result132) return std::unexpected(std::move(result132.error()));
variant.persistent = std::move(*result132);
  auto result133 = variant.parameters.getString("damageTeamType", "enemy");
if (!result133) return std::unexpected(std::move(result133.error()));
if (!TeamTypeNames) return std::unexpected(TeamTypeNames.error());
auto result134 = TeamTypeNames->getLeft(std::move(*result133));
if (!result134) return std::unexpected(std::move(result134.error()));
variant.damageTeamType = result134->get();
  auto result135 = variant.parameters.getUInt("damageTeam", 2);
if (!result135) return std::unexpected(std::move(result135.error()));
variant.damageTeam = std::move(*result135);

  {
  auto result136 = variant.parameters.get("selfDamagePoly", {});
  if (!result136) return std::unexpected(std::move(result136.error()));
  if (auto sdp = std::move(*result136))
      {
    auto result137 = jsonToPolyF(sdp);
    if (!result137) return std::unexpected(std::move(result137.error()));
    variant.selfDamagePoly = std::move(*result137);
  }
    else {
      if (!variant.movementSettings.standingPoly)
        return std::unexpected(MonsterException("Monster has no standing polygon"));
      variant.selfDamagePoly = *variant.movementSettings.standingPoly;
    }
}

  auto result138 = variant.parameters.optString("portraitIcon");
if (!result138) return std::unexpected(std::move(result138.error()));
variant.portraitIcon = std::move(*result138);
  auto result139 = variant.parameters.getFloat("damageReceivedAggressiveDuration", 1.0f);
if (!result139) return std::unexpected(std::move(result139.error()));
variant.damageReceivedAggressiveDuration = std::move(*result139);
  auto result140 = variant.parameters.getFloat("onDamagedOthersAggressiveDuration", 5.0f);
if (!result140) return std::unexpected(std::move(result140.error()));
variant.onDamagedOthersAggressiveDuration = std::move(*result140);
  auto result141 = variant.parameters.getFloat("onFireAggressiveDuration", 5.0f);
if (!result141) return std::unexpected(std::move(result141.error()));
variant.onFireAggressiveDuration = std::move(*result141);

  auto result142 = variant.parameters.get("nametagColor", JsonArray{255, 255, 255});
if (!result142) return std::unexpected(std::move(result142.error()));
auto result143 = jsonToVec3B(std::move(*result142));
if (!result143) return std::unexpected(std::move(result143.error()));
variant.nametagColor = std::move(*result143);

  auto colorSwaps = variant.parameters.optObject("colorSwap");
  if (!colorSwaps) return std::unexpected(std::move(colorSwaps.error()));
  if (*colorSwaps) {
    ColorReplaceMap swaps;
    for (auto const& pair : **colorSwaps) {
      auto from = Color::fromHex(pair.first);
      if (!from) return std::unexpected(std::move(from.error()));
      auto text = pair.second.toString();
      if (!text) return std::unexpected(std::move(text.error()));
      auto to = Color::fromHex(*text);
      if (!to) return std::unexpected(std::move(to.error()));
      swaps.insert(from->toRgba(), to->toRgba());
    }
    variant.colorSwap = std::move(swaps);
  }
  return {};
}

std::expected<MonsterVariant, StarException> MonsterDatabase::produceMonster(String const& typeName, std::uint64_t seed, Json const& uniqueParameters) const {
  RandomSource rand = RandomSource(seed);

  auto result144 = m_monsterTypes.get(typeName);
if (!result144) return std::unexpected(std::move(result144.error()));
auto const& monsterType = result144->get();

  MonsterVariant monsterVariant;
  monsterVariant.type = typeName;
  monsterVariant.shortDescription = monsterType.shortDescription;
  monsterVariant.description = monsterType.description;
  monsterVariant.seed = seed;
  monsterVariant.uniqueParameters = uniqueParameters;

  auto result145 = m_root.assets();
if (!result145) return std::unexpected(std::move(result145.error()));
auto result146 = (*result145)->fetchJson(monsterType.animationConfigPath);
if (!result146) return std::unexpected((*result145)->withAssetContext(std::move(result146.error()), monsterType.animationConfigPath, "Loading monster animation"));
monsterVariant.animatorConfig = std::move(*result146);
  monsterVariant.reversed = monsterType.reversed;

  // select a list of monster parts
  List<MonsterPart> monsterParts;
  auto result147 = rand.randFrom(monsterType.categories);
if (!result147) return std::unexpected(std::move(result147.error()));
auto const& categoryName = result147->get();

  for (auto const& partTypeName : monsterType.partTypes) {
    auto result148 = m_partDirectory.get(categoryName);
if (!result148) return std::unexpected(std::move(result148.error()));
auto result149 = result148->get().get(partTypeName);
if (!result149) return std::unexpected(std::move(result149.error()));
auto result150 = rand.randFrom(result149->get());
if (!result150) return std::unexpected(std::move(result150.error()));
auto const& randPart = result150->get().second;
    auto result151 = uniqueParameters.get("selectedParts", JsonObject());
if (!result151) return std::unexpected(std::move(result151.error()));
auto result152 = std::move(*result151).optString(partTypeName);
if (!result152) return std::unexpected(std::move(result152.error()));
auto selectedPart = std::move(*result152);
    if (selectedPart)
      {
  auto result153 = m_partDirectory.get(categoryName);
  if (!result153) return std::unexpected(std::move(result153.error()));
  auto result154 = result153->get().get(partTypeName);
  if (!result154) return std::unexpected(std::move(result154.error()));
  auto result155 = result154->get().get(*selectedPart);
  if (!result155) return std::unexpected(std::move(result155.error()));
  monsterParts.append(result155->get());
}
    else
      monsterParts.append(randPart);
  }

  for (auto const& partConfig : monsterParts) {
    for (auto const& pair : partConfig.frames)
      {
  auto result156 = pair.second.toString();
  if (!result156) return std::unexpected(std::move(result156.error()));
  monsterVariant.animatorPartTags[pair.first] = AssetPath::relativeTo(partConfig.path, std::move(*result156));
}
  }
  JsonArray partParameterList;
  for (auto const& partConfig : monsterParts) {
    partParameterList.append(partConfig.partParameters);
    // Include part parameter overrides
    {
  bool condition158 = static_cast<bool>(!monsterType.partParameterOverrides.isNull());
  if (condition158) {
    auto result157 = monsterType.partParameterOverrides.contains(partConfig.name);
    if (!result157) return std::unexpected(std::move(result157.error()));
    condition158 = static_cast<bool>(std::move(*result157));
  }
  if (condition158)
        {
    auto result159 = monsterType.partParameterOverrides.getObject(partConfig.name);
    if (!result159) return std::unexpected(std::move(result159.error()));
    partParameterList.append(std::move(*result159));
  }
}
  }

  // merge part parameters and unique parameters into base parameters
  Json baseParameters = monsterType.baseParameters;
  auto result160 = mergePartParameters(monsterType.partParameterDescription, partParameterList);
if (!result160) return std::unexpected(std::move(result160.error()));
Json mergedPartParameters = std::move(*result160);
  auto result161 = mergeFinalParameters({baseParameters, mergedPartParameters});
if (!result161) return std::unexpected(std::move(result161.error()));
monsterVariant.parameters = std::move(*result161);
  monsterVariant.parameters = jsonMerge(monsterVariant.parameters, uniqueParameters);

  auto result162 = chooseSkills(monsterVariant.parameters, monsterVariant.animatorConfig, rand);
if (!result162) return std::unexpected(std::move(result162.error()));
std::tie(monsterVariant.parameters, monsterVariant.animatorConfig) = std::move(*result162);
  monsterVariant.animatorZoom = 1.0f;
  monsterVariant.dropPoolConfig = monsterType.dropPools;

  auto result163 = readCommonParameters(monsterVariant);
if (!result163) return std::unexpected(std::move(result163.error()));

  monsterVariant.animatorZoom = monsterVariant.scale;
  if (monsterVariant.dropPoolConfig.isType(Json::Type::Array))
    {
  auto result164 = monsterVariant.dropPoolConfig.toArray();
  if (!result164) return std::unexpected(std::move(result164.error()));
  monsterVariant.dropPoolConfig = rand.randValueFrom(std::move(*result164));
}

  return monsterVariant;
}

std::expected<std::pair<Json, Json>, StarException> MonsterDatabase::chooseSkills(
    Json const& parameters, Json const& animatorConfig, RandomSource& rand) const {
  // Pick a subset of skills, then merge in any params from those skills
  {
  auto result165 = parameters.contains("baseSkills");
  if (!result165) return std::unexpected(std::move(result165.error()));
  bool condition167 = static_cast<bool>(std::move(*result165));
  if (!condition167) {
    auto result166 = parameters.contains("specialSkills");
    if (!result166) return std::unexpected(std::move(result166.error()));
    condition167 = static_cast<bool>(std::move(*result166));
  }
  if (condition167) {
      auto result168 = parameters.getUInt("skillCount", 2);
  if (!result168) return std::unexpected(std::move(result168.error()));
  auto skillCount = std::move(*result168);

      auto result169 = parameters.get("baseSkills");
  if (!result169) return std::unexpected(std::move(result169.error()));
  auto result170 = jsonToStringList(std::move(*result169));
  if (!result170) return std::unexpected(std::move(result170.error()));
  auto baseSkillNames = std::move(*result170);
      auto result171 = parameters.get("specialSkills");
  if (!result171) return std::unexpected(std::move(result171.error()));
  auto result172 = jsonToStringList(std::move(*result171));
  if (!result172) return std::unexpected(std::move(result172.error()));
  auto specialSkillNames = std::move(*result172);

      List<String> skillNames;

      // First, pick from base skills
      while (!baseSkillNames.empty() && skillNames.size() < skillCount)
        {
    auto result174 = baseSkillNames.takeAt(rand.randInt(baseSkillNames.size() - 1));
    if (!result174) return std::unexpected(std::move(result174.error()));
    skillNames.append(std::move(*result174));
  }

      // ...then fill in from special skills as needed
      while (!specialSkillNames.empty() && skillNames.size() < skillCount)
        {
    auto result176 = specialSkillNames.takeAt(rand.randInt(specialSkillNames.size() - 1));
    if (!result176) return std::unexpected(std::move(result176.error()));
    skillNames.append(std::move(*result176));
  }

      Json finalAnimatorConfig = animatorConfig;
      JsonArray allParameters({parameters});
      for (auto& skillName : skillNames) {
        if (m_skills.contains(skillName)) {
          auto result177 = m_skills.get(skillName);
  if (!result177) return std::unexpected(std::move(result177.error()));
  auto const& skill = result177->get();
          allParameters.append(skill.parameters);
          finalAnimatorConfig = jsonMerge(finalAnimatorConfig, skill.animationParameters);
        }
      }

      // Need to override the final list of skills, instead of merging the lists
      auto result178 = mergeFinalParameters(allParameters);
  if (!result178) return std::unexpected(std::move(result178.error()));
  auto result179 = std::move(*result178).set("skills", jsonFromStringList(skillNames));
  if (!result179) return std::unexpected(std::move(result179.error()));
  Json finalParameters = std::move(*result179);

      return std::make_pair(finalParameters, finalAnimatorConfig);
    } else {
    auto result180 = parameters.contains("skills");
    if (!result180) return std::unexpected(std::move(result180.error()));
    if (std::move(*result180)) {
        auto result181 = parameters.get("skills");
    if (!result181) return std::unexpected(std::move(result181.error()));
    auto result182 = jsonToStringList(std::move(*result181));
    if (!result182) return std::unexpected(std::move(result182.error()));
    auto availableSkillNames = std::move(*result182);
        auto result183 = parameters.getUInt("skillCount", 2);
    if (!result183) return std::unexpected(std::move(result183.error()));
    auto skillCount = std::min<std::size_t>(std::move(*result183), availableSkillNames.size());

        List<String> skillNames;
        for (std::size_t i = 0; i < skillCount; ++i)
          {
      auto result185 = availableSkillNames.takeAt(rand.randInt(availableSkillNames.size() - 1));
      if (!result185) return std::unexpected(std::move(result185.error()));
      skillNames.append(std::move(*result185));
    }

        Json finalAnimatorConfig = animatorConfig;
        JsonArray allParameters({parameters});
        for (auto& skillName : skillNames) {
          if (m_skills.contains(skillName)) {
            auto result186 = m_skills.get(skillName);
    if (!result186) return std::unexpected(std::move(result186.error()));
    auto const& skill = result186->get();
            allParameters.append(skill.parameters);
            finalAnimatorConfig = jsonMerge(finalAnimatorConfig, skill.animationParameters);
          }
        }

        // Need to override the final list of skills, instead of merging the lists
        auto result187 = mergeFinalParameters(allParameters);
    if (!result187) return std::unexpected(std::move(result187.error()));
    auto result188 = std::move(*result187).set("skills", jsonFromStringList(skillNames));
    if (!result188) return std::unexpected(std::move(result188.error()));
    Json finalParameters = std::move(*result188);

        return std::make_pair(finalParameters, finalAnimatorConfig);
      } else {
        return std::make_pair(parameters, animatorConfig);
      }
  }
}
}

Json MonsterDatabase::MonsterType::toJson() const {
  return JsonObject{
    {"type", typeName},
    {"shortdescription", shortDescription.value_or(String{})},
    {"description", description.value_or(String{})},
    {"categories", jsonFromStringList(categories)},
    {"parts", jsonFromStringList(partTypes)},
    {"animation", animationConfigPath},
    {"colors", colors},
    {"reversed", reversed},
    {"dropPools", dropPools},
    {"baseParameters", baseParameters},
    {"partParameters", partParameterOverrides},
    {"partParameterDescription", partParameterDescription}
  };
}
}
