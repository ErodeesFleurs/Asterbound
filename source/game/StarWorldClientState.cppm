module;
import std;
import star.list;

import star.rect;
import star.vector;
import star.net_compatibility;
import star.game_types;
import star.format;

import star.byte_array;
import star.net_element_basic_fields;
import star.net_element_system;

namespace Star {
class Root;

// Class to aid in network syncronization of client state such as viewing area
// and player entity id.
class WorldClientState {
public:
  [[nodiscard]]
  static std::expected<std::shared_ptr<WorldClientState>, StarException> create(Root& root);
  WorldClientState(WorldClientState const&) = delete;
  WorldClientState& operator=(WorldClientState const&) = delete;
  WorldClientState(WorldClientState&&) = delete;
  WorldClientState& operator=(WorldClientState&&) = delete;

  // Actual area of the client visible screen (rounded to nearest block)
  RectI window() const;
  void setWindow(RectI const& window);

  // Shortcut to find the window center of the client.
  Vec2F windowCenter() const;

  // Entity of the unique main Player for this client
  EntityId playerId() const;
  void setPlayer(EntityId playerId);

  // Entities that should contribute to the monitoring regions of the client.
  List<EntityId> const& clientPresenceEntities() const;
  void setClientPresenceEntities(List<EntityId> entities);

  // All areas of the server monitored by the client, takes a function to
  // resolve an entity id to its bound box.
  List<RectI> monitoringRegions(std::function<std::optional<RectI>(EntityId)> entityBounds) const;

  [[nodiscard]]
  std::expected<ByteArray, StarException> writeDelta();
  [[nodiscard]]
  std::expected<void, StarException> readDelta(ByteArray const& delta);

  void setNetCompatibilityRules(NetCompatibilityRules netCompatibilityRules);
  NetCompatibilityRules netCompatibilityRules() const;

  void reset();

private:
  WorldClientState();

  int m_windowMonitoringBorder;
  int m_presenceEntityMonitoringBorder;

  NetElementTopGroup m_netGroup;
  std::uint64_t m_netVersion;

  NetElementInt m_windowXMin;
  NetElementInt m_windowYMin;
  NetElementInt m_windowWidth;
  NetElementInt m_windowHeight;

  NetElementInt m_playerId;
  NetElementData<List<EntityId>> m_clientPresenceEntities;

  NetCompatibilityRules m_netCompatibilityRules;
};

}

export module star.world_client_state;

export namespace Star {
using ::Star::WorldClientState;
}
