module;
import star.format;
import std;
import star.hash;

import star.vector;
import star.game_types;
import star.world_geometry;
import star.data_stream;

namespace Star {

enum class WireDirection {
  Input,
  Output
};

WireDirection other_wire_direction(WireDirection direction);

// Identifier for a specific WireNode in a WireEntity, node indexes for input
// and output nodes are separate.
struct WireNode {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, WireNode& value);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, WireNode const& value);
  WireDirection direction;
  std::size_t node_index;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, WireNode& wire_node);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, WireNode const& wire_node);

// Connection from a given WireNode to another WireNode, the direction must be
// implied based on the context.
struct WireConnection {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, WireConnection& value);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, WireConnection const& value);
  Vec2I entity_location;
  std::size_t node_index;

  bool operator==(WireConnection const& wire_connection) const;
};

template <>
struct ValueHash<WireConnection> {
  std::size_t operator()(WireConnection const& wire_connection) const;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, WireConnection& wire_connection);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, WireConnection const& wire_connection);

class WireCoordinator {
public:
  virtual ~WireCoordinator() = default;

  [[nodiscard]] virtual std::expected<bool, StarException> read_input_connection(WireConnection const& connection) = 0;
};

class WireConnector {
public:
  enum SwingResult {
    Connect,
    Mismatch,
    Protected,
    Nothing
  };

  virtual ~WireConnector() = default;

  [[nodiscard]] virtual std::expected<SwingResult, StarException> swing(WorldGeometry const& geometry, Vec2F position, FireMode mode) = 0;
  virtual bool connecting() = 0;
};

}

export module star.wiring;

export namespace Star {
  template <>
  struct ValueHash<WireConnection>;
}


export namespace Star {
  using ::Star::WireCoordinator;
  using ::Star::WireConnector;
  using ::Star::WireDirection;
  using ::Star::other_wire_direction;
  using ::Star::WireNode;
  using ::Star::WireConnection;
  using ::Star::ValueHash;
}
