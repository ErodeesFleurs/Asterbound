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
  std::size_t set_count(Item& item, std::size_t count);
  std::size_t max_stack(Item& item);
  [[nodiscard]] std::expected<bool, StarException> matches(Item& item, Json const& descriptor, std::optional<bool> exact_match);
  bool consume(Item& item, std::size_t count);
  bool empty(Item& item);
  Json descriptor(Item& item);
  String description(Item& item);
  String friendly_name(Item& item);
  int rarity(Item& item);
  [[nodiscard]] std::expected<String, StarException> rarity_string(Item& item);
  std::size_t price(Item& item);
  [[nodiscard]] std::expected<unsigned, StarException> fuel_amount(Item& item);
  [[nodiscard]] std::expected<Json, StarException> icon_drawables(Item& item);
  [[nodiscard]] std::expected<Json, StarException> drop_drawables(Item& item);
  String large_image(Item& item);
  String tooltip_kind(Item& item);
  String category(Item& item);
  [[nodiscard]] std::expected<String, StarException> pickup_sound(Item& item);
  bool two_handed(Item& item);
  float time_to_live(Item& item);
  [[nodiscard]] std::expected<Json, StarException> learn_blueprints_on_pickup(Item& item);
  [[nodiscard]] std::expected<bool, StarException> has_item_tag(Item& item, String const& item_tag);
  [[nodiscard]] std::expected<Json, StarException> pickup_quest_templates(Item& item);
}
namespace Star {

namespace {

template <typename Resolve>
  requires std::invocable<Resolve const&>
    && (std::same_as<std::invoke_result_t<Resolve const&>, Item&>
      || std::same_as<std::invoke_result_t<Resolve const&>, std::shared_ptr<Item>>)
std::expected<LuaCallbacks, StarException> bind_item_callbacks(Resolve resolve) {
  using namespace LuaBindings::ItemCallbacks;
  LuaCallbacks callbacks;
  auto bind = [&]<auto& callback, typename Return, typename... Args>(String name)
    requires std::invocable<decltype(callback), Item&, Args...>
  {
    return callbacks.register_callback_with_signature<Return, Args...>(std::move(name), [resolve](Args&&... args) {
      // The slot resolver takes ownership for the duration of this call, even
      // if item code reenters Lua and replaces the slot before returning.
      decltype(auto) item = resolve();
      if constexpr (std::same_as<decltype(item), Item&>)
        return callback(item, std::move(args)...);
      else
        return callback(*item, std::move(args)...);
    });
  };

  if (auto result = bind.template operator()<name, String>("name"); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = bind.template operator()<count, std::size_t>("count"); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = bind.template operator()<set_count, std::size_t, std::size_t>("setCount"); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = bind.template operator()<max_stack, std::size_t>("maxStack"); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = bind.template operator()<matches, bool, Json, std::optional<bool>>("matches"); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = bind.template operator()<consume, bool, std::size_t>("consume"); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = bind.template operator()<empty, bool>("empty"); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = bind.template operator()<descriptor, Json>("descriptor"); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = bind.template operator()<description, String>("description"); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = bind.template operator()<friendly_name, String>("friendlyName"); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = bind.template operator()<rarity, int>("rarity"); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = bind.template operator()<rarity_string, String>("rarityString"); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = bind.template operator()<price, std::size_t>("price"); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = bind.template operator()<fuel_amount, Json>("fuelAmount"); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = bind.template operator()<icon_drawables, Json>("iconDrawables"); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = bind.template operator()<drop_drawables, Json>("dropDrawables"); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = bind.template operator()<large_image, String>("largeImage"); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = bind.template operator()<tooltip_kind, String>("tooltipKind"); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = bind.template operator()<category, String>("category"); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = bind.template operator()<pickup_sound, String>("pickupSound"); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = bind.template operator()<two_handed, bool>("twoHanded"); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = bind.template operator()<time_to_live, float>("timeToLive"); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = bind.template operator()<learn_blueprints_on_pickup, Json>("learnBlueprintsOnPickup"); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = bind.template operator()<has_item_tag, bool, String>("hasItemTag"); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = bind.template operator()<pickup_quest_templates, Json>("pickupQuestTemplates"); !result)
    return std::unexpected(std::move(result.error()));
  return callbacks;
}

}

std::expected<LuaCallbacks, StarException> LuaBindings::make_item_callbacks(Item& item) {
  return bind_item_callbacks([&item]() -> Item& { return item; });
}

std::expected<LuaCallbacks, StarException> LuaBindings::make_item_slot_callbacks(
    std::reference_wrapper<std::shared_ptr<Item> const> item_slot) {
  return bind_item_callbacks([item_slot] { return item_slot.get(); });
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
std::size_t LuaBindings::ItemCallbacks::set_count(Item& item, std::size_t count) {
  return item.set_count(count);
}

// Returns the maximum number of items in this item's stack
//
// @return An integer containing the number of items in this item's maximum
// stack
std::size_t LuaBindings::ItemCallbacks::max_stack(Item& item) {
  return item.max_stack();
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
std::expected<bool, StarException> LuaBindings::ItemCallbacks::matches(Item& item, Json const& desc, std::optional<bool> exact_match) {
  auto item_desc = ItemDescriptor::create(desc);
  if (!item_desc)
    return std::unexpected(std::move(item_desc.error()));
  return item.matches(*item_desc, exact_match.value_or(false));
}

// If the given number of this item is available, consumes that number and
// returns true, otherwise
// returns false.
//
// @param count The number of items you'd like to consume from this stack
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
  return item.descriptor().to_json();
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
String LuaBindings::ItemCallbacks::friendly_name(Item& item) {
  return item.friendly_name();
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
std::expected<String, StarException> LuaBindings::ItemCallbacks::rarity_string(Item& item) {
  auto name = enum_name<rarity_names>(item.rarity());
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
std::expected<unsigned, StarException> LuaBindings::ItemCallbacks::fuel_amount(Item& item) {
  auto value = item.instance_value("fuelAmount", 0);
  if (!value)
    return std::unexpected(std::move(value.error()));
  auto amount = value->to_uint();
  if (!amount)
    return std::unexpected(std::move(amount.error()));
  return *amount;
}

// Returns the iconDrawables for this item serialized into json
//
// @return List of tables containing the serialized icon drawables for this
// item.
std::expected<Json, StarException> LuaBindings::ItemCallbacks::icon_drawables(Item& item) {
  auto drawables = item.icon_drawables();
  if (!drawables)
    return std::unexpected(std::move(drawables.error()));
  return json_from_list<Drawable>(*drawables, [](Drawable const& drawable) { return drawable.to_json(); });
}

// Returns the dropDrawables for this item serialized into json
//
// @return List of tables containing the serialized drop drawables for this
// item.
std::expected<Json, StarException> LuaBindings::ItemCallbacks::drop_drawables(Item& item) {
  auto drawables = item.drop_drawables(std::nullopt);
  if (!drawables)
    return std::unexpected(std::move(drawables.error()));
  return json_from_list<Drawable>(*drawables, [](Drawable const& drawable) { return drawable.to_json(); });
}

// Returns the large image for this item as displayed on mouseover in the
// tooltip
//
// @return String containing a path to the largeImage for this item
String LuaBindings::ItemCallbacks::large_image(Item& item) {
  return item.large_image();
}

// Returns the inspection kind of this item (as defined in item config, defaults
// to empty string)
//
// @return String containing the inspection kind
String LuaBindings::ItemCallbacks::tooltip_kind(Item& item) {
  return item.tooltip_kind();
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
std::expected<String, StarException> LuaBindings::ItemCallbacks::pickup_sound(Item& item) {
  return item.pickup_sound();
}

// Returns whether or not the item is two handed
//
// @return bool containing true if the item is twoHanded, false otherwise
bool LuaBindings::ItemCallbacks::two_handed(Item& item) {
  return item.two_handed();
}

// Returns the time to live for this item as an item drop
//
// @return float containing the time to live for this item
float LuaBindings::ItemCallbacks::time_to_live(Item& item) {
  return item.time_to_live();
}

// Returns a list of item descriptors representing recipes whose blueprints you
// learn when you pick
// this item up
//
// @return a list of tables containing serialize item descriptors
std::expected<Json, StarException> LuaBindings::ItemCallbacks::learn_blueprints_on_pickup(Item& item) {
  return json_from_list<ItemDescriptor>(
      item.learn_blueprints_on_pickup(), [](ItemDescriptor const& descriptor) { return descriptor.to_json(); });
}

// Returns whether or not this items has a specific item tag
//
// @param item_tag a string containing the tag value
// @return a bool representing whether or not this item contains that tag
std::expected<bool, StarException> LuaBindings::ItemCallbacks::has_item_tag(Item& item, String const& item_tag) {
  return item.has_item_tag(item_tag);
}

// Returns the pickup Quest Templates for this item
//
// @return a list of string containing the different quest templates triggered
// by this item on
// pickup
std::expected<Json, StarException> LuaBindings::ItemCallbacks::pickup_quest_templates(Item& item) {
  auto templates = item.pickup_quest_templates();
  if (!templates)
    return std::unexpected(std::move(templates.error()));
  return json_from_list<QuestArcDescriptor>(*templates, [](QuestArcDescriptor const& descriptor) { return descriptor.toJson(); });
}

}
