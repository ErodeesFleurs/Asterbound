import star.format;
import star.lua;
import std;
import star.json;
import star.json_extra;
import star.id_map;
import star.audio;
import star.thread;
import star.list;
import star.map;
import star.set;
import star.vector;
import star.bi_map;
import star.color;
import star.string;
import star.data_stream;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.game_types;
import star.casting;
import star.strong_typedef;
import star.rpc_promise;
import star.a_star;
import star.ordered_set;
import star.either;
import star.random;
import star.periodic_function;
import star.ordered_map;

import star.lua_components;
import star.mixer;
import star.drawable;
import star.entity_rendering_types;
import star.animation;
import star.particle;

import star.light_source;
import star.entity_rendering;
import star.lua_animation_component;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
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

import star.player_deployment;
import star.uuid;
import star.animated_part_set;
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
import star.player_types;
import star.lua_actor_movement_component;
import star.radio_message_database;
import star.player;

import star.player_lua_bindings;
import star.entity_lua_bindings;
import star.config_lua_bindings;
import star.status_controller_lua_bindings;

namespace Star {

class Root;

PlayerDeployment::PlayerDeployment(Root& root, Json const& config) : m_root(root), m_config(config), m_scriptComponent(root) {
  m_deploying = false;
  m_deployed = false;
}

std::expected<void, StarException> PlayerDeployment::diskLoad(Json const& diskStore) {
  auto storage = diskStore.getObject("scriptStorage", JsonObject{});
  if (!storage)
    return std::unexpected(std::move(storage.error()));
  return m_scriptComponent.setScriptStorage(std::move(*storage));
}

std::expected<Json, StarException> PlayerDeployment::diskStore() const {
  auto storage = m_scriptComponent.getScriptStorage();
  if (!storage)
    return std::unexpected(std::move(storage.error()));
  return JsonObject{{"scriptStorage", std::move(*storage)}};
}

std::expected<void, StarException> PlayerDeployment::init(Player& player, World& world) {
  m_deployed = m_deploying;
  m_deploying = false;
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
    if (auto result = add("config", LuaBindings::makeConfigCallbacks(
        [this](String const& name, Json const& def) { return m_config.query(name, def); })); !result)
      return std::unexpected(std::move(result.error()));
    return m_scriptComponent.init(world);
  }();
  if (!setup) {
    auto cleanup = uninit();
    if (!cleanup)
      return std::unexpected(setup.error().withContext(strf("Deployment initialization rollback failed: {}", cleanup.error().what())));
  }
  return setup;
}

std::expected<bool, StarException> PlayerDeployment::canDeploy() {
  auto result = m_scriptComponent.invoke<bool>("canDeploy");
  if (!result)
    return std::unexpected(std::move(result.error()));
  return result->value_or(false);
}

void PlayerDeployment::setDeploying(bool deploying) {
  m_deploying = deploying;
}

bool PlayerDeployment::isDeploying() const {
  return m_deploying;
}

bool PlayerDeployment::isDeployed() const {
  return m_deployed;
}

std::expected<void, StarException> PlayerDeployment::uninit() {
  std::optional<StarException> error;
  auto remember = [&](auto result) {
    if (!result) {
      if (error)
        *error = error->withContext(result.error().what());
      else
        error = std::move(result.error());
    }
  };
  remember(m_scriptComponent.uninit());
  for (auto const& name : std::initializer_list<std::string_view>{"entity", "player", "status", "config"})
    remember(m_scriptComponent.removeCallbacks(name));
  if (error)
    return std::unexpected(std::move(*error));
  return {};
}

std::expected<void, StarException> PlayerDeployment::teleportOut() {
  auto result = m_scriptComponent.invoke("teleportOut");
  if (!result)
    return std::unexpected(std::move(result.error()));
  return {};
}

std::expected<std::optional<ChainableJsonMessageResponse>, StarException> PlayerDeployment::receiveMessage(String const& message, bool localMessage, JsonArray const& args) {
  return m_scriptComponent.handleMessage(message, localMessage, args);
}

std::expected<void, StarException> PlayerDeployment::update(float dt) {
  auto updated = m_scriptComponent.update(m_scriptComponent.updateDt(dt));
  if (!updated)
    return std::unexpected(std::move(updated.error()));
  return {};
}

std::expected<void, StarException> PlayerDeployment::collectRenderEvents(RenderCallback& renderCallback, Vec2F const& position) {
  auto particles = renderCallback.addParticles(m_scriptComponent.pullNewParticles());
  if (!particles)
    return std::unexpected(std::move(particles.error()));
  for (auto const& audio : m_scriptComponent.pullNewAudios()) {
    audio->setPosition(position);
    auto added = renderCallback.addAudio(audio);
    if (!added)
      return std::unexpected(std::move(added.error()));
  }
  return {};
}

std::expected<void, StarException> PlayerDeployment::render(RenderCallback& renderCallback, Vec2F const& position) {
  for (auto drawablePair : m_scriptComponent.drawables()) {
    drawablePair.first.translate(position);
    auto added = renderCallback.addDrawable(drawablePair.first, drawablePair.second.value_or(RenderLayerPlayer));
    if (!added)
      return std::unexpected(std::move(added.error()));
  }
  return {};
}

std::expected<void, StarException> PlayerDeployment::renderLightSources(RenderCallback& renderCallback) {
  return renderCallback.addLightSources(m_scriptComponent.lightSources());
}

}
