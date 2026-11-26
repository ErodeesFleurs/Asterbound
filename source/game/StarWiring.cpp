import star.format;
import std;
import star.hash;
import star.game_types;
import star.world_geometry;
import star.data_stream;
import star.wiring;
import star.data_stream_extra;

namespace Star {

WireDirection other_wire_direction(WireDirection direction) {
  return direction == WireDirection::Input ? WireDirection::Output : WireDirection::Input;
}

bool WireConnection::operator==(WireConnection const& wire_connection) const {
  return std::tie(entity_location, node_index) == std::tie(wire_connection.entity_location, wire_connection.node_index);
}

std::size_t ValueHash<WireConnection>::operator()(WireConnection const& wire_connection) const {
  return hash_of(wire_connection.entity_location, wire_connection.node_index);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, WireConnection& wire_connection) {
  if (auto r = ds.viread(wire_connection.entity_location[0]); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.viread(wire_connection.entity_location[1]); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.vuread(wire_connection.node_index); !r) return std::unexpected(std::move(r.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, WireConnection const& wire_connection) {
  if (auto r = ds.viwrite(wire_connection.entity_location[0]); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.viwrite(wire_connection.entity_location[1]); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.vuwrite(wire_connection.node_index); !r) return std::unexpected(std::move(r.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, WireNode& wire_node) {
  if (auto r = ds.read(wire_node.direction); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.vuread(wire_node.node_index); !r) return std::unexpected(std::move(r.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, WireNode const& wire_node) {
  if (auto r = ds.write(wire_node.direction); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.vuwrite(wire_node.node_index); !r) return std::unexpected(std::move(r.error()));
  return std::ref(ds);
}

}
