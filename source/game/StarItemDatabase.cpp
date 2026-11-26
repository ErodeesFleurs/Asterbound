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
template <typename Invoke, typename Result>
concept item_script_invocation = std::invocable<Invoke>
    && std::same_as<std::invoke_result_t<Invoke>, std::expected<std::optional<Result>, StarException>>;

template <typename Ret, item_script_invocation<Ret> Invoke>
std::expected<std::optional<Ret>, StarException> invoke_item_script(LuaBaseComponent& script, Invoke&& invoke) {
  auto result = script.init().and_then([&](bool) { return std::forward<Invoke>(invoke)(); });
  auto cleaned = script.uninit();
  if (!cleaned) {
    if (!result)
      return std::unexpected(cleaned.error().with_context(Star::print_exception(result.error(), true)));
    return std::unexpected(std::move(cleaned.error()));
  }
  return result;
}

template <typename Key>
concept recipe_stock_key = std::same_as<Key, ItemDescriptor> || std::same_as<Key, String>;

template <typename Projection, typename Key>
concept recipe_key_projection = requires(Projection& project, ItemDescriptor const& input) {
  requires std::same_as<std::remove_cvref_t<decltype(project(input))>, Key>;
};

template <recipe_stock_key Key, recipe_key_projection<Key> Projection>
std::uint64_t ingredient_batch_limit(std::span<ItemDescriptor const> inputs, HashMap<Key, std::uint64_t> const& stock,
    std::uint64_t limit, Projection project) {
  HashMap<Key, std::uint64_t> required;
  for (auto const& input : inputs) {
    auto count = input.count();
    if (!count)
      continue;
    auto& total = required.try_emplace(project(input), 0).first->second;
    if (count > std::numeric_limits<std::uint64_t>::max() - total)
      return 0;
    total += count;
  }
  for (auto const& [key, total] : required) {
    auto available = stock.value(key, 0);
    if (available < total)
      return 0;
    limit = std::min(limit, available / total);
  }
  return limit;
}

}// namespace

std::uint64_t ItemDatabase::get_count_of_item(RecipeStock const& bag, ItemDescriptor const& item, bool exact_match) {
  return exact_match ? bag.by_parameters.value(item.singular(), 0) : bag.by_name.value(item.name(), 0);
}

RecipeStock ItemDatabase::normalize_bag(std::span<std::shared_ptr<Item> const> bag) {
  RecipeStock stock;
  for (auto const& item : bag) {
    if (item && !item->empty())
      stock.add(item->descriptor());
  }
  return stock;
}

HashSet<ItemRecipe> ItemDatabase::recipes_from_subset(RecipeStock const& normalized_bag, StringMap<std::uint64_t> const& available_currencies,
    HashSet<ItemRecipe> const& subset, StringSet const& allowed_types) {
  HashSet<ItemRecipe> result;
  for (auto const& recipe : subset) {
    if ((allowed_types.empty() || recipe.groups.empty() || recipe.groups.has_intersection(allowed_types))
        && can_make_recipe(recipe, normalized_bag, available_currencies))
      result.add(recipe);
  }
  return result;
}

std::expected<String, StarException> ItemDatabase::gui_filter_string(std::shared_ptr<Item> const& item) {
  auto lowered = (item->name() + item->friendly_name() + item->description()).to_lower();
  if (!lowered)
    return std::unexpected(std::move(lowered.error()));
  auto split = lowered->split_any(" ,.?*\\+/|\t");
  if (!split)
    return std::unexpected(std::move(split.error()));
  return split->join("");
}

std::expected<bool, StarException> ItemDatabase::craft_recipe(Player& player, ItemRecipe const& recipe, std::uint64_t batches) const {
  if (!batches || recipe.output.is_empty() || recipe.output.count() > std::numeric_limits<std::uint64_t>::max() / batches)
    return false;
  if (!player.is_admin()) {
    auto consumed = player.inventory()->consume_recipe(recipe, batches);
    if (!consumed || !*consumed)
      return consumed;
  }
  auto remaining = recipe.output.count() * batches;
  while (remaining) {
    auto product = item(recipe.output.with_count(remaining));
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

bool ItemDatabase::can_make_recipe(ItemRecipe const& recipe, RecipeStock const& available_ingredients, StringMap<std::uint64_t> const& available_currencies) {
  return max_craftable_in_bag(available_ingredients, available_currencies, recipe) != 0;
}

ItemDatabase::ItemDatabase(ConstructionKey, Root& root) : m_root(root), m_item_cache(root.random()) {}

std::expected<std::shared_ptr<ItemDatabase>, StarException> ItemDatabase::create(Root& root) {
  auto database = std::make_shared<ItemDatabase>(ConstructionKey{}, root);
  auto lua_root = LuaRoot::create(root);
  if (!lua_root)
    return std::unexpected(std::move(lua_root.error()));
  database->m_lua_root = std::move(*lua_root);
  auto scanned = database->scan_items();
  if (!scanned)
    return std::unexpected(std::move(scanned.error()));
  auto objects = database->add_object_items();
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
  auto recipes = database->scan_recipes();
  if (!recipes)
    return std::unexpected(std::move(recipes.error()));
  auto blueprints = database->add_blueprints();
  if (!blueprints)
    return std::unexpected(std::move(blueprints.error()));
  return database;
}

void ItemDatabase::cleanup() {
  std::lock_guard lock(m_cache_mutex);
  m_item_cache.cleanup([](ItemCacheEntry const&, std::shared_ptr<Item> const& item) {
    return item.use_count() != 1;
  });
}


std::expected<ItemDescriptor, StarException> ItemDatabase::load_descriptor(Json const& store) const {
  auto versioning = m_root.versioningDatabase();
  if (!versioning)
    return std::unexpected(std::move(versioning.error()));
  return ItemDescriptor::load_store(**versioning, store);
}

std::expected<std::shared_ptr<Item>, StarException> ItemDatabase::disk_load(Json const& store) const {
  if (store.is_null())
    return std::shared_ptr<Item>();
  auto descriptor = load_descriptor(store);
  if (!descriptor)
    return std::unexpected(std::move(descriptor.error()));
  return item(std::move(*descriptor));
}

std::expected<std::shared_ptr<Item>, StarException> ItemDatabase::from_json(Json const& spec) const {
  auto descriptor = ItemDescriptor::create(spec);
  if (!descriptor)
    return std::unexpected(std::move(descriptor.error()));
  return item(std::move(*descriptor));
}

std::expected<Json, StarException> ItemDatabase::disk_store(std::shared_ptr<Item const> const& item) const {
  if (!item)
    return Json();
  auto versioning = m_root.versioningDatabase();
  if (!versioning)
    return std::unexpected(std::move(versioning.error()));
  return item->descriptor().disk_store(**versioning);
}

Json ItemDatabase::to_json(std::shared_ptr<Item const> const& item) const {
  if (item)
    return item->descriptor().to_json();
  else
    return Json();
}

bool ItemDatabase::has_item(String const& item_name) const {
  return m_items.contains(item_name);
}

std::expected<ItemType, StarException> ItemDatabase::item_type(String const& item_name) const {
  auto data = item_data(item_name);
  if (!data)
    return std::unexpected(std::move(data.error()));
  return (*data)->type;
}

std::expected<String, StarException> ItemDatabase::item_friendly_name(String const& item_name) const {
  auto data = item_data(item_name);
  if (!data)
    return std::unexpected(std::move(data.error()));
  return (*data)->friendly_name;
}

std::expected<StringSet, StarException> ItemDatabase::item_tags(String const& item_name) const {
  auto data = item_data(item_name);
  if (!data)
    return std::unexpected(std::move(data.error()));
  return (*data)->item_tags;
}

std::expected<ItemDatabase::ItemConfig, StarException> ItemDatabase::item_config(String const& item_name, Json parameters, std::optional<float> level, std::optional<std::uint64_t> seed) const {
  auto data = item_data(item_name);
  if (!data)
    return std::unexpected(std::move(data.error()));
  return build_item_config(**data, std::move(parameters), level, seed);
}

std::expected<ItemDatabase::ItemConfig, StarException> ItemDatabase::build_item_config(ItemData const& data, Json parameters, std::optional<float> level, std::optional<std::uint64_t> seed) const {
  auto loaded = [&]() -> std::expected<ItemConfig, StarException> {
    ItemConfig config{data.directory, data.config, std::move(parameters)};
    auto builder = config.config.opt_string("builder");
    if (!builder)
      return std::unexpected(std::move(builder.error()));
    if (!*builder)
      return config;

    std::unique_lock locker(m_lua_mutex, std::defer_lock);
    lock_with_diagnostics(locker);
    auto lua_root = m_lua_root;
    auto context = lua_root->create_context(**builder);
    if (!context)
      return std::unexpected(std::move(context.error()));
    auto root_callbacks = LuaBindings::makeRootCallbacks(m_root, std::cref(*this));
    if (!root_callbacks)
      return std::unexpected(std::move(root_callbacks.error()));
    auto set_root = context->set_callbacks("root", *root_callbacks);
    if (!set_root)
      return std::unexpected(std::move(set_root.error()));
    auto utility_callbacks = LuaBindings::make_utility_callbacks(m_root.random(), m_root.logger(), m_root.logMap());
    if (!utility_callbacks)
      return std::unexpected(std::move(utility_callbacks.error()));
    auto set_utility = context->set_callbacks("sb", *utility_callbacks);
    if (!set_utility)
      return std::unexpected(std::move(set_utility.error()));
    auto built = context->invoke_path<LuaTupleReturn<Json, Json>>(
        "build", config.directory, config.config, config.parameters, level, seed);
    if (!built)
      return std::unexpected(std::move(built.error()));
    config.config = std::move(std::get<0>(*built));
    config.parameters = std::move(std::get<1>(*built));
    return config;
  }();
  if (!loaded && data.asset_config) {
    auto assets = m_root.assets();
    if (!assets)
      return std::unexpected(std::move(assets.error()));
    return std::unexpected((*assets)->with_asset_context(std::move(loaded.error()), data.source_path, "Loading item configuration"));
  }
  return loaded;
}

std::expected<std::optional<String>, StarException> ItemDatabase::item_file(String const& item_name) const {
  auto entry = m_items.find_ref(item_name);
  if (!entry)
    return std::optional<String>{};
  auto const& data = *entry->get();
  return std::optional<String>(data.source_path.empty() ? data.directory : data.source_path);
}

std::expected<std::shared_ptr<Item>, StarException> ItemDatabase::item_shared(ItemDescriptor descriptor, std::optional<float> level, std::optional<std::uint64_t> seed) const {
  if (!descriptor)
    return std::shared_ptr<Item>();
  ItemCacheEntry const entry{std::move(descriptor), level, seed};
  std::unique_lock locker(m_cache_mutex);
  auto cached = m_item_cache.find_ref(entry);
  if (!cached)
    return std::unexpected(std::move(cached.error()));
  if (*cached)
    return (*cached)->get();
  locker.unlock();
  auto created = try_create_item(std::get<0>(entry), level, seed);
  if (!created)
    return std::unexpected(std::move(created.error()));
  auto built_seed = (*created)->parameters().opt_uint("seed");
  if (!built_seed)
    return std::unexpected(std::move(built_seed.error()));
  // Cache by the request, not the builder's output. An unseeded request
  // that generates a seed must neither reuse nor seed a cache entry.
  if (!seed && *built_seed)
    return created;
  locker.lock();
  auto stored = m_item_cache.get(entry, [&](ItemCacheEntry const&) { return std::move(*created); });
  if (!stored)
    return std::unexpected(std::move(stored.error()));
  return stored->get();
}

std::expected<std::shared_ptr<Item>, StarException> ItemDatabase::item(ItemDescriptor descriptor, std::optional<float> level, std::optional<std::uint64_t> seed, bool ignore_invalid) const {
  if (!descriptor)
    return std::shared_ptr<Item>();
  return try_create_item(descriptor, level, seed, ignore_invalid);
}

HashSet<ItemRecipe> ItemDatabase::recipes_for_output_item(String const& item_name) const {
  HashSet<ItemRecipe> result;
  for (auto const& recipe : m_recipes)
    if (recipe.output.name() == item_name)
      result.add(recipe);
  return result;
}

HashSet<ItemRecipe> ItemDatabase::recipes_from_bag_contents(RecipeStock const& bag, StringMap<std::uint64_t> const& available_currencies, StringSet const& allowed_types) const {
  return recipes_from_subset(bag, available_currencies, m_recipes, allowed_types);
}

std::uint64_t ItemDatabase::max_craftable_in_bag(RecipeStock const& stock, StringMap<std::uint64_t> const& currencies, ItemRecipe const& recipe) {
  if (recipe.output.is_empty())
    return 0;
  auto limit = std::numeric_limits<std::uint64_t>::max() / recipe.output.count();
  for (auto const& [name, amount] : recipe.currency_inputs) {
    if (amount) {
      limit = std::min(limit, currencies.value(name, 0) / amount);
      if (!limit)
        return 0;
    }
  }
  if (recipe.inputs.empty())
    return limit;
  if (recipe.inputs.size() == 1) {
    auto const& input = recipe.inputs.front();
    return input.count() ? std::min(limit, get_count_of_item(stock, input, recipe.match_input_parameters) / input.count()) : limit;
  }
  if (recipe.match_input_parameters)
    return ingredient_batch_limit(recipe.inputs, stock.by_parameters, limit, [](ItemDescriptor const& input) { return input.singular(); });
  return ingredient_batch_limit(recipe.inputs, stock.by_name, limit, [](ItemDescriptor const& input) -> String const& { return input.name(); });
}

ItemRecipe ItemDatabase::get_precise_recipe_for_materials(String const& group, std::span<std::shared_ptr<Item> const> bag,
    StringMap<std::uint64_t> const& currencies) const {
  RecipeStock stock;
  if (bag.size() <= 1) {
    stock = normalize_bag(bag);
  } else {
    // Different slots can refer to the same physical stack. Count it only once.
    using Slot = std::reference_wrapper<std::shared_ptr<Item> const>;
    HashSet<Slot, std::hash<std::shared_ptr<Item>>, std::equal_to<std::shared_ptr<Item>>> seen;
    for (auto const& item : bag) {
      if (item && !item->empty() && seen.add(std::cref(item)))
        stock.add(item->descriptor());
    }
  }
  std::optional<std::reference_wrapper<ItemRecipe const>> best;
  // Each count is 64-bit, and several distinct ingredient counts can add a carry.
  using Score = std::pair<std::size_t, std::uint64_t>;
  Score best_score{};
  for (auto const& recipe : m_recipes) {
    if (!recipe.groups.contains(group) || !can_make_recipe(recipe, stock, currencies))
      continue;
    if (!std::ranges::all_of(bag, [&](std::shared_ptr<Item> const& item) {
      return !item || item->empty() || std::ranges::any_of(recipe.inputs, [&](ItemDescriptor const& input) {
        return item->matches(input, recipe.match_input_parameters);
      });
    }))
      continue;
    Score score{};
    for (auto const& input : recipe.inputs) {
      score.second += input.count();
      score.first += score.second < input.count();
    }
    if (!best || score > best_score || (score == best_score && recipe.source_path.utf8() < best->get().source_path.utf8())) {
      best = std::cref(recipe);
      best_score = score;
    }
  }
  return best ? best->get() : ItemRecipe{};
}

std::expected<ItemRecipe, StarException> ItemDatabase::parse_recipe(Json const& config) const {
  RecipeDefaultDuration default_duration;
  return parse_recipe(config, default_duration);
}

std::expected<ItemRecipe, StarException> ItemDatabase::parse_recipe(Json const& config, RecipeDefaultDuration& default_duration) const {
  auto parsed = [&]() -> std::expected<ItemRecipe, StarException> {
    ItemRecipe res;
    auto currencies = config.get("currencyInputs", JsonObject()).and_then([](Json const& value) {
      return json_to_map_v<StringMap<std::uint64_t>>(value, [](Json const& entry) { return entry.to_uint(); });
    });
    if (!currencies)
      return std::unexpected(std::move(currencies.error()));
    res.currency_inputs = std::move(*currencies);
    auto inputs = config.get_array("input");
    if (!inputs)
      return std::unexpected(std::move(inputs.error()));
    for (auto const& input : *inputs) {
      auto id = ItemDescriptor::create(input);
      if (!id)
        return std::unexpected(std::move(id.error()));
      auto type = item_type(id->name());
      if (!type)
        return std::unexpected(std::move(type.error()));
      if (*type == ItemType::CurrencyItem) {
        auto shared = item_shared(*id, {}, {});
        if (!shared)
          return std::unexpected(std::move(shared.error()));
        auto currency = as<CurrencyItem>(std::move(*shared));
        if (!currency)
          return std::unexpected(RecipeException::format("Recipe input '{}' did not produce a currency item", id->name()));
        auto& total = res.currency_inputs[currency->currencyType()];
        auto value = currency->currencyValue();
        if (value && id->count() > (std::numeric_limits<std::uint64_t>::max() - total) / value)
          return std::unexpected(RecipeException("Currency requirement exceeds supported range"));
        total += id->count() * value;
      } else {
        res.inputs.push_back(std::move(*id));
      }
    }
    auto output_config = config.get("output");
    if (!output_config)
      return std::unexpected(std::move(output_config.error()));
    auto output = ItemDescriptor::create(*output_config);
    if (!output)
      return std::unexpected(std::move(output.error()));
    if (output->is_empty())
      return std::unexpected(RecipeException("Recipe requires a nonempty output"));
    res.output = std::move(*output);
    auto duration = config.opt_float("duration");
    if (!duration)
      return std::unexpected(std::move(duration.error()));
    if (*duration) {
      res.duration = **duration;
    } else {
      if (!default_duration) {
        default_duration.emplace(m_root.assets().and_then([](std::shared_ptr<Assets const> const& assets) {
          return assets->json("/items/defaultParameters.config:defaultCraftDuration")
              .and_then([](Json const& value) { return value.to_float(); })
              .transform_error([&](StarException error) {
                return assets->with_asset_context(std::move(error), "/items/defaultParameters.config", "Loading recipe defaults");
              });
        }));
      }
      if (!*default_duration)
        return std::unexpected(default_duration->error());
      res.duration = **default_duration;
    }
    auto groups = config.get("groups", JsonArray()).and_then([](Json const& value) { return json_to_string_set(value); });
    if (!groups)
      return std::unexpected(std::move(groups.error()));
    res.groups = std::move(*groups);
    auto output_item = item_shared(res.output, {}, {});
    if (!output_item)
      return std::unexpected(std::move(output_item.error()));
    if (!*output_item)
      return std::unexpected(RecipeException("Recipe output did not produce an item"));
    res.output_rarity = (*output_item)->rarity();
    auto filter = gui_filter_string(*output_item);
    if (!filter)
      return std::unexpected(std::move(filter.error()));
    res.gui_filter_string = std::move(*filter);
    auto collectables = config.get("collectables", JsonObject()).and_then([](Json const& value) {
      return json_to_map_v<StringMap<String>>(value, [](Json const& entry) { return entry.to_string(); });
    });
    if (!collectables)
      return std::unexpected(std::move(collectables.error()));
    res.collectables = std::move(*collectables);
    auto match = config.get_bool("matchInputParameters", false);
    if (!match)
      return std::unexpected(std::move(match.error()));
    res.match_input_parameters = *match;
    return res;
  }();
  if (!parsed && parsed.error().is<JsonException>())
    return std::unexpected(RecipeException("Recipe missing required ingredient", ExceptionDiagnostic(parsed.error())));
  return parsed;
}

HashSet<ItemRecipe> const& ItemDatabase::all_recipes() const {
  return m_recipes;
}

HashSet<ItemRecipe> ItemDatabase::all_recipes(StringSet const& types) const {
  HashSet<ItemRecipe> result;
  for (auto const& recipe : m_recipes) {
    if (recipe.groups.has_intersection(types))
      result.add(recipe);
  }
  return result;
}

std::expected<std::shared_ptr<Item>, StarException> ItemDatabase::apply_augment(std::shared_ptr<Item> const& item, AugmentItem& augment) const {
  if (!item)
    return item;
  std::unique_lock locker(m_lua_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  LuaBaseComponent script(m_root);
  auto setup = [&]() -> std::expected<void, StarException> {
    auto removed = script.remove_callbacks("root");
    if (!removed)
      return std::unexpected(std::move(removed.error()));
    auto root_callbacks = LuaBindings::makeRootCallbacks(m_root, std::cref(*this));
    if (!root_callbacks)
      return std::unexpected(std::move(root_callbacks.error()));
    if (auto added = script.add_callbacks("root", std::move(*root_callbacks)); !added)
      return std::unexpected(std::move(added.error()));
    if (auto set = script.set_lua_root(m_lua_root); !set)
      return std::unexpected(std::move(set.error()));
    auto scripts = augment.augmentScripts();
    if (!scripts)
      return std::unexpected(std::move(scripts.error()));
    if (auto set = script.set_scripts(std::move(*scripts)); !set)
      return std::unexpected(std::move(set.error()));
    auto item_callbacks = LuaBindings::make_item_callbacks(augment);
    if (!item_callbacks)
      return std::unexpected(std::move(item_callbacks.error()));
    if (auto added = script.add_callbacks("item", std::move(*item_callbacks)); !added)
      return std::unexpected(std::move(added.error()));
    auto config_callbacks = LuaBindings::makeConfigCallbacks(
      [&augment](String const& name, Json const& def) { return augment.instance_value(name, def); });
    if (!config_callbacks)
      return std::unexpected(std::move(config_callbacks.error()));
    return script.add_callbacks("config", std::move(*config_callbacks));
  }();
  if (!setup)
    return std::unexpected(std::move(setup.error()));
  auto lua_result = invoke_item_script<LuaTupleReturn<Json, std::optional<std::uint64_t>>>(script, [&] {
    return script.invoke<LuaTupleReturn<Json, std::optional<std::uint64_t>>>("apply", item->descriptor().to_json());
  });
  locker.unlock();
  if (!lua_result)
    return std::unexpected(std::move(lua_result.error()));
  if (*lua_result && !std::get<0>(**lua_result).is_null()) {
    auto taken = augment.take(std::get<1>(**lua_result).value_or(1));
    if (!taken)
      return std::unexpected(std::move(taken.error()));
    auto descriptor = ItemDescriptor::create(std::get<0>(**lua_result));
    if (!descriptor)
      return std::unexpected(std::move(descriptor.error()));
    return ItemDatabase::item(std::move(*descriptor), {}, {}, false);
  }
  return item;
}

std::expected<bool, StarException> ItemDatabase::age_item(std::shared_ptr<Item>& item, double aging) const {
  if (!item)
    return false;
  auto data = item_data(item->name());
  if (!data)
    return std::unexpected(std::move(data.error()));
  if ((*data)->aging_scripts.empty())
    return false;
  ItemDescriptor original = item->descriptor();
  std::unique_lock locker(m_lua_mutex, std::defer_lock);
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
  if (auto set = script.set_lua_root(m_lua_root); !set)
    return std::unexpected(std::move(set.error()));
  if (auto set = script.set_scripts((*data)->aging_scripts); !set)
    return std::unexpected(std::move(set.error()));
  auto aged_json =
    invoke_item_script<Json>(script, [&] { return script.invoke<Json>("ageItem", original.to_json(), aging); });
  locker.unlock();
  if (!aged_json)
    return std::unexpected(std::move(aged_json.error()));
  if (!*aged_json)
    return false;
  auto aged = ItemDescriptor::create(**aged_json);
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

List<String> ItemDatabase::all_items() const {
  return m_items.keys();
}

std::expected<std::shared_ptr<Item>, StarException> ItemDatabase::create_item(ItemType type, ItemConfig const& config) const {
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

std::expected<std::shared_ptr<Item>, StarException> ItemDatabase::try_create_item(ItemDescriptor const& descriptor, std::optional<float> level, std::optional<std::uint64_t> seed, bool ignore_invalid) const {
  auto instantiate = [&](ItemDescriptor const& current) -> std::expected<std::shared_ptr<Item>, StarException> {
    auto data = m_items.get(current.name()).transform(
        [](std::reference_wrapper<std::shared_ptr<ItemData const> const> entry) { return entry.get(); });
    if (!data)
      return std::unexpected(std::move(data.error()));
    auto const& entry = **data;
    auto item = build_item_config(entry, current.parameters(), level, seed).and_then([&](ItemConfig const& config) {
      return create_item(entry.type, config);
    });
    if (!item && !entry.source_path.empty()) {
      auto assets = m_root.assets();
      if (!assets)
        return std::unexpected(std::move(assets.error()));
      return std::unexpected((*assets)->with_asset_context(std::move(item.error()), entry.source_path, "Constructing item"));
    }
    return item;
  };
  auto initial = [&]() -> std::expected<std::shared_ptr<Item>, StarException> {
    std::optional<ItemDescriptor> restored;
    if (descriptor.name() == "perfectlygenericitem") {
      auto stored = descriptor.parameters().find("genericItemStorage");
      if (!stored)
        return std::unexpected(std::move(stored.error()));
      if (*stored) {
        auto decoded = ItemDescriptor::create((*stored)->get());
        if (!decoded)
          return std::unexpected(std::move(decoded.error()));
        restored = std::move(*decoded);
      }
    }
    auto result = instantiate(restored ? *restored : descriptor);
    if (!result)
      return std::unexpected(std::move(result.error()));
    (*result)->set_count(descriptor.count());
    return result;
  }();
  if (initial || ignore_invalid)
    return initial;
  StarException original_error = std::move(initial.error());
  std::shared_ptr<Item> result;
  auto rebuilt = m_rebuilder->rebuild(descriptor.to_json(), original_error, [&](Json const& store) -> std::expected<void, StarException> {
    auto recovered_descriptor = ItemDescriptor::create(store);
    if (!recovered_descriptor)
      return std::unexpected(std::move(recovered_descriptor.error()));
    auto recovered = instantiate(*recovered_descriptor);
    if (!recovered)
      return std::unexpected(std::move(recovered.error()));
    (*recovered)->set_count(recovered_descriptor->count());
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
    ExceptionDiagnostic(original_error));
  if (!logged)
    return std::unexpected(std::move(logged.error()));
  if (m_root.logger().loggable(LogLevel::Debug)) {
    auto descriptor_text = descriptor.to_json().repr();
    if (!descriptor_text)
      return std::unexpected(std::move(descriptor_text.error()));
    if (auto details = m_root.logger().debug("Item descriptor: {}", *descriptor_text); !details)
      return std::unexpected(std::move(details.error()));
  }
  auto fallback_data = item_data("perfectlygenericitem");
  if (!fallback_data)
    return std::unexpected(std::move(fallback_data.error()));
  Json fallback_parameters = descriptor.parameters();
  auto fallback_level = level;
  auto fallback_seed = seed;
  if (descriptor.name() != "perfectlygenericitem") {
    auto stored = descriptor.to_json().erase("count");
    if (!stored)
      return std::unexpected(std::move(stored.error()));
    fallback_parameters = JsonObject{
      {"genericItemStorage", std::move(*stored)},
      {"shortdescription", descriptor.name()},
      {"description", "Reinstall the parent mod to return this item to normal"}
    };
    fallback_level.reset();
    fallback_seed.reset();
  }
  auto fallback = build_item_config(**fallback_data, std::move(fallback_parameters), fallback_level, fallback_seed)
      .and_then([&](ItemConfig const& config) { return create_item((*fallback_data)->type, config); });
  if (!fallback)
    return std::unexpected(std::move(fallback.error()));
  (*fallback)->set_count(descriptor.count());
  return fallback;
}

std::expected<std::shared_ptr<ItemDatabase::ItemData const>, StarException> ItemDatabase::item_data(String const& name) const {
  if (auto entry = m_items.find_ref(name))
    return entry->get();
  return std::unexpected(ItemException::format("No such item '{}'", name));
}

std::expected<void, StarException> ItemDatabase::add_item_set(Assets const& assets, ItemType type, ItemPaths const& files) {
  for (auto const& asset_file : files) {
    auto const& file = asset_file.string();
    auto loaded = [&]() -> std::expected<std::pair<String, ItemData>, StarException> {
      auto config = assets.json(file);
      if (!config)
        return std::unexpected(std::move(config.error()));
      auto name = config->get_string("itemName");
      if (!name)
        return std::unexpected(std::move(name.error()));
      auto friendly_name = config->get_string("shortdescription", {});
      if (!friendly_name)
        return std::unexpected(std::move(friendly_name.error()));
      auto tags = config->get("itemTags", JsonArray{}).and_then([](Json const& value) { return json_to_string_set(value); });
      if (!tags)
        return std::unexpected(std::move(tags.error()));
      auto scripts = config->get("itemAgingScripts", JsonArray{}).and_then([](Json const& value) { return json_to_string_list(value); });
      if (!scripts)
        return std::unexpected(std::move(scripts.error()));
      auto directory = AssetPath::directory(file);
      for (auto& script : *scripts)
        script = AssetPath::relative_to(directory, script);
      return std::pair(std::move(*name), ItemData{
          type, std::move(*friendly_name), std::move(*tags), std::move(*scripts), std::move(*config), std::move(directory), file, true});
    }();
    if (!loaded)
      return std::unexpected(ItemException(Star::format("Could not load item asset {}", file), ExceptionDiagnostic(assets.with_asset_context(std::move(loaded.error()), file, "Loading item asset"))));
    auto [entry, inserted] = m_items.try_emplace(std::move(loaded->first));
    if (!inserted)
      return std::unexpected(assets.with_asset_context(ItemException::format("Duplicate item name '{}' found", entry->first), file, "Loading item asset"));
    entry->second = std::make_shared<ItemData const>(std::move(loaded->second));
  }
  return {};
}

std::expected<void, StarException> ItemDatabase::add_object_drop_item(Assets const& assets, String const& object_path, Json const& object_config) {
  auto name = object_config.get_string("objectName");
  if (!name)
    return std::unexpected(std::move(name.error()));
  auto friendly_name = object_config.get_string("shortdescription", {});
  if (!friendly_name)
    return std::unexpected(std::move(friendly_name.error()));
  auto tags = object_config.get("itemTags", JsonArray{}).and_then([](Json const& value) { return json_to_string_set(value); });
  if (!tags)
    return std::unexpected(std::move(tags.error()));
  auto scripts = object_config.get("itemAgingScripts", JsonArray{}).and_then([](Json const& value) { return json_to_string_list(value); });
  if (!scripts)
    return std::unexpected(std::move(scripts.error()));
  auto config = object_config.to_object();
  if (!config)
    return std::unexpected(std::move(config.error()));
  if (!config->contains("inventoryIcon")) {
    auto icon = assets.json("/objects/defaultParameters.config:missingIcon");
    if (!icon)
      return std::unexpected(assets.with_asset_context(std::move(icon.error()), "/objects/defaultParameters.config", "Loading object item defaults"));
    (*config)["inventoryIcon"] = std::move(*icon);
    auto logged = m_root.logger().warn("Missing inventoryIcon for {}, using default", *name);
    if (!logged)
      return std::unexpected(std::move(logged.error()));
  }
  (*config)["itemName"] = *name;
  if (!config->contains("tooltipKind"))
    (*config)["tooltipKind"] = "object";
  if (!config->contains("printable"))
    (*config)["printable"] = config->contains("price");
  // Object scripts cannot run in the fireable ObjectItem's script context.
  config->remove("scripts");
  auto [entry, inserted] = m_items.try_emplace(std::move(*name));
  if (!inserted)
    return std::unexpected(ItemException::format("Object drop '{}' shares name with existing item", entry->first));
  entry->second = std::make_shared<ItemData const>(ItemData{
      ItemType::ObjectItem, std::move(*friendly_name), std::move(*tags), std::move(*scripts), std::move(*config),
      AssetPath::directory(object_path), object_path});
  return {};
}

std::expected<void, StarException> ItemDatabase::scan_items() {
  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  static constexpr auto item_sets = std::to_array<std::pair<ItemType, std::string_view>>({
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
  });
  std::array<ItemPaths, item_sets.size()> files_by_type;
  for (std::size_t i = 0; i < item_sets.size(); ++i) {
    auto files = (*assets)->scan_extension(item_sets[i].second);
    if (!files)
      return std::unexpected(std::move(files.error()));
    files_by_type[i] = ItemPaths(files->get().begin(), files->get().end());
  }
  auto queued = (*assets)->queue_jsons(std::views::join(files_by_type));
  if (!queued)
    return std::unexpected(std::move(queued.error()));
  for (std::size_t i = 0; i < item_sets.size(); ++i) {
    auto added = add_item_set(**assets, item_sets[i].first, files_by_type[i]);
    if (!added)
      return std::unexpected(std::move(added.error()));
  }
  return {};
}

std::expected<void, StarException> ItemDatabase::add_object_items() {
  auto objects = m_root.objectDatabase();
  if (!objects)
    return std::unexpected(std::move(objects.error()));
  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  for (auto const& object_name : (*objects)->allObjects()) {
    auto config = (*objects)->getConfig(object_name);
    if (!config)
      return std::unexpected(std::move(config.error()));
    if ((*config)->hasObjectItem) {
      auto added = add_object_drop_item(**assets, (*config)->path, (*config)->config);
      if (!added)
        return std::unexpected((*assets)->with_asset_context(std::move(added.error()), (*config)->path, "Loading object drop item"));
    }
  }
  return {};
}

std::expected<void, StarException> ItemDatabase::scan_recipes() {
  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto files = (*assets)->scan_extension("recipe");
  if (!files)
    return std::unexpected(std::move(files.error()));
  auto queued = (*assets)->queue_jsons(files->get());
  if (!queued)
    return std::unexpected(std::move(queued.error()));
  RecipeDefaultDuration default_duration;
  for (auto const& asset_file : files->get()) {
    auto const& file = asset_file.string();
    auto recipe = (*assets)->json(file).and_then([&](Json const& config) { return parse_recipe(config, default_duration); });
    if (recipe) {
      recipe->source_path = file;
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

std::expected<void, StarException> ItemDatabase::add_blueprints() {
  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));

  struct BlueprintDefaults {
    String description;
    String short_description;
    Json category;
  };
  auto load_defaults = [&]() -> std::expected<BlueprintDefaults, StarException> {
    auto config = (*assets)->json("/blueprint.config");
    if (!config)
      return std::unexpected(std::move(config.error()));
    auto description = config->get_string("description");
    if (!description)
      return std::unexpected(std::move(description.error()));
    auto short_description = config->get_string("shortdescription");
    if (!short_description)
      return std::unexpected(std::move(short_description.error()));
    auto category = config->get_string("category");
    if (!category)
      return std::unexpected(std::move(category.error()));
    return BlueprintDefaults{std::move(*description), std::move(*short_description), std::move(*category)};
  };
  // Check defaults only when a generated blueprint is needed. Keep a failed
  // result too, so each affected recipe can report the original asset error.
  std::optional<std::expected<BlueprintDefaults, StarException>> defaults;
  for (auto const& recipe : m_recipes) {
    auto const& base_desc = recipe.output;
    auto base_item = item_shared(base_desc, {}, {});
    if (!base_item)
      return std::unexpected(std::move(base_item.error()));
    if (!*base_item)
      continue;
    auto const& base = **base_item;
    String blueprint_name = Star::format("{}-recipe", base.name());
    if (m_items.contains(blueprint_name))
      continue;
    auto base_data = item_data(base_desc.name());
    auto created = [&]() -> std::expected<void, StarException> {
      if (!base_data)
        return std::unexpected(std::move(base_data.error()));
      if (!defaults) {
        defaults.emplace(load_defaults().transform_error([&](StarException error) {
          return (*assets)->with_asset_context(std::move(error), "/blueprint.config", "Loading blueprint defaults");
        }));
      }
      if (!*defaults)
        return std::unexpected(defaults->error());

      auto friendly_name = base.friendly_name();
      auto description = (*defaults)->description.replace("<item>", friendly_name);
      if (!description)
        return std::unexpected(std::move(description.error()));
      auto short_description = (*defaults)->short_description.replace("<item>", friendly_name);
      if (!short_description)
        return std::unexpected(std::move(short_description.error()));
      JsonObject config{
          {"recipe", base_desc.singular().to_json()},
          {"description", std::move(*description)},
          {"shortdescription", *short_description},
          {"category", (*defaults)->category},
          {"itemName", blueprint_name}};
      auto icon = base.instance_value("inventoryIcon", false);
      if (!icon)
        return std::unexpected(std::move(icon.error()));
      if (*icon) {
        auto inventory_icon = base.instance_value("inventoryIcon");
        if (!inventory_icon)
          return std::unexpected(std::move(inventory_icon.error()));
        config["inventoryIcon"] = std::move(*inventory_icon);
      }
      auto rarity = enum_name<rarity_names>(base.rarity());
      if (!rarity)
        return std::unexpected(std::move(rarity.error()));
      config["rarity"] = String(*rarity);
      config["price"] = base.price();
      m_items.insert(std::move(blueprint_name), std::make_shared<ItemData const>(ItemData{
          ItemType::Blueprint, std::move(*short_description), {}, {}, std::move(config), (*base_data)->directory, {}}));
      return {};
    }();
    if (!created) {
      auto error = std::move(created.error());
      if (base_data && !(*base_data)->source_path.empty())
        error = (*assets)->with_asset_context(std::move(error), (*base_data)->source_path, "Creating blueprint item");
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
    entry->second = std::make_shared<ItemData const>(ItemData{
        ItemType::Codex, book->title(), {}, {}, std::move(*merged), book->directory(), book->directory() + book->filename()});
  }
  return {};
}

}
