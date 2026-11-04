import star.json;
import star.xx_hash;
import star.ordered_map;
import star.bi_map;
import star.thread;
import star.io_device;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ref_ptr;
import star.logging;
import star.listener;
import star.time;
import star.version;
import star.file;
import star.id_map;
import star.casting;
import star.rpc_promise;
import star.strong_typedef;
import star.game_types;
import star.vector;
import star.rect;
import star.a_star;
import star.ordered_set;
import star.audio;
import star.map;
import star.set;
import star.color;
import star.string;
import star.either;
import star.random;
import star.periodic_function;
import star.image;
import star.weighted_pool;
import star.net_compatibility;
import std;
import star.format;
import star.worker_pool;

import star.sector_array_2d;
import star.perlin;
import star.btree;
import star.lru_cache;
import star.data_stream_devices;
import star.btree_database;
import star.lua;

import star.test_universe;
import star.universe_client;
import star.universe_server;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.statistics_service;
import star.uuid;
import star.item_descriptor;
import star.drawable;
import star.animation;
import star.particle;
import star.animated_part_set;
import star.mixer;
import star.light_source;
import star.networked_animator;
import star.humanoid;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.entity;
import star.interaction_types;
import star.tile_damage;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_modification;
import star.lua_root;
import star.force_regions;
import star.world;
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
import star.celestial_coordinate;
import star.quest_descriptor;
import star.ai_types;
import star.entity_rendering_types;
import star.entity_rendering;
import star.player_types;
import star.lua_components;
import star.lua_actor_movement_component;
import star.lua_animation_component;
import star.radio_message_database;
import star.player;
import star.liquid_types;
import star.tile_sector_array;
import star.weather_types;
import star.sky_types;
import star.world_parameters;
import star.celestial_parameters;
import star.world_layout;
import star.collision_generator;
import star.world_tiles;
import star.celestial_types;
import star.chat_types;
import star.warping;
import star.wiring;
import star.plant;
import star.plant_database;
import star.biome_placement;
import star.versioning_database;
import star.world_storage;
import star.sky_parameters;
import star.system_world;
import star.damage_manager;
import star.net_packets;
import star.sky_render_data;
import star.parallax;
import star.cellular_light_array;
import star.cellular_lighting;
import star.world_render_data;
import star.world_structure;
import star.world_client_state;
import star.interpolation_tracker;
import star.ambient;
import star.weather;
import star.world_client;
import star.world_server;
import star.scripted_entity;
import star.stagehand;

import star.player_storage;
import star.statistics;
import star.client_context;
import star.player_factory;


namespace Star {

TestUniverse::TestUniverse(Vec2U clientWindowSize)
  : m_clientWindowSize(clientWindowSize) {}

std::expected<std::unique_ptr<TestUniverse>, StarException> TestUniverse::create(Root& root, Vec2U clientWindowSize) {
  auto fixture = std::unique_ptr<TestUniverse>(new TestUniverse(clientWindowSize));
  auto storagePath = File::temporaryDirectory(root.random());
  if (!storagePath)
    return std::unexpected(std::move(storagePath.error()));
  fixture->m_storagePath = std::move(*storagePath);
  auto playerStorage = PlayerStorage::create(root, File::relativeTo(fixture->m_storagePath, "player"));
  if (!playerStorage)
    return std::unexpected(std::move(playerStorage.error()));
  auto statistics = Statistics::create(root, File::relativeTo(fixture->m_storagePath, "statistics"));
  if (!statistics)
    return std::unexpected(std::move(statistics.error()));
  auto server = UniverseServer::create(root, File::relativeTo(fixture->m_storagePath, "universe"));
  if (!server)
    return std::unexpected(std::move(server.error()));
  fixture->m_server = std::move(*server);
  auto client = UniverseClient::create(root, *playerStorage, *statistics, File::relativeTo(fixture->m_storagePath, "universeclient"));
  if (!client)
    return std::unexpected(std::move(client.error()));
  fixture->m_client = std::move(*client);
  auto started = fixture->m_server->start();
  if (!started)
    return std::unexpected(std::move(started.error()));
  auto playerFactory = root.playerFactory();
  if (!playerFactory)
    return std::unexpected(std::move(playerFactory.error()));
  auto player = (*playerFactory)->create();
  if (!player)
    return std::unexpected(std::move(player.error()));
  fixture->m_mainPlayer = std::move(*player);
  auto finalized = fixture->m_mainPlayer->finalizeCreation();
  if (!finalized)
    return std::unexpected(std::move(finalized.error()));
  // A player only carries a ship species once it has been saved and loaded
  // (Player::diskStore falls back to the identity species, Player's save/load
  // ctor reads it back), which is what the real client always does
  // (StarClientApplication::loadPlayer).  The connect packet sends the ship
  // species and the server builds the ship world from it, so a player that was
  // only created in memory cannot connect: the server fails to create its ship
  // world and drops the connection.
  fixture->m_mainPlayer->setShipSpecies(fixture->m_mainPlayer->species());
  fixture->m_mainPlayer->setAdmin(true);
  auto mode = fixture->m_mainPlayer->setModeType(PlayerMode::Survival);
  if (!mode)
    return std::unexpected(std::move(mode.error()));
  auto mainPlayer = fixture->m_client->setMainPlayer(fixture->m_mainPlayer);
  if (!mainPlayer)
    return std::unexpected(std::move(mainPlayer.error()));
  // The real client supplies input callbacks bound to its explicit Input owner.
  // This headless fixture has no input device or client Input, so provide
  // callbacks where nothing is ever bound. OpenStarbound's player
  // scripts (e.g. /scripts/opensb/player/copy_paste.lua) call input.bindDown
  // from update() and would otherwise log a Lua error every frame.
  LuaCallbacks inputCallbacks;
  auto bindState = [](String const&, String const&) -> std::optional<unsigned> { return {}; };
  auto noKey = [](String const&) -> std::optional<unsigned> { return {}; };
  auto noButton = [](String const&) -> std::optional<List<Vec2F>> { return {}; };
  auto bindDown = inputCallbacks.registerCallback("bindDown", bindState);
  if (!bindDown)
    return std::unexpected(std::move(bindDown.error()));
  auto bindUp = inputCallbacks.registerCallback("bindUp", bindState);
  if (!bindUp)
    return std::unexpected(std::move(bindUp.error()));
  auto bindHeld = inputCallbacks.registerCallback("bindHeld", [](String const&, String const&) -> bool { return false; });
  if (!bindHeld)
    return std::unexpected(std::move(bindHeld.error()));
  auto bind = inputCallbacks.registerCallback("bind", [](String const&, String const&) -> bool { return false; });
  if (!bind)
    return std::unexpected(std::move(bind.error()));
  auto keyDown = inputCallbacks.registerCallback("keyDown", noKey);
  if (!keyDown)
    return std::unexpected(std::move(keyDown.error()));
  auto keyUp = inputCallbacks.registerCallback("keyUp", noKey);
  if (!keyUp)
    return std::unexpected(std::move(keyUp.error()));
  auto keyHeld = inputCallbacks.registerCallback("keyHeld", [](String const&) -> bool { return false; });
  if (!keyHeld)
    return std::unexpected(std::move(keyHeld.error()));
  auto key = inputCallbacks.registerCallback("key", [](String const&) -> bool { return false; });
  if (!key)
    return std::unexpected(std::move(key.error()));
  auto mouseDown = inputCallbacks.registerCallback("mouseDown", noButton);
  if (!mouseDown)
    return std::unexpected(std::move(mouseDown.error()));
  auto mouseUp = inputCallbacks.registerCallback("mouseUp", noButton);
  if (!mouseUp)
    return std::unexpected(std::move(mouseUp.error()));
  auto mouseHeld = inputCallbacks.registerCallback("mouseHeld", [](String const&) -> bool { return false; });
  if (!mouseHeld)
    return std::unexpected(std::move(mouseHeld.error()));
  auto mouse = inputCallbacks.registerCallback("mouse", [](String const&) -> bool { return false; });
  if (!mouse)
    return std::unexpected(std::move(mouse.error()));
  auto mousePosition = inputCallbacks.registerCallback("mousePosition", []() -> Vec2F { return {}; });
  if (!mousePosition)
    return std::unexpected(std::move(mousePosition.error()));
  auto getTag = inputCallbacks.registerCallback("getTag", [](String const&) -> unsigned { return 0; });
  if (!getTag)
    return std::unexpected(std::move(getTag.error()));
  auto events = inputCallbacks.registerCallback("events", []() -> Json { return JsonArray(); });
  if (!events)
    return std::unexpected(std::move(events.error()));
  auto resetBinds = inputCallbacks.registerCallback("resetBinds", [](String const&, String const&) {});
  if (!resetBinds)
    return std::unexpected(std::move(resetBinds.error()));
  auto setBinds = inputCallbacks.registerCallback("setBinds", [](String const&, String const&, Json const&) {});
  if (!setBinds)
    return std::unexpected(std::move(setBinds.error()));
  auto getDefaultBinds = inputCallbacks.registerCallback("getDefaultBinds", [](String const&, String const&) -> Json { return JsonObject(); });
  if (!getDefaultBinds)
    return std::unexpected(std::move(getDefaultBinds.error()));
  auto getBinds = inputCallbacks.registerCallback("getBinds", [](String const&, String const&) -> Json { return JsonObject(); });
  if (!getBinds)
    return std::unexpected(std::move(getBinds.error()));
  auto callbacks = fixture->m_client->setLuaCallbacks("input", inputCallbacks);
  if (!callbacks)
    return std::unexpected(std::move(callbacks.error()));
  auto localClient = fixture->m_server->addLocalClient();
  if (!localClient)
    return std::unexpected(std::move(localClient.error()));
  auto connected = fixture->m_client->connect(std::move(*localClient), true);
  if (!connected)
    return std::unexpected(std::move(connected.error()));
  if (*connected)
    return std::unexpected(StarException(strf("Test universe login rejected: {}", **connected)));
  return fixture;
}

TestUniverse::~TestUniverse() {
  auto closed = close();
  if (!closed)
    EmergencyLog::log(LogLevel::Error, closed.error().what());
}

std::expected<void, StarException> TestUniverse::close() {
  std::optional<StarException> error;
  if (m_client) {
    auto disconnected = m_client->disconnect();
    if (!disconnected)
      error = std::move(disconnected.error());
    m_client.reset();
  }
  if (m_server) {
    auto stopped = m_server->stop();
    if (!stopped && !error)
      error = std::move(stopped.error());
    auto joined = m_server->join();
    if (!joined && !error)
      error = std::move(joined.error());
    m_server.reset();
  }
  m_mainPlayer.reset();
  if (!m_storagePath.empty()) {
    auto removed = File::removeDirectoryRecursive(m_storagePath);
    if (removed)
      m_storagePath.clear();
    else if (!error)
      error = std::move(removed.error());
  }
  if (error)
    return std::unexpected(std::move(*error));
  return {};
}

std::expected<void, StarException> TestUniverse::warpPlayer(WorldId worldId) {
  // The client context only exists once the server has answered the connect
  // handshake, and both the warp call and the caller's expectations read it, so
  // let the client settle first.
  std::int64_t connectStart = Time::monotonicMilliseconds();
  while (!m_client->clientContext() && Time::monotonicMilliseconds() - connectStart < 60000) {
    auto updated = m_client->update(0.016f);
    if (!updated)
      return std::unexpected(std::move(updated.error()));
    if (!m_client->clientContext())
      Thread::sleep(16);
  }

  if (!m_client->clientContext())
    return std::unexpected(StarException("Timed out waiting for the test universe login handshake"));
  auto warped = m_client->warpPlayer(WarpToWorld(worldId), true);
  if (!warped)
    return std::unexpected(std::move(warped.error()));
  // Bounded by wall clock: if the world cannot be loaded the player stays
  // teleporting forever, so give up after five minutes and let the caller's
  // expectations report the failure instead of hanging the test.  A ship flight
  // to a celestial world legitimately takes tens of seconds, and a sanitizer
  // build several times that, so the runner's per-case timeout
  // (OPENSTARBOUND_TEST_TIMEOUT) is the practical limit.
  std::int64_t startTime = Time::monotonicMilliseconds();
  while (m_mainPlayer->isTeleporting() || std::holds_alternative<std::monostate>(m_client->playerWorld())) {
    auto updated = m_client->update(0.016f);
    if (!updated)
      return std::unexpected(std::move(updated.error()));
    if (!m_mainPlayer->isTeleporting() && !std::holds_alternative<std::monostate>(m_client->playerWorld()))
      break;
    if (Time::monotonicMilliseconds() - startTime > 300000)
      return std::unexpected(StarException("Timed out waiting for the test player to finish warping"));
    Thread::sleep(16);
  }
  return {};
}

WorldId TestUniverse::currentPlayerWorld() const {
  if (auto clientContext = m_client->clientContext())
    return clientContext->playerWorldId();

  return {};
}

std::expected<void, StarException> TestUniverse::update(unsigned times) {
  for (unsigned i = 0; i < times; ++i) {
    auto updated = m_client->update(0.016f);
    if (!updated)
      return std::unexpected(std::move(updated.error()));
    Thread::sleep(16);
  }
  return {};
}

std::shared_ptr<WorldClient> TestUniverse::worldClient() const {
  return m_client ? m_client->worldClient() : nullptr;
}

std::expected<List<Drawable>, StarException> TestUniverse::currentClientDrawables() {
  WorldRenderData renderData;
  auto worldClient = m_client->worldClient();
  if (!worldClient)
    return std::unexpected(StarException("Test player has no world to render"));
  worldClient->centerClientWindowOnPlayer(m_clientWindowSize);
  auto rendered = worldClient->render(renderData, 0, std::nullopt, 1.0f);
  if (!rendered)
    return std::unexpected(std::move(rendered.error()));

  List<Drawable> drawables;
  for (auto& ed : renderData.entityDrawables) {
    for (auto& p : ed.layers)
      drawables.appendAll(std::move(p.second));
  }

  return drawables;
}

std::expected<TestUniverse::ReadyWorld, StarException>
TestUniverse::waitForWorldReady(std::int64_t timeoutMilliseconds) {
  if (!m_client || !m_server || !m_client->clientContext())
    return std::unexpected(TestUniverseException("Cannot wait for a closed or disconnected test universe"));

  struct Observation {
    std::uint64_t firstStep;
    bool present = false;
  };
  std::unordered_map<EntityId, Observation> observedEntities;
  std::unordered_map<std::string, Observation> observedWorldScripts;
  ReadyWorld ready{{}, 0, 0, 0, 0};
  std::optional<RpcPromise<Json>> barrier;
  auto worldId = currentPlayerWorld();
  auto connectionId = m_client->clientContext()->connectionId();
  auto start = Time::monotonicMilliseconds();
  bool sampled = false;
  auto assets = m_mainPlayer->root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto contexts = (*assets)->json("/worldserver.config:scriptContexts");
  if (!contexts)
    return std::unexpected(std::move(contexts.error()));
  auto configuredContexts = contexts->toObject();
  if (!configuredContexts)
    return std::unexpected(std::move(configuredContexts.error()));

  while (true) {
    // Preserve update()'s real client cadence; the independent server/world
    // threads continue advancing throughout this bounded wait.
    auto updated = update(1);
    if (!updated)
      return std::unexpected(std::move(updated.error()));
    if (Time::monotonicMilliseconds() - start >= timeoutMilliseconds)
      return std::unexpected(TestUniverseTimeoutException(
        strf("Timed out waiting for test world readiness (server steps {}..{}, scripted NPCs {}, coordinators {})",
             ready.firstServerStep, ready.readyServerStep, ready.observedScriptedNpcs, ready.observedCoordinators)));
    if (currentPlayerWorld() != worldId)
      return std::unexpected(StarException("Test player left the world while waiting for readiness"));

    if (barrier) {
      auto result = barrier->result();
      if (!result)
        return std::unexpected(std::move(result.error()));
      if (!*result) {
        auto error = barrier->error();
        if (!error)
          return std::unexpected(std::move(error.error()));
        if (*error)
          return std::unexpected(StarException(strf("Test world readiness message failed: {}", **error)));
        continue;
      }
      // WorldServerThread settles messages only after WorldServer::update
      // succeeds, including spawner, all entities and world-script updates.
      auto drawables = currentClientDrawables();
      if (!drawables)
        return std::unexpected(std::move(drawables.error()));
      if (drawables->empty())
        continue;
      ready.drawables = std::move(*drawables);
      return ready;
    }

    bool schedulesReady = true;
    float expiryTime = 0;
    for (auto& [id, observation] : observedEntities)
      observation.present = false;
    auto executed = m_server->executeForClient(
      connectionId, [&](WorldServer& world, std::shared_ptr<Player> player) -> std::expected<void, StarException> {
        auto step = world.currentStep();
        ready.readyServerStep = step;
        if (!sampled) {
          ready.firstServerStep = step;
          sampled = true;
        }
        // A rendered player does not imply its surrounding sectors have
        // finished activation/generation (and thus instantiated their NPCs).
        auto active =
          world.regionActive(RectI::withCenter(Vec2I::floor(player->position()), Vec2I(m_clientWindowSize)));
        if (!active)
          return std::unexpected(std::move(active.error()));
        schedulesReady = *active && step > ready.firstServerStep;
        expiryTime = world.expiryTime();
        float dt = world.root().timing().serverTimestep * world.root().timing().timescale;

        auto settled = [&](Observation& observation, double period) {
          observation.present = true;
          // LuaScriptUpdate::updateDt() is stepCount * lastDt, not elapsed
          // wall time. Periodic(0) deliberately disables updates; otherwise
          // wait a full observed scheduling period after first seeing it.
          if (dt <= 0 || step == observation.firstStep
              || (period > 0 && step - observation.firstStep < std::ceil(period / dt)))
            schedulesReady = false;
        };
        for (auto const& [name, scripts] : *configuredContexts) {
          auto context = world.scriptContext(name);
          if (!context || context->error() || !context->context())
            return std::unexpected(StarException(strf("Test world script context is not healthy: {}", name)));
          auto& observation = observedWorldScripts.try_emplace(name.utf8(), Observation{step}).first->second;
          settled(observation, context->updateDt());
        }
        for (auto id : world.entityIds()) {
          auto entity = world.entity(id);
          if (!entity || (entity->entityType() != EntityType::Npc && entity->entityType() != EntityType::Stagehand))
            continue;
          auto scripted = as<ScriptedEntity>(entity);
          if (!scripted)
            continue;
          auto period = scripted->evalScript("return script.updateDt()");
          if (!period)
            return std::unexpected(std::move(period.error()));
          if (!*period)
            continue;// A stagehand may legitimately have no scripts.
          double seconds;
          if (auto value = std::get_if<LuaFloat>(&**period))
            seconds = *value;
          else if (auto value = std::get_if<LuaInt>(&**period))
            seconds = *value;
          else
            return std::unexpected(StarException("Test entity script did not report its scheduling period"));
          auto [entry, inserted] = observedEntities.try_emplace(id, Observation{step});
          if (inserted) {
            if (entity->entityType() == EntityType::Npc) {
              ++ready.observedScriptedNpcs;
            } else {
              auto type = as<Stagehand>(entity)->typeName();
              if (!type)
                return std::unexpected(std::move(type.error()));
              if (*type == "coordinator")
                ++ready.observedCoordinators;
            }
          }
          settled(entry->second, seconds);
        }
        return {};
      });
    if (!executed)
      return std::unexpected(std::move(executed.error()));
    if (!*executed)
      return std::unexpected(StarException("Test server no longer has the player in a world"));
    // Retired entities must not hold the wait open; their absence is observed
    // under the same server-world lock as the scheduling samples.
    std::erase_if(observedEntities, [](auto const& pair) { return !pair.second.present; });
    if (!schedulesReady)
      continue;
    // Reuse a real handler without altering its expiry setting. Its response
    // proves a *successful* later world update, not merely currentStep++, which
    // happens before entity/script execution and can precede a failure.
    auto message = m_server->sendWorldMessage(worldId, "keepAlive", JsonArray{expiryTime});
    if (!message)
      return std::unexpected(std::move(message.error()));
    barrier = std::move(*message);
  }
}
}
