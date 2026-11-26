module;
import star.format;

import star.vector;
import star.game_types;
import star.world_geometry;
import std;
import star.list;
import star.map;
import star.data_stream;
import star.wiring;

namespace Star {

class WireEntity;
class WorldStorage;

// Propogates WireEntity signals, and keeps networks of WireEntities alive
// together.
class WireProcessor : public WireCoordinator {
public:
  WireProcessor(std::shared_ptr<WorldStorage> worldStorage);

  [[nodiscard]] std::expected<void, StarException> process();

  [[nodiscard]] std::expected<bool, StarException> read_input_connection(WireConnection const& connection) override;

private:
  struct WireEntityState {
    std::reference_wrapper<WireEntity> wireEntity;
    List<bool> outputStates;
    bool networkLoaded;
  };

  // Add the given WireEntity to the working entities set, populating inbound /
  // outbound nodes and states.
  [[nodiscard]] std::expected<void, StarException> populateWorking(WireEntity& wireEntity);
  // Scans a wire network, starting at an entity at the given position, while
  // also loading any unloaded entries in the network and marking each entry as
  // now having been 'networkLoaded'.
  [[nodiscard]] std::expected<void, StarException> loadNetwork(Vec2I tilePosition);

  std::shared_ptr<WorldStorage> m_worldStorage;
  StableHashMap<Vec2I, WireEntityState> m_workingWireEntities;
};

}

export module star.wire_processor;

export namespace Star {
using ::Star::WireProcessor;
using ::Star::WireEntity;
using ::Star::WorldStorage;
using ::Star::WireDirection;
using ::Star::other_wire_direction;
using ::Star::WireNode;
using ::Star::WireConnection;
using ::Star::WireCoordinator;
using ::Star::WireConnector;
}
