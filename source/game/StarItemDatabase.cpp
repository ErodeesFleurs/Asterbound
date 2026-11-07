import star.json_extra;
import star.json;
import star.thread;
import star.data_stream;
import star.bi_map;
import star.ordered_map;
import star.lru_cache;
import star.time;
import star.random;
import star.ttl_cache;
import star.casting;
import star.game_types;
import star.set;
import star.string;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.strong_typedef;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.format;
import star.version;
import star.id_map;
import star.rpc_promise;
import star.lua;
import star.spline;
import star.a_star;
import star.audio;
import star.map;

import star.lua_root;
import star.lua_components;
import star.lua_actor_movement_component;
import star.drawable;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.item;
import star.item_database;
import star.item_descriptor;
import star.item_recipe;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.tile_damage;
import star.interaction_types;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_modification;
import star.force_regions;
import star.world;
import star.physics_entity;
import star.movement_controller;
import star.mobile_entity;
import star.scripted_entity;
import star.game_timers;
import star.item_drop;
import star.augment_item;
import star.non_rotated_drawables_item;
import star.platformer_astar_types;
import star.anchorable_entity;
import star.actor_movement_controller;
import star.actor_entity;
import star.tool_user_entity;
import star.tool_user_item;
import star.beam_item;
import star.status_effect_item;
import star.fireable_item;
import star.swingable_item;
import star.durability_item;
import star.pointable_item;
import star.mixer;
import star.entity_rendering_types;
import star.animation;
import star.particle;
import star.entity_rendering;
struct PreviewTile;
import star.preview_tile_tool;
import star.tools;
import star.effect_source_item;
import star.previewable_item;
import star.armors;
import star.utility_lua_bindings;
import star.rebuilder;

import star.thrown_item;
import star.object_item;
import star.unlock_item;
import star.blueprint_item;
import star.codex_item;
import star.currency_item;
import star.consumable_item;
import star.inspection_tool;
import star.instrument_item;
import star.liquid_item;
import star.material_item;
import star.active_item;
import star.flashlight;
import star.tilling_tool;
import star.config_lua_bindings;
import star.root_lua_bindings;
import star.item_lua_bindings;
import star.codex_database;
import star.codex;
import star.object_database;
import star.versioning_database;

import star.math_common;
import star.algorithm;
import star.output_proxy;

namespace Star {

namespace {

// Synchronous one-shot scripts settle shutdown while the caller still owns the
// Lua lock. A shutdown error remains primary, with the earlier error as context.
template <typename Ret, typename Invoke>
std::expected<std::optional<Ret>, StarException> invokeItemScript(LuaBaseComponent& script, Invoke&& invoke) {
  auto result = script.init().and_then([&](bool) { return std::forward<Invoke>(invoke)(); });
  auto cleaned = script.uninit();
  if (!cleaned) {
    if (!result)
      return std::unexpected(cleaned.error().withContext(printException(result.error(), true)));
    return std::unexpected(std::move(cleaned.error()));
  }
  return result;
}

}// namespace

std::uint64_t ItemDatabase::getCountOfItem(List<std::shared_ptr<Item>> const& bag, ItemDescriptor const& item, bool exactMatch) {
  auto normalizedBag = normalizeBag(bag);
  return getCountOfItem(normalizedBag, item, exactMatch);
}

std::uint64_t ItemDatabase::getCountOfItem(HashMap<ItemDescriptor, std::uint64_t> const& bag, ItemDescriptor const& item, bool exactMatch) {
  ItemDescriptor matchItem = exactMatch ? item.singular() : ItemDescriptor(item.name(), 1, JsonObject(), {});
  auto found = bag.find(matchItem);
  return found == bag.end() ? 0 : found->second;
}

HashMap<ItemDescriptor, std::uint64_t> ItemDatabase::normalizeBag(List<std::shared_ptr<Item>> const& bag) {
  HashMap<ItemDescriptor, std::uint64_t> normalizedBag;
  for (auto const& item : bag) {
    if (!item)
      continue;

    normalizedBag[ItemDescriptor(item->name(), 1, JsonObject(), {})] += item->count();

    if (item->parameters() != Json(JsonObject()))
      normalizedBag[item->descriptor().singular()] += item->count();
  }

  return normalizedBag;
}

HashSet<ItemRecipe> ItemDatabase::recipesFromSubset(HashMap<ItemDescriptor, std::uint64_t> const& normalizedBag, StringMap<std::uint64_t> const& availableCurrencies, HashSet<ItemRecipe> const& subset) {
  HashSet<ItemRecipe> res;
  for (auto const& recipe : subset) {
    // add this recipe if we can make it.
    if (canMakeRecipe(recipe, normalizedBag, availableCurrencies))
      res.add(recipe);
  }

  return res;
}

HashSet<ItemRecipe> ItemDatabase::recipesFromSubset(HashMap<ItemDescriptor, std::uint64_t> const& normalizedBag, StringMap<std::uint64_t> const& availableCurrencies,
    HashSet<ItemRecipe> const& subset, StringSet const& allowedTypes) {
  HashSet<ItemRecipe> res;
  for (auto const& recipe : subset) {
    // is it the right kind of recipe for this check ?
    if (recipe.groups.hasIntersection(allowedTypes) || allowedTypes.empty() || recipe.groups.empty()) {
      // do we have the ingredients to make it.
      if (canMakeRecipe(recipe, normalizedBag, availableCurrencies)) {
        res.add(recipe);
      }
    }
  }

  return res;
}

std::expected<String, StarException> ItemDatabase::guiFilterString(std::shared_ptr<Item> const& item) {
  auto lowered = (item->name() + item->friendlyName() + item->description()).toLower();
  if (!lowered)
    return std::unexpected(std::move(lowered.error()));
  auto split = lowered->splitAny(" ,.?*\\+/|\t");
  if (!split)
    return std::unexpected(std::move(split.error()));
  return split->join("");
}

bool ItemDatabase::canMakeRecipe(ItemRecipe const& recipe, HashMap<ItemDescriptor, std::uint64_t> const& availableIngredients, StringMap<std::uint64_t> const& availableCurrencies) {
  for (auto const& p : recipe.currencyInputs) {
    if (availableCurrencies.value(p.first, 0) < p.second)
      return false;
  }

  for (auto const& input : recipe.inputs) {
    ItemDescriptor matchInput = recipe.matchInputParameters ? input.singular() : ItemDescriptor(input.name(), 1, JsonObject(), {});
    if (availableIngredients.value(matchInput) < input.count())
      return false;
  }

  return true;
}

ItemDatabase::ItemDatabase(Root& root) : m_root(root), m_itemCache(root.random()) {}

std::expected<std::shared_ptr<ItemDatabase>, StarException> ItemDatabase::create(Root& root) {
  auto database = std::shared_ptr<ItemDatabase>(new ItemDatabase(root));
  auto luaRoot = LuaRoot::create(root);
  if (!luaRoot)
    return std::unexpected(std::move(luaRoot.error()));
  database->m_luaRoot = std::move(*luaRoot);
  auto scanned = database->scanItems();
  if (!scanned)
    return std::unexpected(std::move(scanned.error()));
  auto objects = database->addObjectItems();
  if (!objects)
    return std::unexpected(std::move(objects.error()));
  auto codexes = database->addCodexes();
  if (!codexes)
    return std::unexpected(std::move(codexes.error()));
  // Builders can query the complete base catalog without re-entering Root's
  // still-unpublished ItemDatabase loader.
  auto rebuilder = Rebuilder::create(root, "item", std::cref(*database));
  if (!rebuilder)
    return std::unexpected(std::move(rebuilder.error()));
  database->m_rebuilder = std::move(*rebuilder);
  auto recipes = database->scanRecipes();
  if (!recipes)
    return std::unexpected(std::move(recipes.error()));
  auto blueprints = database->addBlueprints();
  if (!blueprints)
    return std::unexpected(std::move(blueprints.error()));
  return database;
}

void ItemDatabase::cleanup() {
  {
    std::lock_guard locker(m_cacheMutex);
    m_itemCache.cleanup([](ItemCacheEntry const&, std::shared_ptr<Item> const& item) {
      return item.use_count() != 1;
    });
  }
}


std::expected<ItemDescriptor, StarException> ItemDatabase::loadDescriptor(Json const& store) const {
  auto versioning = m_root.versioningDatabase();
  if (!versioning)
    return std::unexpected(std::move(versioning.error()));
  return ItemDescriptor::loadStore(**versioning, store);
}

std::expected<std::shared_ptr<Item>, StarException> ItemDatabase::diskLoad(Json const& diskStore) const {
  if (!diskStore)
    return std::shared_ptr<Item>();
  auto descriptor = loadDescriptor(diskStore);
  if (!descriptor)
    return std::unexpected(std::move(descriptor.error()));
  return item(std::move(*descriptor));
}

std::expected<std::shared_ptr<Item>, StarException> ItemDatabase::fromJson(Json const& spec) const {
  auto descriptor = ItemDescriptor::create(spec);
  if (!descriptor)
    return std::unexpected(std::move(descriptor.error()));
  return item(std::move(*descriptor));
}

std::expected<Json, StarException> ItemDatabase::diskStore(std::shared_ptr<Item const> const& itemPtr) const {
  if (!itemPtr)
    return Json();
  auto versioning = m_root.versioningDatabase();
  if (!versioning)
    return std::unexpected(std::move(versioning.error()));
  return itemPtr->descriptor().diskStore(**versioning);
}

Json ItemDatabase::toJson(std::shared_ptr<Item const> const& itemPtr) const {
  if (itemPtr)
    return itemPtr->descriptor().toJson();
  else
    return Json();
}

bool ItemDatabase::hasItem(String const& itemName) const {
  return m_items.contains(itemName);
}

std::expected<ItemType, StarException> ItemDatabase::itemType(String const& itemName) const {
  auto data = itemData(itemName);
  if (!data)
    return std::unexpected(std::move(data.error()));
  return data->get().type;
}

std::expected<String, StarException> ItemDatabase::itemFriendlyName(String const& itemName) const {
  auto data = itemData(itemName);
  if (!data)
    return std::unexpected(std::move(data.error()));
  return data->get().friendlyName;
}

std::expected<StringSet, StarException> ItemDatabase::itemTags(String const& itemName) const {
  auto data = itemData(itemName);
  if (!data)
    return std::unexpected(std::move(data.error()));
  return data->get().itemTags;
}

std::expected<ItemDatabase::ItemConfig, StarException> ItemDatabase::itemConfig(String const& itemName, Json parameters, std::optional<float> level, std::optional<std::uint64_t> seed) const {
  auto data = itemData(itemName);
  if (!data)
    return std::unexpected(std::move(data.error()));
  std::shared_ptr<Assets const> assets;
  if (data->get().assetsConfig) {
    auto sourceAssets = m_root.assets();
    if (!sourceAssets)
      return std::unexpected(std::move(sourceAssets.error()));
    assets = std::move(*sourceAssets);
  }
  auto loaded = [&]() -> std::expected<ItemConfig, StarException> {
  ItemConfig itemConfig;
  if (data->get().assetsConfig) {
    auto config = assets->json(*data->get().assetsConfig);
    if (!config)
      return std::unexpected(std::move(config.error()));
    itemConfig.config = std::move(*config);
  }
  itemConfig.directory = data->get().directory;
  itemConfig.config = jsonMerge(itemConfig.config, data->get().customConfig);
  itemConfig.parameters = std::move(parameters);
  auto builder = itemConfig.config.optString("builder");
  if (!builder)
    return std::unexpected(std::move(builder.error()));
  if (*builder) {
    std::unique_lock locker(m_luaMutex, std::defer_lock);
    lockWithDiagnostics(locker);
    auto context = m_luaRoot->createContext(**builder);
    if (!context)
      return std::unexpected(std::move(context.error()));
    auto rootCallbacks = LuaBindings::makeRootCallbacks(m_root, std::cref(*this));
    if (!rootCallbacks)
      return std::unexpected(std::move(rootCallbacks.error()));
    auto setRoot = context->setCallbacks("root", *rootCallbacks);
    if (!setRoot)
      return std::unexpected(std::move(setRoot.error()));
    auto utilityCallbacks = LuaBindings::makeUtilityCallbacks(m_root.random(), m_root.logger(), m_root.logMap());
    if (!utilityCallbacks)
      return std::unexpected(std::move(utilityCallbacks.error()));
    auto setUtility = context->setCallbacks("sb", *utilityCallbacks);
    if (!setUtility)
      return std::unexpected(std::move(setUtility.error()));
    auto built = context->invokePath<LuaTupleReturn<Json, Json>>(
        "build", itemConfig.directory, itemConfig.config, itemConfig.parameters, level, seed);
    if (!built)
      return std::unexpected(std::move(built.error()));
    itemConfig.config = std::move(std::get<0>(*built));
    itemConfig.parameters = std::move(std::get<1>(*built));
  }
  return itemConfig;
  }();
  if (!loaded && assets)
    return std::unexpected(assets->withAssetContext(std::move(loaded.error()), *data->get().assetsConfig, "Loading item configuration"));
  return loaded;
}

std::expected<std::optional<String>, StarException> ItemDatabase::itemFile(String const& itemName) const {
  if (!hasItem(itemName))
    return std::optional<String>();
  auto data = itemData(itemName);
  if (!data)
    return std::unexpected(std::move(data.error()));
  return std::optional<String>(data->get().directory + data->get().filename);
}

std::expected<std::shared_ptr<Item>, StarException> ItemDatabase::itemShared(ItemDescriptor descriptor, std::optional<float> level, std::optional<std::uint64_t> seed) const {
  if (!descriptor)
    return std::shared_ptr<Item>();
  ItemCacheEntry entry{descriptor, level, seed};
  std::unique_lock locker(m_cacheMutex);
  auto cached = m_itemCache.find_ref(entry);
  if (!cached)
    return std::unexpected(std::move(cached.error()));
  if (*cached)
    return (*cached)->get();
  locker.unlock();
  auto created = tryCreateItem(descriptor, level, seed);
  if (!created)
    return std::unexpected(std::move(created.error()));
  auto builtSeed = (*created)->parameters().optUInt("seed");
  if (!builtSeed)
    return std::unexpected(std::move(builtSeed.error()));
  std::get<2>(entry) = *builtSeed;
  locker.lock();
  auto stored = m_itemCache.get(entry, [&](ItemCacheEntry const&) -> std::shared_ptr<Item> { return std::move(*created); });
  if (!stored)
    return std::unexpected(std::move(stored.error()));
  return stored->get();
}

std::expected<std::shared_ptr<Item>, StarException> ItemDatabase::item(ItemDescriptor descriptor, std::optional<float> level, std::optional<std::uint64_t> seed, bool ignoreInvalid) const {
  if (!descriptor)
    return std::shared_ptr<Item>();
  return tryCreateItem(descriptor, level, seed, ignoreInvalid);
}

bool ItemDatabase::hasRecipeToMake(ItemDescriptor const& item) const {
  auto si = item.singular();
  for (auto const& recipe : m_recipes)
    if (recipe.output.singular() == si)
      return true;
  return false;
}

bool ItemDatabase::hasRecipeToMake(ItemDescriptor const& item, StringSet const& allowedTypes) const {
  auto si = item.singular();
  for (auto const& recipe : m_recipes)
    if (recipe.output.singular() == si)
      for (auto const& allowedType : allowedTypes)
        if (recipe.groups.contains(allowedType))
          return true;
  return false;
}

HashSet<ItemRecipe> ItemDatabase::recipesForOutputItem(String itemName) const {
  HashSet<ItemRecipe> result;
  for (auto const& recipe : m_recipes)
    if (recipe.output.name() == itemName)
      result.add(recipe);
  return result;
}

HashSet<ItemRecipe> ItemDatabase::recipesFromBagContents(List<std::shared_ptr<Item>> const& bag, StringMap<std::uint64_t> const& availableCurrencies) const {
  auto normalizedBag = normalizeBag(bag);
  return recipesFromBagContents(normalizedBag, availableCurrencies);
}

HashSet<ItemRecipe> ItemDatabase::recipesFromBagContents(HashMap<ItemDescriptor, std::uint64_t> const& bag, StringMap<std::uint64_t> const& availableCurrencies) const {
  return recipesFromSubset(bag, availableCurrencies, m_recipes);
}

HashSet<ItemRecipe> ItemDatabase::recipesFromBagContents(List<std::shared_ptr<Item>> const& bag, StringMap<std::uint64_t> const& availableCurrencies, StringSet const& allowedTypes) const {
  auto normalizedBag = normalizeBag(bag);
  return recipesFromBagContents(normalizedBag, availableCurrencies, allowedTypes);
}

HashSet<ItemRecipe> ItemDatabase::recipesFromBagContents(HashMap<ItemDescriptor, std::uint64_t> const& bag, StringMap<std::uint64_t> const& availableCurrencies, StringSet const& allowedTypes) const {
  return recipesFromSubset(bag, availableCurrencies, m_recipes, allowedTypes);
}

std::uint64_t ItemDatabase::maxCraftableInBag(List<std::shared_ptr<Item>> const& bag, StringMap<std::uint64_t> const& availableCurrencies, ItemRecipe const& recipe) const {
  auto normalizedBag = normalizeBag(bag);

  return maxCraftableInBag(normalizedBag, availableCurrencies, recipe);
}

std::uint64_t ItemDatabase::maxCraftableInBag(HashMap<ItemDescriptor, std::uint64_t> const& bag, StringMap<std::uint64_t> const& availableCurrencies, ItemRecipe const& recipe) const {
  std::uint64_t res = std::numeric_limits<std::uint64_t>::max();

  for (auto const& p : recipe.currencyInputs) {
    std::uint64_t available = availableCurrencies.value(p.first, 0);
    if (available == 0)
      return 0;
    else if (p.second > 0)
      res = std::min(available / p.second, res);
  }

  for (auto const& input : recipe.inputs) {
    auto found = bag.find(input.singular());
    if (found == bag.end())
      return 0;
    else if (input.count() > 0)
      res = std::min(found->second / input.count(), res);
  }

  return res;
}

ItemRecipe ItemDatabase::getPreciseRecipeForMaterials(String const& group, List<std::shared_ptr<Item>> const& bag, StringMap<std::uint64_t> const& availableCurrencies) const {
  // picks the recipe that:
  // * can be crafted (duh)
  // * uses all the input material types
  // * uses the most materials (if recipes exist with the same input materials)

  auto options = recipesFromBagContents(bag, availableCurrencies);
  ItemRecipe result;
  int ingredientsCount = 0;
  for (auto const& recipe : options) {
    if (!recipe.groups.contains(group))
      continue;
    bool usesAllItemTypes = true;
    for (auto const& item : bag) {
      bool match = false;
      for (auto const& input : recipe.inputs)
        if (item->matches(input, recipe.matchInputParameters))
          match = true;
      if (!match)
        usesAllItemTypes = false;
    }
    if (!usesAllItemTypes)
      continue;
    int count = 0;
    for (auto const& input : recipe.inputs)
      count += input.count();
    if (count > ingredientsCount)
      result = recipe;
  }
  return result;
}

std::expected<ItemRecipe, StarException> ItemDatabase::parseRecipe(Json const& config) const {
  auto parsed = [&]() -> std::expected<ItemRecipe, StarException> {
    ItemRecipe res;
    auto currencies = config.get("currencyInputs", JsonObject()).and_then([](Json const& value) {
      return jsonToMapV<StringMap<std::uint64_t>>(value, [](Json const& entry) { return entry.toUInt(); });
    });
    if (!currencies)
      return std::unexpected(std::move(currencies.error()));
    res.currencyInputs = std::move(*currencies);
    auto inputs = config.getArray("input");
    if (!inputs)
      return std::unexpected(std::move(inputs.error()));
    for (auto const& input : *inputs) {
      auto id = ItemDescriptor::create(input);
      if (!id)
        return std::unexpected(std::move(id.error()));
      auto type = itemType(id->name());
      if (!type)
        return std::unexpected(std::move(type.error()));
      if (*type == ItemType::CurrencyItem) {
        auto shared = itemShared(*id, {}, {});
        if (!shared)
          return std::unexpected(std::move(shared.error()));
        auto currencyItem = as<CurrencyItem>(*shared);
        res.currencyInputs[currencyItem->currencyType()] += currencyItem->totalValue();
      } else {
        res.inputs.append(std::move(*id));
      }
    }
    auto outputConfig = config.get("output");
    if (!outputConfig)
      return std::unexpected(std::move(outputConfig.error()));
    auto output = ItemDescriptor::create(*outputConfig);
    if (!output)
      return std::unexpected(std::move(output.error()));
    res.output = std::move(*output);
    auto assets = m_root.assets();
    if (!assets)
      return std::unexpected(std::move(assets.error()));
    auto defaultDuration = (*assets)->json("/items/defaultParameters.config:defaultCraftDuration").and_then([](Json const& value) { return value.toFloat(); });
    if (!defaultDuration)
      return std::unexpected((*assets)->withAssetContext(std::move(defaultDuration.error()), "/items/defaultParameters.config", "Loading recipe defaults"));
    auto duration = config.getFloat("duration", *defaultDuration);
    if (!duration)
      return std::unexpected(std::move(duration.error()));
    res.duration = *duration;
    auto groups = config.get("groups", JsonArray()).and_then([](Json const& value) { return jsonToStringSet(value); });
    if (!groups)
      return std::unexpected(std::move(groups.error()));
    res.groups = std::move(*groups);
    auto outputItem = itemShared(res.output, {}, {});
    if (!outputItem)
      return std::unexpected(std::move(outputItem.error()));
    if (*outputItem) {
      res.outputRarity = (*outputItem)->rarity();
      auto filter = guiFilterString(*outputItem);
      if (!filter)
        return std::unexpected(std::move(filter.error()));
      res.guiFilterString = std::move(*filter);
    }
    auto collectables = config.get("collectables", JsonObject()).and_then([](Json const& value) {
      return jsonToMapV<StringMap<String>>(value, [](Json const& entry) { return entry.toString(); });
    });
    if (!collectables)
      return std::unexpected(std::move(collectables.error()));
    res.collectables = std::move(*collectables);
    auto match = config.getBool("matchInputParameters", false);
    if (!match)
      return std::unexpected(std::move(match.error()));
    res.matchInputParameters = *match;
    return res;
  }();
  if (!parsed && parsed.error().is<JsonException>())
    return std::unexpected(RecipeException("Recipe missing required ingredient", ExceptionDiagnostic(parsed.error())));
  return parsed;
}

HashSet<ItemRecipe> const& ItemDatabase::allRecipes() const {
  return m_recipes;
}

HashSet<ItemRecipe> ItemDatabase::allRecipes(StringSet const& types) const {
  HashSet<ItemRecipe> res;
  for (auto const& i : m_recipes) {
    if (i.groups.hasIntersection(types))
      res.add(i);
  }
  return res;
}

std::expected<std::shared_ptr<Item>, StarException> ItemDatabase::applyAugment(std::shared_ptr<Item> const item, AugmentItem& augment) const {
  if (!item)
    return item;
  std::unique_lock locker(m_luaMutex, std::defer_lock);
  lockWithDiagnostics(locker);
  LuaBaseComponent script(m_root);
  auto setup = [&]() -> std::expected<void, StarException> {
    auto removed = script.removeCallbacks("root");
    if (!removed)
      return std::unexpected(std::move(removed.error()));
    auto rootCallbacks = LuaBindings::makeRootCallbacks(m_root, std::cref(*this));
    if (!rootCallbacks)
      return std::unexpected(std::move(rootCallbacks.error()));
    if (auto added = script.addCallbacks("root", std::move(*rootCallbacks)); !added)
      return std::unexpected(std::move(added.error()));
    if (auto set = script.setLuaRoot(m_luaRoot); !set)
      return std::unexpected(std::move(set.error()));
    auto scripts = augment.augmentScripts();
    if (!scripts)
      return std::unexpected(std::move(scripts.error()));
    if (auto set = script.setScripts(std::move(*scripts)); !set)
      return std::unexpected(std::move(set.error()));
    auto itemCallbacks = LuaBindings::makeItemCallbacks(augment);
    if (!itemCallbacks)
      return std::unexpected(std::move(itemCallbacks.error()));
    if (auto added = script.addCallbacks("item", std::move(*itemCallbacks)); !added)
      return std::unexpected(std::move(added.error()));
    auto configCallbacks = LuaBindings::makeConfigCallbacks(
      [&augment](String const& name, Json const& def) { return augment.instanceValue(name, def); });
    if (!configCallbacks)
      return std::unexpected(std::move(configCallbacks.error()));
    return script.addCallbacks("config", std::move(*configCallbacks));
  }();
  if (!setup)
    return std::unexpected(std::move(setup.error()));
  auto luaResult = invokeItemScript<LuaTupleReturn<Json, std::optional<std::uint64_t>>>(script, [&] {
    return script.invoke<LuaTupleReturn<Json, std::optional<std::uint64_t>>>("apply", item->descriptor().toJson());
  });
  locker.unlock();
  if (!luaResult)
    return std::unexpected(std::move(luaResult.error()));
  if (*luaResult && !std::get<0>(**luaResult).isNull()) {
    auto taken = augment.take(std::get<1>(**luaResult).value_or(1));
    if (!taken)
      return std::unexpected(std::move(taken.error()));
    auto descriptor = ItemDescriptor::create(std::get<0>(**luaResult));
    if (!descriptor)
      return std::unexpected(std::move(descriptor.error()));
    return ItemDatabase::item(std::move(*descriptor), {}, {}, false);
  }
  return item;
}

std::expected<bool, StarException> ItemDatabase::ageItem(std::shared_ptr<Item>& item, double aging) const {
  if (!item)
    return false;
  auto data = itemData(item->name());
  if (!data)
    return std::unexpected(std::move(data.error()));
  if (data->get().agingScripts.empty())
    return false;
  ItemDescriptor original = item->descriptor();
  std::unique_lock locker(m_luaMutex, std::defer_lock);
  lockWithDiagnostics(locker);
  LuaBaseComponent script(m_root);
  auto removed = script.removeCallbacks("root");
  if (!removed)
    return std::unexpected(std::move(removed.error()));
  auto callbacks = LuaBindings::makeRootCallbacks(m_root, std::cref(*this));
  if (!callbacks)
    return std::unexpected(std::move(callbacks.error()));
  if (auto added = script.addCallbacks("root", std::move(*callbacks)); !added)
    return std::unexpected(std::move(added.error()));
  if (auto set = script.setLuaRoot(m_luaRoot); !set)
    return std::unexpected(std::move(set.error()));
  if (auto set = script.setScripts(data->get().agingScripts); !set)
    return std::unexpected(std::move(set.error()));
  auto agedJson =
    invokeItemScript<Json>(script, [&] { return script.invoke<Json>("ageItem", original.toJson(), aging); });
  locker.unlock();
  if (!agedJson)
    return std::unexpected(std::move(agedJson.error()));
  if (!*agedJson)
    return false;
  auto aged = ItemDescriptor::create(**agedJson);
  if (!aged)
    return std::unexpected(std::move(aged.error()));
  if (*aged != original) {
    auto replacement = ItemDatabase::item(*aged, {}, {}, false);
    if (!replacement)
      return std::unexpected(std::move(replacement.error()));
    item = std::move(*replacement);
    return true;
  }
  return false;
}

List<String> ItemDatabase::allItems() const {
  return m_items.keys();
}

std::expected<std::shared_ptr<Item>, StarException> ItemDatabase::createItem(ItemType type, ItemConfig const& config) const {
  switch (type) {
  case ItemType::Generic: return GenericItem::create(m_root, config.config, config.directory, config.parameters);
  case ItemType::LiquidItem: return LiquidItem::create(m_root, config.config, config.directory, config.parameters);
  case ItemType::MaterialItem: return MaterialItem::create(m_root, config.config, config.directory, config.parameters);
  case ItemType::ObjectItem: return ObjectItem::create(m_root, config.config, config.directory, config.parameters);
  case ItemType::CurrencyItem: return CurrencyItem::create(m_root, config.config, config.directory);
  case ItemType::MiningTool: return MiningTool::create(m_root, config.config, config.directory, config.parameters);
  case ItemType::Flashlight: return Flashlight::create(m_root, config.config, config.directory, config.parameters);
  case ItemType::WireTool: return WireTool::create(m_root, config.config, config.directory, config.parameters);
  case ItemType::BeamMiningTool: return BeamMiningTool::create(m_root, config.config, config.directory, config.parameters);
  case ItemType::PaintingBeamTool: return PaintingBeamTool::create(m_root, config.config, config.directory, config.parameters);
  case ItemType::TillingTool: return TillingTool::create(m_root, config.config, config.directory, config.parameters);
  case ItemType::HarvestingTool: return HarvestingTool::create(m_root, config.config, config.directory, config.parameters);
  case ItemType::HeadArmor: return HeadArmor::create(m_root, config.config, config.directory, config.parameters);
  case ItemType::ChestArmor: return ChestArmor::create(m_root, config.config, config.directory, config.parameters);
  case ItemType::LegsArmor: return LegsArmor::create(m_root, config.config, config.directory, config.parameters);
  case ItemType::BackArmor: return BackArmor::create(m_root, config.config, config.directory, config.parameters);
  case ItemType::Consumable: return ConsumableItem::create(m_root, config.config, config.directory, config.parameters);
  case ItemType::Blueprint: return BlueprintItem::create(m_root, config.config, config.directory, config.parameters);
  case ItemType::Codex: return CodexItem::create(m_root, config.config, config.directory, config.parameters);
  case ItemType::InspectionTool: return InspectionTool::create(m_root, config.config, config.directory, config.parameters);
  case ItemType::InstrumentItem: return InstrumentItem::create(m_root, config.config, config.directory, config.parameters);
  case ItemType::ThrownItem: return ThrownItem::create(m_root, config.config, config.directory, config.parameters);
  case ItemType::UnlockItem: return UnlockItem::create(m_root, config.config, config.directory, config.parameters);
  case ItemType::ActiveItem: return ActiveItem::create(m_root, config.config, config.directory, config.parameters);
  case ItemType::AugmentItem: return AugmentItem::create(m_root, config.config, config.directory, config.parameters);
  default: return std::unexpected(ItemException::format("Unknown item type {}", static_cast<int>(type)));
  }
}

std::expected<std::shared_ptr<Item>, StarException> ItemDatabase::tryCreateItem(ItemDescriptor const& descriptor, std::optional<float> level, std::optional<std::uint64_t> seed, bool ignoreInvalid) const {
  auto instantiate = [&](ItemDescriptor const& current) -> std::expected<std::shared_ptr<Item>, StarException> {
    auto data = m_items.get(current.name());
    if (!data)
      return std::unexpected(std::move(data.error()));
    auto item = itemConfig(current.name(), current.parameters(), level, seed).and_then([&](ItemConfig const& config) {
      return createItem(data->get().type, config);
    });
    if (!item && !data->get().filename.empty()) {
      auto assets = m_root.assets();
      if (!assets)
        return std::unexpected(std::move(assets.error()));
      return std::unexpected((*assets)->withAssetContext(std::move(item.error()), data->get().directory + data->get().filename, "Constructing item"));
    }
    return item;
  };
  auto initial = [&]() -> std::expected<std::shared_ptr<Item>, StarException> {
    ItemDescriptor current = descriptor;
    if (current.name() == "perfectlygenericitem") {
      auto hasStored = current.parameters().contains("genericItemStorage");
      if (!hasStored)
        return std::unexpected(std::move(hasStored.error()));
      if (*hasStored) {
        auto stored = current.parameters().get("genericItemStorage");
        if (!stored)
          return std::unexpected(std::move(stored.error()));
        auto restored = ItemDescriptor::create(*stored);
        if (!restored)
          return std::unexpected(std::move(restored.error()));
        current = std::move(*restored);
      }
    }
    auto result = instantiate(current);
    if (!result)
      return std::unexpected(std::move(result.error()));
    (*result)->setCount(descriptor.count());
    return result;
  }();
  if (initial || ignoreInvalid)
    return initial;
  StarException originalError = std::move(initial.error());
  std::shared_ptr<Item> result;
  auto rebuilt = m_rebuilder->rebuild(descriptor.toJson(), originalError, [&](Json const& store) -> std::expected<void, StarException> {
    auto recoveredDescriptor = ItemDescriptor::create(store);
    if (!recoveredDescriptor)
      return std::unexpected(std::move(recoveredDescriptor.error()));
    auto recovered = instantiate(*recoveredDescriptor);
    if (!recovered)
      return std::unexpected(std::move(recovered.error()));
    (*recovered)->setCount(recoveredDescriptor->count());
    result = std::move(*recovered);
    return {};
  });
  if (!rebuilt)
    return std::unexpected(std::move(rebuilt.error()));
  if (*rebuilt)
    return result;
  auto logged = m_root.logger().errorException(
      strf("Could not {} item '{}' (count {})",
          descriptor.name() == "perfectlygenericitem" ? "re-instantiate" : "instantiate", descriptor.name(), descriptor.count()),
      ExceptionDiagnostic(originalError));
  if (!logged)
    return std::unexpected(std::move(logged.error()));
  if (m_root.logger().loggable(LogLevel::Debug)) {
    auto descriptorText = descriptor.toJson().repr();
    if (!descriptorText)
      return std::unexpected(std::move(descriptorText.error()));
    if (auto details = m_root.logger().debug("Item descriptor: {}", *descriptorText); !details)
      return std::unexpected(std::move(details.error()));
  }
  auto fallbackData = itemData("perfectlygenericitem");
  if (!fallbackData)
    return std::unexpected(std::move(fallbackData.error()));
  Json fallbackParameters = descriptor.parameters();
  std::optional<float> fallbackLevel = level;
  std::optional<std::uint64_t> fallbackSeed = seed;
  if (descriptor.name() != "perfectlygenericitem") {
    auto stored = descriptor.toJson().eraseKey("count");
    if (!stored)
      return std::unexpected(std::move(stored.error()));
    fallbackParameters = JsonObject{
      {"genericItemStorage", std::move(*stored)},
      {"shortdescription", descriptor.name()},
      {"description", "Reinstall the parent mod to return this item to normal"}
    };
    fallbackLevel.reset();
    fallbackSeed.reset();
  }
  auto fallbackConfig = itemConfig("perfectlygenericitem", std::move(fallbackParameters), fallbackLevel, fallbackSeed);
  if (!fallbackConfig)
    return std::unexpected(std::move(fallbackConfig.error()));
  auto fallback = createItem(fallbackData->get().type, *fallbackConfig);
  if (!fallback)
    return std::unexpected(std::move(fallback.error()));
  (*fallback)->setCount(descriptor.count());
  return fallback;
}

std::expected<std::reference_wrapper<ItemDatabase::ItemData const>, StarException> ItemDatabase::itemData(String const& name) const {
  if (auto p = m_items.find_ref(name))
    return *p;
  return std::unexpected(ItemException::format("No such item '{}'", name));
}

std::expected<ItemRecipe, StarException> ItemDatabase::makeRecipe(List<ItemDescriptor> inputs, ItemDescriptor output, float duration, StringSet groups) const {
  ItemRecipe res;
  res.inputs = std::move(inputs);
  res.output = std::move(output);
  res.duration = duration;
  res.groups = std::move(groups);
  auto item = itemShared(res.output, {}, {});
  if (!item)
    return std::unexpected(std::move(item.error()));
  if (*item) {
    res.outputRarity = (*item)->rarity();
    auto filter = guiFilterString(*item);
    if (!filter)
      return std::unexpected(std::move(filter.error()));
    res.guiFilterString = std::move(*filter);
  }
  return res;
}

std::expected<void, StarException> ItemDatabase::addItemSet(ItemType type, String const& extension) {
  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto files = (*assets)->scanExtension(extension);
  if (!files)
    return std::unexpected(std::move(files.error()));
  for (auto const& assetFile : files->get()) {
    auto const& file = assetFile.string();
    auto loaded = [&]() -> std::expected<ItemData, StarException> {
      auto config = (*assets)->json(file);
      if (!config)
        return std::unexpected(std::move(config.error()));
      ItemData data;
      data.type = type;
      data.assetsConfig = file;
      auto name = config->get("itemName").and_then([](Json const& value) { return value.toString(); });
      if (!name)
        return std::unexpected(std::move(name.error()));
      data.name = std::move(*name);
      auto friendlyName = config->getString("shortdescription", {});
      if (!friendlyName)
        return std::unexpected(std::move(friendlyName.error()));
      data.friendlyName = std::move(*friendlyName);
      auto tags = config->get("itemTags", JsonArray{}).and_then([](Json const& value) { return jsonToStringSet(value); });
      if (!tags)
        return std::unexpected(std::move(tags.error()));
      data.itemTags = std::move(*tags);
      auto scripts = config->get("itemAgingScripts", JsonArray{}).and_then([](Json const& value) { return jsonToStringList(value); });
      if (!scripts)
        return std::unexpected(std::move(scripts.error()));
      data.directory = AssetPath::directory(file);
      data.filename = AssetPath::filename(file);
      for (auto const& script : *scripts)
        data.agingScripts.append(AssetPath::relativeTo(data.directory, script));
      return data;
    }();
    if (!loaded)
      return std::unexpected(ItemException(strf("Could not load item asset {}", file), ExceptionDiagnostic((*assets)->withAssetContext(std::move(loaded.error()), file, "Loading item asset"))));
    if (m_items.contains(loaded->name))
      return std::unexpected((*assets)->withAssetContext(ItemException::format("Duplicate item name '{}' found", loaded->name), file, "Loading item asset"));
    m_items[loaded->name] = std::move(*loaded);
  }
  return {};
}

std::expected<void, StarException> ItemDatabase::addObjectDropItem(String const& objectPath, Json const& objectConfig) {
  ItemData data;
  data.type = ItemType::ObjectItem;
  auto name = objectConfig.get("objectName").and_then([](Json const& value) { return value.toString(); });
  if (!name)
    return std::unexpected(std::move(name.error()));
  data.name = std::move(*name);
  auto friendlyName = objectConfig.getString("shortdescription", {});
  if (!friendlyName)
    return std::unexpected(std::move(friendlyName.error()));
  data.friendlyName = std::move(*friendlyName);
  auto tags = objectConfig.get("itemTags", JsonArray{}).and_then([](Json const& value) { return jsonToStringSet(value); });
  if (!tags)
    return std::unexpected(std::move(tags.error()));
  data.itemTags = std::move(*tags);
  auto scripts = objectConfig.get("itemAgingScripts", JsonArray{}).and_then([](Json const& value) { return jsonToStringList(value); });
  if (!scripts)
    return std::unexpected(std::move(scripts.error()));
  data.agingScripts = std::move(*scripts);
  data.directory = AssetPath::directory(objectPath);
  data.filename = AssetPath::filename(objectPath);
  auto configObject = objectConfig.toObject();
  if (!configObject)
    return std::unexpected(std::move(configObject.error()));
  auto customConfig = std::move(*configObject);
  if (!customConfig.contains("inventoryIcon")) {
    auto assets = m_root.assets();
    if (!assets)
      return std::unexpected(std::move(assets.error()));
    auto icon = (*assets)->json("/objects/defaultParameters.config:missingIcon");
    if (!icon)
      return std::unexpected((*assets)->withAssetContext(std::move(icon.error()), "/objects/defaultParameters.config", "Loading object item defaults"));
    customConfig["inventoryIcon"] = std::move(*icon);
    auto logged = m_root.logger().warn("Missing inventoryIcon for {}, using default", data.name);
    if (!logged)
      return std::unexpected(std::move(logged.error()));
  }
  customConfig["itemName"] = data.name;
  if (!customConfig.contains("tooltipKind"))
    customConfig["tooltipKind"] = "object";
  if (!customConfig.contains("printable"))
    customConfig["printable"] = customConfig.contains("price");
  // Object scripts cannot run in the fireable ObjectItem's script context.
  customConfig.remove("scripts");
  data.customConfig = std::move(customConfig);
  if (m_items.contains(data.name))
    return std::unexpected(ItemException::format("Object drop '{}' shares name with existing item", data.name));
  m_items[data.name] = std::move(data);
  return {};
}

std::expected<void, StarException> ItemDatabase::scanItems() {
  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  List<std::pair<ItemType, String>> itemSets{
    {ItemType::Generic, "item"},
    {ItemType::LiquidItem, "liqitem"},
    {ItemType::MaterialItem, "matitem"},
    {ItemType::MiningTool, "miningtool"},
    {ItemType::Flashlight, "flashlight"},
    {ItemType::WireTool, "wiretool"},
    {ItemType::BeamMiningTool, "beamaxe"},
    {ItemType::TillingTool, "tillingtool"},
    {ItemType::PaintingBeamTool, "painttool"},
    {ItemType::HarvestingTool, "harvestingtool"},
    {ItemType::HeadArmor, "head"},
    {ItemType::ChestArmor, "chest"},
    {ItemType::LegsArmor, "legs"},
    {ItemType::BackArmor, "back"},
    {ItemType::CurrencyItem, "currency"},
    {ItemType::Consumable, "consumable"},
    {ItemType::Blueprint, "blueprint"},
    {ItemType::InspectionTool, "inspectiontool"},
    {ItemType::InstrumentItem, "instrument"},
    {ItemType::ThrownItem, "thrownitem"},
    {ItemType::UnlockItem, "unlock"},
    {ItemType::ActiveItem, "activeitem"},
    {ItemType::AugmentItem, "augment"}
  };
  for (auto const& itemSet : itemSets) {
    auto files = (*assets)->scanExtension(itemSet.second);
    if (!files)
      return std::unexpected(std::move(files.error()));
    auto queued = (*assets)->queueJsons(files->get());
    if (!queued)
      return std::unexpected(std::move(queued.error()));
  }
  for (auto const& itemSet : itemSets) {
    auto added = addItemSet(itemSet.first, itemSet.second);
    if (!added)
      return std::unexpected(std::move(added.error()));
  }
  return {};
}

std::expected<void, StarException> ItemDatabase::addObjectItems() {
  auto objects = m_root.objectDatabase();
  if (!objects)
    return std::unexpected(std::move(objects.error()));
  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  for (auto const& objectName : (*objects)->allObjects()) {
    auto config = (*objects)->getConfig(objectName);
    if (!config)
      return std::unexpected(std::move(config.error()));
    if ((*config)->hasObjectItem) {
      auto added = addObjectDropItem((*config)->path, (*config)->config);
      if (!added)
        return std::unexpected((*assets)->withAssetContext(std::move(added.error()), (*config)->path, "Loading object drop item"));
    }
  }
  return {};
}

std::expected<void, StarException> ItemDatabase::scanRecipes() {
  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto files = (*assets)->scanExtension("recipe");
  if (!files)
    return std::unexpected(std::move(files.error()));
  auto queued = (*assets)->queueJsons(files->get());
  if (!queued)
    return std::unexpected(std::move(queued.error()));
  for (auto const& assetFile : files->get()) {
    auto const& file = assetFile.string();
    auto recipe = (*assets)->json(file).and_then([&](Json const& config) { return parseRecipe(config); });
    if (recipe) {
      m_recipes.add(std::move(*recipe));
    } else {
      auto logged = m_root.logger().errorException(strf("Could not load recipe {}", file), ExceptionDiagnostic((*assets)->withAssetContext(std::move(recipe.error()), file, "Loading recipe")));
      if (!logged)
        return std::unexpected(std::move(logged.error()));
    }
  }
  return {};
}

std::expected<void, StarException> ItemDatabase::addBlueprints() {
  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  for (auto const& recipe : m_recipes) {
    auto baseDesc = recipe.output;
    auto baseItem = itemShared(baseDesc, {}, {});
    if (!baseItem)
      return std::unexpected(std::move(baseItem.error()));
    String blueprintName = strf("{}-recipe", (*baseItem)->name());
    if (m_items.contains(blueprintName))
      continue;
    String sourcePath;
    auto created = [&]() -> std::expected<void, StarException> {
      auto baseData = itemData(baseDesc.name());
      if (!baseData)
        return std::unexpected(std::move(baseData.error()));
      if (!baseData->get().filename.empty())
        sourcePath = baseData->get().directory + baseData->get().filename;
      ItemData data;
      data.type = ItemType::Blueprint;
      JsonObject config;
      config["recipe"] = baseDesc.singular().toJson();
      auto description = (*assets)->json("/blueprint.config:description").and_then([](Json const& value) { return value.toString(); });
      if (!description)
        return std::unexpected((*assets)->withAssetContext(std::move(description.error()), "/blueprint.config", "Loading blueprint defaults"));
      auto replacedDescription = description->replace("<item>", (*baseItem)->friendlyName());
      if (!replacedDescription)
        return std::unexpected(std::move(replacedDescription.error()));
      config["description"] = std::move(*replacedDescription);
      auto shortDescription = (*assets)->json("/blueprint.config:shortdescription").and_then([](Json const& value) { return value.toString(); });
      if (!shortDescription)
        return std::unexpected((*assets)->withAssetContext(std::move(shortDescription.error()), "/blueprint.config", "Loading blueprint defaults"));
      auto replacedShortDescription = shortDescription->replace("<item>", (*baseItem)->friendlyName());
      if (!replacedShortDescription)
        return std::unexpected(std::move(replacedShortDescription.error()));
      data.friendlyName = std::move(*replacedShortDescription);
      config["shortdescription"] = data.friendlyName;
      auto category = (*assets)->json("/blueprint.config:category").and_then([](Json const& value) { return value.toString(); });
      if (!category)
        return std::unexpected((*assets)->withAssetContext(std::move(category.error()), "/blueprint.config", "Loading blueprint defaults"));
      config["category"] = std::move(*category);
      data.name = blueprintName;
      config["itemName"] = data.name;
      auto icon = (*baseItem)->instanceValue("inventoryIcon", false);
      if (!icon)
        return std::unexpected(std::move(icon.error()));
      if (*icon) {
        auto inventoryIcon = (*baseItem)->instanceValue("inventoryIcon");
        if (!inventoryIcon)
          return std::unexpected(std::move(inventoryIcon.error()));
        config["inventoryIcon"] = std::move(*inventoryIcon);
      }
      auto rarity = enumName<RarityNames>((*baseItem)->rarity());
      if (!rarity)
        return std::unexpected(std::move(rarity.error()));
      config["rarity"] = String(*rarity);
      config["price"] = (*baseItem)->price();
      data.customConfig = std::move(config);
      data.directory = baseData->get().directory;
      m_items[data.name] = std::move(data);
      return {};
    }();
    if (!created) {
      auto error = std::move(created.error());
      if (!sourcePath.empty())
        error = (*assets)->withAssetContext(std::move(error), sourcePath, "Creating blueprint item");
      auto logged = m_root.logger().errorException("Could not create blueprint item from recipe", ExceptionDiagnostic(error));
      if (!logged)
        return std::unexpected(std::move(logged.error()));
    }
  }
  return {};
}

std::expected<void, StarException> ItemDatabase::addCodexes() {
  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto codexConfig = (*assets)->json("/codex.config");
  if (!codexConfig)
    return std::unexpected((*assets)->withAssetContext(std::move(codexConfig.error()), "/codex.config", "Loading codex item defaults"));
  auto codexDatabase = m_root.codexDatabase();
  if (!codexDatabase)
    return std::unexpected(std::move(codexDatabase.error()));
  for (auto const& codexPair : (*codexDatabase)->codexes()) {
    String codexItemName = strf("{}-codex", codexPair.second->id());
    if (m_items.contains(codexItemName)) {
      auto logged = m_root.logger().warn("Couldn't create codex item {} because an item with that name is already defined", codexItemName);
      if (!logged)
        return std::unexpected(std::move(logged.error()));
      continue;
    }
    auto created = [&]() -> std::expected<void, StarException> {
      ItemData data;
      data.type = ItemType::Codex;
      data.name = codexItemName;
      data.friendlyName = codexPair.second->title();
      data.directory = codexPair.second->directory();
      data.filename = codexPair.second->filename();
      auto defaults = codexConfig->get("defaultItemConfig");
      if (!defaults)
        return std::unexpected((*assets)->withAssetContext(std::move(defaults.error()), "/codex.config", "Loading codex item defaults"));
      auto merged = jsonMerge(*defaults, codexPair.second->itemConfig()).toObject();
      if (!merged)
        return std::unexpected(std::move(merged.error()));
      auto config = std::move(*merged);
      config["itemName"] = codexItemName;
      config["codexId"] = codexPair.second->id();
      config["shortdescription"] = codexPair.second->title();
      config["description"] = codexPair.second->description();
      config["codexIcon"] = codexPair.second->icon();
      data.customConfig = std::move(config);
      m_items[codexItemName] = std::move(data);
      return {};
    }();
    if (!created) {
      auto logged = m_root.logger().errorException(strf("Could not create item for codex {}", codexPair.second->id()), ExceptionDiagnostic((*assets)->withAssetContext(std::move(created.error()), codexPair.second->directory() + codexPair.second->filename(), "Creating codex item")));
      if (!logged)
        return std::unexpected(std::move(logged.error()));
    }
  }
  return {};
}

}
