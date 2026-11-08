import star.json;
import star.xx_hash;
import star.json_extra;
import star.ordered_map;
import star.lru_cache;
import star.thread;
import star.random;
import star.ttl_cache;
import star.casting;
import star.data_stream;
import star.bi_map;
import star.game_types;
import star.set;
import star.json_rpc;
import star.string;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.vector;
import star.strong_typedef;
import star.file;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import star.format;
import star.version;
import star.id_map;
import star.rpc_promise;
import star.lua;
import star.audio;
import star.map;
import star.a_star;
import star.periodic_function;
import star.periodic;
import star.ordered_set;
import star.either;
import star.weighted_pool;
import star.byte_array;
import star.data_stream_devices;

import std;
using ZSTD_CCtx = struct ZSTD_CCtx_s;
using ZSTD_DCtx = struct ZSTD_DCtx_s;
using ZSTD_DStream = ZSTD_DCtx;
using ZSTD_CStream = ZSTD_CCtx;
import star.zstd_compression;
import star.net_compatibility;
import star.worker_pool;

import star.sector_array_2d;
import star.perlin;
import star.btree;
import star.btree_database;
import star.image;

import star.lua_root;
import star.lua_actor_movement_component;
import star.item;
import star.item_descriptor;
import star.item_database;
import star.item_recipe;
import star.drawable;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.lua_components;
import star.uuid;
import star.warping;
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
import star.mixer;
import star.entity_rendering_types;
import star.animation;
import star.particle;
import star.entity_rendering;
import star.platformer_astar_types;
import star.anchorable_entity;
import star.actor_movement_controller;
import star.animated_part_set;
import star.networked_animator;
import star.portrait_entity;
import star.damage_bar_entity;
import star.nametag_entity;
import star.aggressive_entity;
import star.chat_action;
import star.chatty_entity;
import star.lua_animation_component;
import star.actor_entity;
import star.monster_database;
import star.effect_emitter;
import star.monster;
import star.humanoid;
import star.emote_entity;
import star.lounging_entities;
import star.tool_user_entity;
import star.npc_database;
import star.npc;
import star.status_effect_entity;
import star.wiring;
import star.wire_entity;
import star.inspectable_entity;
import star.object;
import star.quests;
import star.quest_manager;
import star.inventory_types;
import star.ai_types;
import star.player_types;
import star.radio_message_database;
import star.player;
import star.host_address;
import star.sky_types;
import star.weather_types;
import star.world_parameters;
import star.celestial_parameters;
import star.chat_types;
import star.socket;
import star.tcp;
import star.p2p_networking_service;
import star.liquid_types;
import star.tile_sector_array;
import star.world_layout;
import star.collision_generator;
import star.world_tiles;
import star.celestial_types;
import star.plant;
import star.plant_database;
import star.biome_placement;
import star.world_storage;
import star.sky_parameters;
import star.system_world;
import star.damage_manager;
import star.net_packets;
import star.net_packet_socket;
import star.universe_connection;
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
import star.world_client_thread;
import star.universe;
// TODO: make this more thread safe
import star.universe_client;

import star.player_tech;

import star.player_lua_bindings;
import star.entity_lua_bindings;
import star.config_lua_bindings;
import star.celestial_lua_bindings;
import star.status_controller_lua_bindings;
import star.quest_template_database;
import star.versioning_database;
import star.object_database;


import star.player_inventory;
import star.client_context;

import star.algorithm;
namespace Star {

extern std::expected<EnumMap<QuestState>, StarException> const QuestStateNames = EnumMap<QuestState>::create({
  {QuestState::New, "New"}, {QuestState::Offer, "Offer"}, {QuestState::Active, "Active"},
  {QuestState::Complete, "Complete"}, {QuestState::Failed, "Failed"}
});

namespace {
std::uint64_t nextQuestPresentationRevision() {
  // Replacing a quest with the same stable ID must invalidate its consumers,
  // even when neither old nor new object has received a script mutation.
  static std::atomic<std::uint64_t> revision{0};
  return revision.fetch_add(1, std::memory_order_relaxed) + 1;
}
}

Quest::Quest(Root& root)
  : m_root(root),
    m_state(QuestState::New), m_inited(false), m_showDialog(false), m_arcPos(0),
    m_displayParameters{}, m_money(0), m_lastUpdatedOn(),
    m_unread(true), m_canTurnIn(false), m_presentationRevision(nextQuestPresentationRevision()),
    m_portraitRevision(m_presentationRevision), m_rewardsRevision(m_presentationRevision),
    m_stateRevision(m_presentationRevision), m_scriptComponent(root) {}

std::expected<std::shared_ptr<Quest>, StarException> Quest::create(Root& root, QuestArcDescriptor const& arc, std::size_t arcPos, Player& player) {
  auto quest = std::shared_ptr<Quest>(new Quest(root));
  auto assets = root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto tracked = (*assets)->json("/quests/quests.config:trackedCustomIndicator");
  if (!tracked) return std::unexpected(std::move(tracked.error()));
  auto trackedString = tracked->toString();
  if (!trackedString) return std::unexpected(std::move(trackedString.error()));
  quest->m_trackedIndicator = std::move(*trackedString);
  auto untracked = (*assets)->json("/quests/quests.config:untrackedCustomIndicator");
  if (!untracked) return std::unexpected(std::move(untracked.error()));
  auto untrackedString = untracked->toString();
  if (!untrackedString) return std::unexpected(std::move(untrackedString.error()));
  quest->m_untrackedIndicator = std::move(*untrackedString);
  quest->m_lastUpdatedOn = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500));
  if (arcPos >= arc.quests.size())
    return std::unexpected(StarException::format("Quest arc position {} out of range", arcPos));
  quest->m_arc = arc;
  quest->m_arcPos = arcPos;
  auto items = root.itemDatabase();
  if (!items) return std::unexpected(std::move(items.error()));
  auto config = quest->getTemplate();
  if (!config) return std::unexpected(std::move(config.error()));
  auto const& qt = **config;
  quest->m_parameters = quest->questDescriptor().parameters;
  quest->m_displayParameters = DisplayParameters{qt.ephemeral, qt.showInLog, qt.showAcceptDialog,
      qt.showCompleteDialog, qt.showFailDialog, qt.mainQuest, qt.hideCrossServer};
  auto parameter = quest->setEntityParameter("player", player);
  if (!parameter) return std::unexpected(std::move(parameter.error()));
  auto money = root.random().randUInt(qt.moneyRange[0], qt.moneyRange[1]);
  if (!money) return std::unexpected(std::move(money.error()));
  quest->m_money = *money;
  for (auto const& descriptor : root.random().randValueFrom(qt.rewards, {})) {
    auto item = (*items)->item(descriptor);
    if (!item) return std::unexpected(std::move(item.error()));
    quest->m_rewards.push_back(*item);
  }
  for (auto const& name : qt.rewardParameters) {
    auto parameter = quest->m_parameters.find_ref(name);
    if (!parameter) continue;
    if (std::holds_alternative<QuestItem>(parameter->get().detail)) {
      auto descriptor = std::get<QuestItem>(parameter->get().detail).descriptor();
      if (!descriptor) return std::unexpected(std::move(descriptor.error()));
      auto added = quest->addReward(*descriptor);
      if (!added) return std::unexpected(std::move(added.error()));
    } else if (std::holds_alternative<QuestItemList>(parameter->get().detail)) {
      for (auto const& item : std::get<QuestItemList>(parameter->get().detail)) {
        auto added = quest->addReward(item);
        if (!added) return std::unexpected(std::move(added.error()));
      }
    } else return std::unexpected(StarException::format("Quest parameter {} cannot be used as a reward parameter", name));
  }
  quest->m_title = qt.title;
  quest->m_text = qt.text;
  quest->m_completionText = qt.completionText;
  quest->m_failureText = qt.failureText;
  return quest;
}

std::expected<std::shared_ptr<Quest>, StarException> Quest::create(Root& root, Json const& spec) {
  auto quest = std::shared_ptr<Quest>(new Quest(root));
  auto assets = root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto tracked = (*assets)->json("/quests/quests.config:trackedCustomIndicator");
  if (!tracked) return std::unexpected(std::move(tracked.error()));
  auto trackedString = tracked->toString();
  if (!trackedString) return std::unexpected(std::move(trackedString.error()));
  quest->m_trackedIndicator = std::move(*trackedString);
  auto untracked = (*assets)->json("/quests/quests.config:untrackedCustomIndicator");
  if (!untracked) return std::unexpected(std::move(untracked.error()));
  auto untrackedString = untracked->toString();
  if (!untrackedString) return std::unexpected(std::move(untrackedString.error()));
  quest->m_untrackedIndicator = std::move(*untrackedString);
  auto versions = root.versioningDatabase();
  if (!versions) return std::unexpected(std::move(versions.error()));
  auto versioned = VersionedJson::fromJson(spec);
  if (!versioned) return std::unexpected(std::move(versioned.error()));
  auto store = (*versions)->loadVersionedJson(*versioned, "Quest");
  if (!store) return std::unexpected(std::move(store.error()));
  auto stateName = store->getString("state");
  if (!stateName) return std::unexpected(std::move(stateName.error()));
  if (!QuestStateNames) return std::unexpected(QuestStateNames.error());
  auto state = QuestStateNames->get_left(*stateName);
  if (!state) return std::unexpected(std::move(state.error()));
  quest->m_state = state->get();
  auto arcJson = store->get("arc");
  if (!arcJson) return std::unexpected(std::move(arcJson.error()));
  auto arc = QuestArcDescriptor::diskLoad(root, *arcJson);
  if (!arc) return std::unexpected(std::move(arc.error()));
  quest->m_arc = std::move(*arc);
  auto position = store->getUInt("arcPos");
  if (!position) return std::unexpected(std::move(position.error()));
  if (*position >= quest->m_arc.quests.size())
    return std::unexpected(StarException::format("Quest arc position {} out of range", *position));
  quest->m_arcPos = *position;
  auto parametersJson = store->get("parameters");
  if (!parametersJson) return std::unexpected(std::move(parametersJson.error()));
  auto parameters = questParamsDiskLoad(root, *parametersJson);
  if (!parameters) return std::unexpected(std::move(parameters.error()));
  quest->m_parameters = std::move(*parameters);
  auto worldName = store->optString("worldId");
  if (!worldName) return std::unexpected(std::move(worldName.error()));
  if (*worldName) {
    auto world = parseWorldId(**worldName);
    if (!world) return std::unexpected(std::move(world.error()));
    quest->m_worldId = std::move(*world);
  }
  auto locationJson = store->opt("location");
  if (!locationJson) return std::unexpected(std::move(locationJson.error()));
  if (*locationJson) {
    auto systemJson = (**locationJson).get("system");
    if (!systemJson) return std::unexpected(std::move(systemJson.error()));
    auto system = jsonToVec3I(*systemJson);
    if (!system) return std::unexpected(std::move(system.error()));
    auto value = (**locationJson).get("location");
    if (!value) return std::unexpected(std::move(value.error()));
    auto location = jsonToSystemLocation(*value);
    if (!location) return std::unexpected(std::move(location.error()));
    quest->m_location = std::make_pair(*system, std::move(*location));
  }
  auto uuidString = store->optString("serverUuid");
  if (!uuidString) return std::unexpected(std::move(uuidString.error()));
  if (*uuidString) {
    auto uuid = Uuid::create(**uuidString);
    if (!uuid) return std::unexpected(std::move(uuid.error()));
    quest->m_serverUuid = std::move(*uuid);
  }
  auto money = store->getUInt("money");
  if (!money) return std::unexpected(std::move(money.error()));
  quest->m_money = *money;
  auto items = root.itemDatabase();
  if (!items) return std::unexpected(std::move(items.error()));
  auto rewards = store->getArray("rewards");
  if (!rewards) return std::unexpected(std::move(rewards.error()));
  for (auto const& entry : *rewards) {
    auto item = (*items)->diskLoad(entry);
    if (!item) return std::unexpected(std::move(item.error()));
    quest->m_rewards.push_back(*item);
  }
  auto updated = store->getInt("lastUpdatedOn");
  if (!updated) return std::unexpected(std::move(updated.error()));
  quest->m_lastUpdatedOn = std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds>(std::chrono::milliseconds(*updated));
  auto unread = store->getBool("unread", true);
  if (!unread) return std::unexpected(std::move(unread.error()));
  quest->m_unread = *unread;
  auto turnIn = store->getBool("canTurnIn", false);
  if (!turnIn) return std::unexpected(std::move(turnIn.error()));
  quest->m_canTurnIn = *turnIn;
  auto indicatorsJson = store->get("indicators", JsonArray{});
  if (!indicatorsJson) return std::unexpected(std::move(indicatorsJson.error()));
  auto indicators = jsonToStringSet(*indicatorsJson);
  if (!indicators) return std::unexpected(std::move(indicators.error()));
  quest->m_indicators = std::move(*indicators);
  auto storage = store->getObject("scriptStorage", {});
  if (!storage) return std::unexpected(std::move(storage.error()));
  auto setStorage = quest->m_scriptComponent.setScriptStorage(std::move(*storage));
  if (!setStorage) return std::unexpected(std::move(setStorage.error()));
  auto config = quest->getTemplate();
  if (!config) return std::unexpected(std::move(config.error()));
  auto const& qt = **config;
  quest->m_displayParameters = DisplayParameters{qt.ephemeral, qt.showInLog, qt.showAcceptDialog,
      qt.showCompleteDialog, qt.showFailDialog, qt.mainQuest, qt.hideCrossServer};
  auto title = store->getString("title", qt.title);
  if (!title) return std::unexpected(std::move(title.error()));
  quest->m_title = std::move(*title);
  auto text = store->getString("text", qt.text);
  if (!text) return std::unexpected(std::move(text.error()));
  quest->m_text = std::move(*text);
  auto completion = store->getString("completionText", qt.completionText);
  if (!completion) return std::unexpected(std::move(completion.error()));
  quest->m_completionText = std::move(*completion);
  auto failure = store->getString("failureText", qt.failureText);
  if (!failure) return std::unexpected(std::move(failure.error()));
  quest->m_failureText = std::move(*failure);
  auto portraitsJson = store->get("portraits", JsonObject{});
  if (!portraitsJson) return std::unexpected(std::move(portraitsJson.error()));
  auto portraits = jsonToMapV<StringMap<List<Drawable>>>(*portraitsJson, [&root](Json const& portrait) -> std::expected<List<Drawable>, StarException> {
    auto metadata = root.imageMetadataDatabase();
    if (!metadata) return std::unexpected(std::move(metadata.error()));
    auto array = portrait.toArray();
    if (!array) return std::unexpected(std::move(array.error()));
    List<Drawable> result;
    for (auto const& value : *array) {
      auto drawable = Drawable::create(**metadata, value);
      if (!drawable) return std::unexpected(std::move(drawable.error()));
      result.push_back(std::move(*drawable));
    }
    return result;
  });
  if (!portraits) return std::unexpected(std::move(portraits.error()));
  quest->m_portraits = std::move(*portraits);
  auto titlesJson = store->get("portraitTitles", JsonObject{});
  if (!titlesJson) return std::unexpected(std::move(titlesJson.error()));
  auto titles = jsonToMapV<StringMap<String>>(*titlesJson, [](Json const& json) { return json.toString(); });
  if (!titles) return std::unexpected(std::move(titles.error()));
  quest->m_portraitTitles = std::move(*titles);
  auto show = store->getBool("showDialog", false);
  if (!show) return std::unexpected(std::move(show.error()));
  quest->m_showDialog = *show;
  return quest;
}

std::expected<Json, StarException> Quest::diskStore() const {
  auto versions = m_root.versioningDatabase();
  if (!versions) return std::unexpected(std::move(versions.error()));
  if (!QuestStateNames) return std::unexpected(QuestStateNames.error());
  auto state = QuestStateNames->get_right(m_state);
  if (!state) return std::unexpected(std::move(state.error()));
  auto arc = m_arc.diskStore(m_root);
  if (!arc) return std::unexpected(std::move(arc.error()));
  auto parameters = questParamsDiskStore(m_root, m_parameters);
  if (!parameters) return std::unexpected(std::move(parameters.error()));
  JsonObject result{{"state", state->get()}, {"arc", *arc}, {"arcPos", m_arcPos},
      {"parameters", *parameters}, {"money", m_money}};
  auto world = jsonFromOptional(m_worldId, printWorldId);
  if (!world) return std::unexpected(std::move(world.error()));
  result["worldId"] = std::move(*world);
  auto location = jsonFromOptional(m_location, [](std::pair<Vec3I, SystemLocation> const& location) -> Json {
    return JsonObject{{"system", jsonFromVec3I(location.first)}, {"location", jsonFromSystemLocation(location.second)}};
  });
  if (!location) return std::unexpected(std::move(location.error()));
  result["location"] = std::move(*location);
  result["serverUuid"] = jsonFromOptional(m_serverUuid.transform(std::mem_fn(&Uuid::hex)));
  auto items = m_root.itemDatabase();
  if (!items) return std::unexpected(std::move(items.error()));
  auto rewards = jsonFromList(m_rewards, [&items](auto const& item) { return (*items)->diskStore(item); });
  if (!rewards) return std::unexpected(std::move(rewards.error()));
  result["rewards"] = std::move(*rewards);
  result["lastUpdatedOn"] = m_lastUpdatedOn.time_since_epoch().count();
  result["unread"] = m_unread;
  result["canTurnIn"] = m_canTurnIn;
  result["indicators"] = jsonFromStringSet(m_indicators);
  auto storage = m_scriptComponent.getScriptStorage();
  if (!storage) return std::unexpected(std::move(storage.error()));
  result["scriptStorage"] = std::move(*storage);
  result["title"] = m_title;
  result["text"] = m_text;
  result["completionText"] = m_completionText;
  result["failureText"] = m_failureText;
  auto portraits = jsonFromMapV(m_portraits, [](List<Drawable> const& portrait) {
    return jsonFromList(portrait, [](Drawable const& drawable) { return drawable.toJson(); });
  });
  if (!portraits) return std::unexpected(std::move(portraits.error()));
  result["portraits"] = std::move(*portraits);
  auto titles = jsonFromMap(m_portraitTitles);
  if (!titles) return std::unexpected(std::move(titles.error()));
  result["portraitTitles"] = std::move(*titles);
  result["showDialog"] = m_showDialog;
  auto versioned = (*versions)->makeCurrentVersionedJson("Quest", result);
  if (!versioned) return std::unexpected(std::move(versioned.error()));
  return versioned->toJson();
}

std::expected<std::shared_ptr<QuestTemplate>, StarException> Quest::getTemplate() const {
  auto database = m_root.questTemplateDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  auto result = (*database)->questTemplate(templateId());
  if (!result) return std::unexpected(std::move(result.error()));
  if (!*result) return std::unexpected(StarException::format("No quest template found for id '{}'", templateId()));
  return *result;
}

std::expected<void, StarException> Quest::init(Player& player, World& world, std::optional<std::reference_wrapper<UniverseClient>> client) {
  m_player = player;
  m_world = world;
  m_client = client;
  if (m_state == QuestState::Offer || m_state == QuestState::Active) {
    auto result = initScript();
    if (!result) detachPlayer();
    return result;
  }
  return {};
}
std::expected<void, StarException> Quest::uninit() {
  auto result = uninitScript();
  detachPlayer();
  return result;
}
void Quest::detachPlayer() {
  m_player.reset();
  m_world.reset();
  m_client.reset();
}
std::expected<std::optional<ChainableJsonMessageResponse>, StarException> Quest::receiveMessage(String const& message, bool localMessage, JsonArray const& args) {
  if (!m_inited) return std::optional<ChainableJsonMessageResponse>();
  return m_scriptComponent.handleMessage(message, localMessage, args);
}
std::expected<std::optional<LuaValue>, StarException> Quest::callScript(String const& func, LuaVariadic<LuaValue> const& args) {
  if (!m_inited) return std::optional<LuaValue>();
  return m_scriptComponent.invoke(func, args);
}
std::expected<void, StarException> Quest::update(float dt) {
  if (!m_inited) return {};
  auto result = m_scriptComponent.update(m_scriptComponent.updateDt(dt));
  if (!result) return std::unexpected(std::move(result.error()));
  return {};
}
std::expected<void, StarException> Quest::offer() {
  if (!showAcceptDialog()) return start();
  setState(QuestState::Offer);
  auto initialized = initScript();
  if (!initialized) return std::unexpected(std::move(initialized.error()));
  auto result = m_scriptComponent.invoke("questOffer");
  if (!result) return std::unexpected(std::move(result.error()));
  return {};
}
std::expected<void, StarException> Quest::declineOffer() {
  setState(QuestState::New);
  auto result = m_scriptComponent.invoke("questDecline");
  if (!result) return std::unexpected(std::move(result.error()));
  return uninitScript();
}
std::expected<void, StarException> Quest::cancelOffer() {
  setState(QuestState::New);
  auto result = m_scriptComponent.invoke("questCancel");
  if (!result) return std::unexpected(std::move(result.error()));
  return uninitScript();
}
std::expected<void, StarException> Quest::start() {
  if (!m_player || !m_world)
    return std::unexpected(StarException("Cannot start a quest without an attached player world"));
  setState(QuestState::Active);
  auto initialized = initScript();
  if (!initialized) return std::unexpected(std::move(initialized.error()));
  auto current = m_player->get().questManager()->trackedQuest();
  if (mainQuest() || !current) m_player->get().questManager()->setAsTracked(questId());
  auto result = m_scriptComponent.invoke("questStart");
  if (!result) return std::unexpected(std::move(result.error()));
  return {};
}
std::expected<void, StarException> Quest::complete(std::optional<std::size_t> followupIndex) {
  if (!m_player || !m_world)
    return std::unexpected(StarException("Cannot complete a quest without an attached player world"));
  setState(QuestState::Complete);
  m_showDialog = showCompleteDialog();
  auto result = m_scriptComponent.invoke("questComplete");
  if (!result) return std::unexpected(std::move(result.error()));
  auto uninitialized = uninitScript();
  if (!uninitialized) return std::unexpected(std::move(uninitialized.error()));
  for (auto const& item : rewards()) {
    auto clone = item->clone();
    if (!clone) return std::unexpected(std::move(clone.error()));
    auto given = m_player->get().giveItem(*clone);
    if (!given) return std::unexpected(std::move(given.error()));
  }
  auto currency = m_player->get().inventory()->addCurrency("money", money());
  if (!currency) return std::unexpected(std::move(currency.error()));
  bool track = m_player->get().questManager()->isTracked(questId());
  std::size_t next = followupIndex.value_or(questArcPosition() + 1);
  if (next < m_arc.quests.size()) {
    auto followup = Quest::create(m_root, m_arc, next, m_player->get());
    if (!followup) return std::unexpected(std::move(followup.error()));
    (*followup)->setWorldId(worldId());
    (*followup)->setLocation(location());
    (*followup)->setServerUuid(serverUuid());
    auto offered = m_player->get().questManager()->offer(*followup);
    if (!offered) return std::unexpected(std::move(offered.error()));
    if (track) m_player->get().questManager()->setAsTracked((*followup)->questId());
  } else if (track) {
    if (auto main = m_player->get().questManager()->getFirstMainQuest()) m_player->get().questManager()->setAsTracked((*main)->questId());
    else m_player->get().questManager()->setAsTracked({});
  }
  return {};
}
std::expected<void, StarException> Quest::fail() {
  setState(QuestState::Failed);
  m_showDialog = showFailDialog();
  auto result = m_scriptComponent.invoke("questFail", false);
  if (!result) return std::unexpected(std::move(result.error()));
  return uninitScript();
}
std::expected<void, StarException> Quest::abandon() {
  setState(QuestState::Failed);
  m_showDialog = false;
  auto result = m_scriptComponent.invoke("questFail", true);
  if (!result) return std::unexpected(std::move(result.error()));
  return uninitScript();
}
std::expected<bool, StarException> Quest::interactWithEntity(EntityId entity) {
  auto result = m_scriptComponent.invoke<bool>("questInteract", entity);
  if (!result) return std::unexpected(std::move(result.error()));
  return result->value_or(false);
}

String Quest::questId() const {
  return m_arc.quests[m_arcPos].questId;
}

String Quest::templateId() const {
  return m_arc.quests[m_arcPos].templateId;
}

StringMap<QuestParam> const& Quest::parameters() const {
  return m_parameters;
}

QuestState Quest::state() const {
  return m_state;
}

bool Quest::showDialog() const {
  return m_showDialog;
}

void Quest::setDialogShown() {
  m_showDialog = false;
}

std::expected<void, StarException> Quest::setEntityParameter(String const& paramName, std::shared_ptr<Entity const> const& entity) {
  return setEntityParameter(paramName, *entity);
}

void Quest::setParameter(String const& paramName, QuestParam const& paramValue) {
  m_parameters[paramName] = paramValue;
}

std::optional<List<Drawable>> Quest::portrait(String const& portraitName) const {
  return m_portraits.maybe(portraitName);
}

std::optional<String> Quest::portraitTitle(String const& portraitName) const {
  return m_portraitTitles.maybe(portraitName);
}

QuestDescriptor Quest::questDescriptor() const {
  return m_arc.quests[m_arcPos];
}

QuestArcDescriptor Quest::questArcDescriptor() const {
  return m_arc;
}

std::size_t Quest::questArcPosition() const {
  return m_arcPos;
}

std::optional<WorldId> Quest::worldId() const {
  return m_worldId;
}

std::optional<std::pair<Vec3I, SystemLocation>> Quest::location() const {
  return m_location;
}

std::optional<Uuid> Quest::serverUuid() const {
  return m_serverUuid;
}

void Quest::setWorldId(std::optional<WorldId> worldId) { m_worldId = std::move(worldId); }

void Quest::setLocation(std::optional<std::pair<Vec3I, SystemLocation>> location) {
  m_location = std::move(location);
}

void Quest::setServerUuid(std::optional<Uuid> serverUuid) {
  m_serverUuid = serverUuid;
}

String Quest::title() const {
  return m_title;
}

String Quest::text() const {
  return m_text;
}

String Quest::completionText() const {
  return m_completionText;
}

String Quest::failureText() const {
  return m_failureText;
}

std::size_t Quest::money() const {
  return m_money;
}

List<std::shared_ptr<Item const>> const& Quest::rewards() const {
  return m_rewards;
}

std::uint64_t Quest::presentationRevision() const {
  return m_presentationRevision;
}

std::uint64_t Quest::portraitRevision() const {
  return m_portraitRevision;
}

std::uint64_t Quest::rewardsRevision() const {
  return m_rewardsRevision;
}

std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds> Quest::lastUpdatedOn() const {
  return m_lastUpdatedOn;
}

bool Quest::unread() const {
  return m_unread;
}

void Quest::markAsRead() {
  m_unread = false;
}

bool Quest::canTurnIn() const {
  return m_canTurnIn;
}

std::expected<String, StarException> Quest::questGiverIndicator() const {
  auto config = getTemplate();
  if (!config) return std::unexpected(std::move(config.error()));
  return (*config)->questGiverIndicator;
}
std::expected<String, StarException> Quest::questReceiverIndicator() const {
  auto config = getTemplate();
  if (!config) return std::unexpected(std::move(config.error()));
  return (*config)->questReceiverIndicator;
}
[[nodiscard]] std::expected<bool, StarException> hasItemIndicator(Root& root, std::shared_ptr<Entity> const& entity, List<ItemDescriptor> const& items) {
  if (auto drop = as<ItemDrop>(entity)) {
    for (auto const& item : items) if (drop->item()->matches(item, true)) return true;
  } else if (auto object = as<Object>(entity)) {
    auto database = root.objectDatabase();
    if (!database) return std::unexpected(std::move(database.error()));
    auto name = object->name();
    if (!name) return std::unexpected(std::move(name.error()));
    auto config = (*database)->getConfig(*name);
    if (!config) return std::unexpected(std::move(config.error()));
    if (!(*config)->hasObjectItem) return false;
    for (auto const& item : items) if (*name == item.name()) return true;
  }
  return false;
}
std::expected<std::optional<String>, StarException> Quest::customIndicator(std::shared_ptr<Entity> const& entity) const {
  if (!m_inited) return std::optional<String>();
  for (auto const& indicator : m_indicators) {
    auto parameter = parameters().get(indicator);
    if (!parameter) return std::unexpected(std::move(parameter.error()));
    auto const& param = parameter->get();
    bool matches = false;
    if (std::holds_alternative<QuestEntity>(param.detail)) {
      auto const& questEntity = std::get<QuestEntity>(param.detail);
      matches = questEntity.uniqueId && entity->uniqueId() == questEntity.uniqueId;
    } else if (std::holds_alternative<QuestItem>(param.detail)) {
      auto descriptor = std::get<QuestItem>(param.detail).descriptor();
      if (!descriptor) return std::unexpected(std::move(descriptor.error()));
      auto result = hasItemIndicator(m_root, entity, {*descriptor});
      if (!result) return std::unexpected(std::move(result.error()));
      matches = *result;
    } else if (std::holds_alternative<QuestItemTag>(param.detail)) {
      if (auto drop = as<ItemDrop>(entity)) {
        auto tags = drop->item()->itemTags();
        if (!tags) return std::unexpected(std::move(tags.error()));
        matches = tags->contains(std::get<QuestItemTag>(param.detail));
      }
    } else if (std::holds_alternative<QuestItemList>(param.detail)) {
      auto result = hasItemIndicator(m_root, entity, std::get<QuestItemList>(param.detail));
      if (!result) return std::unexpected(std::move(result.error()));
      matches = *result;
    } else if (std::holds_alternative<QuestMonsterType>(param.detail)) {
      if (auto monster = as<Monster>(entity)) {
        if (monster->typeName() == std::get<QuestMonsterType>(param.detail).typeName) {
          auto team = monster->getTeam().type;
          matches = team == TeamType::Enemy || team == TeamType::Passive;
        }
      }
    }
    if (matches) return std::optional<String>(param.indicator.value_or(defaultCustomIndicator()));
  }
  return std::optional<String>();
}

std::optional<JsonArray> const& Quest::objectiveList() const {
  return m_objectiveList;
}

std::optional<float> Quest::progress() const {
  return m_progress;
}

std::optional<float> Quest::compassDirection() const {
  return m_compassDirection;
}

void Quest::setObjectiveList(std::optional<JsonArray> const& objectiveList) {
  m_objectiveList = objectiveList;
}

void Quest::setProgress(std::optional<float> const& progress) {
  m_progress = progress;
}

void Quest::setCompassDirection(std::optional<float> const& compassDirection) {
  m_compassDirection = compassDirection;
}

std::expected<std::optional<String>, StarException> Quest::completionCinema() const {
  auto config = getTemplate();
  if (!config) return std::unexpected(std::move(config.error()));
  return (*config)->completionCinema;
}
std::expected<bool, StarException> Quest::canBeAbandoned() const {
  auto config = getTemplate();
  if (!config) return std::unexpected(std::move(config.error()));
  return (*config)->canBeAbandoned;
}

bool Quest::ephemeral() const {
  return m_displayParameters.ephemeral;
}

bool Quest::showInLog() const {
  return m_displayParameters.showInLog;
}

bool Quest::showAcceptDialog() const {
  return m_displayParameters.showAcceptDialog;
}

bool Quest::showCompleteDialog() const {
  return m_displayParameters.showCompleteDialog;
}

bool Quest::showFailDialog() const {
  return m_displayParameters.showFailDialog;
}

bool Quest::mainQuest() const {
  return m_displayParameters.mainQuest;
}

bool Quest::hideCrossServer() const {
  return m_displayParameters.hideCrossServer;
}

void Quest::setState(QuestState state) {
  m_state = state;
  m_stateRevision = nextQuestPresentationRevision();
  m_lastUpdatedOn = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500));
}

std::expected<void, StarException> Quest::initScript() {
  if (!m_player || !m_world || m_inited) return {};
  auto result = initScriptBindings();
  if (!result) {
    auto error = std::move(result.error());
    auto shutdown = uninitScript();
    if (!shutdown) error = error.withRelated(shutdown.error(), "Error rolling back quest script");
    return std::unexpected(std::move(error));
  }
  return {};
}
std::expected<void, StarException> Quest::initScriptBindings() {
  auto config = getTemplate();
  if (!config) return std::unexpected(std::move(config.error()));
  auto scripts = (*config)->script ? m_scriptComponent.setScript(*(*config)->script)
      : m_scriptComponent.setScripts(StringList{});
  if (!scripts) return std::unexpected(std::move(scripts.error()));
  m_scriptComponent.setUpdateDelta((*config)->updateDelta);
  auto quest = makeQuestCallbacks(m_player->get());
  if (!quest) return std::unexpected(std::move(quest.error()));
  auto addedQuest = m_scriptComponent.addCallbacks("quest", std::move(*quest));
  if (!addedQuest) return std::unexpected(std::move(addedQuest.error()));
  if (m_client) {
    auto celestial = LuaBindings::makeCelestialCallbacks(m_root, m_client->get());
    if (!celestial) return std::unexpected(std::move(celestial.error()));
    auto addedCelestial = m_scriptComponent.addCallbacks("celestial", std::move(*celestial));
    if (!addedCelestial) return std::unexpected(std::move(addedCelestial.error()));
  }
  auto player = LuaBindings::makePlayerCallbacks(m_root, m_player->get());
  if (!player) return std::unexpected(std::move(player.error()));
  auto addedPlayer = m_scriptComponent.addCallbacks("player", std::move(*player));
  if (!addedPlayer) return std::unexpected(std::move(addedPlayer.error()));
  auto configuration = LuaBindings::makeConfigCallbacks([&quest = *this](String const& name, Json const& def) -> std::expected<Json, StarException> {
    auto config = quest.getTemplate();
    if (!config) return std::unexpected(std::move(config.error()));
    return Json((*config)->scriptConfig).query(name, def);
  });
  if (!configuration) return std::unexpected(std::move(configuration.error()));
  auto addedConfig = m_scriptComponent.addCallbacks("config", std::move(*configuration));
  if (!addedConfig) return std::unexpected(std::move(addedConfig.error()));
  auto entity = LuaBindings::makeEntityCallbacks(m_player->get());
  if (!entity) return std::unexpected(std::move(entity.error()));
  auto addedEntity = m_scriptComponent.addCallbacks("entity", std::move(*entity));
  if (!addedEntity) return std::unexpected(std::move(addedEntity.error()));
  auto status = LuaBindings::makeStatusControllerCallbacks(m_player->get().statusController());
  if (!status) return std::unexpected(std::move(status.error()));
  auto addedStatus = m_scriptComponent.addCallbacks("status", std::move(*status));
  if (!addedStatus) return std::unexpected(std::move(addedStatus.error()));
  auto movement = m_scriptComponent.addActorMovementCallbacks(m_player->get().movementController());
  if (!movement) return std::unexpected(std::move(movement.error()));
  m_inited = true;
  return m_scriptComponent.init(m_world->get());
}
std::expected<void, StarException> Quest::uninitScript() {
  std::optional<StarException> error;
  auto uninitialized = m_scriptComponent.uninit();
  if (!uninitialized) error = std::move(uninitialized.error());
  for (String const& group : {String("quest"), String("celestial"), String("player"), String("config"), String("entity"), String("status")}) {
    auto removed = m_scriptComponent.removeCallbacks(group);
    if (!removed && !error) error = std::move(removed.error());
  }
  auto movement = m_scriptComponent.removeActorMovementCallbacks();
  if (!movement && !error) error = std::move(movement.error());
  m_inited = false;
  if (error) return std::unexpected(std::move(*error));
  return {};
}

std::expected<LuaCallbacks, StarException> Quest::makeQuestCallbacks(Player& player) {
  LuaCallbacks callbacks;
  if (auto r = callbacks.registerCallback("context", [&quest = *this]() { quest.m_scriptComponent.context(); }); !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("state", [&quest = *this]() -> std::expected<String, StarException> {
    if (!QuestStateNames) return std::unexpected(QuestStateNames.error());
    auto name = QuestStateNames->get_right(quest.state());
    if (!name) return std::unexpected(std::move(name.error()));
    return name->get();
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("complete", [&quest = *this](std::optional<std::size_t> followup) { return quest.complete(followup); }); !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("fail", [&quest = *this]() { return quest.fail(); }); !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("abandon", [&quest = *this]() { return quest.abandon(); }); !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("decline", [&quest = *this]() { return quest.declineOffer(); }); !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("cancel", [&quest = *this]() { return quest.cancelOffer(); }); !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("setCanTurnIn", [&quest = *this](bool value) { quest.m_canTurnIn = value; }); !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("questId", [&quest = *this]() { return quest.questId(); }); !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("templateId", [&quest = *this]() { return quest.templateId(); }); !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("seed", [&quest = *this]() { return quest.questDescriptor().seed; }); !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("questDescriptor", [&quest = *this]() { return quest.questDescriptor().toJson(); }); !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("questArcDescriptor", [&quest = *this]() { return quest.questArcDescriptor().toJson(); }); !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("questArcPosition", [&quest = *this]() { return quest.m_arcPos; }); !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("worldId", [&quest = *this]() -> std::expected<std::optional<String>, StarException> {
    if (!quest.m_worldId) return std::optional<String>();
    auto name = printWorldId(*quest.m_worldId);
    if (!name) return std::unexpected(std::move(name.error()));
    return std::optional<String>(*name);
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("setWorldId", [&quest = *this](std::optional<String> const& name) -> std::expected<void, StarException> {
    if (!name) { quest.setWorldId({}); return {}; }
    auto world = parseWorldId(*name);
    if (!world) return std::unexpected(std::move(world.error()));
    quest.setWorldId(std::move(*world));
    return {};
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("serverUuid", [&quest = *this]() { return quest.serverUuid().transform(std::mem_fn(&Uuid::hex)); }); !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("setServerUuid", [&quest = *this](String const& value) -> std::expected<void, StarException> {
    auto uuid = Uuid::create(value);
    if (!uuid) return std::unexpected(std::move(uuid.error()));
    quest.setServerUuid(std::move(*uuid));
    return {};
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("isCurrent", [&quest = *this, &player]() { return player.questManager()->isCurrent(quest.questId()); }); !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("location", [&quest = *this]() -> std::expected<Json, StarException> {
    if (auto loc = quest.location()) {
      return JsonObject{{"system", jsonFromVec3I(loc->first)}, {"location", jsonFromSystemLocation(loc->second)}};
    }
    return Json();
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("setLocation", [&quest = *this](Json const& json) -> std::expected<void, StarException> {
    if (json.isNull()) { quest.setLocation({}); return {}; }
    auto systemJson = json.get("system");
    if (!systemJson) return std::unexpected(std::move(systemJson.error()));
    auto system = jsonToVec3I(*systemJson);
    if (!system) return std::unexpected(std::move(system.error()));
    auto locationJson = json.opt("location");
    if (!locationJson) return std::unexpected(std::move(locationJson.error()));
    auto location = jsonToSystemLocation(locationJson->value_or(Json()));
    if (!location) return std::unexpected(std::move(location.error()));
    quest.setLocation(std::make_pair(*system, std::move(*location)));
    return {};
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("parameters", [&quest = *this]() { return questParamsToJson(quest.parameters()); }); !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("setParameter", [&quest = *this](String const& name, Json const& json) -> std::expected<void, StarException> {
    auto parameter = QuestParam::fromJson(json);
    if (!parameter) return std::unexpected(std::move(parameter.error()));
    quest.m_parameters[name] = std::move(*parameter);
    return {};
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("setIndicators", [&quest = *this](StringList indicators) { quest.m_indicators = StringSet::from(indicators); }); !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("setObjectiveList", [&quest = *this](std::optional<JsonArray> const& value) { quest.setObjectiveList(value); }); !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("setProgress", [&quest = *this](std::optional<float> const& value) { quest.setProgress(value); }); !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("setCompassDirection", [&quest = *this](std::optional<float> const& value) { quest.setCompassDirection(value); }); !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("setTitle", [&quest = *this](String const& value) {
    if (quest.m_title != value) { quest.m_title = value; quest.m_presentationRevision = nextQuestPresentationRevision(); }
  }); !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("setText", [&quest = *this](String const& value) {
    if (quest.m_text != value) { quest.m_text = value; quest.m_presentationRevision = nextQuestPresentationRevision(); }
  }); !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("setCompletionText", [&quest = *this](String const& value) { quest.m_completionText = value; }); !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("setFailureText", [&quest = *this](String const& value) { quest.m_failureText = value; }); !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("setPortrait", [&quest = *this](String const& name, std::optional<JsonArray> const& portrait) -> std::expected<void, StarException> {
    if (!portrait) {
      if (quest.m_portraits.contains(name)) {
        quest.m_portraits.remove(name);
        quest.m_portraitRevision = nextQuestPresentationRevision();
      }
      return {};
    }
    auto metadata = quest.m_root.imageMetadataDatabase();
    if (!metadata) return std::unexpected(std::move(metadata.error()));
    List<Drawable> result;
    result.reserve(portrait->size());
    for (auto const& json : *portrait) {
      auto drawable = Drawable::create(**metadata, json);
      if (!drawable) return std::unexpected(std::move(drawable.error()));
      result.push_back(std::move(*drawable));
    }
    if (auto previous = quest.m_portraits.find_ref(name); previous && previous->get() == result)
      return {};
    quest.m_portraits[name] = std::move(result);
    quest.m_portraitRevision = nextQuestPresentationRevision();
    return {};
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r =
        callbacks.registerCallback("setPortraitTitle",
                                   [&quest = *this](String const& name, std::optional<String> const& title) {
                                     auto previous = quest.m_portraitTitles.find_ref(name);
                                     if ((!previous && !title) || (previous && title && previous->get() == *title))
                                       return;
                                     if (title)
                                       quest.m_portraitTitles[name] = *title;
                                     else
                                       quest.m_portraitTitles.remove(name);
                                     quest.m_portraitRevision = nextQuestPresentationRevision();
                                   });
      !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("addReward", [&quest = *this](Json const& reward) -> std::expected<void, StarException> {
    auto descriptor = ItemDescriptor::create(reward);
    if (!descriptor) return std::unexpected(std::move(descriptor.error()));
    return quest.addReward(*descriptor);
  }); !r) return std::unexpected(std::move(r.error()));
  return callbacks;
}

std::expected<void, StarException> Quest::setEntityParameter(String const& paramName, Entity const& entity) {
  std::optional<Json> portrait;
  std::optional<String> name;
  std::optional<String> species;
  std::optional<Gender> gender;
  if (auto portraitEntity = asRef<PortraitEntity>(entity)) {
    auto drawables = portraitEntity->get().portrait(PortraitMode::Full);
    if (!drawables) return std::unexpected(std::move(drawables.error()));
    auto json = jsonFromList(*drawables, [](Drawable const& drawable) { return drawable.toJson(); });
    if (!json) return std::unexpected(std::move(json.error()));
    portrait = std::move(*json);
    auto entityName = portraitEntity->get().name();
    if (!entityName) return std::unexpected(std::move(entityName.error()));
    name = std::move(*entityName);
  }
  if (auto npc = asRef<Npc>(entity)) {
    species = npc->get().species();
    gender = npc->get().gender();
  } else if (auto player = asRef<Player>(entity)) {
    species = player->get().species();
    gender = player->get().gender();
  }
  m_parameters[paramName] = QuestParam{QuestEntity{entity.uniqueId(), species, gender}, name, portrait, {}};
  return {};
}
std::expected<void, StarException> Quest::addReward(ItemDescriptor const& reward) {
  if (reward.name() == "money") { m_money += reward.count(); return {}; }
  auto database = m_root.itemDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  auto item = (*database)->item(reward);
  if (!item) return std::unexpected(std::move(item.error()));
  m_rewards.push_back(*item);
  m_rewardsRevision = nextQuestPresentationRevision();
  return {};
}

String const& Quest::defaultCustomIndicator() const {
  return m_player->get().questManager()->isCurrent(questId()) ? m_trackedIndicator : m_untrackedIndicator;
}

std::expected<std::shared_ptr<Quest>, StarException> createPreviewQuest(
    Root& root, String const& templateId, String const& position, String const& questGiverSpecies, Player& player) {
  auto templates = root.questTemplateDatabase();
  if (!templates) return std::unexpected(std::move(templates.error()));
  auto config = (*templates)->questTemplate(templateId);
  if (!config) return std::unexpected(std::move(config.error()));
  if (!*config) return std::shared_ptr<Quest>();
  auto drawables = player.portrait(PortraitMode::Full);
  if (!drawables) return std::unexpected(std::move(drawables.error()));
  auto portrait = jsonFromList(*drawables, [](Drawable const& drawable) { return drawable.toJson(); });
  if (!portrait) return std::unexpected(std::move(portrait.error()));
  JsonObject paramJson = (*config)->parameterExamples;
  for (auto&& [name, parameter] : paramJson) {
    auto typed = parameter.set("type", (*config)->parameterTypes[name]);
    if (!typed) return std::unexpected(std::move(typed.error()));
    auto pictured = typed->set("portrait", *portrait);
    if (!pictured) return std::unexpected(std::move(pictured.error()));
    parameter = std::move(*pictured);
  }
  auto parameters = questParamsFromJson(paramJson);
  if (!parameters) return std::unexpected(std::move(parameters.error()));
  QuestDescriptor descriptor{"preview", templateId, std::move(*parameters), root.random().randu64()};
  auto next = position.equalsIgnoreCase("next");
  if (!next) return std::unexpected(std::move(next.error()));
  bool isLast = false, isFirst = false;
  if (!*next) {
    auto last = position.equalsIgnoreCase("last");
    if (!last) return std::unexpected(std::move(last.error()));
    isLast = *last;
    if (!isLast) {
      auto first = position.equalsIgnoreCase("first");
      if (!first) return std::unexpected(std::move(first.error()));
      isFirst = *first;
    }
  }
  List<QuestDescriptor> quests;
  if (*next || isLast || isFirst) quests = {descriptor, descriptor, descriptor};
  else quests = {descriptor};
  QuestArcDescriptor arc{std::move(quests), {}};
  auto quest = Quest::create(root, arc, *next ? 1 : isLast ? 2 : 0, player);
  if (!quest) return std::unexpected(std::move(quest.error()));
  (*quest)->setParameter("questGiver", QuestParam{QuestEntity{{}, questGiverSpecies, {}}, {"Quest Giver"}, *portrait, {}});
  return *quest;
}

}
