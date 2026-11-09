module;
import std;
import star.format;
import star.json;
import star.json_extra;
import star.id_map;
import star.lua;
import star.casting;
import star.data_stream;
import star.game_types;
import star.strong_typedef;
import star.bi_map;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.rpc_promise;
import star.a_star;
import star.set;
import star.string;


import star.lua_game_converters;
import star.drawable;
import star.item;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.animation;
import star.particle;
import star.interaction_types;
import star.tile_damage;
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
import star.mobile_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.tool_user_item;

module star.item_lua_bindings;

namespace Star::LuaBindings::ItemCallbacks {
  String name(Item& item);
  std::size_t count(Item& item);
  std::size_t setCount(Item& item, std::size_t count);
  std::size_t maxStack(Item& item);
  [[nodiscard]] std::expected<bool, StarException> matches(Item& item, Json const& descriptor, std::optional<bool> exactMatch);
  Json matchingDescriptors(Item& item);
  bool consume(Item& item, std::size_t count);
  bool empty(Item& item);
  Json descriptor(Item& item);
  String description(Item& item);
  String friendlyName(Item& item);
  int rarity(Item& item);
  [[nodiscard]] std::expected<String, StarException> rarityString(Item& item);
  std::size_t price(Item& item);
  [[nodiscard]] std::expected<unsigned, StarException> fuelAmount(Item& item);
  [[nodiscard]] std::expected<Json, StarException> iconDrawables(Item& item);
  [[nodiscard]] std::expected<Json, StarException> dropDrawables(Item& item);
  String largeImage(Item& item);
  String tooltipKind(Item& item);
  String category(Item& item);
  [[nodiscard]] std::expected<String, StarException> pickupSound(Item& item);
  bool twoHanded(Item& item);
  float timeToLive(Item& item);
  [[nodiscard]] std::expected<Json, StarException> learnBlueprintsOnPickup(Item& item);
  [[nodiscard]] std::expected<bool, StarException> hasItemTag(Item& item, String const& itemTag);
  [[nodiscard]] std::expected<Json, StarException> pickupQuestTemplates(Item& item);
}
namespace Star {

std::expected<LuaCallbacks, StarException> LuaBindings::makeItemCallbacks(Item& item) {
  LuaCallbacks callbacks;

  if (auto registration =
        callbacks.registerCallbackWithSignature<String>("name", [&item]() { return ItemCallbacks::name(item); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration =
        callbacks.registerCallbackWithSignature<std::size_t>("count", [&item]() { return ItemCallbacks::count(item); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<std::size_t, std::size_t>(
        "setCount", [&item](std::size_t count) { return ItemCallbacks::setCount(item, count); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<std::size_t>(
        "maxStack", [&item]() { return ItemCallbacks::maxStack(item); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<bool, Json, std::optional<bool>>(
        "matches",
        [&item](Json const& descriptor, std::optional<bool> exactMatch) {
          return ItemCallbacks::matches(item, descriptor, exactMatch);
        });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<bool, std::size_t>(
        "consume", [&item](std::size_t count) { return ItemCallbacks::consume(item, count); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration =
        callbacks.registerCallbackWithSignature<bool>("empty", [&item]() { return ItemCallbacks::empty(item); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<Json>(
        "descriptor", [&item]() { return ItemCallbacks::descriptor(item); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<String>(
        "description", [&item]() { return ItemCallbacks::description(item); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<String>(
        "friendlyName", [&item]() { return ItemCallbacks::friendlyName(item); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration =
        callbacks.registerCallbackWithSignature<int>("rarity", [&item]() { return ItemCallbacks::rarity(item); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<String>(
        "rarityString", [&item]() { return ItemCallbacks::rarityString(item); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration =
        callbacks.registerCallbackWithSignature<std::size_t>("price", [&item]() { return ItemCallbacks::price(item); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<Json>(
        "fuelAmount", [&item]() { return ItemCallbacks::fuelAmount(item); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<Json>(
        "iconDrawables", [&item]() { return ItemCallbacks::iconDrawables(item); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<Json>(
        "dropDrawables", [&item]() { return ItemCallbacks::dropDrawables(item); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<String>(
        "largeImage", [&item]() { return ItemCallbacks::largeImage(item); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<String>(
        "tooltipKind", [&item]() { return ItemCallbacks::tooltipKind(item); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<String>(
        "category", [&item]() { return ItemCallbacks::category(item); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<String>(
        "pickupSound", [&item]() { return ItemCallbacks::pickupSound(item); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<bool>(
        "twoHanded", [&item]() { return ItemCallbacks::twoHanded(item); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<float>(
        "timeToLive", [&item]() { return ItemCallbacks::timeToLive(item); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<Json>(
        "learnBlueprintsOnPickup", [&item]() { return ItemCallbacks::learnBlueprintsOnPickup(item); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<bool, String>(
        "hasItemTag", [&item](String const& tag) { return ItemCallbacks::hasItemTag(item, tag); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<Json>(
        "pickupQuestTemplates", [&item]() { return ItemCallbacks::pickupQuestTemplates(item); });
      !registration)
    return std::unexpected(std::move(registration.error()));

  return callbacks;
}

// Returns the name of the item. (Unique identifier)
//
// @return A string containing the name of the item as specified in its
// configuration
String LuaBindings::ItemCallbacks::name(Item& item) {
  return item.name();
}

// Returns the number of items in this stack
//
// @return An integer containing the number of items in this stack
std::size_t LuaBindings::ItemCallbacks::count(Item& item) {
  return item.count();
}

// Sets the number of items in the stack (up to maxStack)
//
// @param count The desired amount
// @return An integer containing the number of items that overflowed
std::size_t LuaBindings::ItemCallbacks::setCount(Item& item, std::size_t count) {
  return item.setCount(count);
}

// Returns the maximum number of items in this item's stack
//
// @return An integer containing the number of items in this item's maximum
// stack
std::size_t LuaBindings::ItemCallbacks::maxStack(Item& item) {
  return item.maxStack();
}

// Returns whether or not the serialized item descriptor passed logically
// matches this item
// Checks both name and parameters and uses the item's internal list of matching
// descriptors
//
// @see is
// @param descriptor A properly serialized item descriptor
// @return A bool true if matches, false if not.
//
std::expected<bool, StarException> LuaBindings::ItemCallbacks::matches(Item& item, Json const& desc, std::optional<bool> exactMatch) {
  auto itemDesc = ItemDescriptor::create(desc);
  if (!itemDesc)
    return std::unexpected(std::move(itemDesc.error()));
  return item.matches(*itemDesc, exactMatch.value_or(false));
}

// If the given number of this item is available, consumes that number and
// returns true, otherwise
// returns false.
//
// @param toConsume The number of items you'd like to consume from this stack
// @return true if items were successfully consumed, false otherwise
bool LuaBindings::ItemCallbacks::consume(Item& item, std::size_t count) {
  return item.consume(count);
}

// Returns the number of items in the stack is equal to 0
//
// @return true if stack is empty, false otherwise
bool LuaBindings::ItemCallbacks::empty(Item& item) {
  return item.empty();
}

// Returns the descriptor of this item
//
// @return a table containing a serialized item descriptor
Json LuaBindings::ItemCallbacks::descriptor(Item& item) {
  return item.descriptor().toJson();
}

// Returns the item description
//
// @return a string containing the item's description
String LuaBindings::ItemCallbacks::description(Item& item) {
  return item.description();
}

// Returns the friendly name of the item
//
// @return a string containing the friendly name of the item
String LuaBindings::ItemCallbacks::friendlyName(Item& item) {
  return item.friendlyName();
}

// Returns the rarity of the item as an integer
// Common = 0,
// Uncommon = 1,
// Rare = 2,
// Legendary = 3
// Essential = 4
//
// @return an integer representing the rarity of the item
int LuaBindings::ItemCallbacks::rarity(Item& item) { return static_cast<int>(item.rarity()); }

// Returns the rarity of the item as a string
//
// @return a string representing the rarity of the item
std::expected<String, StarException> LuaBindings::ItemCallbacks::rarityString(Item& item) {
  auto name = enumName<RarityNames>(item.rarity());
  if (!name)
    return std::unexpected(std::move(name.error()));
  return String(*name);
}

// Returns the shop price of the item
//
// @return an integer representing the shop price of the item in pixels (before
// modifiers)
std::size_t LuaBindings::ItemCallbacks::price(Item& item) {
  return item.price();
}

// Returns the amount of fuel given for buring this item stack in an engine
//
// @return an integer representing the amount of fuel in the entire stack
std::expected<unsigned, StarException> LuaBindings::ItemCallbacks::fuelAmount(Item& item) {
  auto value = item.instanceValue("fuelAmount", 0);
  if (!value)
    return std::unexpected(std::move(value.error()));
  auto amount = value->toUInt();
  if (!amount)
    return std::unexpected(std::move(amount.error()));
  return *amount;
}

// Returns the iconDrawables for this item serialized into json
//
// @return List of tables containing the serialized icon drawables for this
// item.
std::expected<Json, StarException> LuaBindings::ItemCallbacks::iconDrawables(Item& item) {
  auto drawables = item.iconDrawables();
  if (!drawables)
    return std::unexpected(std::move(drawables.error()));
  return jsonFromList<Drawable>(*drawables, [](Drawable const& drawable) { return drawable.toJson(); });
}

// Returns the dropDrawables for this item serialized into json
//
// @return List of tables containing the serialized drop drawables for this
// item.
std::expected<Json, StarException> LuaBindings::ItemCallbacks::dropDrawables(Item& item) {
  auto drawables = item.dropDrawables(std::nullopt);
  if (!drawables)
    return std::unexpected(std::move(drawables.error()));
  return jsonFromList<Drawable>(*drawables, [](Drawable const& drawable) { return drawable.toJson(); });
}

// Returns the large image for this item as displayed on mouseover in the
// tooltip
//
// @return String containing a path to the largeImage for this item
String LuaBindings::ItemCallbacks::largeImage(Item& item) {
  return item.largeImage();
}

// Returns the inspection kind of this item (as defined in item config, defaults
// to empty string)
//
// @return String containing the inspection kind
String LuaBindings::ItemCallbacks::tooltipKind(Item& item) {
  return item.tooltipKind();
}

// Returns the category of this item (as defined in item config)
//
// @return String containing the category
String LuaBindings::ItemCallbacks::category(Item& item) {
  return item.category();
}

// Returns the pickup sound for the item
//
// @return string containing the path to the pickup sound for this item
std::expected<String, StarException> LuaBindings::ItemCallbacks::pickupSound(Item& item) {
  return item.pickupSound();
}

// Returns whether or not the item is two handed
//
// @return bool containing true if the item is twoHanded, false otherwise
bool LuaBindings::ItemCallbacks::twoHanded(Item& item) {
  return item.twoHanded();
}

// Returns the time to live for this item as an item drop
//
// @return float containing the time to live for this item
float LuaBindings::ItemCallbacks::timeToLive(Item& item) {
  return item.timeToLive();
}

// Returns a list of item descriptors representing recipes whose blueprints you
// learn when you pick
// this item up
//
// @return a list of tables containing serialize item descriptors
std::expected<Json, StarException> LuaBindings::ItemCallbacks::learnBlueprintsOnPickup(Item& item) {
  return jsonFromList<ItemDescriptor>(
      item.learnBlueprintsOnPickup(), [](ItemDescriptor const& descriptor) { return descriptor.toJson(); });
}

// Returns whether or not this items has a specific item tag
//
// @param itemTag a string containing the tag value
// @return a bool representing whether or not this item contains that tag
std::expected<bool, StarException> LuaBindings::ItemCallbacks::hasItemTag(Item& item, String const& itemTag) {
  return item.hasItemTag(itemTag);
}

// Returns the pickup Quest Templates for this item
//
// @return a list of string containing the different quest templates triggered
// by this item on
// pickup
std::expected<Json, StarException> LuaBindings::ItemCallbacks::pickupQuestTemplates(Item& item) {
  auto templates = item.pickupQuestTemplates();
  if (!templates)
    return std::unexpected(std::move(templates.error()));
  return jsonFromList<QuestArcDescriptor>(*templates, [](QuestArcDescriptor const& descriptor) { return descriptor.toJson(); });
}

}
