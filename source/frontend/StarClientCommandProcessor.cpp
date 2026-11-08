import star.buffer;
import star.json_extra;
import std;
import star.json;
import star.xx_hash;
import star.ordered_map;
import star.lru_cache;
import star.thread;
import star.time;
import star.random;
import star.ttl_cache;
import star.casting;
import star.data_stream;
import star.bi_map;
import star.game_types;
import star.json_rpc;
import star.strong_typedef;
import star.vector;
import star.string;
import star.byte_array;
import star.encode;
import star.bytes;
import star.format;
import star.file;
import star.ordered_set;
import star.input_event;
import star.color;
import star.font;
import star.list;
import star.rect;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.text;
import star.asset_path;
import star.listener;
import star.set;
import star.audio;
import star.map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.output_proxy;
import star.version;
import star.either;
import star.weighted_pool;
import star.data_stream_devices;

import star.lexical_cast;
typedef struct ZSTD_CCtx_s ZSTD_CCtx;
typedef struct ZSTD_DCtx_s ZSTD_DCtx;
typedef ZSTD_DCtx ZSTD_DStream;
typedef ZSTD_CCtx ZSTD_CStream;
import star.zstd_compression;
import star.net_compatibility;
import star.id_map;
import star.worker_pool;

import star.sector_array_2d;
import star.perlin;
import star.btree;
import star.btree_database;
import star.rpc_promise;
import star.image;
import star.a_star;
import star.periodic_function;

import star.lua_root;
import star.lua_actor_movement_component;
import star.item;
import star.application;
import star.statistics_service;
import star.p2p_networking_service;
import star.user_generated_content_service;
import star.desktop_service;
import star.application_controller;
import star.renderer;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.item_descriptor;
import star.item_database;
import star.item_recipe;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.lua_components;
import star.uuid;
import star.warping;
import star.quests;
import star.quest_manager;
import star.shell_parser;
import star.host_address;
import star.sky_types;
import star.particle;
import star.weather_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.chat_types;
import star.socket;
import star.tcp;
import star.collision_block;
import star.liquid_types;
import star.tile_damage;
import star.tile_sector_array;
import star.world_layout;
import star.collision_generator;
import star.world_tiles;
import star.celestial_types;
import star.tile_modification;
import star.damage_types;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.interaction_types;
import star.world_geometry;
import star.wiring;
import star.interactive_entity;
import star.tile_entity;
import star.inspectable_entity;
import star.plant;
import star.plant_database;
import star.biome_placement;
import star.versioning_database;
import star.world_storage;
import star.player_types;
import star.sky_parameters;
import star.system_world;
import star.damage_manager;
import star.net_packets;
import star.net_packet_socket;
import star.universe_connection;
import star.entity_rendering_types;
import star.sky_render_data;
import star.parallax;
import star.cellular_light_array;
import star.cellular_lighting;
import star.world_render_data;
import star.world_structure;
import star.chat_action;
import star.entity_rendering;
import star.world;
import star.world_client_state;
import star.interpolation_tracker;
import star.ambient;
import star.weather;
import star.world_client;
import star.world_client_thread;
import star.universe;
// TODO: make this more thread safe
import star.universe_client;
import star.cinematic;
import star.client_command_processor;
import star.animated_part_set;
import star.networked_animator;
import star.humanoid;
import star.physics_entity;
import star.anchorable_entity;
import star.mobile_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.lounging_entities;
import star.chatty_entity;
import star.emote_entity;
import star.portrait_entity;
import star.damage_bar_entity;
import star.nametag_entity;
import star.inventory_types;
import star.movement_controller;
import star.platformer_astar_types;
import star.actor_movement_controller;
import star.lua_animation_component;
import star.radio_message_database;
import star.player;
import star.player_tech;
import star.player_log;

import star.ai_types;
import star.font_texture_group;
import star.anchor_types;
import star.text_painter;
import star.drawable;
import star.asset_texture_group;
import star.drawable_painter;
import star.gui_types;
import star.key_bindings;
import star.mixer;
import star.gui_context;
import star.widget;
import star.pane;

import star.game_timers;
import star.pane_manager;
import star.registered_pane_manager;


import star.main_interface_types;
import star.animation;
import star.tech_database;



import star.ai_interface;






import star.player_inventory;
import star.statistics;

namespace Star {

ClientCommandProcessor::ClientCommandProcessor(GuiContext& context, std::shared_ptr<UniverseClient> universeClient, std::shared_ptr<Cinematic> cinematicOverlay,
  MainInterfacePaneManager& paneManager, StringMap<StringList> macroCommands)
  : m_context(context), m_universeClient(std::move(universeClient)), m_cinematicOverlay(std::move(cinematicOverlay)),
  m_paneManager(paneManager), m_macroCommands(std::move(macroCommands)) {}

std::expected<CaseInsensitiveStringMap<ClientCommandProcessor::BuiltinCommand>, StarException> ClientCommandProcessor::makeBuiltinCommands() {
  std::pair<std::string_view, BuiltinCommand> const entries[] = {
    {"reload", invokeBuiltin<&ClientCommandProcessor::reload>},
    {"hotReload", invokeBuiltin<&ClientCommandProcessor::hotReload>},
    {"whoami", invokeBuiltin<&ClientCommandProcessor::whoami>},
    {"gravity", invokeBuiltin<&ClientCommandProcessor::gravity>},
    {"debug", invokeBuiltin<&ClientCommandProcessor::debug>},
    {"boxes", invokeBuiltin<&ClientCommandProcessor::boxes>},
    {"fullbright", invokeBuiltin<&ClientCommandProcessor::fullbright>},
    {"asyncLighting", invokeBuiltin<&ClientCommandProcessor::asyncLighting>},
    {"setGravity", invokeBuiltin<&ClientCommandProcessor::setGravity>},
    {"resetGravity", invokeBuiltin<&ClientCommandProcessor::resetGravity>},
    {"fixedCamera", invokeBuiltin<&ClientCommandProcessor::fixedCamera>},
    {"monochromeLighting", invokeBuiltin<&ClientCommandProcessor::monochromeLighting>},
    {"radioMessage", invokeBuiltin<&ClientCommandProcessor::radioMessage>},
    {"clearRadioMessages", invokeBuiltin<&ClientCommandProcessor::clearRadioMessages>},
    {"clearCinematics", invokeBuiltin<&ClientCommandProcessor::clearCinematics>},
    {"startQuest", invokeBuiltin<&ClientCommandProcessor::startQuest>},
    {"completeQuest", invokeBuiltin<&ClientCommandProcessor::completeQuest>},
    {"failQuest", invokeBuiltin<&ClientCommandProcessor::failQuest>},
    {"previewNewQuest", invokeBuiltin<&ClientCommandProcessor::previewNewQuest>},
    {"previewQuestComplete", invokeBuiltin<&ClientCommandProcessor::previewQuestComplete>},
    {"previewQuestFailed", invokeBuiltin<&ClientCommandProcessor::previewQuestFailed>},
    {"clearScannedObjects", invokeBuiltin<&ClientCommandProcessor::clearScannedObjects>},
    {"played", invokeBuiltin<&ClientCommandProcessor::playTime>},
    {"deaths", invokeBuiltin<&ClientCommandProcessor::deathCount>},
    {"cinema", invokeBuiltin<&ClientCommandProcessor::cinema>},
    {"suicide", invokeBuiltin<&ClientCommandProcessor::suicide>},
    {"naked", invokeBuiltin<&ClientCommandProcessor::naked>},
    {"resetAchievements", invokeBuiltin<&ClientCommandProcessor::resetAchievements>},
    {"statistic", invokeBuiltin<&ClientCommandProcessor::statistic>},
    {"giveessentialitem", invokeBuiltin<&ClientCommandProcessor::giveEssentialItem>},
    {"maketechavailable", invokeBuiltin<&ClientCommandProcessor::makeTechAvailable>},
    {"enabletech", invokeBuiltin<&ClientCommandProcessor::enableTech>},
    {"upgradeship", invokeBuiltin<&ClientCommandProcessor::upgradeShip>},
    {"swap", invokeBuiltin<&ClientCommandProcessor::swap>},
    {"respawnInWorld", invokeBuiltin<&ClientCommandProcessor::respawnInWorld>},
    {"render", invokeBuiltin<&ClientCommandProcessor::render>}
  };
  CaseInsensitiveStringMap<BuiltinCommand> commands;
  for (auto const& entry : entries) {
    auto key = CaseInsensitiveString::create(String(entry.first));
    if (!key) return std::unexpected(std::move(key.error()));
    commands.emplace(std::move(*key), entry.second);
  }
  return commands;
}

const std::expected<CaseInsensitiveStringMap<ClientCommandProcessor::BuiltinCommand>, StarException> ClientCommandProcessor::s_builtinCommands = makeBuiltinCommands();

std::expected<bool, StarException> ClientCommandProcessor::adminCommandAllowed() const {
  auto config = m_context.root().configuration();
  if (!config) return std::unexpected(std::move(config.error()));
  auto allow = (*config)->get("allowAdminCommandsFromAnyone");
  if (!allow) return std::unexpected(std::move(allow.error()));
  auto enabled = allow->toBool();
  if (!enabled) return std::unexpected(std::move(enabled.error()));
  return *enabled || m_universeClient->mainPlayer()->isAdmin();
}

std::expected<String, StarException> ClientCommandProcessor::previewQuestDialog(StringList const& arguments, String const& kind) {
  if (arguments.size() < 3)
    return std::unexpected(StarException("Quest preview requires a template, title, and text"));
  auto quest = createPreviewQuest(m_context.root(), arguments[0], arguments[1], arguments[2], *m_universeClient->mainPlayer());
  if (!quest) return std::unexpected(std::move(quest.error()));
  if (*quest) {
    auto previewed = m_universeClient->questManager()->previewDialog(std::move(*quest), kind);
    if (!previewed) return std::unexpected(std::move(previewed.error()));
    return "Previewed quest";
  }
  return "No such quest";
}

std::expected<StringList, StarException> ClientCommandProcessor::handleCommand(String const& commandLine) {
  auto result = executeCommand(commandLine);
  if (result) return result;
  if (result.error().is<ShellParsingException>()) {
    auto logged = m_context.root().logger().error("Shell parsing exception: {}", Star::output_exception(ExceptionDiagnostic(result.error()), false));
    if (!logged) return std::unexpected(std::move(logged.error()));
    return StringList{"Shell parsing exception"};
  }
  auto logged = m_context.root().logger().error("Exception caught handling client command {}: {}", commandLine, Star::output_exception(ExceptionDiagnostic(result.error()), true));
  if (!logged) return std::unexpected(std::move(logged.error()));
  return StringList{Star::format("Exception caught handling client command {}", commandLine)};
}

std::expected<StringList, StarException> ClientCommandProcessor::executeCommand(String const& commandLine) {
  auto prefixed = commandLine.starts_with("/");
  if (!prefixed) return std::unexpected(std::move(prefixed.error()));
  if (!*prefixed)
    return std::unexpected(StarException("ClientCommandProcessor expected command, does not start with '/'"));
  auto arguments = commandLine.substr(1);
  if (!arguments) return std::unexpected(std::move(arguments.error()));
  auto command = arguments->extract();
  if (!command) return std::unexpected(std::move(command.error()));
  if (!s_builtinCommands) return std::unexpected(s_builtinCommands.error());
  auto key = CaseInsensitiveStringView::create(*command);
  if (!key) return std::unexpected(std::move(key.error()));
  StringList result;
  if (auto builtin = s_builtinCommands->maybe(*key)) {
    auto output = (*builtin)(*this, *arguments);
    if (!output) return std::unexpected(std::move(output.error()));
    result.push_back(std::move(*output));
  } else if (auto macro = m_macroCommands.maybe(*command)) {
    for (auto const& entry : *macro) {
      auto isCommand = entry.starts_with("/");
      if (!isCommand) return std::unexpected(std::move(isCommand.error()));
      if (*isCommand) {
        auto output = handleCommand(entry);
        if (!output) return std::unexpected(std::move(output.error()));
        result.append_range(std::move(*output));
      } else {
        result.push_back(entry);
      }
    }
  } else {
    auto player = m_universeClient->mainPlayer();
    auto response = player->receiveMessage(connectionForEntity(player->entityId()), "/" + *command, {*arguments});
    if (!response) return std::unexpected(std::move(response.error()));
    if (*response) {
      if (std::holds_alternative<Json>(**response)) {
        auto const& value = std::get<Json>(**response);
        if (value.isType(Json::Type::String)) {
          auto text = value.stringPtr();
          if (!text) return std::unexpected(std::move(text.error()));
          result.push_back(**text);
        } else if (!value.isNull()) {
          auto printed = value.repr(1, true);
          if (!printed) return std::unexpected(std::move(printed.error()));
          result.push_back(std::move(*printed));
        }
      } else {
        Uuid uuid(m_context.root().random());
        m_commandPromises[uuid] = std::get<RpcPromise<Json>>(**response);
      }
    } else if (auto sent = m_universeClient->sendChat(commandLine, ChatSendMode::Broadcast); !sent) {
      return std::unexpected(std::move(sent.error()));
    }
  }
  return result;
}

std::expected<StringList, StarException> ClientCommandProcessor::updatePromises() {
  StringList result;
  for (auto const& uuid : m_commandPromises.keys()) {
    auto& promise = m_commandPromises[uuid];
    auto finished = promise.finished();
    if (!finished) return std::unexpected(std::move(finished.error()));
    if (!*finished) continue;
    auto succeeded = promise.succeeded();
    if (!succeeded) return std::unexpected(std::move(succeeded.error()));
    if (*succeeded) {
      auto response = promise.result();
      if (!response) return std::unexpected(std::move(response.error()));
      if (!*response)
        return std::unexpected(StarException("Successful client command promise has no result"));
      auto& value = **response;
      if (value.isType(Json::Type::String)) {
        auto text = value.stringPtr();
        if (!text) return std::unexpected(std::move(text.error()));
        result.push_back(**text);
      } else if (!value.isNull()) {
        auto printed = value.repr(1, true);
        if (!printed) return std::unexpected(std::move(printed.error()));
        result.push_back(std::move(*printed));
      }
    } else {
      auto error = promise.error();
      if (!error) return std::unexpected(std::move(error.error()));
      if (!*error)
        return std::unexpected(StarException("Failed client command promise has no error"));
      result.push_back(Star::format("Error: {}", **error));
    }
    m_commandPromises.remove(uuid);
  }
  return result;
}

bool ClientCommandProcessor::debugDisplayEnabled() const {
  return m_debugDisplayEnabled;
}

bool ClientCommandProcessor::debugHudEnabled() const {
  return m_debugHudEnabled;
}

bool ClientCommandProcessor::fixedCameraEnabled() const {
  return m_fixedCameraEnabled;
}

std::expected<String, StarException> ClientCommandProcessor::reload() {
  if (auto r = m_context.root().reload(); !r) return std::unexpected(std::move(r.error()));
  return "Client Star::Root reloaded";
}

std::expected<String, StarException> ClientCommandProcessor::hotReload() {
  if (auto r = m_context.root().hotReload(); !r) return std::unexpected(std::move(r.error()));
  return "Hot-reloaded assets";
}

std::expected<String, StarException> ClientCommandProcessor::whoami() {
  auto name = m_universeClient->mainPlayer()->name();
  if (!name) return std::unexpected(std::move(name.error()));
  return Star::format("Client: You are {}. You are {}an Admin.", *name, m_universeClient->mainPlayer()->isAdmin() ? std::string_view("") : std::string_view("not "));
}

std::expected<String, StarException> ClientCommandProcessor::gravity() {
  auto allowed = adminCommandAllowed();
  if (!allowed) return std::unexpected(std::move(allowed.error()));
  if (!*allowed) return "You must be an admin to use this command.";
  auto gravity = m_universeClient->worldClient()->gravity(m_universeClient->mainPlayer()->position());
  if (!gravity) return std::unexpected(std::move(gravity.error()));
  return Star::to_string(*gravity);
}

std::expected<String, StarException> ClientCommandProcessor::debug(String const& argumentsString) {
  auto parsed = m_parser.tokenizeToStringList(argumentsString);
  if (!parsed) return std::unexpected(std::move(parsed.error()));
  bool hud = false;
  if (!parsed->empty()) {
    auto matches = (*parsed)[0].equals_ignore_case("hud");
    if (!matches) return std::unexpected(std::move(matches.error()));
    hud = *matches;
  }
  if (hud) {
    m_debugHudEnabled = !m_debugHudEnabled;
    return Star::format("Debug HUD {}", m_debugHudEnabled ? std::string_view("enabled") : std::string_view("disabled"));
  }
  m_debugDisplayEnabled = !m_debugDisplayEnabled;
  return Star::format("Debug display {}", m_debugDisplayEnabled ? std::string_view("enabled") : std::string_view("disabled"));
}

std::expected<String, StarException> ClientCommandProcessor::boxes() {
  auto worldClient = m_universeClient->worldClient();
  bool state = !worldClient->collisionDebug();
  worldClient->setCollisionDebug(state);
  return Star::format("Geometry debug display {}", state ? std::string_view("enabled") : std::string_view("disabled"));
}

std::expected<String, StarException> ClientCommandProcessor::fullbright() {
  auto worldClient = m_universeClient->worldClient();
  bool state = !worldClient->fullBright();
  worldClient->setFullBright(state);
  return Star::format("Fullbright render lighting {}", state ? std::string_view("enabled") : std::string_view("disabled"));
}

std::expected<String, StarException> ClientCommandProcessor::asyncLighting() {
  auto worldClient = m_universeClient->worldClient();
  bool state = !worldClient->asyncLighting();
  worldClient->setAsyncLighting(state);
  return Star::format("Asynchronous render lighting {}", state ? std::string_view("enabled") : std::string_view("disabled"));
}

std::expected<String, StarException> ClientCommandProcessor::setGravity(String const& argumentsString) {
  auto arguments = m_parser.tokenizeToStringList(argumentsString);
  if (!arguments) return std::unexpected(std::move(arguments.error()));
  auto allowed = adminCommandAllowed();
  if (!allowed) return std::unexpected(std::move(allowed.error()));
  if (!*allowed) return "You must be an admin to use this command.";
  auto argument = arguments->at(0);
  if (!argument) return std::unexpected(std::move(argument.error()));
  auto value = lexical_cast<float>(argument->get());
  if (!value) return std::unexpected(std::move(value.error()));
  m_universeClient->worldClient()->overrideGravity(*value);
  return Star::format("Gravity set to {} (This is client-side!)", argument->get());
}

std::expected<String, StarException> ClientCommandProcessor::resetGravity() {
  auto allowed = adminCommandAllowed();
  if (!allowed) return std::unexpected(std::move(allowed.error()));
  if (!*allowed) return "You must be an admin to use this command.";
  m_universeClient->worldClient()->resetGravity();
  return "Gravity reset";
}

std::expected<String, StarException> ClientCommandProcessor::fixedCamera() {
  m_fixedCameraEnabled = !m_fixedCameraEnabled;
  return Star::format("Fixed camera {}", m_fixedCameraEnabled ? std::string_view("enabled") : std::string_view("disabled"));
}

std::expected<String, StarException> ClientCommandProcessor::monochromeLighting() {
  auto config = m_context.root().configuration();
  if (!config) return std::unexpected(std::move(config.error()));
  auto value = (*config)->get("monochromeLighting");
  if (!value) return std::unexpected(std::move(value.error()));
  auto enabled = value->toBool();
  if (!enabled) return std::unexpected(std::move(enabled.error()));
  bool monochrome = !*enabled;
  if (auto set = (*config)->set("monochromeLighting", monochrome); !set)
    return std::unexpected(std::move(set.error()));
  return Star::format("Monochrome lighting {}", monochrome ? std::string_view("enabled") : std::string_view("disabled"));
}

std::expected<String, StarException> ClientCommandProcessor::radioMessage(String const& argumentsString) {
  auto arguments = m_parser.tokenizeToStringList(argumentsString);
  if (!arguments) return std::unexpected(std::move(arguments.error()));
  auto allowed = adminCommandAllowed();
  if (!allowed) return std::unexpected(std::move(allowed.error()));
  if (!*allowed) return "You must be an admin to use this command.";
  if (arguments->size() != 1) return "Must provide one argument";
  if (auto queued = m_universeClient->mainPlayer()->queueRadioMessage((*arguments)[0]); !queued)
    return std::unexpected(std::move(queued.error()));
  return "Queued radio message";
}

std::expected<String, StarException> ClientCommandProcessor::clearRadioMessages() {
  auto allowed = adminCommandAllowed();
  if (!allowed) return std::unexpected(std::move(allowed.error()));
  if (!*allowed) return "You must be an admin to use this command.";
  m_universeClient->mainPlayer()->log()->clearRadioMessages();
  return "Player radio message records cleared!";
}

std::expected<String, StarException> ClientCommandProcessor::clearCinematics() {
  auto allowed = adminCommandAllowed();
  if (!allowed) return std::unexpected(std::move(allowed.error()));
  if (!*allowed) return "You must be an admin to use this command.";
  m_universeClient->mainPlayer()->log()->clearCinematics();
  return "Player cinematic records cleared!";
}

std::expected<String, StarException> ClientCommandProcessor::startQuest(String const& argumentsString) {
  auto arguments = m_parser.tokenizeToStringList(argumentsString);
  if (!arguments) return std::unexpected(std::move(arguments.error()));
  auto allowed = adminCommandAllowed();
  if (!allowed) return std::unexpected(std::move(allowed.error()));
  if (!*allowed) return "You must be an admin to use this command.";
  auto argument = arguments->at(0);
  if (!argument) return std::unexpected(std::move(argument.error()));
  auto parsed = Json::parseSequence(argument->get());
  if (!parsed) return std::unexpected(std::move(parsed.error()));
  auto descriptor = parsed->get(0);
  if (!descriptor) return std::unexpected(std::move(descriptor.error()));
  auto arc = QuestArcDescriptor::fromJson(m_context.root(), *descriptor);
  if (!arc) return std::unexpected(std::move(arc.error()));
  auto quest = Quest::create(m_context.root(), *arc, 0, *m_universeClient->mainPlayer());
  if (!quest) return std::unexpected(std::move(quest.error()));
  if (auto offered = m_universeClient->questManager()->offer(std::move(*quest)); !offered)
    return std::unexpected(std::move(offered.error()));
  return "Quest started";
}

std::expected<String, StarException> ClientCommandProcessor::completeQuest(String const& argumentsString) {
  auto arguments = m_parser.tokenizeToStringList(argumentsString);
  if (!arguments) return std::unexpected(std::move(arguments.error()));
  auto allowed = adminCommandAllowed();
  if (!allowed) return std::unexpected(std::move(allowed.error()));
  if (!*allowed) return "You must be an admin to use this command.";
  auto argument = arguments->at(0);
  if (!argument) return std::unexpected(std::move(argument.error()));
  auto quest = m_universeClient->questManager()->getQuest(argument->get());
  if (!quest) return std::unexpected(std::move(quest.error()));
  if (auto complete = (*quest)->complete(); !complete) return std::unexpected(std::move(complete.error()));
  return Star::format("Quest {} complete", argument->get());
}

std::expected<String, StarException> ClientCommandProcessor::failQuest(String const& argumentsString) {
  auto arguments = m_parser.tokenizeToStringList(argumentsString);
  if (!arguments) return std::unexpected(std::move(arguments.error()));
  auto allowed = adminCommandAllowed();
  if (!allowed) return std::unexpected(std::move(allowed.error()));
  if (!*allowed) return "You must be an admin to use this command.";
  auto argument = arguments->at(0);
  if (!argument) return std::unexpected(std::move(argument.error()));
  auto quest = m_universeClient->questManager()->getQuest(argument->get());
  if (!quest) return std::unexpected(std::move(quest.error()));
  if (auto failed = (*quest)->fail(); !failed) return std::unexpected(std::move(failed.error()));
  return Star::format("Quest {} failed", argument->get());
}

std::expected<String, StarException> ClientCommandProcessor::previewNewQuest(String const& argumentsString) {
  auto arguments = m_parser.tokenizeToStringList(argumentsString);
  if (!arguments) return std::unexpected(std::move(arguments.error()));
  auto allowed = adminCommandAllowed();
  if (!allowed) return std::unexpected(std::move(allowed.error()));
  if (!*allowed) return "You must be an admin to use this command.";
  return previewQuestDialog(*arguments, "offer");
}

std::expected<String, StarException> ClientCommandProcessor::previewQuestComplete(String const& argumentsString) {
  auto arguments = m_parser.tokenizeToStringList(argumentsString);
  if (!arguments) return std::unexpected(std::move(arguments.error()));
  auto allowed = adminCommandAllowed();
  if (!allowed) return std::unexpected(std::move(allowed.error()));
  if (!*allowed) return "You must be an admin to use this command.";
  return previewQuestDialog(*arguments, "complete");
}

std::expected<String, StarException> ClientCommandProcessor::previewQuestFailed(String const& argumentsString) {
  auto arguments = m_parser.tokenizeToStringList(argumentsString);
  if (!arguments) return std::unexpected(std::move(arguments.error()));
  auto allowed = adminCommandAllowed();
  if (!allowed) return std::unexpected(std::move(allowed.error()));
  if (!*allowed) return "You must be an admin to use this command.";
  return previewQuestDialog(*arguments, "failed");
}

std::expected<String, StarException> ClientCommandProcessor::clearScannedObjects() {
  auto allowed = adminCommandAllowed();
  if (!allowed) return std::unexpected(std::move(allowed.error()));
  if (!*allowed) return "You must be an admin to use this command.";
  m_universeClient->mainPlayer()->log()->clearScannedObjects();
  return "Player scanned objects cleared!";
}

std::expected<String, StarException> ClientCommandProcessor::playTime() {
  return Star::format("Total play time: {}", Time::printDuration(m_universeClient->mainPlayer()->log()->playTime()));
}

std::expected<String, StarException> ClientCommandProcessor::deathCount() {
  auto deaths = m_universeClient->mainPlayer()->log()->deathCount();
  return deaths ? Star::format("Total deaths: {}", deaths) : "Total deaths: 0. Well done!";
}

std::expected<String, StarException> ClientCommandProcessor::cinema(String const& argumentsString) {
  auto arguments = m_parser.tokenizeToStringList(argumentsString);
  if (!arguments) return std::unexpected(std::move(arguments.error()));
  auto allowed = adminCommandAllowed();
  if (!allowed) return std::unexpected(std::move(allowed.error()));
  if (!*allowed) return "You must be an admin to use this command.";
  auto argument = arguments->at(0);
  if (!argument) return std::unexpected(std::move(argument.error()));
  auto assets = m_context.root().assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json(argument->get());
  if (!config) return std::unexpected(std::move(config.error()));
  if (auto loaded = m_cinematicOverlay->load(*config); !loaded) return std::unexpected(std::move(loaded.error()));
  if (arguments->size() > 1) {
    auto time = lexical_cast<float>((*arguments)[1]);
    if (!time) return std::unexpected(std::move(time.error()));
    m_cinematicOverlay->setTime(*time);
  }
  return Star::format("Started cinematic {} at {}", argument->get(), arguments->size() > 1 ? (*arguments)[1] : "beginning");
}

std::expected<String, StarException> ClientCommandProcessor::suicide() {
  if (auto killed = m_universeClient->mainPlayer()->kill(); !killed) return std::unexpected(std::move(killed.error()));
  return "You are now dead";
}

std::expected<String, StarException> ClientCommandProcessor::naked() {
  if (!EquipmentSlotNames) return std::unexpected(EquipmentSlotNames.error());
  auto inventory = m_universeClient->mainPlayer()->inventory();
  for (auto slot : EquipmentSlotNames->left_values()) {
    auto item = inventory->takeSlot(slot);
    if (!item) return std::unexpected(std::move(item.error()));
    auto remaining = inventory->addToBags(std::move(*item));
    if (!remaining) return std::unexpected(std::move(remaining.error()));
    if (auto added = inventory->addItems(std::move(*remaining)); !added) return std::unexpected(std::move(added.error()));
  }
  return "You are now naked";
}

std::expected<String, StarException> ClientCommandProcessor::resetAchievements() {
  auto allowed = adminCommandAllowed();
  if (!allowed) return std::unexpected(std::move(allowed.error()));
  if (!*allowed) return "You must be an admin to use this command.";
  return m_universeClient->statistics()->reset() ? String("Achievements reset") : String("Unable to reset achievements");
}

std::expected<String, StarException> ClientCommandProcessor::statistic(String const& argumentsString) {
  auto arguments = m_parser.tokenizeToStringList(argumentsString);
  if (!arguments) return std::unexpected(std::move(arguments.error()));
  auto allowed = adminCommandAllowed();
  if (!allowed) return std::unexpected(std::move(allowed.error()));
  if (!*allowed) return "You must be an admin to use this command.";
  StringList values;
  for (auto const& name : *arguments) {
    auto value = m_universeClient->statistics()->stat(name);
    auto printed = value.repr();
    if (!printed) return std::unexpected(std::move(printed.error()));
    values.push_back(Star::format("{} = {}", name, *printed));
  }
  return values.join("\n");
}

std::expected<String, StarException> ClientCommandProcessor::giveEssentialItem(String const& argumentsString) {
  auto arguments = m_parser.tokenizeToStringList(argumentsString);
  if (!arguments) return std::unexpected(std::move(arguments.error()));
  auto allowed = adminCommandAllowed();
  if (!allowed) return std::unexpected(std::move(allowed.error()));
  if (!*allowed) return "You must be an admin to use this command.";
  if (arguments->size() < 2) return "Not enough arguments to /giveessentialitem";
  auto database = m_context.root().itemDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  auto descriptor = ItemDescriptor::create((*arguments)[0]);
  if (!descriptor) return std::unexpected(std::move(descriptor.error()));
  auto item = (*database)->item(*descriptor);
  if (!item) return std::unexpected(std::move(item.error()));
  if (!EssentialItemNames) return std::unexpected(EssentialItemNames.error());
  auto slot = EssentialItemNames->get_left((*arguments)[1]);
  if (!slot) {
    if (slot.error().is<MapException>()) return Star::format("Invalid essential item slot {}.", (*arguments)[1]);
    return std::unexpected(std::move(slot.error()));
  }
  m_universeClient->mainPlayer()->inventory()->setEssentialItem(slot->get(), *item);
  return Star::format("Put {} in player slot {}", (*item)->name(), (*arguments)[1]);
}

std::expected<String, StarException> ClientCommandProcessor::makeTechAvailable(String const& argumentsString) {
  auto arguments = m_parser.tokenizeToStringList(argumentsString);
  if (!arguments) return std::unexpected(std::move(arguments.error()));
  auto allowed = adminCommandAllowed();
  if (!allowed) return std::unexpected(std::move(allowed.error()));
  if (!*allowed) return "You must be an admin to use this command.";
  if (arguments->empty()) return "Not enough arguments to /maketechavailable";
  m_universeClient->mainPlayer()->techs()->makeAvailable((*arguments)[0]);
  return Star::format("Added {} to player's visible techs", (*arguments)[0]);
}

std::expected<String, StarException> ClientCommandProcessor::enableTech(String const& argumentsString) {
  auto arguments = m_parser.tokenizeToStringList(argumentsString);
  if (!arguments) return std::unexpected(std::move(arguments.error()));
  auto allowed = adminCommandAllowed();
  if (!allowed) return std::unexpected(std::move(allowed.error()));
  if (!*allowed) return "You must be an admin to use this command.";
  if (arguments->empty()) return "Not enough arguments to /enabletech";
  auto techs = m_universeClient->mainPlayer()->techs();
  techs->makeAvailable((*arguments)[0]);
  if (auto enabled = techs->enable((*arguments)[0]); !enabled) return std::unexpected(std::move(enabled.error()));
  return Star::format("Player tech {} enabled", (*arguments)[0]);
}

std::expected<String, StarException> ClientCommandProcessor::upgradeShip(String const& argumentsString) {
  auto arguments = m_parser.tokenizeToStringList(argumentsString);
  if (!arguments) return std::unexpected(std::move(arguments.error()));
  auto allowed = adminCommandAllowed();
  if (!allowed) return std::unexpected(std::move(allowed.error()));
  if (!*allowed) return "You must be an admin to use this command.";
  if (arguments->empty()) return "Not enough arguments to /upgradeship";
  auto upgrades = Json::parseJson((*arguments)[0]);
  if (!upgrades) return std::unexpected(std::move(upgrades.error()));
  auto invoked = m_universeClient->rpcInterface()->invokeRemote("ship.applyShipUpgrades", *upgrades);
  if (!invoked) return std::unexpected(std::move(invoked.error()));
  return "Upgraded ship";
}

std::expected<String, StarException> ClientCommandProcessor::swap(String const& argumentsString) {
  auto arguments = m_parser.tokenizeToStringList(argumentsString);
  if (!arguments) return std::unexpected(std::move(arguments.error()));
  if (arguments->empty()) {
    if (!m_paneManager) return std::unexpected(StarException("Command processor is detached from its interface"));
    auto displayed = m_paneManager->get().displayRegisteredPane(MainInterfacePanes::CharacterSwap);
    if (!displayed) return std::unexpected(std::move(displayed.error()));
    return "";
  }
  auto switched = m_universeClient->switchPlayer((*arguments)[0]);
  if (!switched) return std::unexpected(std::move(switched.error()));
  return *switched ? String("Successfully swapped player") : String("Failed to swap player");
}

std::expected<String, StarException> ClientCommandProcessor::respawnInWorld(String const& argumentsString) {
  auto arguments = m_parser.tokenizeToStringList(argumentsString);
  if (!arguments) return std::unexpected(std::move(arguments.error()));
  auto worldClient = m_universeClient->worldClient();
  if (arguments->empty())
    return Star::format("Respawn in this world is currently {}", worldClient->respawnInWorld() ? std::string_view("true") : std::string_view("false"));
  auto parsed = Json::parse((*arguments)[0]);
  if (!parsed) return std::unexpected(std::move(parsed.error()));
  auto enabled = parsed->toBool();
  if (!enabled) return std::unexpected(std::move(enabled.error()));
  worldClient->setRespawnInWorld(*enabled);
  return Star::format("Respawn in this world set to {} (This is client-side!)", *enabled ? std::string_view("true") : std::string_view("false"));
}

// Hardcoded render command, future version will write to the clipboard and possibly be implemented in Lua
std::expected<String, StarException> ClientCommandProcessor::render(String const& path) {
  if (path.empty()) {
    return "Specify a path to render an image, or for worn armor: "
           "^cyan;hat^reset;/^cyan;chest^reset;/^cyan;legs^reset;/^cyan;back^reset; "
           "or for body parts: "
           "^cyan;head^reset;/^cyan;body^reset;/^cyan;hair^reset;/^cyan;facialhair^reset;/"
           "^cyan;facialmask^reset;/^cyan;frontarm^reset;/^cyan;backarm^reset;/^cyan;emote^reset;";
  }
  AssetPath assetPath;
  bool outputSheet = false;
  String outputName = "render";
  auto player = m_universeClient->mainPlayer();
  if (player && path.utf8_size() < 100) {
    auto parsedArgs = m_parser.tokenizeToStringList(path);
    if (!parsedArgs) return std::unexpected(std::move(parsedArgs.error()));
    auto& args = *parsedArgs;
    auto lowered = args.front_value().value_or(String{}).to_lower();
    if (!lowered) return std::unexpected(std::move(lowered.error()));
    auto const& first = *lowered;
    auto humanoid = player->humanoid();
    auto& identity = humanoid->identity();
    outputSheet = true;
    outputName = first;
    if (first == "hat") {
      assetPath.basePath = humanoid->headArmorFrameset();
      assetPath.directives += humanoid->headArmorDirectives();
    } else if (first == "chest") {
      if (args.size() <= 1) {
        return "Chest armors have multiple spritesheets. Do: "
               "^white;/render chest ^cyan;front^reset;/^cyan;torso^reset;/^cyan;back^reset;. "
               "To repair old generated clothes, then also specify ^cyan;old^reset;.";
      }
      auto loweredSheet = args[1].to_lower();
      if (!loweredSheet) return std::unexpected(std::move(loweredSheet.error()));
      String sheet = std::move(*loweredSheet);
      outputName += " " + sheet;
      if (sheet == "torso") {
        assetPath.basePath = humanoid->chestArmorFrameset();
        assetPath.directives += humanoid->chestArmorDirectives();
      } else if (sheet == "front") {
        assetPath.basePath = humanoid->frontSleeveFrameset();
        assetPath.directives += humanoid->chestArmorDirectives();
      } else if (sheet == "back") {
        assetPath.basePath = humanoid->backSleeveFrameset();
        assetPath.directives += humanoid->chestArmorDirectives();
      } else {
        return Star::format("^red;Invalid chest sheet type '{}'^reset;", sheet);
      }
      // recovery for custom chests made by a very old generator
      if (args.size() > 2) {
        auto old = args[2].to_lower();
        if (!old) return std::unexpected(std::move(old.error()));
        if (*old == "old") {
          auto prefix = assetPath.basePath.starts_with("/items/armors/avian/avian-tier6separator/");
          if (!prefix) return std::unexpected(std::move(prefix.error()));
          if (*prefix) {
            auto suffix = assetPath.basePath.substr(41);
            if (!suffix) return std::unexpected(std::move(suffix.error()));
            assetPath.basePath = "/items/armors/avian/avian-tier6separator/old/" + *suffix;
          }
        }
      }
    } else if (first == "legs") {
      assetPath.basePath = humanoid->legsArmorFrameset();
      assetPath.directives += humanoid->legsArmorDirectives();
    } else if (first == "back") {
      assetPath.basePath = humanoid->backArmorFrameset();
      assetPath.directives += humanoid->backArmorDirectives();
    } else if (first == "body") {
      assetPath.basePath = humanoid->getBodyFromIdentity();
      assetPath.directives += identity.bodyDirectives;
    } else if (first == "head") {
      outputSheet = false;
      assetPath.basePath = humanoid->getHeadFromIdentity();
      assetPath.subPath = String("normal");
      assetPath.directives += identity.bodyDirectives;
    } else if (first == "hair") {
      outputSheet = false;
      assetPath.basePath = humanoid->getHairFromIdentity();
      assetPath.subPath = String("normal");
      assetPath.directives += identity.hairDirectives;
    } else if (first == "facialhair") {
      outputSheet = false;
      assetPath.basePath = humanoid->getFacialHairFromIdentity();
      assetPath.subPath = String("normal");
      assetPath.directives += identity.facialHairDirectives;
    } else if (first == "facialmask") {
      outputSheet = false;
      assetPath.basePath = humanoid->getFacialMaskFromIdentity();
      assetPath.subPath = String("normal");
      assetPath.directives += identity.facialMaskDirectives;
    } else if (first == "frontarm") {
      assetPath.basePath = humanoid->getFrontArmFromIdentity();
      assetPath.directives += identity.bodyDirectives;
    } else if (first == "backarm") {
      assetPath.basePath = humanoid->getBackArmFromIdentity();
      assetPath.directives += identity.bodyDirectives;
    } else if (first == "emote") {
      assetPath.basePath = humanoid->getFacialEmotesFromIdentity();
      assetPath.directives += identity.emoteDirectives;
    } else {
      outputSheet = false;
      outputName = "render";
    }
  }
  if (assetPath == AssetPath()) {
    assetPath = AssetPath::split(path);
    auto absolute = assetPath.basePath.starts_with("/");
    if (!absolute) return std::unexpected(std::move(absolute.error()));
    if (!*absolute)
      assetPath.basePath = "/assetmissing.png" + assetPath.basePath;
  }
  auto assets = m_context.root().assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  std::shared_ptr<Image const> image;
  if (outputSheet) {
    auto original = (*assets)->image(assetPath.basePath);
    if (!original) return std::unexpected(std::move(original.error()));
    auto sheet = std::make_shared<Image>(**original);
    sheet->convert(PixelFormat::RGBA32);
    AssetPath framePath = assetPath;

    StringMap<std::pair<RectU, std::shared_ptr<Image const>>> frames;
    auto imageFrames = (*assets)->imageFrames(assetPath.basePath);
    if (!imageFrames) return std::unexpected(std::move(imageFrames.error()));
    if (*imageFrames)
      for (auto const& pair : (*imageFrames)->frames)
        frames[pair.first] = std::make_pair(pair.second, std::shared_ptr<Image const>());

    if (frames.empty())
      return "^red;Failed to save image^reset;";

    for (auto&& entry : frames) {
      framePath.subPath = entry.first;
      auto frame = (*assets)->image(framePath);
      if (!frame) return std::unexpected(std::move(frame.error()));
      entry.second.second = std::move(*frame);
    }

    Vec2U frameSize = frames.begin()->second.first.size();
    if (frameSize.min() == 0)
      return std::unexpected(StarException("Sprite sheet frame size is empty"));
    Vec2U imageSize = frames.begin()->second.second->size().piecewise_min(Vec2U{256, 256});
    if (imageSize.min() == 0)
      return "^red;Resulting image is empty^reset;";

    for (auto&& frame : frames) {
      RectU& box = frame.second.first;
      box.setXMin((box.xMin() / frameSize[0]) * imageSize[0]);
      box.setYMin(((sheet->height() - box.yMin() - box.height()) / frameSize[1]) * imageSize[1]);
      box.setXMax(box.xMin() + imageSize[0]);
      box.setYMax(box.yMin() + imageSize[1]);
    }

    if (frameSize != imageSize) {
      unsigned sheetWidth  = (sheet->width()  / frameSize[0]) * imageSize[0];
      unsigned sheetHeight = (sheet->height() / frameSize[0]) * imageSize[0];
      sheet->reset(sheetWidth, sheetHeight, PixelFormat::RGBA32);
    }

    for (auto&& entry : frames) {
      auto copied = sheet->copyInto(entry.second.first.min(), *entry.second.second);
      if (!copied) return std::unexpected(std::move(copied.error()));
    }

    image = std::move(sheet);
  } else {
    auto loaded = (*assets)->image(assetPath);
    if (!loaded) return std::unexpected(std::move(loaded.error()));
    image = std::move(*loaded);
  }
  if (image->size().min() == 0)
    return "^red;Resulting image is empty^reset;";
  auto outputDirectory = m_context.root().toStoragePath("output");
  auto outputPath = File::relative_to(outputDirectory, Star::format("{}.png", outputName));
  auto directoryExists = File::is_directory(outputDirectory);
  if (!directoryExists) return std::unexpected(std::move(directoryExists.error()));
  if (!*directoryExists) {
    if (auto created = File::create_directory(outputDirectory); !created)
      return std::unexpected(std::move(created.error()));
  }
  auto buffer = std::make_shared<Buffer>();
  if (auto written = image->writePng(m_context.root().logger(), buffer); !written)
    return std::unexpected(std::move(written.error()));
  auto file = File::open(outputPath, IOMode::Write | IOMode::Truncate);
  if (!file) return std::unexpected(std::move(file.error()));
  auto size = buffer->size();
  if (!size) return std::unexpected(std::move(size.error()));
  auto written = (*file)->write_full(buffer->span().first(static_cast<std::size_t>(*size)));
  auto closed = (*file)->close();
  if (!written) {
    if (!closed)
      return std::unexpected(written.error().with_context(Star::format("Closing output file also failed: {}", closed.error().what())));
    return std::unexpected(std::move(written.error()));
  }
  if (!closed) return std::unexpected(std::move(closed.error()));
  auto fullPath = File::full_path(outputPath);
  if (!fullPath) return std::unexpected(std::move(fullPath.error()));
  auto copied = m_context.setClipboardImage(*image, std::ref(buffer->data()), std::cref(*fullPath));
  if (!copied) return std::unexpected(std::move(copied.error()));
  return Star::format("Saved '{}.png' ({}x{}) and copied to clipboard", outputName, image->width(), image->height());
}


}
