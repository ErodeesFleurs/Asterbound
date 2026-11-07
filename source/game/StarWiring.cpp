import star.format;
import std;
import star.hash;
import star.game_types;
import star.world_geometry;
import star.data_stream;
import star.wiring;
import star.data_stream_extra;

namespace Star {

WireDirection otherWireDirection(WireDirection direction) {
  return direction == WireDirection::Input ? WireDirection::Output : WireDirection::Input;
}

bool WireConnection::operator==(WireConnection const& wireConnection) const {
  return std::tie(entityLocation, nodeIndex) == std::tie(wireConnection.entityLocation, wireConnection.nodeIndex);
}

std::size_t hash<WireConnection>::operator()(WireConnection const& wireConnection) const {
  return hash_of(wireConnection.entityLocation, wireConnection.nodeIndex);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, WireConnection& wireConnection) {
  if (auto r = ds.viread(wireConnection.entityLocation[0]); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.viread(wireConnection.entityLocation[1]); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.vuread(wireConnection.nodeIndex); !r) return std::unexpected(std::move(r.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, WireConnection const& wireConnection) {
  if (auto r = ds.viwrite(wireConnection.entityLocation[0]); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.viwrite(wireConnection.entityLocation[1]); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.vuwrite(wireConnection.nodeIndex); !r) return std::unexpected(std::move(r.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, WireNode& wireNode) {
  if (auto r = ds.read(wireNode.direction); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.vuread(wireNode.nodeIndex); !r) return std::unexpected(std::move(r.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, WireNode const& wireNode) {
  if (auto r = ds.write(wireNode.direction); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.vuwrite(wireNode.nodeIndex); !r) return std::unexpected(std::move(r.error()));
  return std::ref(ds);
}

}
