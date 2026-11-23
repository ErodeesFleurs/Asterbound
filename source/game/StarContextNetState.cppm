module;
import std;
import star.format;
import star.data_stream;
import star.data_stream_extra;
import star.uuid;
import star.celestial_coordinate;
import star.damage_types;
import star.warping;
import star.player_types;
import star.net_compatibility;
import star.net_element_basic_fields;
import star.net_element_system;

namespace Star {

// Both endpoints use the same ordered group. Registered fields borrow its clock,
// and the group borrows the fields, so neither may move after initialization.
struct ContextNetState {
  ContextNetState() = default;
  ContextNetState(ContextNetState const&) = delete;
  ContextNetState& operator=(ContextNetState const&) = delete;

  [[nodiscard]] std::expected<void, StarException> initialize();

  // Decode on a dedicated scratch instance. A rejected or abandoned record
  // never contributes fields to the next publication, including sparse deltas.
  [[nodiscard]] std::expected<void, StarException> stage_update(std::span<char const> bytes, NetCompatibilityRules rules);
  void publish_to(ContextNetState& target);

  NetElementData<std::optional<std::pair<WarpAction, WarpMode>>> orbit_warp_action;
  NetElementData<WorldId> player_world_id;
  NetElementBool is_admin;
  NetElementData<EntityDamageTeam> team;
  NetElementData<ShipUpgrades> ship_upgrades;
  NetElementData<CelestialCoordinate> ship_coordinate;
  NetElementTopGroup group;

private:
  enum class Stage { Clean, Reading, Ready };
  Stage stage = Stage::Clean;
  void consume_fields(ContextNetState& target, bool publish);
};

}

export module star.context_net_state;

export namespace Star {
  using ::Star::ContextNetState;
}
