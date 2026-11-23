import std;
import star.math_common;
import star.format;
import star.game_types;

namespace Star {

std::expected<std::pair<EntityId, EntityId>, StarException> connection_entity_space(ConnectionId connection_id) {
  if (connection_id == server_connection_id) {
    return std::pair{min_server_entity_id, max_server_entity_id};
  }
  if (connection_id > max_client_sub_world_connection_id)
    return std::unexpected(StarException::format("Invalid connection id in connection_entity_space({})", connection_id));
  EntityId first = -entity_ids_per_connection * connection_id;
  return std::pair{first, first + (entity_ids_per_connection - 1)};
}

std::expected<bool, StarException> entity_id_in_space(EntityId entity_id, ConnectionId connection_id) {
  if (connection_id > max_client_sub_world_connection_id)
    return std::unexpected(StarException::format("Invalid connection id in entity_id_in_space({})", connection_id));
  return connection_for_entity(entity_id) == connection_id;
}

std::pair<float, Direction> get_angle_side(float angle, bool cc_rotation) {
  angle = normalize_angle(angle);
  Direction direction = Direction::Right;
  if (angle > std::numbers::pi / 2) {
    direction = Direction::Left;
    angle = std::numbers::pi - angle;
  } else if (angle < -std::numbers::pi / 2) {
    direction = Direction::Left;
    angle = -std::numbers::pi - angle;
  }

  if (direction == Direction::Left && cc_rotation)
    angle *= -1;

  return std::make_pair(angle, direction);
}

}
