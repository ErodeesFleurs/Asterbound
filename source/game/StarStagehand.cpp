import star.json;
import star.json_extra;
import star.id_map;
import star.casting;
import star.vector;
import star.data_stream;
import star.game_types;
import star.poly;
import star.strong_typedef;
import star.bi_map;
import star.lua;
import star.ordered_map;
import star.rect;
import star.thread;
import star.io_device;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.format;
import star.string;
import star.version;
import star.data_stream_extra;

import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.lua_components;
import star.scripted_entity;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.stagehand;
import star.lua_game_converters;

import star.behavior_lua_bindings;
import star.entity_lua_bindings;
import star.config_lua_bindings;

import star.behavior_state;

import star.data_stream_devices;

import star.net_compatibility;
import star.byte_array;
namespace Star {

std::expected<std::shared_ptr<Stagehand>, StarException> Stagehand::create(Root& rootContext, Json const& config) {
  struct MakeShared final : Stagehand {
    explicit MakeShared(Root& root) : Entity(root), Stagehand(root) {}
  };
  auto stagehand = std::make_shared<MakeShared>(rootContext);
  if (auto result = stagehand->m_netGroup.addNetElement(stagehand->m_xPosition); !result) return std::unexpected(std::move(result.error()));
  if (auto result = stagehand->m_netGroup.addNetElement(stagehand->m_yPosition); !result) return std::unexpected(std::move(result.error()));
  if (auto result = stagehand->m_netGroup.addNetElement(stagehand->m_uniqueIdNetState); !result) return std::unexpected(std::move(result.error()));
  auto uniqueId = config.optString("uniqueId");
  if (!uniqueId)
    return std::unexpected(std::move(uniqueId.error()));
  stagehand->setUniqueId(std::move(*uniqueId));
  if (auto result = stagehand->readConfig(config); !result)
    return std::unexpected(std::move(result.error()));
  return stagehand;
}

std::expected<std::shared_ptr<Stagehand>, StarException> Stagehand::create(Root& rootContext, ByteArray const& netStore, NetCompatibilityRules) {
  auto config = DataStreamBuffer::deserialize<Json>(netStore);
  if (!config)
    return std::unexpected(std::move(config.error()));
  struct MakeShared final : Stagehand {
    explicit MakeShared(Root& root) : Entity(root), Stagehand(root) {}
  };
  auto stagehand = std::make_shared<MakeShared>(rootContext);
  if (auto result = stagehand->m_netGroup.addNetElement(stagehand->m_xPosition); !result) return std::unexpected(std::move(result.error()));
  if (auto result = stagehand->m_netGroup.addNetElement(stagehand->m_yPosition); !result) return std::unexpected(std::move(result.error()));
  if (auto result = stagehand->m_netGroup.addNetElement(stagehand->m_uniqueIdNetState); !result) return std::unexpected(std::move(result.error()));
  if (auto result = stagehand->readConfig(std::move(*config)); !result)
    return std::unexpected(std::move(result.error()));
  return stagehand;
}

std::expected<Json, StarException> Stagehand::diskStore() const {
  auto saveData = m_config.setAll({
      {"position", jsonFromVec2F({m_xPosition.get(), m_yPosition.get()})},
      {"uniqueId", jsonFromOptional(uniqueId())}
    });
  if (!saveData)
    return std::unexpected(std::move(saveData.error()));
  if (!m_scripted)
    return saveData;
  auto storage = m_scriptComponent.getScriptStorage();
  if (!storage)
    return std::unexpected(std::move(storage.error()));
  return saveData->set("scriptStorage", std::move(*storage));
}

std::expected<ByteArray, StarException> Stagehand::netStore(NetCompatibilityRules) {
  return DataStreamBuffer::serialize(m_config);
}

std::expected<void, StarException> Stagehand::init(World& world, EntityId entityId, EntityMode mode) {
  if (auto result = Entity::init(world, entityId, mode); !result)
    return result;
  auto initialize = [&self = *this, &world]() -> std::expected<void, StarException> {
    if (self.isMaster() && self.m_scripted) {
      auto stagehandCallbacks = self.makeStagehandCallbacks();
      if (!stagehandCallbacks)
        return std::unexpected(std::move(stagehandCallbacks.error()));
      if (auto result = self.m_scriptComponent.addCallbacks("stagehand", std::move(*stagehandCallbacks)); !result)
        return result;
      auto configCallbacks = LuaBindings::makeConfigCallbacks([&self](String const& name, Json const& def) {
          return self.m_config.query(name, def);
        });
      if (!configCallbacks)
        return std::unexpected(std::move(configCallbacks.error()));
      if (auto result = self.m_scriptComponent.addCallbacks("config", std::move(*configCallbacks)); !result)
        return result;
      auto entityCallbacks = LuaBindings::makeEntityCallbacks(self);
      if (!entityCallbacks)
        return std::unexpected(std::move(entityCallbacks.error()));
      if (auto result = self.m_scriptComponent.addCallbacks("entity", std::move(*entityCallbacks)); !result)
        return result;
      auto behaviorCallbacks = LuaBindings::makeBehaviorCallbacks(self.root(), self.m_behaviors);
      if (!behaviorCallbacks)
        return std::unexpected(std::move(behaviorCallbacks.error()));
      if (auto result = self.m_scriptComponent.addCallbacks("behavior", std::move(*behaviorCallbacks)); !result)
        return result;
      return self.m_scriptComponent.init(world);
    }
    return {};
  }();
  if (!initialize) {
    auto cleanup = uninit();
    if (!cleanup)
      return std::unexpected(initialize.error().withContext(std::string("Stagehand initialization rollback also failed: ") + cleanup.error().what()));
  }
  return initialize;
}

std::expected<void, StarException> Stagehand::uninit() {
  std::optional<StarException> error;
  auto remember = [&](auto result) {
    if (!result && !error)
      error = std::move(result.error());
  };
  remember(Entity::uninit());
  if (m_scripted) {
    remember(m_scriptComponent.uninit());
    remember(m_scriptComponent.removeCallbacks("stagehand"));
    remember(m_scriptComponent.removeCallbacks("config"));
    remember(m_scriptComponent.removeCallbacks("entity"));
  }
  if (error)
    return std::unexpected(std::move(*error));
  return {};
}

EntityType Stagehand::entityType() const {
  return EntityType::Stagehand;
}

void Stagehand::setPosition(Vec2F const& position) {
  m_xPosition.set(position[0]);
  m_yPosition.set(position[1]);
}

Vec2F Stagehand::position() const {
  return {m_xPosition.get(), m_yPosition.get()};
}

RectF Stagehand::metaBoundBox() const {
  return m_boundBox;
}

std::expected<std::pair<ByteArray, std::uint64_t>, StarException> Stagehand::writeNetState(std::uint64_t fromVersion, NetCompatibilityRules rules) {
  return m_netGroup.writeNetState(fromVersion, rules);
}

std::expected<void, StarException> Stagehand::readNetState(ByteArray data, float interpolationTime, NetCompatibilityRules rules) {
  return m_netGroup.readNetState(data, interpolationTime, rules);
}

std::expected<String, StarException> Stagehand::name() const {
  return typeName();
}

std::expected<void, StarException> Stagehand::update(float dt, std::uint64_t) {
  if (!inWorld())
    return {};

  if (isMaster() && m_scripted) {
    auto result = m_scriptComponent.update(m_scriptComponent.updateDt(dt));
    if (!result)
      return std::unexpected(std::move(result.error()));
  }

  auto currentWorld = world();
  if (!currentWorld)
    return std::unexpected(std::move(currentWorld.error()));
  if (currentWorld->get().isClient()) {
    auto boundBox = metaBoundBox().translated(position());
    this->root().spatialLogger().logPoly("world", PolyF(boundBox), { 0, 255, 255, 255 });
    this->root().spatialLogger().logLine("world", boundBox.min(), boundBox.max(), {0, 255, 255, 255});
    this->root().spatialLogger().logLine("world", { boundBox.xMin(), boundBox.yMax() }, { boundBox.xMax(), boundBox.yMin() }, {0, 255, 255, 255});
  }
  return {};
}

std::expected<bool, StarException> Stagehand::shouldDestroy() const {
  return m_dead;
}

std::expected<std::optional<LuaValue>, StarException> Stagehand::callScript(String const& func, LuaVariadic<LuaValue> const& args) {
  return m_scriptComponent.invoke(func, args);
}

std::expected<std::optional<LuaValue>, StarException> Stagehand::evalScript(String const& code) {
  return m_scriptComponent.eval(code);
}

std::expected<Json, StarException> Stagehand::configValue(String const& name, Json const& def) const {
  return m_config.query(name, def);
}

Stagehand::Stagehand(Root& rootContext) : Entity(rootContext), m_scriptComponent(rootContext) {
  setPersistent(true);


  m_netGroup.setNeedsLoadCallback([&self = *this](bool) -> std::expected<void, StarException> {
      self.setUniqueId(self.m_uniqueIdNetState.get());
      return {};
    });

  m_netGroup.setNeedsStoreCallback([&self = *this]() -> std::expected<void, StarException> {
      self.m_uniqueIdNetState.set(self.uniqueId());
      return {};
    });
}

std::expected<void, StarException> Stagehand::readConfig(Json config) {
  m_config = std::move(config);
  auto scripted = m_config.contains("scripts");
  if (!scripted)
    return std::unexpected(std::move(scripted.error()));
  m_scripted = *scripted;
  auto clientModeName = m_config.getString("clientEntityMode", "ClientSlaveOnly");
  if (!clientModeName)
    return std::unexpected(std::move(clientModeName.error()));
  if (!ClientEntityModeNames)
    return std::unexpected(ClientEntityModeNames.error());
  auto clientMode = ClientEntityModeNames->get_left(*clientModeName);
  if (!clientMode)
    return std::unexpected(std::move(clientMode.error()));
  m_clientEntityMode = clientMode->get();
  auto position = m_config.opt("position");
  if (!position)
    return std::unexpected(std::move(position.error()));
  if (*position) {
    auto pos = jsonToVec2F(**position);
    if (!pos)
      return std::unexpected(std::move(pos.error()));
    m_xPosition.set((*pos)[0]);
    m_yPosition.set((*pos)[1]);
  }
  auto areaJson = m_config.opt("broadcastArea");
  if (!areaJson)
    return std::unexpected(std::move(areaJson.error()));
  auto broadcastArea = jsonToOptional<RectF>(areaJson->value_or(Json()), jsonToRectF);
  if (!broadcastArea)
    return std::unexpected(std::move(broadcastArea.error()));
  if (*broadcastArea && ((**broadcastArea).size()[0] < 0.0 || (**broadcastArea).size()[1] < 0.0))
    broadcastArea->reset();
  m_boundBox = broadcastArea->value_or(RectF(-5.0, -5.0, 5.0, 5.0));
  if (m_scripted) {
    auto scriptsJson = m_config.getArray("scripts", JsonArray());
    if (!scriptsJson)
      return std::unexpected(std::move(scriptsJson.error()));
    auto scripts = jsonToStringList(*scriptsJson);
    if (!scripts)
      return std::unexpected(std::move(scripts.error()));
    if (auto result = m_scriptComponent.setScripts(std::move(*scripts)); !result)
      return result;
    auto delta = m_config.getInt("scriptDelta", 5);
    if (!delta)
      return std::unexpected(std::move(delta.error()));
    m_scriptComponent.setUpdateDelta(*delta);
    auto storage = m_config.opt("scriptStorage");
    if (!storage)
      return std::unexpected(std::move(storage.error()));
    if (*storage) {
      auto object = (**storage).toObject();
      if (!object)
        return std::unexpected(std::move(object.error()));
      if (auto result = m_scriptComponent.setScriptStorage(std::move(*object)); !result)
        return result;
    }
  }
  auto keepAlive = m_config.getBool("keepAlive", false);
  if (!keepAlive)
    return std::unexpected(std::move(keepAlive.error()));
  setKeepAlive(*keepAlive);
  return {};
}

std::expected<LuaCallbacks, StarException> Stagehand::makeStagehandCallbacks() {
  LuaCallbacks callbacks;
  if (auto result = callbacks.registerCallback("id", [&self = *this]() { return self.entityId(); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("position", [&self = *this]() { return self.position(); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("setPosition", [&self = *this](Vec2F const& position) { self.setPosition(position); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("die", [&self = *this]() { self.m_dead = true; }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("typeName", [&self = *this]() { return self.typeName(); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("setUniqueId", [&self = *this](std::optional<String> const& uniqueId) { self.setUniqueId(uniqueId); }); !result)
    return std::unexpected(std::move(result.error()));
  return callbacks;
}

ClientEntityMode Stagehand::clientEntityMode() const {
  return m_clientEntityMode;
}

std::expected<String, StarException> Stagehand::typeName() const {
  return m_config.getString("type");
}

std::expected<std::optional<ChainableJsonMessageResponse>, StarException> Stagehand::receiveMessage(ConnectionId sendingConnection, String const& message, JsonArray const& args) {
  auto currentWorld = world();
  if (!currentWorld)
    return std::unexpected(std::move(currentWorld.error()));
  return m_scriptComponent.handleMessage(message, sendingConnection == currentWorld->get().connection(), args);
}

}
