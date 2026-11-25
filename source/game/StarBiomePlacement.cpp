import star.json;
import star.json_extra;
import star.bi_map;
import star.random;
import star.perlin;
import star.weighted_pool;
import star.set;
import star.list;
import star.vector;
import star.string_view;
import star.logging;
import std;
import star.string;
import star.format;

import star.plant_database;
import star.biome_placement;
import star.assets;
import star.root;
import star.static_random;
import star.math_common;
namespace Star {
namespace {
std::expected<BiomeItem, StarException> read_micro_dungeon_names(Json const& value) {
  auto names = json_to_string_set(value);
  if (!names) return std::unexpected(std::move(names.error()));
  if (names->empty() || names->contains(String{}))
    return std::unexpected(BiomeException("Microdungeon selection needs at least one name and cannot contain an empty name"));
  return MicroDungeonNames{std::move(*names)};
}
}

std::expected<BiomeItem, StarException> variant_to_biome_item(Assets const& assets, Json const& store) {
  auto type = store.get(0);
  if (!type) return std::unexpected(std::move(type.error()));
  auto value = store.get(1);
  if (!value) return std::unexpected(std::move(value.error()));
  // Keep the owning Json alive while comparing its UTF-8 tag. A non-string
  // tag still goes through the historical unrecognized-type diagnostic.
  std::string_view tag;
  if (type->type() == Json::Type::String) {
    auto text = type->string_ptr();
    if (!text) return std::unexpected(std::move(text.error()));
    tag = (**text).utf8();
  }
  if (tag == "grass") {
    return GrassVariant::create(assets, *value);
  } else if (tag == "bush") {
    return BushVariant::create(assets, *value);
  } else if (tag == "treePair") {
    auto tree_at = [&](std::size_t index) {
      return value->get(index).and_then([&](Json const& config) { return TreeVariant::create(assets, config); });
    };
    auto first = tree_at(0);
    if (!first) return std::unexpected(std::move(first.error()));
    auto second = tree_at(1);
    if (!second) return std::unexpected(std::move(second.error()));
    return BiomeItem{std::in_place_type<TreePair>, std::move(*first), std::move(*second)};
  } else if (tag == "objectPool") {
    ObjectPool pool;
    auto pairs = value->iterate_array();
    if (!pairs) return std::unexpected(std::move(pairs.error()));
    for (auto const& pair : *pairs) {
      auto weight = pair.get_float(0);
      if (!weight) return std::unexpected(std::move(weight.error()));
      auto object = pair.get(1);
      if (!object) return std::unexpected(std::move(object.error()));
      auto name = object->get_string(0);
      if (!name) return std::unexpected(std::move(name.error()));
      auto parameters = object->get(1);
      if (!parameters) return std::unexpected(std::move(parameters.error()));
      auto inserted = pool.insert(*weight, std::pair{std::move(*name), std::move(*parameters)});
      if (!inserted) return std::unexpected(std::move(inserted.error()));
    }
    return BiomeItem{std::move(pool)};
  } else if (tag == "treasureBoxSet") {
    auto name = value->to_string();
    if (!name) return std::unexpected(std::move(name.error()));
    return BiomeItem{TreasureBoxSet(std::move(*name))};
  } else if (tag == "microDungeon") {
    return read_micro_dungeon_names(*value);
  }
  auto text = type->repr();
  if (!text) return std::unexpected(std::move(text.error()));
  return std::unexpected(BiomeException(Star::format("Unrecognized biome item type '{}'", *text)));
}

namespace {

// Tagged items and pairs are always two-element arrays in existing stores.
Json json_pair(Json first, Json second) {
  JsonArray pair;
  pair.reserve(2);
  pair.push_back(std::move(first));
  pair.push_back(std::move(second));
  return pair;
}

struct StoreBiomeItem {
  using Result = std::expected<Json, StarException>;

  Result operator()(GrassVariant const& grass) const {
    return grass.toJson().transform([](Json value) { return json_pair("grass", std::move(value)); });
  }

  Result operator()(BushVariant const& bush) const {
    return bush.toJson().transform([](Json value) { return json_pair("bush", std::move(value)); });
  }

  Result operator()(TreePair const& trees) const {
    auto first = trees.first.toJson();
    if (!first) return std::unexpected(std::move(first.error()));
    auto second = trees.second.toJson();
    if (!second) return std::unexpected(std::move(second.error()));
    return json_pair("treePair", json_pair(std::move(*first), std::move(*second)));
  }

  Result operator()(ObjectPool const& pool) const {
    JsonArray objects;
    objects.reserve(pool.size());
    for (auto const& [weight, object] : pool.items())
      objects.push_back(json_pair(weight, json_pair(object.first, object.second)));
    return json_pair("objectPool", std::move(objects));
  }

  Result operator()(TreasureBoxSet const& set) const {
    return json_pair("treasureBoxSet", set.name);
  }

  Result operator()(MicroDungeonNames const& dungeons) const {
    return json_pair("microDungeon", json_from_string_set(*dungeons.names));
  }
};

}

std::expected<Json, StarException> variant_from_biome_item(BiomeItem const& item) {
  return std::visit(StoreBiomeItem{}, item);
}

namespace {

struct TreeCandidates {
  StringList stems;
  StringList foliages;
  std::vector<std::pair<std::size_t, std::size_t>> matches;
  std::shared_ptr<PlantDatabase const> plants;
  float stem_hue_max = 0;
  float foliage_hue_max = 0;

  std::expected<std::reference_wrapper<PlantDatabase const>, StarException> plant_database(Root& root) {
    if (!plants) {
      auto database = root.plantDatabase();
      if (!database) return std::unexpected(std::move(database.error()));
      plants = std::move(*database);
    }
    return std::cref(*plants);
  }
};

std::expected<TreeCandidates, StarException> prepare_tree_candidates(Root& root, Json const& config) {
  auto stem_config = config.get("treeStemList", JsonArray());
  if (!stem_config) return std::unexpected(std::move(stem_config.error()));
  auto stems = json_to_string_list(*stem_config);
  if (!stems) return std::unexpected(std::move(stems.error()));
  auto foliage_config = config.get("treeFoliageList", JsonArray());
  if (!foliage_config) return std::unexpected(std::move(foliage_config.error()));
  auto foliages = json_to_string_list(*foliage_config);
  if (!foliages) return std::unexpected(std::move(foliages.error()));

  TreeCandidates candidates;
  candidates.stems = std::move(*stems);
  candidates.foliages = std::move(*foliages);
  if (candidates.stems.empty() || candidates.foliages.empty())
    return candidates;

  // Resolve shapes on first encounter: a bad stem still fails before a bad
  // foliage, and leafless pairs do not consult the database.
  std::vector<String> foliage_shapes;
  for (std::size_t i = 0; i != candidates.stems.size(); ++i) {
    std::optional<String> stem_shape;
    std::size_t shape_index = 0;
    for (std::size_t j = 0; j != candidates.foliages.size(); ++j) {
      auto const& foliage = candidates.foliages[j];
      if (!foliage.empty()) {
        if (!stem_shape) {
          auto database = candidates.plant_database(root);
          if (!database) return std::unexpected(std::move(database.error()));
          auto shape = database->get().treeStemShape(candidates.stems[i]);
          if (!shape) return std::unexpected(std::move(shape.error()));
          stem_shape.emplace(std::move(*shape));
        }
        if (i == 0) {
          auto shape = candidates.plants->treeFoliageShape(foliage);
          if (!shape) return std::unexpected(std::move(shape.error()));
          bool matches = *stem_shape == *shape;
          if (candidates.stems.size() != 1)
            foliage_shapes.emplace_back(std::move(*shape));
          if (!matches)
            continue;
        } else if (*stem_shape != foliage_shapes[shape_index++]) {
          continue;
        }
      }
      candidates.matches.emplace_back(i, j);
    }
  }
  return candidates;
}

std::expected<std::optional<BiomeItem>, StarException> create_tree_item(
    Root& root, Json const& config, RandomSource& rand, std::optional<TreeCandidates>& cached) {
  bool first_variant = !cached;
  if (first_variant) {
    auto candidates = prepare_tree_candidates(root, config);
    if (!candidates) return std::unexpected(std::move(candidates.error()));
    cached.emplace(std::move(*candidates));
  }
  auto& candidates = *cached;
  if (candidates.matches.empty() && !candidates.stems.empty() && !candidates.foliages.empty()) {
    auto logged = root.logger().warn("Specified stemList and foliageList, but no matching pairs found.");
    if (!logged) return std::unexpected(std::move(logged.error()));
  }

  // An empty sample consumes no draw, but all three hue draws still occur.
  auto pair = rand.sample_value(candidates.matches);
  if (!pair) return std::unexpected(std::move(pair.error()));
  auto stem_random = rand.uniform_float(-1.0f, 1.0f);
  if (!stem_random) return std::unexpected(std::move(stem_random.error()));
  if (first_variant) {
    auto limit = config.get_float("treeStemHueShiftMax", 0);
    if (!limit) return std::unexpected(std::move(limit.error()));
    candidates.stem_hue_max = *limit;
  }
  auto foliage_random = rand.uniform_float(-1.0f, 1.0f);
  if (!foliage_random) return std::unexpected(std::move(foliage_random.error()));
  if (first_variant) {
    auto limit = config.get_float("treeFoliageHueShiftMax", 0);
    if (!limit) return std::unexpected(std::move(limit.error()));
    candidates.foliage_hue_max = *limit;
  }
  auto alternate_random = rand.uniform_float(-1.0f, 1.0f);
  if (!alternate_random) return std::unexpected(std::move(alternate_random.error()));
  if (candidates.matches.empty() || candidates.stems[pair->first].empty())
    return std::nullopt;

  auto database = candidates.plant_database(root);
  if (!database) return std::unexpected(std::move(database.error()));
  auto const& stem = candidates.stems[pair->first];
  auto const& foliage = candidates.foliages[pair->second];
  float stem_hue = *stem_random * candidates.stem_hue_max;
  auto build = [&](float foliage_random_value) {
    return foliage.empty() ? database->get().buildTreeVariant(stem, stem_hue)
                           : database->get().buildTreeVariant(stem, stem_hue, foliage, foliage_random_value * candidates.foliage_hue_max);
  };
  auto primary = build(*foliage_random);
  if (!primary) return std::unexpected(std::move(primary.error()));
  auto alternate = build(*alternate_random);
  if (!alternate) return std::unexpected(std::move(alternate.error()));
  return BiomeItem{TreePair{std::move(*primary), std::move(*alternate)}};
}

std::expected<std::optional<BiomeItem>, StarException> create_item(
    Root& root, Json const& config, RandomSource& rand, float biome_hue_shift, std::optional<TreeCandidates>& tree_candidates) {
  if (tree_candidates)
    return create_tree_item(root, config, rand, tree_candidates);
  auto type = config.get_string("type");
  if (!type) return std::unexpected(std::move(type.error()));
  if (auto match = type->equals_ignore_case("grass"); !match) return std::unexpected(std::move(match.error()));
  else if (*match) {
    auto grass_config = config.get("grasses");
    if (!grass_config) return std::unexpected(std::move(grass_config.error()));
    auto grasses = json_to_string_list(*grass_config);
    if (!grasses) return std::unexpected(std::move(grasses.error()));
    auto plants = root.plantDatabase();
    if (!plants) return std::unexpected(std::move(plants.error()));
    auto grass = rand.sample_ref(*grasses);
    if (!grass) return std::unexpected(std::move(grass.error()));
    auto variant = (*plants)->buildGrassVariant(grass->get(), biome_hue_shift);
    if (!variant) return std::unexpected(std::move(variant.error()));
    return BiomeItem{std::move(*variant)};
  } else if (auto match = type->equals_ignore_case("bush"); !match) return std::unexpected(std::move(match.error()));
  else if (*match) {
    auto bushes = config.get_array("bushes", {});
    if (!bushes) return std::unexpected(std::move(bushes.error()));
    auto settings = rand.sample_value(*bushes);
    if (!settings) return std::unexpected(std::move(settings.error()));
    auto name = settings->get_string("name");
    if (!name) return std::unexpected(std::move(name.error()));
    auto plants = root.plantDatabase();
    if (!plants) return std::unexpected(std::move(plants.error()));
    auto mods = (*plants)->bushMods(*name);
    if (!mods) return std::unexpected(std::move(mods.error()));
    auto mod = rand.sample_value(*mods);
    if (!mod) return std::unexpected(std::move(mod.error()));
    auto base_random = rand.uniform_float(-1.0f, 1.0f);
    if (!base_random) return std::unexpected(std::move(base_random.error()));
    auto base_max = settings->get_float("baseHueShiftMax");
    if (!base_max) return std::unexpected(std::move(base_max.error()));
    float base_hue = *base_random * *base_max;
    auto mod_random = rand.uniform_float(-1.0f, 1.0f);
    if (!mod_random) return std::unexpected(std::move(mod_random.error()));
    auto mod_max = settings->get_float("modHueShiftMax");
    if (!mod_max) return std::unexpected(std::move(mod_max.error()));
    auto variant = (*plants)->buildBushVariant(*name, base_hue, *mod, *mod_random * *mod_max);
    if (!variant) return std::unexpected(std::move(variant.error()));
    return BiomeItem{std::move(*variant)};
  } else if (auto match = type->equals_ignore_case("tree"); !match) return std::unexpected(std::move(match.error()));
  else if (*match) {
    return create_tree_item(root, config, rand, tree_candidates);
  } else if (auto match = type->equals_ignore_case("object"); !match) return std::unexpected(std::move(match.error()));
  else if (*match) {
    auto sets = config.get_array("objectSets");
    if (!sets) return std::unexpected(std::move(sets.error()));
    auto pool_config = rand.sample_value(*sets);
    if (!pool_config) return std::unexpected(std::move(pool_config.error()));
    ObjectPool pool;
    auto parameters = pool_config->get("parameters", JsonObject());
    if (!parameters) return std::unexpected(std::move(parameters.error()));
    auto pairs = pool_config->get_array("pool");
    if (!pairs) return std::unexpected(std::move(pairs.error()));
    for (auto const& pair : *pairs) {
      auto size = pair.size();
      if (!size) return std::unexpected(std::move(size.error()));
      if (*size != 2) return std::unexpected(BiomeException("Wrong size for objects weight / list pair in biome items"));
      auto weight = pair.get_float(0);
      if (!weight) return std::unexpected(std::move(weight.error()));
      auto name = pair.get_string(1);
      if (!name) return std::unexpected(std::move(name.error()));
      auto inserted = pool.insert(*weight, std::pair{std::move(*name), *parameters});
      if (!inserted) return std::unexpected(std::move(inserted.error()));
    }
    return BiomeItem{std::move(pool)};
  } else if (auto match = type->equals_ignore_case("treasureBox"); !match) return std::unexpected(std::move(match.error()));
  else if (*match) {
    auto sets = config.get_array("treasureBoxSets");
    if (!sets) return std::unexpected(std::move(sets.error()));
    auto name = rand.sample_value(*sets).and_then([](Json const& value) { return value.to_string(); });
    if (!name) return std::unexpected(std::move(name.error()));
    return BiomeItem{TreasureBoxSet(std::move(*name))};
  } else if (auto match = type->equals_ignore_case("microdungeon"); !match) return std::unexpected(std::move(match.error()));
  else if (*match) {
    auto names = config.get("microdungeons");
    if (!names) return std::unexpected(std::move(names.error()));
    return read_micro_dungeon_names(*names);
  } else {
    return std::unexpected(BiomeException(Star::format("No such item type '{}' in item distribution", *type)));
  }
}

} // namespace

std::expected<BiomeItemDistribution, StarException> BiomeItemDistribution::create(Root& root, Json const& config, std::uint64_t seed, float biome_hue_shift) {
  BiomeItemDistribution result;
  RandomSource rand(seed);
  auto mode_name = config.get_string("mode", "floor");
  if (!mode_name) return std::unexpected(std::move(mode_name.error()));
  auto mode = enum_value<biome_placement_mode_names>(*mode_name);
  if (!mode) return std::unexpected(std::move(mode.error()));
  result.m_mode = *mode;
  {
    auto value = config.get_float("priority", 0.0f);
    if (!value) return std::unexpected(std::move(value.error()));
    result.m_priority = std::move(*value);
  }
  auto count = config.get_double("variants", 1);
  if (!count) return std::unexpected(std::move(count.error()));
  if (!(*count >= 0 && *count <= std::numeric_limits<int>::max()) || *count != std::trunc(*count))
    return std::unexpected(BiomeException("Biome variants must be a nonnegative int"));
  int const variants = static_cast<int>(*count);
  auto settings = config.get("distribution", JsonObject());
  if (!settings) return std::unexpected(std::move(settings.error()));
  if (settings->type() == Json::Type::String) {
    auto assets = root.assets();
    if (!assets) return std::unexpected(std::move(assets.error()));
    auto path = settings->to_string();
    if (!path) return std::unexpected(std::move(path.error()));
    {
      auto value = (*assets)->json(*path);
      if (!value) return std::unexpected(std::move(value.error()));
      *settings = std::move(*value);
    }
  }
  auto type_name = settings->get_string("type");
  if (!type_name) return std::unexpected(std::move(type_name.error()));
  auto type = enum_value<distribution_type_names>(*type_name);
  if (!type) return std::unexpected(std::move(type.error()));
  result.m_distribution = *type;
  std::optional<TreeCandidates> tree_candidates;
  if (result.m_distribution == DistributionType::Random) {
    {
      auto value = settings->get_float("blockProbability");
      if (!value) return std::unexpected(std::move(value.error()));
      result.m_block_probability = *value;
    }
    result.m_block_seed = rand.next_u64();
    for (int i = 0; i < variants; ++i) {
      auto item = create_item(root, config, rand, biome_hue_shift, tree_candidates);
      if (!item) return std::unexpected(std::move(item.error()));
      if (*item)
        result.m_random_items.push_back(std::move(**item));
    }
  } else if (result.m_distribution == DistributionType::Periodic) {
    auto octaves = settings->get_double("octaves", 1);
    if (!octaves) return std::unexpected(std::move(octaves.error()));
    auto alpha = settings->get_double("alpha", 2.0);
    if (!alpha) return std::unexpected(std::move(alpha.error()));
    auto beta = settings->get_double("beta", 2.0);
    if (!beta) return std::unexpected(std::move(beta.error()));
    // The legacy formulas round these inputs to float before multiplying the
    // variance or taking the reciprocal. Keep that first rounding.
    auto read_scalar = [&](String const& key, double fallback) -> std::expected<float, StarException> {
      auto value = settings->get_double(key, fallback);
      if (!value) return std::unexpected(std::move(value.error()));
      if (!is_finite(*value) || std::abs(*value) > std::numeric_limits<float>::max())
        return std::unexpected(BiomeException::format("{} is outside the float range", key));
      return static_cast<float>(*value);
    };
    auto modulus_variance = read_scalar("modulusVariance", 0.0);
    if (!modulus_variance) return std::unexpected(std::move(modulus_variance.error()));
    auto density_period = read_scalar("densityPeriod", 10);
    if (!density_period) return std::unexpected(std::move(density_period.error()));
    auto density_offset = settings->get_double("densityOffset", 2.0);
    if (!density_offset) return std::unexpected(std::move(density_offset.error()));
    auto type_period = read_scalar("typePeriod", 10);
    if (!type_period) return std::unexpected(std::move(type_period.error()));
    {
      auto value = settings->get_int("modulus", 1);
      if (!value) return std::unexpected(std::move(value.error()));
      if (!std::in_range<int>(*value) || *value == std::numeric_limits<int>::min() || *value == 0)
        return std::unexpected(BiomeException("Invalid periodic biome modulus"));
      result.m_modulus = static_cast<int>(*value);
    }
    {
      auto value = rand.uniform_int(-result.m_modulus, result.m_modulus);
      if (!value) return std::unexpected(std::move(value.error()));
      result.m_modulus_offset = *value;
    }
    if (*density_period == 0)
      return std::unexpected(BiomeException("Periodic biome densityPeriod must be nonzero"));
    auto density = PerlinF::create(PerlinType::Perlin, *octaves, 1.0f / *density_period, 1.0, *density_offset,
                                   *alpha, *beta, rand.next_u64());
    if (!density) return std::unexpected(std::move(density.error()));
    result.m_density_function = std::move(*density);
    auto distortion = PerlinF::create(PerlinType::Perlin, *octaves, 1.0f / result.m_modulus, *modulus_variance,
                                      static_cast<double>(*modulus_variance) * 2, *alpha, *beta, rand.next_u64());
    if (!distortion) return std::unexpected(std::move(distortion.error()));
    result.m_modulus_distortion = std::move(*distortion);
    std::optional<PerlinF::Parameters> weight_parameters;
    for (int i = 0; i < variants; ++i) {
      auto item = create_item(root, config, rand, biome_hue_shift, tree_candidates);
      if (!item) return std::unexpected(std::move(item.error()));
      if (*item) {
        if (!weight_parameters) {
          if (*type_period == 0)
            return std::unexpected(BiomeException("Periodic biome typePeriod must be nonzero"));
          auto parameters = PerlinF::make_parameters(PerlinType::Perlin, *octaves, 1.0f / *type_period, 1.0, 0.0, *alpha, *beta);
          if (!parameters) return std::unexpected(std::move(parameters.error()));
          weight_parameters.emplace(*parameters);
        }
        result.m_weighted_items.emplace_back(std::move(**item), *weight_parameters, rand.next_u64());
      }
    }
  }
  return result;
}

std::expected<BiomeItemDistribution, StarException> BiomeItemDistribution::create(Assets const& assets, Json const& store) {
  BiomeItemDistribution result;
  auto mode = store.get_string("mode").and_then([](String const& name) { return enum_value<biome_placement_mode_names>(name); });
  if (!mode) return std::unexpected(std::move(mode.error()));
  result.m_mode = *mode;
  auto distribution = store.get_string("distribution").and_then([](String const& name) { return enum_value<distribution_type_names>(name); });
  if (!distribution) return std::unexpected(std::move(distribution.error()));
  result.m_distribution = *distribution;
  auto priority = store.get_float("priority");
  if (!priority) return std::unexpected(std::move(priority.error()));
  result.m_priority = *priority;
  auto probability = store.get_float("blockProbability");
  if (!probability) return std::unexpected(std::move(probability.error()));
  result.m_block_probability = *probability;
  auto seed = store.get_uint("blockSeed");
  if (!seed) return std::unexpected(std::move(seed.error()));
  result.m_block_seed = *seed;

  auto read_array = [&](String const& key) {
    return store.get(key).and_then([](Json const& value) { return value.array_ptr(); });
  };
  auto random_items = read_array("randomItems");
  if (!random_items) return std::unexpected(std::move(random_items.error()));
  result.m_random_items.reserve((*random_items)->size());
  for (auto const& entry : **random_items) {
    auto item = variant_to_biome_item(assets, entry);
    if (!item) return std::unexpected(std::move(item.error()));
    result.m_random_items.push_back(std::move(*item));
  }

  auto read_noise = [&](String const& key) {
    return store.get(key).and_then([](Json const& config) { return PerlinF::create(config); });
  };
  auto density = read_noise("densityFunction");
  if (!density) return std::unexpected(std::move(density.error()));
  result.m_density_function = std::move(*density);
  auto distortion = read_noise("modulusDistortion");
  if (!distortion) return std::unexpected(std::move(distortion.error()));
  result.m_modulus_distortion = std::move(*distortion);
  {
    auto value = store.get_int("modulus");
    if (!value) return std::unexpected(std::move(value.error()));
    if (!std::in_range<int>(*value) || (result.m_distribution == DistributionType::Periodic && *value == 0))
      return std::unexpected(BiomeException("Invalid stored biome modulus"));
    result.m_modulus = static_cast<int>(*value);
  }
  {
    auto value = store.get_int("modulusOffset");
    if (!value) return std::unexpected(std::move(value.error()));
    if (!std::in_range<int>(*value))
      return std::unexpected(BiomeException("Stored biome modulusOffset is outside the int range"));
    result.m_modulus_offset = static_cast<int>(*value);
  }
  auto weighted_items = read_array("weightedItems");
  if (!weighted_items) return std::unexpected(std::move(weighted_items.error()));
  result.m_weighted_items.reserve((*weighted_items)->size());
  for (auto const& entry : **weighted_items) {
    auto item = entry.get(0).and_then([&](Json const& config) { return variant_to_biome_item(assets, config); });
    if (!item) return std::unexpected(std::move(item.error()));
    auto weight = entry.get(1).and_then([](Json const& config) { return PerlinF::create(config); });
    if (!weight) return std::unexpected(std::move(weight.error()));
    result.m_weighted_items.emplace_back(std::move(*item), std::move(*weight));
  }
  return result;
}

bool BiomeItemDistribution::can_place_micro_dungeon() const {
  auto is_micro = [](BiomeItem const& item) { return std::holds_alternative<MicroDungeonNames>(item); };
  if (m_distribution == DistributionType::Random)
    return m_block_probability >= 0 && std::ranges::any_of(m_random_items, is_micro);
  return std::ranges::any_of(m_weighted_items, is_micro, &PeriodicWeightedItem::item);
}

std::expected<Json, StarException> BiomeItemDistribution::to_json() const {
  auto mode = enum_name<biome_placement_mode_names>(m_mode);
  if (!mode) return std::unexpected(std::move(mode.error()));
  auto distribution = enum_name<distribution_type_names>(m_distribution);
  if (!distribution) return std::unexpected(std::move(distribution.error()));
  JsonArray random_items;
  random_items.reserve(m_random_items.size());
  for (auto const& item : m_random_items) {
    auto json = variant_from_biome_item(item);
    if (!json) return std::unexpected(std::move(json.error()));
    random_items.push_back(std::move(*json));
  }
  auto density = m_density_function.to_json();
  if (!density) return std::unexpected(std::move(density.error()));
  auto distortion = m_modulus_distortion.to_json();
  if (!distortion) return std::unexpected(std::move(distortion.error()));
  JsonArray weighted_items;
  weighted_items.reserve(m_weighted_items.size());
  for (auto const& entry : m_weighted_items) {
    auto item = variant_from_biome_item(entry.item);
    if (!item) return std::unexpected(std::move(item.error()));
    auto weight = entry.weight.to_json();
    if (!weight) return std::unexpected(std::move(weight.error()));
    weighted_items.push_back(json_pair(std::move(*item), std::move(*weight)));
  }
  JsonObject store;
  store.reserve(11);
  store.emplace("mode", *mode);
  store.emplace("distribution", *distribution);
  store.emplace("priority", m_priority);
  store.emplace("blockProbability", m_block_probability);
  store.emplace("blockSeed", m_block_seed);
  store.emplace("randomItems", std::move(random_items));
  store.emplace("densityFunction", std::move(*density));
  store.emplace("modulusDistortion", std::move(*distortion));
  store.emplace("modulus", m_modulus);
  store.emplace("modulusOffset", m_modulus_offset);
  store.emplace("weightedItems", std::move(weighted_items));
  return store;
}

std::size_t BiomeItemDistribution::size() const {
  return m_distribution == DistributionType::Random ? m_random_items.size() : m_weighted_items.size();
}

std::optional<std::reference_wrapper<TreeVariant const>> BiomeItemDistribution::first_tree() const & {
  if (m_distribution == DistributionType::Random) {
    for (auto const& item : m_random_items)
      if (std::holds_alternative<TreePair>(item))
        return std::cref(std::get<TreePair>(item).first);
  } else {
    for (auto const& entry : m_weighted_items)
      if (std::holds_alternative<TreePair>(entry.item))
        return std::cref(std::get<TreePair>(entry.item).first);
  }
  return std::nullopt;
}

std::expected<std::optional<std::reference_wrapper<BiomeItem const>>, StarException>
BiomeItemDistribution::item_at(int x, int y) const & {
  if (m_distribution == DistributionType::Random) {
    if (m_random_items.empty() || !(static_random_float(x, y, m_block_seed) <= m_block_probability))
      return std::nullopt;
    auto item = static_random_from(m_random_items, x, y, m_block_seed);
    if (!item) return std::unexpected(std::move(item.error()));
    return *item;
  }

  auto density = m_density_function.get(x, y);
  if (!density) return std::unexpected(std::move(density.error()));
  if (!(*density > 0))
    return std::nullopt;
  auto distortion = m_modulus_distortion.get(x, y);
  if (!distortion) return std::unexpected(std::move(distortion.error()));

  // Legacy placement adds the offset as an int, then rounds to float before
  // adding noise. Wider floating-point arithmetic here would move boundaries.
  auto offset_x = static_cast<std::int64_t>(x) + m_modulus_offset;
  if (!std::in_range<int>(offset_x))
    return std::unexpected(BiomeException("Periodic biome coordinate plus offset is outside the int range"));
  float distorted_x = static_cast<float>(static_cast<int>(offset_x)) + *distortion;
  constexpr float int_limit = -static_cast<float>(std::numeric_limits<int>::min());
  if (!(distorted_x >= -int_limit && distorted_x < int_limit))
    return std::unexpected(BiomeException("Distorted periodic biome coordinate is outside the int range"));
  // Every coordinate is divisible by -1; evaluating INT_MIN % -1 is undefined.
  if (m_modulus != -1 && static_cast<int>(distorted_x) % m_modulus != 0)
    return std::nullopt;

  float best_weight = std::numeric_limits<float>::lowest();
  std::optional<std::reference_wrapper<BiomeItem const>> best;
  for (auto const& entry : m_weighted_items) {
    auto weight = entry.weight.get(x, y);
    if (!weight) return std::unexpected(std::move(weight.error()));
    if (*weight > best_weight) {
      best_weight = *weight;
      best = std::cref(entry.item);
    }
  }
  return best;
}

std::expected<std::optional<BiomeItemPlacement>, StarException> BiomeItemDistribution::item_to_place(int x, int y) const {
  auto item = item_at(x, y);
  if (!item) return std::unexpected(std::move(item.error()));
  if (!*item) return std::nullopt;
  return std::expected<std::optional<BiomeItemPlacement>, StarException>(
      std::in_place, std::in_place, (**item).get(), Vec2I(x, y), m_priority);
}


}
