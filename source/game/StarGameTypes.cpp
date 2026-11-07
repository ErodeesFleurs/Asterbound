import std;
import star.math_common;
import star.format;
import star.game_types;

namespace Star {

std::expected<std::pair<EntityId, EntityId>, StarException> connectionEntitySpace(ConnectionId connectionId) {
  if (connectionId == ServerConnectionId) {
    return std::pair{MinServerEntityId, MaxServerEntityId};
  } else if (connectionId >= MinClientConnectionId && connectionId <= MaxClientSubWorldConnectionId) {
    EntityId beginIdSpace = static_cast<EntityId>(connectionId) * -65536;
    EntityId endIdSpace = beginIdSpace + 65535;
    return std::pair{beginIdSpace, endIdSpace};
  } else {
    return std::unexpected(StarException::format("Invalid connection id in clientEntitySpace({})", connectionId));
  }
}

std::expected<bool, StarException> entityIdInSpace(EntityId entityId, ConnectionId connectionId) {
  auto pair = connectionEntitySpace(connectionId);
  if (!pair) return std::unexpected(std::move(pair.error()));
  return entityId >= pair->first && entityId <= pair->second;
}

ConnectionId connectionForEntity(EntityId entityId) {
  if (entityId > 0)
    return ServerConnectionId;
  else
    return (-entityId - 1) / 65536 + 1;
}

std::pair<float, Direction> getAngleSide(float angle, bool ccRotation) {
  angle = constrainAngle(angle);
  Direction direction = Direction::Right;
  if (angle > std::numbers::pi / 2) {
    direction = Direction::Left;
    angle = std::numbers::pi - angle;
  } else if (angle < -std::numbers::pi / 2) {
    direction = Direction::Left;
    angle = -std::numbers::pi - angle;
  }

  if (direction == Direction::Left && ccRotation)
    angle *= -1;

  return std::make_pair(angle, direction);
}

}
