import std;
import star.format;
import star.net_compatibility;
import star.net_element;
import star.net_element_basic_fields;
import star.data_stream;
import star.data_stream_devices;
import star.context_net_state;

namespace Star {

std::expected<void, StarException> ContextNetState::initialize() {
  // Wire slots 1..6, including the legacy full-state order.
  for (auto element : std::initializer_list<std::reference_wrapper<NetElement>>{
      orbit_warp_action, player_world_id, is_admin, team, ship_upgrades, ship_coordinate}) {
    if (auto added = group.add_net_element(element); !added)
      return std::unexpected(std::move(added.error()));
  }
  return {};
}

std::expected<void, StarException> ContextNetState::stage_update(std::span<char const> bytes, NetCompatibilityRules rules) {
  if (stage != Stage::Clean)
    consume_fields(*this, false);
  stage = Stage::Clean;
  if (bytes.empty())
    return {};

  DataStreamReader stream(bytes);
  stream.set_stream_compatibility_version(rules);
  auto full = stream.read<bool>();
  if (!full)
    return std::unexpected(std::move(full.error()));
  stage = Stage::Reading;
  NetElement& fields = group;
  auto decoded = *full ? fields.net_load(stream, rules) : fields.read_net_delta(stream, 0.0f, rules);
  if (!decoded)
    return std::unexpected(std::move(decoded.error()));
  auto end = stream.at_end();
  if (!end)
    return std::unexpected(std::move(end.error()));
  if (!*end)
    return std::unexpected(DataStreamException("Trailing bytes in context net state"));
  stage = Stage::Ready;
  return {};
}

void ContextNetState::publish_to(ContextNetState& target) {
  if (stage == Stage::Ready) {
    consume_fields(target, true);
    stage = Stage::Clean;
  }
}

void ContextNetState::consume_fields(ContextNetState& target, bool publish) {
  auto consume = [publish]<std::movable T>(NetElementBasicField<T>& field, NetElementBasicField<T>& destination) {
    if (field.pull_updated() && publish) {
      field.update([&destination](T& value) {
        destination.push(std::move(value));
        return false; // Taking the scratch payload is not a new network update.
      });
    }
  };
  consume(orbit_warp_action, target.orbit_warp_action);
  consume(player_world_id, target.player_world_id);
  consume(is_admin, target.is_admin);
  consume(team, target.team);
  consume(ship_upgrades, target.ship_upgrades);
  consume(ship_coordinate, target.ship_coordinate);
}

}
