module;
import std;
import star.format;
import star.string;
import star.text_formatter;
import star.hash;
import star.json;

import star.strong_typedef;
import star.data_stream;
import star.uuid;
import star.vector;
import star.celestial_coordinate;
import star.bi_map;
import star.game_types;

namespace Star {

struct ClientCustomWorldId;

enum class WarpMode : std::uint8_t {
  None,
  BeamOnly,
  DeployOnly,
  BeamOrDeploy
};
extern std::expected<EnumMap<WarpMode>, StarException> const WarpModeNames;

struct InstanceWorldId {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, InstanceWorldId& instanceWorldId);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, InstanceWorldId const& instanceWorldId);

  String instance;
  std::optional<Uuid> uuid;
  std::optional<float> level;

  InstanceWorldId();
  InstanceWorldId(String instance, std::optional<Uuid> uuid = {}, std::optional<float> level = {});

  bool operator==(InstanceWorldId const& rhs) const;
  bool operator<(InstanceWorldId const& rhs) const;
};

template <>
struct ValueHash<InstanceWorldId> {
  std::size_t operator()(InstanceWorldId const& id) const;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, InstanceWorldId& missionWorldId);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, InstanceWorldId const& missionWorldId);

struct ClientCustomWorldId {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, ClientCustomWorldId& clientWorldId);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, ClientCustomWorldId const& clientWorldId);
  friend void write_text(TextBuffer& output, ClientCustomWorldId const& worldId);

  Uuid uuid;
  String name;

  ClientCustomWorldId(Uuid uuid, String name);

  bool operator==(ClientCustomWorldId const& rhs) const;
  bool operator<(ClientCustomWorldId const& rhs) const;
};

template <>
struct ValueHash<ClientCustomWorldId> {
  std::size_t operator()(ClientCustomWorldId const& id) const;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, ClientCustomWorldId& clientWorldId);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, ClientCustomWorldId const& clientWorldId);

  struct CelestialWorldId : CelestialCoordinate {
    using CelestialCoordinate::CelestialCoordinate;
    CelestialWorldId() = default;
    explicit CelestialWorldId(CelestialCoordinate const& value) : CelestialCoordinate(value) {}
    explicit CelestialWorldId(CelestialCoordinate&& value) : CelestialCoordinate(std::move(value)) {}
    friend void write_text(TextBuffer& output, CelestialWorldId const& value);
  };

  struct ClientShipWorldId : Uuid {
    using Uuid::Uuid;
    explicit ClientShipWorldId(Uuid const& value) : Uuid(value) {}
    explicit ClientShipWorldId(Uuid&& value) : Uuid(std::move(value)) {}
    friend void write_text(TextBuffer& output, ClientShipWorldId const& value);
  };

  struct CustomWorldId : String {
    using String::String;
    CustomWorldId() = default;
    explicit CustomWorldId(String const& value) : String(value) {}
    explicit CustomWorldId(String&& value) : String(std::move(value)) {}
    friend void write_text(TextBuffer& output, CustomWorldId const& value);
  };
  using WorldId = std::variant<std::monostate, CelestialWorldId, ClientShipWorldId, InstanceWorldId, CustomWorldId,
                               ClientCustomWorldId>;

  // A negative instance level is reported rather than formatted as a valid world ID.
  [[nodiscard]] std::expected<String, StarException> print_world_id(WorldId const& worldId);
  [[nodiscard]] std::expected<void, StarException> write_text(TextBuffer& output, WorldId const& worldId);
  [[nodiscard]] std::expected<void, StarException> write_text(TextBuffer& output, InstanceWorldId const& worldId);
  [[nodiscard]] std::expected<WorldId, StarException> parseWorldId(String const& printedId);

  void write_text(TextBuffer& output, CelestialWorldId const& worldId);
  void write_text(TextBuffer& output, ClientShipWorldId const& worldId);
  void write_text(TextBuffer& output, CustomWorldId const& worldId);
  void write_text(TextBuffer& output, ClientCustomWorldId const& worldId);

  struct SpawnTargetUniqueEntity : String {
    using String::String;
    SpawnTargetUniqueEntity() = default;
    explicit SpawnTargetUniqueEntity(String const& value) : String(value) {}
    explicit SpawnTargetUniqueEntity(String&& value) : String(std::move(value)) {}
  };
  struct SpawnTargetPosition : Vec2F {
    using Vec2F::Vec2F;
    SpawnTargetPosition() = default;
    explicit SpawnTargetPosition(Vec2F const& value) : Vec2F(value) {}
    explicit SpawnTargetPosition(Vec2F&& value) : Vec2F(std::move(value)) {}
  };
  struct SpawnTargetXTag {
    friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, StrongTypedef<float, SpawnTargetXTag> const& x) { return ds << x.value; }
    friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, StrongTypedef<float, SpawnTargetXTag>& x) { return ds >> x.value; }
  };
  using SpawnTargetX = StrongTypedef<float, SpawnTargetXTag>;
  using SpawnTarget = std::variant<std::monostate, SpawnTargetUniqueEntity, SpawnTargetPosition, SpawnTargetX>;

  Json spawn_target_to_json(SpawnTarget const& spawn_target);
  [[nodiscard]] std::expected<SpawnTarget, StarException> spawn_target_from_json(Json const& v);

  String print_spawn_target(SpawnTarget const& spawn_target);

  struct WarpToWorld {
    friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds,
                                                                                       WarpToWorld& warpToWorld);
    friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds,
                                                                                       WarpToWorld const& warpToWorld);

    WarpToWorld();
    explicit WarpToWorld(WorldId world, SpawnTarget spawn = {});
    [[nodiscard]] static std::expected<WarpToWorld, StarException> create(Json v);

    WorldId world;
    SpawnTarget target;

    bool operator==(WarpToWorld const& rhs) const;
    explicit operator bool() const;

    [[nodiscard]] std::expected<Json, StarException> toJson() const;
  };

struct WarpToPlayer : Uuid {
  using Uuid::Uuid;
  explicit WarpToPlayer(Uuid const& value) : Uuid(value) {}
  explicit WarpToPlayer(Uuid&& value) : Uuid(std::move(value)) {}
};

enum class WarpAlias {
  Return,
  OrbitedWorld,
  OwnShip
};

using WarpAction = std::variant<std::monostate, WarpToWorld, WarpToPlayer, WarpAlias>;

[[nodiscard]] std::expected<WarpAction, StarException> parseWarpAction(String const& warpString);
[[nodiscard]] std::expected<String, StarException> print_warp_action(WarpAction const& warpAction);
[[nodiscard]] std::expected<JsonObject, StarException> warpActionToJson(WarpAction const& warpAction);

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, WarpToWorld& warpToWorld);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, WarpToWorld const& warpToWorld);

// Construct identity-bearing values from their wire fields, never a generated ID.
template <>
struct HasStreamReadFactory<ClientShipWorldId> : std::true_type {};
template <>
struct HasStreamReadFactory<ClientCustomWorldId> : std::true_type {};
template <>
struct HasStreamReadFactory<WarpToPlayer> : std::true_type {};
template <>
[[nodiscard]] std::expected<ClientShipWorldId, StarException> DataStream::read<ClientShipWorldId>();
template <>
[[nodiscard]] std::expected<ClientCustomWorldId, StarException> DataStream::read<ClientCustomWorldId>();
template <>
[[nodiscard]] std::expected<WarpToPlayer, StarException> DataStream::read<WarpToPlayer>();

}

export module star.warping;

export namespace Star {
  template <>
  struct HasStreamReadFactory<ClientShipWorldId>;
  template <>
  struct HasStreamReadFactory<ClientCustomWorldId>;
  template <>
  struct HasStreamReadFactory<WarpToPlayer>;
  template <>
  struct ValueHash<InstanceWorldId>;
  template <>
  struct ValueHash<ClientCustomWorldId>;
}

export extern "C++" {
namespace Star {
  template <>
  [[nodiscard]] std::expected<ClientShipWorldId, StarException> DataStream::read<ClientShipWorldId>();
  template <>
  [[nodiscard]] std::expected<ClientCustomWorldId, StarException> DataStream::read<ClientCustomWorldId>();
  template <>
  [[nodiscard]] std::expected<WarpToPlayer, StarException> DataStream::read<WarpToPlayer>();
}
}


export template <> struct std::formatter<Star::CelestialWorldId> : Star::TextFormatter {};
export template <> struct std::formatter<Star::ClientShipWorldId> : Star::TextFormatter {};
export template <> struct std::formatter<Star::CustomWorldId> : Star::TextFormatter {};
export template <> struct std::formatter<Star::ClientCustomWorldId> : Star::TextFormatter {};

export namespace Star {
  using ::Star::WarpMode;
  using ::Star::WarpModeNames;
  using ::Star::InstanceWorldId;
  using ::Star::ClientCustomWorldId;
  using ::Star::CelestialWorldId;
  using ::Star::ClientShipWorldId;
  using ::Star::CustomWorldId;
  using ::Star::WorldId;
  using ::Star::print_world_id;
  using ::Star::write_text;
  using ::Star::parseWorldId;
  using ::Star::SpawnTargetUniqueEntity;
  using ::Star::SpawnTargetPosition;
  using ::Star::SpawnTargetXTag;
  using ::Star::SpawnTargetX;
  using ::Star::SpawnTarget;
  using ::Star::spawn_target_to_json;
  using ::Star::spawn_target_from_json;
  using ::Star::print_spawn_target;
  using ::Star::WarpToWorld;
  using ::Star::WarpToPlayer;
  using ::Star::WarpAlias;
  using ::Star::WarpAction;
  using ::Star::parseWarpAction;
  using ::Star::print_warp_action;
  using ::Star::warpActionToJson;
}
