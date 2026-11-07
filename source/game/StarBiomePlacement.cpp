import star.json;
import star.json_extra;
import star.bi_map;
import star.random;
import star.perlin;
import star.weighted_pool;
import star.id_map;
import star.set;
import star.casting;
import star.data_stream;
import star.game_types;
import star.list;
import star.vector;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.strong_typedef;
import star.logging;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.listener;
import std;
import star.optional;
import star.string;
import star.format;
import star.version;

import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.interaction_types;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_damage;
import star.inspectable_entity;
import star.plant;
import star.plant_database;
import star.biome_placement;
import star.asset_source;
import star.assets;

import star.configuration;
import star.root;


import star.static_random;
import star.math_common;
import star.algorithm;
namespace Star {

std::expected<BiomeItem, StarException> variantToBiomeItem(Assets const& assets, Json const& store) {
  auto type = store.get(0);
  if (!type) return std::unexpected(std::move(type.error()));
  auto value = store.get(1);
  if (!value) return std::unexpected(std::move(value.error()));
  if (*type == "grass") {
    auto variant = GrassVariant::create(assets, *value);
    if (!variant) return std::unexpected(std::move(variant.error()));
    return BiomeItem{std::move(*variant)};
  } else if (*type == "bush") {
    auto variant = BushVariant::create(assets, *value);
    if (!variant) return std::unexpected(std::move(variant.error()));
    return BiomeItem{std::move(*variant)};
  } else if (*type == "treePair") {
    auto firstConfig = value->get(0);
    if (!firstConfig) return std::unexpected(std::move(firstConfig.error()));
    auto first = TreeVariant::create(assets, *firstConfig);
    if (!first) return std::unexpected(std::move(first.error()));
    auto secondConfig = value->get(1);
    if (!secondConfig) return std::unexpected(std::move(secondConfig.error()));
    auto second = TreeVariant::create(assets, *secondConfig);
    if (!second) return std::unexpected(std::move(second.error()));
    return BiomeItem{TreePair{std::move(*first), std::move(*second)}};
  } else if (*type == "objectPool") {
    ObjectPool pool;
    auto pairs = value->toArray();
    if (!pairs) return std::unexpected(std::move(pairs.error()));
    for (auto const& pair : *pairs) {
      auto weight = pair.getFloat(0);
      if (!weight) return std::unexpected(std::move(weight.error()));
      auto object = pair.get(1);
      if (!object) return std::unexpected(std::move(object.error()));
      auto name = object->getString(0);
      if (!name) return std::unexpected(std::move(name.error()));
      auto parameters = object->get(1);
      if (!parameters) return std::unexpected(std::move(parameters.error()));
      pool.add(*weight, {std::move(*name), std::move(*parameters)});
    }
    return BiomeItem{std::move(pool)};
  } else if (*type == "treasureBoxSet") {
    auto name = value->toString();
    if (!name) return std::unexpected(std::move(name.error()));
    return BiomeItem{TreasureBoxSet(std::move(*name))};
  } else if (*type == "microDungeon") {
    auto names = jsonToStringSet(*value);
    if (!names) return std::unexpected(std::move(names.error()));
    return BiomeItem{MicroDungeonNames(std::move(*names))};
  }
  auto text = type->repr();
  if (!text) return std::unexpected(std::move(text.error()));
  return std::unexpected(BiomeException(strf("Unrecognized biome item type '{}'", *text)));
}

std::expected<Json, StarException> variantFromBiomeItem(BiomeItem const& item) {
  return std::visit([](auto const& value) -> std::expected<Json, StarException> {
    using Value = std::remove_cvref_t<decltype(value)>;
    if constexpr (std::is_same_v<Value, GrassVariant>) {
      auto json = value.toJson();
      if (!json) return std::unexpected(std::move(json.error()));
      return JsonArray{"grass", std::move(*json)};
    } else if constexpr (std::is_same_v<Value, BushVariant>) {
      auto json = value.toJson();
      if (!json) return std::unexpected(std::move(json.error()));
      return JsonArray{"bush", std::move(*json)};
    } else if constexpr (std::is_same_v<Value, TreePair>) {
      auto first = value.first.toJson();
      if (!first) return std::unexpected(std::move(first.error()));
      auto second = value.second.toJson();
      if (!second) return std::unexpected(std::move(second.error()));
      return JsonArray{"treePair", JsonArray{std::move(*first), std::move(*second)}};
    } else if constexpr (std::is_same_v<Value, ObjectPool>) {
      return JsonArray{"objectPool", transform<JsonArray>(value.items(), [](auto const& p) { return JsonArray{p.first, JsonArray{p.second.first, p.second.second}}; })};
    } else if constexpr (std::is_same_v<Value, TreasureBoxSet>) {
      return JsonArray{"treasureBoxSet", String(value)};
    } else if constexpr (std::is_same_v<Value, MicroDungeonNames>) {
      return JsonArray{"microDungeon", jsonFromStringSet(value)};
    } else {
      return std::unexpected(BiomeException("Unrecognized biome item type"));
    }
  }, item);
}


BiomeItemPlacement::BiomeItemPlacement(BiomeItem item, Vec2I position, float priority)
  : item(std::move(item)), position(position), priority(priority) {}

bool BiomeItemPlacement::operator<(BiomeItemPlacement const& rhs) const {
  return priority < rhs.priority;
}

std::expected<std::optional<BiomeItem>, StarException> BiomeItemDistribution::createItem(Root& root, Json const& config, RandomSource& rand, float biomeHueShift) {
  auto type = config.getString("type");
  if (!type) return std::unexpected(std::move(type.error()));
  if (auto match = type->equalsIgnoreCase("grass"); !match) return std::unexpected(std::move(match.error()));
  else if (*match) {
    auto grassConfig = config.get("grasses");
    if (!grassConfig) return std::unexpected(std::move(grassConfig.error()));
    auto grasses = jsonToStringList(*grassConfig);
    if (!grasses) return std::unexpected(std::move(grasses.error()));
    auto plants = root.plantDatabase();
    if (!plants) return std::unexpected(std::move(plants.error()));
    auto grass = rand.randFrom(*grasses);
    if (!grass) return std::unexpected(std::move(grass.error()));
    auto variant = (*plants)->buildGrassVariant(grass->get(), biomeHueShift);
    if (!variant) return std::unexpected(std::move(variant.error()));
    return BiomeItem{std::move(*variant)};
  } else if (auto match = type->equalsIgnoreCase("bush"); !match) return std::unexpected(std::move(match.error()));
  else if (*match) {
    auto bushes = config.getArray("bushes", {});
    if (!bushes) return std::unexpected(std::move(bushes.error()));
    auto settings = rand.randValueFrom(*bushes);
    auto name = settings.getString("name");
    if (!name) return std::unexpected(std::move(name.error()));
    auto plants = root.plantDatabase();
    if (!plants) return std::unexpected(std::move(plants.error()));
    auto mods = (*plants)->bushMods(*name);
    if (!mods) return std::unexpected(std::move(mods.error()));
    auto mod = rand.randValueFrom(*mods);
    auto baseRandom = rand.randf(-1.0f, 1.0f);
    if (!baseRandom) return std::unexpected(std::move(baseRandom.error()));
    auto baseMax = settings.getFloat("baseHueShiftMax");
    if (!baseMax) return std::unexpected(std::move(baseMax.error()));
    float baseHue = *baseRandom * *baseMax;
    auto modRandom = rand.randf(-1.0f, 1.0f);
    if (!modRandom) return std::unexpected(std::move(modRandom.error()));
    auto modMax = settings.getFloat("modHueShiftMax");
    if (!modMax) return std::unexpected(std::move(modMax.error()));
    float modHue = *modRandom * *modMax;
    auto variant = (*plants)->buildBushVariant(*name, baseHue, mod, modHue);
    if (!variant) return std::unexpected(std::move(variant.error()));
    return BiomeItem{std::move(*variant)};
  } else if (auto match = type->equalsIgnoreCase("tree"); !match) return std::unexpected(std::move(match.error()));
  else if (*match) {
    auto stemConfig = config.get("treeStemList", JsonArray());
    if (!stemConfig) return std::unexpected(std::move(stemConfig.error()));
    auto stems = jsonToStringList(*stemConfig);
    if (!stems) return std::unexpected(std::move(stems.error()));
    auto foliageConfig = config.get("treeFoliageList", JsonArray());
    if (!foliageConfig) return std::unexpected(std::move(foliageConfig.error()));
    auto foliages = jsonToStringList(*foliageConfig);
    if (!foliages) return std::unexpected(std::move(foliages.error()));
    List<std::pair<String, String>> matchingPairs;
    for (auto const& stem : *stems) {
      for (auto const& foliage : *foliages) {
        bool matches = foliage.empty();
        if (!matches) {
          auto plants = root.plantDatabase();
          if (!plants) return std::unexpected(std::move(plants.error()));
          auto stemShape = (*plants)->treeStemShape(stem);
          if (!stemShape) return std::unexpected(std::move(stemShape.error()));
          auto foliageShape = (*plants)->treeFoliageShape(foliage);
          if (!foliageShape) return std::unexpected(std::move(foliageShape.error()));
          matches = *stemShape == *foliageShape;
        }
        if (matches) matchingPairs.append({stem, foliage});
      }
    }
    if (matchingPairs.empty() && !stems->empty() && !foliages->empty()) {
      auto logged = root.logger().warn("Specified stemList and foliageList, but no matching pairs found.");
      if (!logged) return std::unexpected(std::move(logged.error()));
    }
    auto pair = rand.randValueFrom(matchingPairs);
    auto stemHueRandom = rand.randf(-1.0f, 1.0f);
    if (!stemHueRandom) return std::unexpected(std::move(stemHueRandom.error()));
    auto stemHueMax = config.getFloat("treeStemHueShiftMax", 0);
    if (!stemHueMax) return std::unexpected(std::move(stemHueMax.error()));
    float stemHue = *stemHueRandom * *stemHueMax;
    auto foliageHueRandom = rand.randf(-1.0f, 1.0f);
    if (!foliageHueRandom) return std::unexpected(std::move(foliageHueRandom.error()));
    auto foliageHueMax = config.getFloat("treeFoliageHueShiftMax", 0);
    if (!foliageHueMax) return std::unexpected(std::move(foliageHueMax.error()));
    float foliageHue = *foliageHueRandom * *foliageHueMax;
    auto altHueRandom = rand.randf(-1.0f, 1.0f);
    if (!altHueRandom) return std::unexpected(std::move(altHueRandom.error()));
    auto altHueMax = config.getFloat("treeFoliageHueShiftMax", 0);
    if (!altHueMax) return std::unexpected(std::move(altHueMax.error()));
    float altHue = *altHueRandom * *altHueMax;
    if (!pair.first.empty()) {
      auto plants = root.plantDatabase();
      if (!plants) return std::unexpected(std::move(plants.error()));
      TreeVariant primary;
      TreeVariant alternate;
      if (pair.second.empty()) {
        {
          auto value = (*plants)->buildTreeVariant(pair.first, stemHue);
          if (!value) return std::unexpected(std::move(value.error()));
          primary = std::move(*value);
        }
        {
          auto value = (*plants)->buildTreeVariant(pair.first, stemHue);
          if (!value) return std::unexpected(std::move(value.error()));
          alternate = std::move(*value);
        }
      } else {
        {
          auto value = (*plants)->buildTreeVariant(pair.first, stemHue, pair.second, foliageHue);
          if (!value) return std::unexpected(std::move(value.error()));
          primary = std::move(*value);
        }
        {
          auto value = (*plants)->buildTreeVariant(pair.first, stemHue, pair.second, altHue);
          if (!value) return std::unexpected(std::move(value.error()));
          alternate = std::move(*value);
        }
      }
      return BiomeItem{TreePair{std::move(primary), std::move(alternate)}};
    }
  } else if (auto match = type->equalsIgnoreCase("object"); !match) return std::unexpected(std::move(match.error()));
  else if (*match) {
    auto sets = config.getArray("objectSets");
    if (!sets) return std::unexpected(std::move(sets.error()));
    auto poolConfig = rand.randValueFrom(*sets);
    ObjectPool pool;
    auto parameters = poolConfig.get("parameters", JsonObject());
    if (!parameters) return std::unexpected(std::move(parameters.error()));
    auto pairs = poolConfig.getArray("pool");
    if (!pairs) return std::unexpected(std::move(pairs.error()));
    for (auto const& pair : *pairs) {
      auto size = pair.size();
      if (!size) return std::unexpected(std::move(size.error()));
      if (*size != 2) return std::unexpected(BiomeException("Wrong size for objects weight / list pair in biome items"));
      auto weight = pair.getFloat(0);
      if (!weight) return std::unexpected(std::move(weight.error()));
      auto name = pair.getString(1);
      if (!name) return std::unexpected(std::move(name.error()));
      pool.add(*weight, {std::move(*name), *parameters});
    }
    return BiomeItem{std::move(pool)};
  } else if (auto match = type->equalsIgnoreCase("treasureBox"); !match) return std::unexpected(std::move(match.error()));
  else if (*match) {
    auto sets = config.getArray("treasureBoxSets");
    if (!sets) return std::unexpected(std::move(sets.error()));
    auto name = rand.randValueFrom(*sets).toString();
    if (!name) return std::unexpected(std::move(name.error()));
    return BiomeItem{TreasureBoxSet(std::move(*name))};
  } else if (auto match = type->equalsIgnoreCase("microdungeon"); !match) return std::unexpected(std::move(match.error()));
  else if (*match) {
    auto namesConfig = config.get("microdungeons", JsonArray());
    if (!namesConfig) return std::unexpected(std::move(namesConfig.error()));
    auto names = jsonToStringSet(*namesConfig);
    if (!names) return std::unexpected(std::move(names.error()));
    return BiomeItem{MicroDungeonNames(std::move(*names))};
  } else {
    return std::unexpected(BiomeException(strf("No such item type '{}' in item distribution", *type)));
  }
  return std::optional<BiomeItem>();
}

BiomeItemDistribution::BiomeItemDistribution() {
  m_mode = BiomePlacementMode::Floor;
  m_distribution = DistributionType::Random;
  m_modulus = 1;
  m_modulusOffset = 0;
  m_blockSeed = 0;
  m_blockProbability = 0.0f;
  m_priority = 0.0f;
}

std::expected<BiomeItemDistribution, StarException> BiomeItemDistribution::create(Root& root, Json const& config, std::uint64_t seed, float biomeHueShift) {
  BiomeItemDistribution result;
  RandomSource rand(seed);
  auto modeName = config.getString("mode", "floor");
  if (!modeName) return std::unexpected(std::move(modeName.error()));
  auto mode = enumValue<BiomePlacementModeNames>(*modeName);
  if (!mode) return std::unexpected(std::move(mode.error()));
  result.m_mode = *mode;
  {
    auto value = config.getFloat("priority", 0.0f);
    if (!value) return std::unexpected(std::move(value.error()));
    result.m_priority = std::move(*value);
  }
  auto variants = config.getInt("variants", 1);
  if (!variants) return std::unexpected(std::move(variants.error()));
  auto settings = config.get("distribution", JsonObject());
  if (!settings) return std::unexpected(std::move(settings.error()));
  if (settings->type() == Json::Type::String) {
    auto assets = root.assets();
    if (!assets) return std::unexpected(std::move(assets.error()));
    auto path = settings->toString();
    if (!path) return std::unexpected(std::move(path.error()));
    {
      auto value = (*assets)->json(*path);
      if (!value) return std::unexpected(std::move(value.error()));
      *settings = std::move(*value);
    }
  }
  auto typeName = settings->getString("type");
  if (!typeName) return std::unexpected(std::move(typeName.error()));
  auto type = enumValue<DistributionTypeNames>(*typeName);
  if (!type) return std::unexpected(std::move(type.error()));
  result.m_distribution = *type;
  if (result.m_distribution == DistributionType::Random) {
    {
      auto value = settings->getFloat("blockProbability");
      if (!value) return std::unexpected(std::move(value.error()));
      result.m_blockProbability = std::move(*value);
    }
    result.m_blockSeed = rand.randu64();
    for (int i = 0; i < *variants; ++i) {
      auto item = createItem(root, config, rand, biomeHueShift);
      if (!item) return std::unexpected(std::move(item.error()));
      if (*item) result.m_randomItems.append(std::move(**item));
    }
  } else if (result.m_distribution == DistributionType::Periodic) {
    auto octaves = settings->getInt("octaves", 1);
    if (!octaves) return std::unexpected(std::move(octaves.error()));
    auto alpha = settings->getFloat("alpha", 2.0);
    if (!alpha) return std::unexpected(std::move(alpha.error()));
    auto beta = settings->getFloat("beta", 2.0);
    if (!beta) return std::unexpected(std::move(beta.error()));
    auto modulusVariance = settings->getFloat("modulusVariance", 0.0);
    if (!modulusVariance) return std::unexpected(std::move(modulusVariance.error()));
    auto densityPeriod = settings->getFloat("densityPeriod", 10);
    if (!densityPeriod) return std::unexpected(std::move(densityPeriod.error()));
    auto densityOffset = settings->getFloat("densityOffset", 2.0);
    if (!densityOffset) return std::unexpected(std::move(densityOffset.error()));
    auto typePeriod = settings->getFloat("typePeriod", 10);
    if (!typePeriod) return std::unexpected(std::move(typePeriod.error()));
    {
      auto value = settings->getInt("modulus", 1);
      if (!value) return std::unexpected(std::move(value.error()));
      result.m_modulus = std::move(*value);
    }
    {
      auto value = rand.randInt(-result.m_modulus, result.m_modulus);
      if (!value) return std::unexpected(std::move(value.error()));
      result.m_modulusOffset = std::move(*value);
    }
    result.m_densityFunction = PerlinF(static_cast<unsigned>(*octaves), 1.0f / *densityPeriod, 1.0, *densityOffset,
                                       *alpha, *beta, rand.randu64());
    result.m_modulusDistortion = PerlinF(static_cast<unsigned>(*octaves), 1.0f / result.m_modulus, *modulusVariance,
                                         *modulusVariance * 2, *alpha, *beta, rand.randu64());
    for (int i = 0; i < *variants; ++i) {
      auto item = createItem(root, config, rand, biomeHueShift);
      if (!item) return std::unexpected(std::move(item.error()));
      if (*item) {
        PerlinF weight(static_cast<unsigned>(*octaves), 1.0f / *typePeriod, 1.0, 0.0, *alpha, *beta, rand.randu64());
        result.m_weightedItems.append({std::move(**item), weight});
      }
    }
  }
  return result;
}

std::expected<BiomeItemDistribution, StarException> BiomeItemDistribution::create(Assets const& assets, Json const& store) {
  BiomeItemDistribution result;
  auto modeName = store.getString("mode");
  if (!modeName) return std::unexpected(std::move(modeName.error()));
  auto mode = enumValue<BiomePlacementModeNames>(*modeName);
  if (!mode) return std::unexpected(std::move(mode.error()));
  result.m_mode = *mode;
  auto distributionName = store.getString("distribution");
  if (!distributionName) return std::unexpected(std::move(distributionName.error()));
  auto distribution = enumValue<DistributionTypeNames>(*distributionName);
  if (!distribution) return std::unexpected(std::move(distribution.error()));
  result.m_distribution = *distribution;
  {
    auto value = store.getFloat("priority");
    if (!value) return std::unexpected(std::move(value.error()));
    result.m_priority = std::move(*value);
  }
  {
    auto value = store.getFloat("blockProbability");
    if (!value) return std::unexpected(std::move(value.error()));
    result.m_blockProbability = std::move(*value);
  }
  {
    auto value = store.getUInt("blockSeed");
    if (!value) return std::unexpected(std::move(value.error()));
    result.m_blockSeed = std::move(*value);
  }
  auto randomItems = store.getArray("randomItems");
  if (!randomItems) return std::unexpected(std::move(randomItems.error()));
  for (auto const& entry : *randomItems) {
    auto item = variantToBiomeItem(assets, entry);
    if (!item) return std::unexpected(std::move(item.error()));
    result.m_randomItems.append(std::move(*item));
  }
  auto densityFunctionConfig = store.get("densityFunction");
  if (!densityFunctionConfig) return std::unexpected(std::move(densityFunctionConfig.error()));
  {
    auto value = PerlinF::create(*densityFunctionConfig);
    if (!value) return std::unexpected(std::move(value.error()));
    result.m_densityFunction = std::move(*value);
  }
  auto modulusDistortionConfig = store.get("modulusDistortion");
  if (!modulusDistortionConfig) return std::unexpected(std::move(modulusDistortionConfig.error()));
  {
    auto value = PerlinF::create(*modulusDistortionConfig);
    if (!value) return std::unexpected(std::move(value.error()));
    result.m_modulusDistortion = std::move(*value);
  }
  {
    auto value = store.getInt("modulus");
    if (!value) return std::unexpected(std::move(value.error()));
    result.m_modulus = std::move(*value);
  }
  {
    auto value = store.getInt("modulusOffset");
    if (!value) return std::unexpected(std::move(value.error()));
    result.m_modulusOffset = std::move(*value);
  }
  auto weightedItems = store.getArray("weightedItems");
  if (!weightedItems) return std::unexpected(std::move(weightedItems.error()));
  for (auto const& entry : *weightedItems) {
    auto itemConfig = entry.get(0);
    if (!itemConfig) return std::unexpected(std::move(itemConfig.error()));
    auto item = variantToBiomeItem(assets, *itemConfig);
    if (!item) return std::unexpected(std::move(item.error()));
    auto weightConfig = entry.get(1);
    if (!weightConfig) return std::unexpected(std::move(weightConfig.error()));
    auto weight = PerlinF::create(*weightConfig);
    if (!weight) return std::unexpected(std::move(weight.error()));
    result.m_weightedItems.append({std::move(*item), std::move(*weight)});
  }
  return result;
}

std::expected<Json, StarException> BiomeItemDistribution::toJson() const {
  auto mode = enumName<BiomePlacementModeNames>(m_mode);
  if (!mode) return std::unexpected(std::move(mode.error()));
  auto distribution = enumName<DistributionTypeNames>(m_distribution);
  if (!distribution) return std::unexpected(std::move(distribution.error()));
  JsonArray randomItems;
  for (auto const& item : m_randomItems) {
    auto json = variantFromBiomeItem(item);
    if (!json) return std::unexpected(std::move(json.error()));
    randomItems.append(std::move(*json));
  }
  auto density = m_densityFunction.toJson();
  if (!density) return std::unexpected(std::move(density.error()));
  auto distortion = m_modulusDistortion.toJson();
  if (!distortion) return std::unexpected(std::move(distortion.error()));
  JsonArray weightedItems;
  for (auto const& pair : m_weightedItems) {
    auto item = variantFromBiomeItem(pair.first);
    if (!item) return std::unexpected(std::move(item.error()));
    auto weight = pair.second.toJson();
    if (!weight) return std::unexpected(std::move(weight.error()));
    weightedItems.append(JsonArray{std::move(*item), std::move(*weight)});
  }
  return JsonObject{{"mode", String(*mode)}, {"distribution", String(*distribution)}, {"priority", m_priority},
      {"blockProbability", m_blockProbability}, {"blockSeed", m_blockSeed}, {"randomItems", std::move(randomItems)},
      {"densityFunction", std::move(*density)}, {"modulusDistortion", std::move(*distortion)},
      {"modulus", m_modulus}, {"modulusOffset", m_modulusOffset}, {"weightedItems", std::move(weightedItems)}};
}

BiomePlacementMode BiomeItemDistribution::mode() const {
  return m_mode;
}

List<BiomeItem> BiomeItemDistribution::allItems() const {
  if (m_distribution == DistributionType::Random) {
    return m_randomItems;
  } else if (m_distribution == DistributionType::Periodic) {
    List<BiomeItem> items;
    for (auto const& pair : m_weightedItems)
      items.append(pair.first);
    return items;
  } else {
    return {};
  }
}

std::expected<std::optional<BiomeItemPlacement>, StarException> BiomeItemDistribution::itemToPlace(int x, int y) const {
  if (m_distribution == DistributionType::Random) {
    if (staticRandomFloat(x, y, m_blockSeed) <= m_blockProbability) {
      auto item = staticRandomValueFrom(m_randomItems, x, y, m_blockSeed);
      return BiomeItemPlacement{std::move(item), Vec2I(x, y), m_priority};
    }

  } else if (m_distribution == DistributionType::Periodic) {
    std::optional<std::reference_wrapper<BiomeItem const>> biomeItem;
    auto density = m_densityFunction.get(x, y);
    if (!density) return std::unexpected(std::move(density.error()));
    if (*density > 0) {
      auto distortion = m_modulusDistortion.get(x, y);
      if (!distortion) return std::unexpected(std::move(distortion.error()));
      if (static_cast<int>(x + m_modulusOffset + *distortion) % m_modulus == 0) {
        float maxWeight = std::numeric_limits<float>::lowest();
        for (auto const& weightedItem : m_weightedItems) {
          auto weightResult = weightedItem.second.get(x, y);
          if (!weightResult) return std::unexpected(std::move(weightResult.error()));
          float weight = *weightResult;
          if (weight > maxWeight) {
            maxWeight = weight;
            biomeItem = std::cref(weightedItem.first);
          }
        }
      }
    }

    if (biomeItem)
      return BiomeItemPlacement{biomeItem->get(), Vec2I(x, y), m_priority};
  }

  return std::optional<BiomeItemPlacement>();
}


}
