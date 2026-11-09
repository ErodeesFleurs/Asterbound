import star.format;
import star.json;
import star.rect;
import star.vector;
import star.game_types;
import star.data_stream_extra;
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
import std;
import star.version;

import star.asset_source;
import star.assets;

import star.configuration;
import star.root;

import star.world_client_state;

import star.net_compatibility;
import star.byte_array;
namespace Star {

std::expected<std::shared_ptr<WorldClientState>, StarException> WorldClientState::create(Root& root) {
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto clientConfig = (*assets)->json("/client.config");
  if (!clientConfig)
    return std::unexpected(std::move(clientConfig.error()));
  auto windowBorder = clientConfig->getInt("windowMonitoringBorder");
  if (!windowBorder)
    return std::unexpected(std::move(windowBorder.error()));
  auto presenceBorder = clientConfig->getInt("presenceEntityMonitoringBorder");
  if (!presenceBorder)
    return std::unexpected(std::move(presenceBorder.error()));
  auto state = std::shared_ptr<WorldClientState>(new WorldClientState());
  state->m_windowMonitoringBorder = *windowBorder;
  state->m_presenceEntityMonitoringBorder = *presenceBorder;
  auto registered = state->m_netGroup.addNetElement(state->m_windowXMin);
  if (!registered)
    return std::unexpected(std::move(registered.error()));
  registered = state->m_netGroup.addNetElement(state->m_windowYMin);
  if (!registered)
    return std::unexpected(std::move(registered.error()));
  registered = state->m_netGroup.addNetElement(state->m_windowWidth);
  if (!registered)
    return std::unexpected(std::move(registered.error()));
  registered = state->m_netGroup.addNetElement(state->m_windowHeight);
  if (!registered)
    return std::unexpected(std::move(registered.error()));
  registered = state->m_netGroup.addNetElement(state->m_playerId);
  if (!registered)
    return std::unexpected(std::move(registered.error()));
  registered = state->m_netGroup.addNetElement(state->m_clientPresenceEntities);
  if (!registered)
    return std::unexpected(std::move(registered.error()));
  return state;
}

WorldClientState::WorldClientState() {
  m_playerId.set(NullEntityId);

  m_netVersion = 0;
}

RectI WorldClientState::window() const {
  return RectI::withSize(Vec2I(m_windowXMin.get(), m_windowYMin.get()), Vec2I(m_windowWidth.get(), m_windowHeight.get()));
}

void WorldClientState::setWindow(RectI const& window) {
  m_windowXMin.set(window.xMin());
  m_windowYMin.set(window.yMin());
  m_windowWidth.set(window.width());
  m_windowHeight.set(window.height());
}

Vec2F WorldClientState::windowCenter() const {
  return RectF(window()).center();
}

EntityId WorldClientState::playerId() const {
  return m_playerId.get();
}

void WorldClientState::setPlayer(EntityId playerId) {
  return m_playerId.set(playerId);
}

List<EntityId> const& WorldClientState::clientPresenceEntities() const {
  return m_clientPresenceEntities.get();
}

void WorldClientState::setClientPresenceEntities(List<EntityId> entities) {
  m_clientPresenceEntities.set(std::move(entities));
}

List<RectI> WorldClientState::monitoringRegions(std::function<std::optional<RectI>(EntityId)> entityBounds) const {
  List<RectI> regions;
  
  auto windowRegion = window().padded(m_windowMonitoringBorder);

  if (window() != RectI())
    regions.push_back(windowRegion);

  if (auto playerBounds = entityBounds(m_playerId.get())) {
    // add an extra region the size of the window centered on the player's position to prevent nearby sectors
    // being unloaded due to camera panning or centering on other entities
    regions.push_back(RectI::withCenter(playerBounds->center(), windowRegion.size()));
  }

  for (auto entityId : m_clientPresenceEntities.get()) {
    if (auto bounds = entityBounds(entityId))
      regions.push_back(bounds->padded(m_presenceEntityMonitoringBorder));
  }

  return regions;
}

std::expected<ByteArray, StarException> WorldClientState::writeDelta() {
  auto state = m_netGroup.writeNetState(m_netVersion, m_netCompatibilityRules);
  if (!state)
    return std::unexpected(std::move(state.error()));
  m_netVersion = state->second;
  return std::move(state->first);
}

std::expected<void, StarException> WorldClientState::readDelta(ByteArray delta) {
  return m_netGroup.readNetState(std::move(delta), 0.0f, m_netCompatibilityRules);
}

void WorldClientState::setNetCompatibilityRules(NetCompatibilityRules netCompatibilityRules) {
  m_netCompatibilityRules = netCompatibilityRules;
}

NetCompatibilityRules WorldClientState::netCompatibilityRules() const {
  return m_netCompatibilityRules;
}

void WorldClientState::reset() {
  m_netVersion = 0;
}

}
