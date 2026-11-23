module;
import star.format;
import star.bi_map;
import star.vector;
import star.math_common;
import std;

namespace Star {

enum class Direction : std::uint8_t {
  Left,
  Right
};
inline constexpr auto direction_names = sorted_enum_names(std::to_array<std::pair<Direction, std::string_view>>({
  {Direction::Left, "left"},
  {Direction::Right, "right"},
}));

inline Direction operator-(Direction dir) {
  if (dir == Direction::Left)
    return Direction::Right;
  return Direction::Left;
}

inline int numerical_direction(std::optional<Direction> direction) {
  if (!direction)
    return 0;
  else
    return *direction == Direction::Left ? -1 : 1;
}

template <typename T>
concept direction_scalar = (std::integral<T> || std::floating_point<T>) && !std::same_as<T, bool>;

template <direction_scalar NumType>
inline std::optional<Direction> direction_of(NumType const& n) {
  if (n == 0)
    return {};
  else
    return n < 0 ? Direction::Left : Direction::Right;
}

enum class Gender : std::uint8_t {
  Male,
  Female
};
inline constexpr auto gender_names = sorted_enum_names(std::to_array<std::pair<Gender, std::string_view>>({
  {Gender::Male, "male"},
  {Gender::Female, "female"},
}));

enum class FireMode : std::uint8_t {
  None,
  Primary,
  Alt
};
inline constexpr auto fire_mode_names = sorted_enum_names(std::to_array<std::pair<FireMode, std::string_view>>({
  {FireMode::None, "none"},
  {FireMode::Primary, "primary"},
  {FireMode::Alt, "alt"},
}));

enum class ToolHand : std::uint8_t {
  Primary,
  Alt
};
inline constexpr auto tool_hand_names = sorted_enum_names(std::to_array<std::pair<ToolHand, std::string_view>>({
  {ToolHand::Primary, "primary"},
  {ToolHand::Alt, "alt"},
}));

enum class TileLayer : std::uint8_t {
  Foreground,
  Background
};
inline constexpr auto tile_layer_names = sorted_enum_names(std::to_array<std::pair<TileLayer, std::string_view>>({
  {TileLayer::Foreground, "foreground"},
  {TileLayer::Background, "background"},
}));

enum class MoveControlType : std::uint8_t {
  Left,
  Right,
  Down,
  Up,
  Jump
};
inline constexpr auto move_control_type_names =
  sorted_enum_names(std::to_array<std::pair<MoveControlType, std::string_view>>({
    {MoveControlType::Left, "left"},
    {MoveControlType::Right, "right"},
    {MoveControlType::Down, "down"},
    {MoveControlType::Up, "up"},
    {MoveControlType::Jump, "jump"},
  }));

enum class PortraitMode {
  Head,
  Bust,
  Full,
  FullNeutral,
  FullNude,
  FullNeutralNude
};
inline constexpr auto portrait_mode_names = sorted_enum_names(std::to_array<std::pair<PortraitMode, std::string_view>>({
  {PortraitMode::Head, "head"},
  {PortraitMode::Bust, "bust"},
  {PortraitMode::Full, "full"},
  {PortraitMode::FullNeutral, "fullneutral"},
  {PortraitMode::FullNude, "fullnude"},
  {PortraitMode::FullNeutralNude, "fullneutralnude"},
}));

enum class Rarity {
  Common,
  Uncommon,
  Rare,
  Legendary,
  Essential
};
inline constexpr auto rarity_names = sorted_enum_names(std::to_array<std::pair<Rarity, std::string_view>>({
  {Rarity::Common, "common"},
  {Rarity::Uncommon, "uncommon"},
  {Rarity::Rare, "rare"},
  {Rarity::Legendary, "legendary"},
  {Rarity::Essential, "essential"},
}));

// Transformation from tile space to pixel space.  Number of pixels in 1.0
// distance (one tile).
inline constexpr unsigned tile_pixels = 8;

struct GameTiming {
  float time_scale = 1.0f;
  float time_step = 1.0f / 60.0f;
  float server_time_step = 1.0f / 60.0f;
};
inline constexpr float system_world_time_step = 1.0f / 20.0f;

inline constexpr std::size_t world_sector_size = 32;

using EntityId = std::int32_t;
inline constexpr EntityId null_entity_id = 0;
inline constexpr EntityId min_server_entity_id = 1;
inline constexpr EntityId max_server_entity_id = std::numeric_limits<EntityId>::max();

// Whether this entity is controlled by it's world, or synced from a different
// world.  Does not necessarily correspond to client / server world (player is
// master on client).
enum class EntityMode {
  Master,
  Slave
};

using ConnectionId = std::uint16_t;
inline constexpr ConnectionId server_connection_id = 0;
// Minimum and maximum valid client ids
inline constexpr ConnectionId min_client_connection_id = 1;
inline constexpr ConnectionId max_client_connection_id = 16383;
inline constexpr ConnectionId min_client_sub_world_connection_id = max_client_connection_id + 1;
inline constexpr ConnectionId sub_world_connection_offset = min_client_sub_world_connection_id - min_client_connection_id;
inline constexpr ConnectionId max_client_sub_world_connection_id = max_client_connection_id + sub_world_connection_offset;

[[nodiscard]] constexpr ConnectionId main_to_sub_world_connection_id(ConnectionId id) {
  return id + sub_world_connection_offset;
}
[[nodiscard]] constexpr ConnectionId sub_world_to_main_connection_id(ConnectionId id) {
  return id - sub_world_connection_offset;
}

// for client sub-world threads. indexes by *world* rather than by thread.
// this should allow the client to possibly split the subworlds to multiple threads in the future.
using ClientSubWorldId = std::uint16_t;
inline constexpr ClientSubWorldId main_client_world_id = 0;
inline constexpr ClientSubWorldId min_client_sub_world_id = 1;
inline constexpr ClientSubWorldId max_client_sub_world_id = std::numeric_limits<ClientSubWorldId>::max();

template <typename T>
concept tile_coordinate = requires(T const& tile) { Vec2F(tile.floor()); };

template <tile_coordinate Vec2T>
inline Vec2F center_of_tile(Vec2T const& tile) {
  return Vec2F(tile.floor()) + Vec2F::filled(0.5);
}

using DungeonId = std::uint16_t;

inline constexpr DungeonId no_dungeon_id = 65535;
inline constexpr DungeonId spawn_dungeon_id = 65534;
inline constexpr DungeonId biome_micro_dungeon_id = 65533;
// meta dungeon signalling player built structures
inline constexpr DungeonId construction_dungeon_id = 65532;
// indicates a block that has been destroyed
inline constexpr DungeonId destroyed_block_dungeon_id = 65531;

// dungeonId for zero-g areas with and without tile protection
inline constexpr DungeonId zero_g_dungeon_id = 65525;
inline constexpr DungeonId protected_zero_g_dungeon_id = 65524;

// The first dungeon id that is reserved for special hard-coded dungeon values.
inline constexpr DungeonId first_meta_dungeon_id = 65520;

inline bool is_real_dungeon(DungeonId dungeon) {
  return dungeon < first_meta_dungeon_id;
}

inline constexpr EntityId entity_ids_per_connection = 65536;

// Inclusive bounds. Clients occupy consecutive negative blocks; lower IDs are unassigned.
[[nodiscard]] std::expected<std::pair<EntityId, EntityId>, StarException> connection_entity_space(ConnectionId connection_id);

[[nodiscard]] std::expected<bool, StarException> entity_id_in_space(EntityId entity_id, ConnectionId connection_id);

[[nodiscard]] constexpr std::optional<ConnectionId> connection_for_entity(EntityId entity_id) {
  if (entity_id > null_entity_id)
    return server_connection_id;
  if (entity_id == null_entity_id || entity_id < -entity_ids_per_connection * max_client_sub_world_connection_id)
    return {};
  return static_cast<ConnectionId>(1 + (-(entity_id + 1)) / entity_ids_per_connection);
}

// Returns an angle in the range [-pi / 2, pi / 2], and the horizontal
// hemisphere of the angle.  The angle is specified as positive being upward
// rotation and negative being downward rotation, unless cc_rotation is true, in
// which case the angle is always positive == counter-clocwise.
std::pair<float, Direction> get_angle_side(float angle, bool cc_rotation = false);

enum class TileDamageResult {
  None = 0,
  Protected = 1,
  Normal = 2,
};

}


export module star.game_types;

export namespace Star {
  using ::Star::Direction;
  using ::Star::direction_names;
  using ::Star::operator-;
  using ::Star::numerical_direction;
  using ::Star::direction_scalar;
  using ::Star::direction_of;
  using ::Star::Gender;
  using ::Star::gender_names;
  using ::Star::FireMode;
  using ::Star::fire_mode_names;
  using ::Star::ToolHand;
  using ::Star::tool_hand_names;
  using ::Star::TileLayer;
  using ::Star::tile_layer_names;
  using ::Star::MoveControlType;
  using ::Star::move_control_type_names;
  using ::Star::PortraitMode;
  using ::Star::portrait_mode_names;
  using ::Star::Rarity;
  using ::Star::rarity_names;
  using ::Star::tile_pixels;
  using ::Star::GameTiming;
  using ::Star::system_world_time_step;
  using ::Star::world_sector_size;
  using ::Star::EntityId;
  using ::Star::null_entity_id;
  using ::Star::min_server_entity_id;
  using ::Star::max_server_entity_id;
  using ::Star::EntityMode;
  using ::Star::ConnectionId;
  using ::Star::server_connection_id;
  using ::Star::min_client_connection_id;
  using ::Star::max_client_connection_id;
  using ::Star::min_client_sub_world_connection_id;
  using ::Star::max_client_sub_world_connection_id;
  using ::Star::main_to_sub_world_connection_id;
  using ::Star::entity_ids_per_connection;
  using ::Star::sub_world_to_main_connection_id;
  using ::Star::ClientSubWorldId;
  using ::Star::main_client_world_id;
  using ::Star::min_client_sub_world_id;
  using ::Star::max_client_sub_world_id;
  using ::Star::tile_coordinate;
  using ::Star::center_of_tile;
  using ::Star::DungeonId;
  using ::Star::no_dungeon_id;
  using ::Star::spawn_dungeon_id;
  using ::Star::biome_micro_dungeon_id;
  using ::Star::construction_dungeon_id;
  using ::Star::destroyed_block_dungeon_id;
  using ::Star::zero_g_dungeon_id;
  using ::Star::protected_zero_g_dungeon_id;
  using ::Star::first_meta_dungeon_id;
  using ::Star::is_real_dungeon;
  using ::Star::connection_entity_space;
  using ::Star::entity_id_in_space;
  using ::Star::connection_for_entity;
  using ::Star::get_angle_side;
  using ::Star::TileDamageResult;
}
