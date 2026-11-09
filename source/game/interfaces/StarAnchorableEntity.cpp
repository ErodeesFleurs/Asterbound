import std;
import star.format;
import star.json;
import star.id_map;
import star.casting;
import star.data_stream;
import star.game_types;
import star.strong_typedef;
import star.bi_map;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.anchorable_entity;

namespace Star {

bool EntityAnchorState::operator==(EntityAnchorState const& eas) const {
  return std::tie(entityId, positionIndex) == std::tie(eas.entityId, eas.positionIndex);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, EntityAnchorState& anchorState) {
  EntityAnchorState decoded;
  auto entityId = ds.read(decoded.entityId);
  if (!entityId)
    return std::unexpected(std::move(entityId.error()));
  auto position = ds.read_vlq_s(decoded.positionIndex);
  if (!position)
    return std::unexpected(std::move(position.error()));
  anchorState = decoded;
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, EntityAnchorState const& anchorState) {
  auto entityId = ds.write(anchorState.entityId);
  if (!entityId)
    return std::unexpected(std::move(entityId.error()));
  auto position = ds.write_vlq_s(anchorState.positionIndex);
  if (!position)
    return std::unexpected(std::move(position.error()));
  return std::ref(ds);
}

}
