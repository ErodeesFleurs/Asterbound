import star.format;
import std;
import star.lua;
import star.json;
import star.json_extra;
import star.id_map;
import star.data_stream;
import star.string;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.casting;
import star.game_types;
import star.strong_typedef;
import star.bi_map;
import star.rpc_promise;
import star.a_star;
import star.ordered_set;
import star.audio;
import star.map;
import star.set;
import star.either;
import star.random;
import star.periodic_function;
import star.ordered_map;

import star.lua_components;
import star.uuid;
import star.drawable;
import star.damage_types;
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

import star.player_companions;
import star.animation;
import star.particle;
import star.animated_part_set;
import star.mixer;
import star.networked_animator;
import star.humanoid;
import star.physics_entity;
import star.anchorable_entity;
import star.mobile_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.lounging_entities;
import star.chat_action;
import star.chatty_entity;
import star.emote_entity;
import star.portrait_entity;
import star.damage_bar_entity;
import star.nametag_entity;
import star.inspectable_entity;
import star.inventory_types;
import star.movement_controller;
import star.platformer_astar_types;
import star.game_timers;
import star.actor_movement_controller;
import star.ai_types;
import star.entity_rendering_types;
import star.entity_rendering;
import star.player_types;
import star.lua_actor_movement_component;
import star.lua_animation_component;
import star.radio_message_database;
import star.player;
import star.root;

import star.player_lua_bindings;
import star.entity_lua_bindings;
import star.config_lua_bindings;
import star.status_controller_lua_bindings;

import star.algorithm;
namespace Star {

std::expected<std::shared_ptr<Companion>, StarException> Companion::create(Root& root, Json const& json) {
  auto companion = std::shared_ptr<Companion>(new Companion(json));
  auto portrait = json.getArray("portrait", JsonArray{});
  if (!portrait)
    return std::unexpected(std::move(portrait.error()));
  if (portrait->empty())
    return companion;
  auto metadata = root.imageMetadataDatabase();
  if (!metadata)
    return std::unexpected(std::move(metadata.error()));
  for (auto const& entry : *portrait) {
    auto drawable = Drawable::create(**metadata, entry);
    if (!drawable)
      return std::unexpected(std::move(drawable.error()));
    companion->m_portrait.push_back(std::move(*drawable));
  }
  return companion;
}

Json Companion::toJson() const {
  return m_json;
}

std::expected<Uuid, StarException> Companion::podUuid() const {
  auto text = m_json.getString("podUuid");
  if (!text)
    return std::unexpected(std::move(text.error()));
  return Uuid::create(*text);
}

std::expected<std::optional<String>, StarException> Companion::name() const {
  return m_json.optString("name");
}

std::expected<std::optional<String>, StarException> Companion::description() const {
  return m_json.optString("description");
}

List<Drawable> Companion::portrait() const {
  return m_portrait;
}

std::expected<std::optional<float>, StarException> Companion::statusValue(String const& group, String const& name) const {
  auto status = m_json.opt("status");
  if (!status)
    return std::unexpected(std::move(status.error()));
  if (!*status)
    return std::optional<float>();
  auto values = (*status)->opt(group);
  if (!values)
    return std::unexpected(std::move(values.error()));
  if (!*values)
    return std::optional<float>();
  return (*values)->optFloat(name);
}

std::expected<std::optional<float>, StarException> Companion::resource(String const& name) const {
  return statusValue("resources", name);
}

std::expected<std::optional<float>, StarException> Companion::resourceMax(String const& name) const {
  return statusValue("resourceMax", name);
}

std::expected<std::optional<float>, StarException> Companion::stat(String const& name) const {
  return statusValue("stats", name);
}

PlayerCompanions::PlayerCompanions(Root& root, Json const& config) : m_root(root), m_config(config), m_scriptComponent(root) {}

std::expected<void, StarException> PlayerCompanions::diskLoad(Json const& diskStore) {
  auto storage = diskStore.getObject("scriptStorage", JsonObject{});
  if (!storage)
    return std::unexpected(std::move(storage.error()));
  auto stored = m_scriptComponent.setScriptStorage(std::move(*storage));
  if (!stored)
    return std::unexpected(std::move(stored.error()));
  auto categories = diskStore.getObject("companions", JsonObject{});
  if (!categories)
    return std::unexpected(std::move(categories.error()));
  StringMap<List<std::shared_ptr<Companion>>> loaded;
  for (auto const& category : *categories) {
    auto entries = category.second.iterateArray();
    if (!entries)
      return std::unexpected(std::move(entries.error()));
    for (auto const& entry : *entries) {
      auto companion = Companion::create(m_root, entry);
      if (!companion)
        return std::unexpected(std::move(companion.error()));
      loaded[category.first].push_back(std::move(*companion));
    }
    // Retain empty categories in the saved state.
    loaded[category.first];
  }
  m_companions = std::move(loaded);
  return {};
}

std::expected<Json, StarException> PlayerCompanions::diskStore() const {
  auto storage = m_scriptComponent.getScriptStorage();
  if (!storage)
    return std::unexpected(std::move(storage.error()));
  JsonObject companions;
  for (auto const& category : m_companions) {
    JsonArray entries;
    for (auto const& companion : category.second)
      entries.push_back(companion->toJson());
    companions[category.first] = std::move(entries);
  }
  return JsonObject{{"scriptStorage", std::move(*storage)}, {"companions", std::move(companions)}};
}

List<std::shared_ptr<Companion>> PlayerCompanions::getCompanions(String const& category) const {
  if (auto companions = m_companions.find_ref(category))
    return companions->get();
  return {};
}

std::expected<void, StarException> PlayerCompanions::init(Player& player, World& world) {
  auto setup = [&]() -> std::expected<void, StarException> {
    auto scriptsJson = m_config.get("scripts", JsonArray());
    if (!scriptsJson)
      return std::unexpected(std::move(scriptsJson.error()));
    auto scripts = jsonToStringList(*scriptsJson);
    if (!scripts)
      return std::unexpected(std::move(scripts.error()));
    auto installed = m_scriptComponent.setScripts(std::move(*scripts));
    if (!installed)
      return std::unexpected(std::move(installed.error()));
    auto delta = m_config.getInt("scriptDelta", 10);
    if (!delta)
      return std::unexpected(std::move(delta.error()));
    m_scriptComponent.setUpdateDelta(*delta);
    auto add = [&](String name, std::expected<LuaCallbacks, StarException> callbacks) -> std::expected<void, StarException> {
      if (!callbacks)
        return std::unexpected(std::move(callbacks.error()));
      return m_scriptComponent.addCallbacks(std::move(name), std::move(*callbacks));
    };
    if (auto result = add("entity", LuaBindings::makeEntityCallbacks(player)); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = add("player", LuaBindings::makePlayerCallbacks(m_root, player)); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = add("status", LuaBindings::makeStatusControllerCallbacks(player.statusController())); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = add("playerCompanions", makeCompanionsCallbacks()); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = add("config", LuaBindings::makeConfigCallbacks(
        [this](String const& name, Json const& def) { return m_config.query(name, def); })); !result)
      return std::unexpected(std::move(result.error()));
    return recoverLuaScriptError(m_scriptComponent.init(world));
  }();
  if (!setup) {
    auto cleanup = uninit();
    if (!cleanup)
      return std::unexpected(setup.error().with_context(Star::format("Companion initialization rollback failed: {}", cleanup.error().what())));
  }
  return setup;
}

std::expected<void, StarException> PlayerCompanions::uninit() {
  std::optional<StarException> error;
  auto remember = [&](auto result) {
    if (!result) {
      if (error)
        *error = error->with_context(result.error().what());
      else
        error = std::move(result.error());
    }
  };
  remember(recoverLuaScriptError(m_scriptComponent.uninit()));
  for (auto const& name : std::initializer_list<std::string_view>{"entity", "player", "status", "playerCompanions", "config"})
    remember(m_scriptComponent.removeCallbacks(name));
  if (error)
    return std::unexpected(std::move(*error));
  return {};
}

std::expected<void, StarException> PlayerCompanions::dismissCompanion(String const& category, Uuid const& podUuid) {
  auto result = recoverLuaScriptError(m_scriptComponent.invoke("dismissCompanion", category, podUuid.hex()));
  if (!result)
    return std::unexpected(std::move(result.error()));
  return {};
}

std::expected<std::optional<ChainableJsonMessageResponse>, StarException> PlayerCompanions::receiveMessage(String const& message, bool localMessage, JsonArray const& args) {
  return recoverLuaScriptError(m_scriptComponent.handleMessage(message, localMessage, args));
}

std::expected<void, StarException> PlayerCompanions::update(float dt) {
  auto updated = recoverLuaScriptError(m_scriptComponent.update(m_scriptComponent.updateDt(dt)));
  if (!updated)
    return std::unexpected(std::move(updated.error()));
  return {};
}

std::expected<LuaCallbacks, StarException> PlayerCompanions::makeCompanionsCallbacks() {
  LuaCallbacks callbacks;
  auto get = callbacks.registerCallback("getCompanions", [this](String const& category) {
    return m_companions[category].transformed([](std::shared_ptr<Companion> const& companion) { return companion->toJson(); });
  });
  if (!get)
    return std::unexpected(std::move(get.error()));
  auto set = callbacks.registerCallback("setCompanions",
      [this](String const& category, JsonArray const& companions) -> std::expected<void, StarException> {
    List<std::shared_ptr<Companion>> loaded;
    for (auto const& json : companions) {
      auto companion = Companion::create(m_root, json);
      if (!companion)
        return std::unexpected(std::move(companion.error()));
      loaded.push_back(std::move(*companion));
    }
    m_companions[category] = std::move(loaded);
    return {};
  });
  if (!set)
    return std::unexpected(std::move(set.error()));
  return callbacks;
}

}
