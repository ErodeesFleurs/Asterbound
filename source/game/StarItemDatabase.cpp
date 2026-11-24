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
import star.player;
import star.player_inventory;
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
      return std::unexpected(cleaned.error().with_context(Star::print_exception(result.error(), true)));
    return std::unexpected(std::move(cleaned.error()));
  }
  return result;
}

}// namespace

std::uint64_t ItemDatabase::get_count_of_item(List<std::shared_ptr<Item>> const& bag, ItemDescriptor const& item, bool exactMatch) {
  auto normalizedBag = normalize_bag(bag);
  return get_count_of_item(normalizedBag, item, exactMatch);
}

std::uint64_t ItemDatabase::get_count_of_item(RecipeStock const& bag, ItemDescriptor const& item, bool exactMatch) {
  return exactMatch ? bag.by_parameters.value(item.singular(), 0) : bag.by_name.value(item.name(), 0);
}

RecipeStock ItemDatabase::normalize_bag(List<std::shared_ptr<Item>> const& bag) {
  RecipeStock stock;
  for (auto const& item : bag) {
    if (item)
      stock.add(item->descriptor());
  }
  return stock;
}

HashSet<ItemRecipe> ItemDatabase::recipesFromSubset(RecipeStock const& normalizedBag, StringMap<std::uint64_t> const& availableCurrencies, HashSet<ItemRecipe> const& subset) {
  HashSet<ItemRecipe> res;
  for (auto const& recipe : subset) {
    // add this recipe if we can make it.
    if (can_make_recipe(recipe, normalizedBag, availableCurrencies))
      res.add(recipe);
  }

  return res;
}

HashSet<ItemRecipe> ItemDatabase::recipesFromSubset(RecipeStock const& normalizedBag, StringMap<std::uint64_t> const& availableCurrencies,
    HashSet<ItemRecipe> const& subset, StringSet const& allowedTypes) {
  HashSet<ItemRecipe> res;
  for (auto const& recipe : subset) {
    // is it the right kind of recipe for this check ?
    if (recipe.groups.has_intersection(allowedTypes) || allowedTypes.empty() || recipe.groups.empty()) {
      // do we have the ingredients to make it.
      if (can_make_recipe(recipe, normalizedBag, availableCurrencies)) {
        res.add(recipe);
      }
    }
  }

  return res;
}

std::expected<String, StarException> ItemDatabase::guiFilterString(std::shared_ptr<Item> const& item) {
  auto lowered = (item->name() + item->friendlyName() + item->description()).to_lower();
  if (!lowered)
    return std::unexpected(std::move(lowered.error()));
  auto split = lowered->split_any(" ,.?*\\+/|\t");
  if (!split)
    return std::unexpected(std::move(split.error()));
  return split->join("");
}

std::expected<bool, StarException> ItemDatabase::craft_recipe(Player& player, ItemRecipe const& recipe, std::uint64_t batches) const {
  if (!batches || recipe.output.count() > std::numeric_limits<std::uint64_t>::max() / batches)
    return false;
  if (!player.isAdmin()) {
    auto consumed = player.inventory()->consume_recipe(recipe, batches);
    if (!consumed || !*consumed)
      return consumed;
  }
  auto remaining = recipe.output.count() * batches;
  while (remaining) {
    auto product = item(recipe.output.withCount(remaining));
    if (!product)
      return std::unexpected(std::move(product.error()));
    if (!*product || !(*product)->count() || (*product)->count() > remaining)
      return std::unexpected(RecipeException("Invalid crafted stack size"));
    remaining -= (*product)->count();
    if (auto given = player.giveItem(*product); !given)
      return std::unexpected(std::move(given.error()));
    for (auto const& [collection, name] : recipe.collectables) {
      if (auto added = player.add_collectable(collection, name); !added)
        return std::unexpected(std::move(added.error()));
    }
  }
  return true;
}

bool ItemDatabase::can_make_recipe(ItemRecipe const& recipe, RecipeStock const& availableIngredients, StringMap<std::uint64_t> const& availableCurrencies) {
  return max_craftable_in_bag(availableIngredients, availableCurrencies, recipe) != 0;
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
  auto codexes = database->add_codexes();
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
  itemConfig.config = json_merge(itemConfig.config, data->get().customConfig);
  itemConfig.parameters = std::move(parameters);
  auto builder = itemConfig.config.opt_string("builder");
  if (!builder)
    return std::unexpected(std::move(builder.error()));
  if (*builder) {
    std::unique_lock locker(m_luaMutex, std::defer_lock);
    lock_with_diagnostics(locker);
    auto context = m_luaRoot->create_context(**builder);
    if (!context)
      return std::unexpected(std::move(context.error()));
    auto rootCallbacks = LuaBindings::makeRootCallbacks(m_root, std::cref(*this));
    if (!rootCallbacks)
      return std::unexpected(std::move(rootCallbacks.error()));
    auto setRoot = context->set_callbacks("root", *rootCallbacks);
    if (!setRoot)
      return std::unexpected(std::move(setRoot.error()));
    auto utilityCallbacks = LuaBindings::make_utility_callbacks(m_root.random(), m_root.logger(), m_root.logMap());
    if (!utilityCallbacks)
      return std::unexpected(std::move(utilityCallbacks.error()));
    auto setUtility = context->set_callbacks("sb", *utilityCallbacks);
    if (!setUtility)
      return std::unexpected(std::move(setUtility.error()));
    auto built = context->invoke_path<LuaTupleReturn<Json, Json>>(
        "build", itemConfig.directory, itemConfig.config, itemConfig.parameters, level, seed);
    if (!built)
      return std::unexpected(std::move(built.error()));
    itemConfig.config = std::move(std::get<0>(*built));
    itemConfig.parameters = std::move(std::get<1>(*built));
  }
  return itemConfig;
  }();
  if (!loaded && assets)
    return std::unexpected(assets->with_asset_context(std::move(loaded.error()), *data->get().assetsConfig, "Loading item configuration"));
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
  auto builtSeed = (*created)->parameters().opt_uint("seed");
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
  auto normalizedBag = normalize_bag(bag);
  return recipesFromBagContents(normalizedBag, availableCurrencies);
}

HashSet<ItemRecipe> ItemDatabase::recipesFromBagContents(RecipeStock const& bag, StringMap<std::uint64_t> const& availableCurrencies) const {
  return recipesFromSubset(bag, availableCurrencies, m_recipes);
}

HashSet<ItemRecipe> ItemDatabase::recipesFromBagContents(List<std::shared_ptr<Item>> const& bag, StringMap<std::uint64_t> const& availableCurrencies, StringSet const& allowedTypes) const {
  auto normalizedBag = normalize_bag(bag);
  return recipesFromBagContents(normalizedBag, availableCurrencies, allowedTypes);
}

HashSet<ItemRecipe> ItemDatabase::recipesFromBagContents(RecipeStock const& bag, StringMap<std::uint64_t> const& availableCurrencies, StringSet const& allowedTypes) const {
  return recipesFromSubset(bag, availableCurrencies, m_recipes, allowedTypes);
}

std::uint64_t ItemDatabase::max_craftable_in_bag(List<std::shared_ptr<Item>> const& bag, StringMap<std::uint64_t> const& availableCurrencies, ItemRecipe const& recipe) const {
  auto normalizedBag = normalize_bag(bag);

  return max_craftable_in_bag(normalizedBag, availableCurrencies, recipe);
}

std::uint64_t ItemDatabase::max_craftable_in_bag(RecipeStock const& stock, StringMap<std::uint64_t> const& currencies, ItemRecipe const& recipe) {
  auto limit = std::numeric_limits<std::uint64_t>::max();
  if (recipe.output.count())
    limit /= recipe.output.count();
  for (auto const& [name, amount] : recipe.currencyInputs) {
    if (amount)
      limit = std::min(limit, currencies.value(name, 0) / amount);
  }
  if (!limit || recipe.inputs.empty())
    return limit;
  if (recipe.inputs.size() == 1) {
    auto const& input = recipe.inputs.front();
    return input.count() ? std::min(limit, get_count_of_item(stock, input, recipe.matchInputParameters) / input.count()) : limit;
  }
  auto aggregate = [&](auto const& available, std::invocable<ItemDescriptor const&> auto key) {
    std::remove_cvref_t<decltype(available)> required;
    for (auto const& input : recipe.inputs) {
      if (!input.count())
        continue;
      auto&& id = key(input);
      auto& amount = required[id];
      if (input.count() > std::numeric_limits<std::uint64_t>::max() - amount)
        return std::uint64_t{0};
      amount += input.count();
      limit = std::min(limit, available.value(id, 0) / amount);
      if (!limit)
        break;
    }
    return limit;
  };
  if (recipe.matchInputParameters)
    return aggregate(stock.by_parameters, [](ItemDescriptor const& input) { return input.singular(); });
  return aggregate(stock.by_name, [](ItemDescriptor const& input) -> String const& { return input.name(); });
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
      return json_to_map_v<StringMap<std::uint64_t>>(value, [](Json const& entry) { return entry.to_uint(); });
    });
    if (!currencies)
      return std::unexpected(std::move(currencies.error()));
    res.currencyInputs = std::move(*currencies);
    auto inputs = config.get_array("input");
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
        auto& total = res.currencyInputs[currencyItem->currencyType()];
        auto value = currencyItem->currencyValue();
        if (value && id->count() > (std::numeric_limits<std::uint64_t>::max() - total) / value)
          return std::unexpected(RecipeException("Currency requirement exceeds supported range"));
        total += id->count() * value;
      } else {
        res.inputs.push_back(std::move(*id));
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
    auto defaultDuration = (*assets)->json("/items/defaultParameters.config:defaultCraftDuration").and_then([](Json const& value) { return value.to_float(); });
    if (!defaultDuration)
      return std::unexpected((*assets)->with_asset_context(std::move(defaultDuration.error()), "/items/defaultParameters.config", "Loading recipe defaults"));
    auto duration = config.get_float("duration", *defaultDuration);
    if (!duration)
      return std::unexpected(std::move(duration.error()));
    res.duration = *duration;
    auto groups = config.get("groups", JsonArray()).and_then([](Json const& value) { return json_to_string_set(value); });
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
      return json_to_map_v<StringMap<String>>(value, [](Json const& entry) { return entry.to_string(); });
    });
    if (!collectables)
      return std::unexpected(std::move(collectables.error()));
    res.collectables = std::move(*collectables);
    auto match = config.get_bool("matchInputParameters", false);
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
    if (i.groups.has_intersection(types))
      res.add(i);
  }
  return res;
}

std::expected<std::shared_ptr<Item>, StarException> ItemDatabase::applyAugment(std::shared_ptr<Item> const item, AugmentItem& augment) const {
  if (!item)
    return item;
  std::unique_lock locker(m_luaMutex, std::defer_lock);
  lock_with_diagnostics(locker);
  LuaBaseComponent script(m_root);
  auto setup = [&]() -> std::expected<void, StarException> {
    auto removed = script.remove_callbacks("root");
    if (!removed)
      return std::unexpected(std::move(removed.error()));
    auto rootCallbacks = LuaBindings::makeRootCallbacks(m_root, std::cref(*this));
    if (!rootCallbacks)
      return std::unexpected(std::move(rootCallbacks.error()));
    if (auto added = script.add_callbacks("root", std::move(*rootCallbacks)); !added)
      return std::unexpected(std::move(added.error()));
    if (auto set = script.set_lua_root(m_luaRoot); !set)
      return std::unexpected(std::move(set.error()));
    auto scripts = augment.augmentScripts();
    if (!scripts)
      return std::unexpected(std::move(scripts.error()));
    if (auto set = script.set_scripts(std::move(*scripts)); !set)
      return std::unexpected(std::move(set.error()));
    auto itemCallbacks = LuaBindings::makeItemCallbacks(augment);
    if (!itemCallbacks)
      return std::unexpected(std::move(itemCallbacks.error()));
    if (auto added = script.add_callbacks("item", std::move(*itemCallbacks)); !added)
      return std::unexpected(std::move(added.error()));
    auto configCallbacks = LuaBindings::makeConfigCallbacks(
      [&augment](String const& name, Json const& def) { return augment.instanceValue(name, def); });
    if (!configCallbacks)
      return std::unexpected(std::move(configCallbacks.error()));
    return script.add_callbacks("config", std::move(*configCallbacks));
  }();
  if (!setup)
    return std::unexpected(std::move(setup.error()));
  auto luaResult = invokeItemScript<LuaTupleReturn<Json, std::optional<std::uint64_t>>>(script, [&] {
    return script.invoke<LuaTupleReturn<Json, std::optional<std::uint64_t>>>("apply", item->descriptor().toJson());
  });
  locker.unlock();
  if (!luaResult)
    return std::unexpected(std::move(luaResult.error()));
  if (*luaResult && !std::get<0>(**luaResult).is_null()) {
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
  lock_with_diagnostics(locker);
  LuaBaseComponent script(m_root);
  auto removed = script.remove_callbacks("root");
  if (!removed)
    return std::unexpected(std::move(removed.error()));
  auto callbacks = LuaBindings::makeRootCallbacks(m_root, std::cref(*this));
  if (!callbacks)
    return std::unexpected(std::move(callbacks.error()));
  if (auto added = script.add_callbacks("root", std::move(*callbacks)); !added)
    return std::unexpected(std::move(added.error()));
  if (auto set = script.set_lua_root(m_luaRoot); !set)
    return std::unexpected(std::move(set.error()));
  if (auto set = script.set_scripts(data->get().agingScripts); !set)
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
      return std::unexpected((*assets)->with_asset_context(std::move(item.error()), data->get().directory + data->get().filename, "Constructing item"));
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
  auto logged = m_root.logger().error_exception(
    Star::format("Could not {} item '{}' (count {})",
                 descriptor.name() == "perfectlygenericitem" ? "re-instantiate" : "instantiate", descriptor.name(),
                 descriptor.count()),
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
    auto stored = descriptor.toJson().erase("count");
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
  auto files = (*assets)->scan_extension(extension);
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
      auto name = config->get("itemName").and_then([](Json const& value) { return value.to_string(); });
      if (!name)
        return std::unexpected(std::move(name.error()));
      data.name = std::move(*name);
      auto friendlyName = config->get_string("shortdescription", {});
      if (!friendlyName)
        return std::unexpected(std::move(friendlyName.error()));
      data.friendlyName = std::move(*friendlyName);
      auto tags = config->get("itemTags", JsonArray{}).and_then([](Json const& value) { return json_to_string_set(value); });
      if (!tags)
        return std::unexpected(std::move(tags.error()));
      data.itemTags = std::move(*tags);
      auto scripts = config->get("itemAgingScripts", JsonArray{}).and_then([](Json const& value) { return json_to_string_list(value); });
      if (!scripts)
        return std::unexpected(std::move(scripts.error()));
      data.directory = AssetPath::directory(file);
      data.filename = AssetPath::filename(file);
      for (auto const& script : *scripts)
        data.agingScripts.push_back(AssetPath::relative_to(data.directory, script));
      return data;
    }();
    if (!loaded)
      return std::unexpected(ItemException(Star::format("Could not load item asset {}", file), ExceptionDiagnostic((*assets)->with_asset_context(std::move(loaded.error()), file, "Loading item asset"))));
    if (m_items.contains(loaded->name))
      return std::unexpected((*assets)->with_asset_context(ItemException::format("Duplicate item name '{}' found", loaded->name), file, "Loading item asset"));
    m_items[loaded->name] = std::move(*loaded);
  }
  return {};
}

std::expected<void, StarException> ItemDatabase::addObjectDropItem(String const& objectPath, Json const& objectConfig) {
  ItemData data;
  data.type = ItemType::ObjectItem;
  auto name = objectConfig.get("objectName").and_then([](Json const& value) { return value.to_string(); });
  if (!name)
    return std::unexpected(std::move(name.error()));
  data.name = std::move(*name);
  auto friendlyName = objectConfig.get_string("shortdescription", {});
  if (!friendlyName)
    return std::unexpected(std::move(friendlyName.error()));
  data.friendlyName = std::move(*friendlyName);
  auto tags = objectConfig.get("itemTags", JsonArray{}).and_then([](Json const& value) { return json_to_string_set(value); });
  if (!tags)
    return std::unexpected(std::move(tags.error()));
  data.itemTags = std::move(*tags);
  auto scripts = objectConfig.get("itemAgingScripts", JsonArray{}).and_then([](Json const& value) { return json_to_string_list(value); });
  if (!scripts)
    return std::unexpected(std::move(scripts.error()));
  data.agingScripts = std::move(*scripts);
  data.directory = AssetPath::directory(objectPath);
  data.filename = AssetPath::filename(objectPath);
  auto configObject = objectConfig.to_object();
  if (!configObject)
    return std::unexpected(std::move(configObject.error()));
  auto customConfig = std::move(*configObject);
  if (!customConfig.contains("inventoryIcon")) {
    auto assets = m_root.assets();
    if (!assets)
      return std::unexpected(std::move(assets.error()));
    auto icon = (*assets)->json("/objects/defaultParameters.config:missingIcon");
    if (!icon)
      return std::unexpected((*assets)->with_asset_context(std::move(icon.error()), "/objects/defaultParameters.config", "Loading object item defaults"));
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
    auto files = (*assets)->scan_extension(itemSet.second);
    if (!files)
      return std::unexpected(std::move(files.error()));
    auto queued = (*assets)->queue_jsons(files->get());
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
        return std::unexpected((*assets)->with_asset_context(std::move(added.error()), (*config)->path, "Loading object drop item"));
    }
  }
  return {};
}

std::expected<void, StarException> ItemDatabase::scanRecipes() {
  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto files = (*assets)->scan_extension("recipe");
  if (!files)
    return std::unexpected(std::move(files.error()));
  auto queued = (*assets)->queue_jsons(files->get());
  if (!queued)
    return std::unexpected(std::move(queued.error()));
  for (auto const& assetFile : files->get()) {
    auto const& file = assetFile.string();
    auto recipe = (*assets)->json(file).and_then([&](Json const& config) { return parseRecipe(config); });
    if (recipe) {
      m_recipes.add(std::move(*recipe));
    } else {
      auto logged = m_root.logger().error_exception(
        Star::format("Could not load recipe {}", file),
        ExceptionDiagnostic((*assets)->with_asset_context(std::move(recipe.error()), file, "Loading recipe")));
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
    String blueprintName = Star::format("{}-recipe", (*baseItem)->name());
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
      auto description = (*assets)->json("/blueprint.config:description").and_then([](Json const& value) { return value.to_string(); });
      if (!description)
        return std::unexpected((*assets)->with_asset_context(std::move(description.error()), "/blueprint.config", "Loading blueprint defaults"));
      auto replacedDescription = description->replace("<item>", (*baseItem)->friendlyName());
      if (!replacedDescription)
        return std::unexpected(std::move(replacedDescription.error()));
      config["description"] = std::move(*replacedDescription);
      auto shortDescription = (*assets)->json("/blueprint.config:shortdescription").and_then([](Json const& value) { return value.to_string(); });
      if (!shortDescription)
        return std::unexpected((*assets)->with_asset_context(std::move(shortDescription.error()), "/blueprint.config", "Loading blueprint defaults"));
      auto replacedShortDescription = shortDescription->replace("<item>", (*baseItem)->friendlyName());
      if (!replacedShortDescription)
        return std::unexpected(std::move(replacedShortDescription.error()));
      data.friendlyName = std::move(*replacedShortDescription);
      config["shortdescription"] = data.friendlyName;
      auto category = (*assets)->json("/blueprint.config:category").and_then([](Json const& value) { return value.to_string(); });
      if (!category)
        return std::unexpected((*assets)->with_asset_context(std::move(category.error()), "/blueprint.config", "Loading blueprint defaults"));
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
      auto rarity = enum_name<rarity_names>((*baseItem)->rarity());
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
        error = (*assets)->with_asset_context(std::move(error), sourcePath, "Creating blueprint item");
      auto logged =
        m_root.logger().error_exception("Could not create blueprint item from recipe", ExceptionDiagnostic(error));
      if (!logged)
        return std::unexpected(std::move(logged.error()));
    }
  }
  return {};
}

std::expected<void, StarException> ItemDatabase::add_codexes() {
  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/codex.config");
  if (!config)
    return std::unexpected((*assets)->with_asset_context(std::move(config.error()), "/codex.config", "Loading codex item defaults"));
  auto database = m_root.codex_database();
  if (!database)
    return std::unexpected(std::move(database.error()));

  // Only non-conflicting books need the defaults; a failure is reported for each affected book.
  std::optional<std::expected<Json, StarException>> defaults;
  constexpr std::string_view suffix = "-codex";
  for (auto const& [id, book] : (*database)->entries()) {
    String name;
    name.reserve(id.utf8().size() + suffix.size());
    name.append(id);
    name.append(suffix);
    auto [entry, inserted] = m_items.try_emplace(std::move(name));
    if (!inserted) {
      auto logged = m_root.logger().warn("Couldn't create codex item {} because an item with that name is already defined", entry->first);
      if (!logged)
        return std::unexpected(std::move(logged.error()));
      continue;
    }
    if (!defaults) {
      defaults.emplace(config->get("defaultItemConfig").transform_error([&](StarException error) {
        return (*assets)->with_asset_context(std::move(error), "/codex.config", "Loading codex item defaults");
      }));
    }
    auto merged = defaults->and_then([&](Json const& base) { return json_merge_object(base, book->item_config()); });
    if (!merged) {
      m_items.erase(entry);
      auto logged = m_root.logger().error_exception(
          Star::format("Could not create item for codex {}", id),
          ExceptionDiagnostic((*assets)->with_asset_context(std::move(merged.error()), book->directory() + book->filename(),
                                                            "Creating codex item")));
      if (!logged)
        return std::unexpected(std::move(logged.error()));
      continue;
    }
    merged->insert_or_assign("itemName", entry->first);
    merged->insert_or_assign("codexId", id);
    merged->insert_or_assign("shortdescription", book->title());
    merged->insert_or_assign("description", book->description());
    merged->insert_or_assign("codexIcon", book->icon());
    auto& data = entry->second;
    data.type = ItemType::Codex;
    data.name = entry->first;
    data.friendlyName = book->title();
    data.directory = book->directory();
    data.filename = book->filename();
    data.customConfig = std::move(*merged);
  }
  return {};
}

}
