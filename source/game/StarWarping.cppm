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

template <typename BaseType> struct CelestialWorldIdWrapper;
template <typename BaseType> struct ClientShipWorldIdWrapper;
template <typename BaseType> struct CustomWorldIdWrapper;
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
struct hash<InstanceWorldId> {
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
struct hash<ClientCustomWorldId> {
  std::size_t operator()(ClientCustomWorldId const& id) const;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, ClientCustomWorldId& clientWorldId);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, ClientCustomWorldId const& clientWorldId);

  template <typename BaseType>
  struct CelestialWorldIdWrapper : BaseType {
    friend void write_text(TextBuffer& output, CelestialWorldIdWrapper<CelestialCoordinate> const& worldId);
    using BaseType::BaseType;

    CelestialWorldIdWrapper() : BaseType() {}

    CelestialWorldIdWrapper(CelestialWorldIdWrapper const& nt) : BaseType(nt) {}

    CelestialWorldIdWrapper(CelestialWorldIdWrapper&& nt) : BaseType(std::move(nt)) {}

    explicit CelestialWorldIdWrapper(BaseType const& bt) : BaseType(bt) {}

    explicit CelestialWorldIdWrapper(BaseType&& bt) : BaseType(std::move(bt)) {}

    CelestialWorldIdWrapper& operator=(CelestialWorldIdWrapper const& rhs) {
      BaseType::operator=(rhs);
      return *this;
    }

    CelestialWorldIdWrapper& operator=(CelestialWorldIdWrapper&& rhs) {
      BaseType::operator=(std::move(rhs));
      return *this;
    }

    template <class Arg>
    CelestialWorldIdWrapper& operator=(Arg&& other) {
      static_assert(std::is_base_of_v<BaseType, std::decay_t<Arg>> == false
                      || std::is_same_v<CelestialWorldIdWrapper, std::decay_t<Arg>>,
                    ""
                    "CelestialWorldId"
                    " can not implicitly be assigned from "
                    "CelestialCoordinate"
                    "-derived classes or strong "
                    "CelestialCoordinate"
                    " typedefs");

      BaseType::operator=(std::forward<Arg>(other));
      return *this;
    }
  };
  using CelestialWorldId = CelestialWorldIdWrapper<CelestialCoordinate>;

  template <typename BaseType>
  struct ClientShipWorldIdWrapper : BaseType {
    friend void write_text(TextBuffer& output, ClientShipWorldIdWrapper<Uuid> const& worldId);
    using BaseType::BaseType;


    ClientShipWorldIdWrapper(ClientShipWorldIdWrapper const& nt) : BaseType(nt) {}

    ClientShipWorldIdWrapper(ClientShipWorldIdWrapper&& nt) : BaseType(std::move(nt)) {}

    explicit ClientShipWorldIdWrapper(BaseType const& bt) : BaseType(bt) {}

    explicit ClientShipWorldIdWrapper(BaseType&& bt) : BaseType(std::move(bt)) {}

    ClientShipWorldIdWrapper& operator=(ClientShipWorldIdWrapper const& rhs) {
      BaseType::operator=(rhs);
      return *this;
    }

    ClientShipWorldIdWrapper& operator=(ClientShipWorldIdWrapper&& rhs) {
      BaseType::operator=(std::move(rhs));
      return *this;
    }

    template <class Arg>
    ClientShipWorldIdWrapper& operator=(Arg&& other) {
      static_assert(std::is_base_of_v<BaseType, std::decay_t<Arg>> == false
                      || std::is_same_v<ClientShipWorldIdWrapper, std::decay_t<Arg>>,
                    ""
                    "ClientShipWorldId"
                    " can not implicitly be assigned from "
                    "Uuid"
                    "-derived classes or strong "
                    "Uuid"
                    " typedefs");

      BaseType::operator=(std::forward<Arg>(other));
      return *this;
    }
  };
  using ClientShipWorldId = ClientShipWorldIdWrapper<Uuid>;

  template <typename BaseType>
  struct CustomWorldIdWrapper : BaseType {
    friend void write_text(TextBuffer& output, CustomWorldIdWrapper<String> const& worldId);
    using BaseType::BaseType;

    CustomWorldIdWrapper() : BaseType() {}

    CustomWorldIdWrapper(CustomWorldIdWrapper const& nt) : BaseType(nt) {}

    CustomWorldIdWrapper(CustomWorldIdWrapper&& nt) : BaseType(std::move(nt)) {}

    explicit CustomWorldIdWrapper(BaseType const& bt) : BaseType(bt) {}

    explicit CustomWorldIdWrapper(BaseType&& bt) : BaseType(std::move(bt)) {}

    CustomWorldIdWrapper& operator=(CustomWorldIdWrapper const& rhs) {
      BaseType::operator=(rhs);
      return *this;
    }

    CustomWorldIdWrapper& operator=(CustomWorldIdWrapper&& rhs) {
      BaseType::operator=(std::move(rhs));
      return *this;
    }

    template <class Arg>
    CustomWorldIdWrapper& operator=(Arg&& other) {
      static_assert(std::is_base_of_v<BaseType, std::decay_t<Arg>> == false
                      || std::is_same_v<CustomWorldIdWrapper, std::decay_t<Arg>>,
                    ""
                    "CustomWorldId"
                    " can not implicitly be assigned from "
                    "String"
                    "-derived classes or strong "
                    "String"
                    " typedefs");

      BaseType::operator=(std::forward<Arg>(other));
      return *this;
    }
  };
  using CustomWorldId = CustomWorldIdWrapper<String>;
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

  template <typename BaseType> struct SpawnTargetUniqueEntityWrapper : BaseType {
    using BaseType::BaseType;

    SpawnTargetUniqueEntityWrapper() : BaseType() {}

    SpawnTargetUniqueEntityWrapper(SpawnTargetUniqueEntityWrapper const& nt) : BaseType(nt) {}

    SpawnTargetUniqueEntityWrapper(SpawnTargetUniqueEntityWrapper&& nt) : BaseType(std::move(nt)) {}

    explicit SpawnTargetUniqueEntityWrapper(BaseType const& bt) : BaseType(bt) {}

    explicit SpawnTargetUniqueEntityWrapper(BaseType&& bt) : BaseType(std::move(bt)) {}

    SpawnTargetUniqueEntityWrapper& operator=(SpawnTargetUniqueEntityWrapper const& rhs) {
      BaseType::operator=(rhs);
      return *this;
    }

    SpawnTargetUniqueEntityWrapper& operator=(SpawnTargetUniqueEntityWrapper&& rhs) {
      BaseType::operator=(std::move(rhs));
      return *this;
    }

    template <class Arg> SpawnTargetUniqueEntityWrapper& operator=(Arg&& other) {
      static_assert(
        std::is_base_of_v<BaseType, std::decay_t<Arg>> == false
          || std::is_same_v<SpawnTargetUniqueEntityWrapper, std::decay_t<Arg>>,
        "SpawnTargetUniqueEntity can not implicitly be assigned from String-derived classes or strong String typedefs");

      BaseType::operator=(std::forward<Arg>(other));
      return *this;
    }
  };
  using SpawnTargetUniqueEntity = SpawnTargetUniqueEntityWrapper<String>;
  template <typename BaseType> struct SpawnTargetPositionWrapper : BaseType {
    using BaseType::BaseType;

    SpawnTargetPositionWrapper() : BaseType() {}

    SpawnTargetPositionWrapper(SpawnTargetPositionWrapper const& nt) : BaseType(nt) {}

    SpawnTargetPositionWrapper(SpawnTargetPositionWrapper&& nt) : BaseType(std::move(nt)) {}

    explicit SpawnTargetPositionWrapper(BaseType const& bt) : BaseType(bt) {}

    explicit SpawnTargetPositionWrapper(BaseType&& bt) : BaseType(std::move(bt)) {}

    SpawnTargetPositionWrapper& operator=(SpawnTargetPositionWrapper const& rhs) {
      BaseType::operator=(rhs);
      return *this;
    }

    SpawnTargetPositionWrapper& operator=(SpawnTargetPositionWrapper&& rhs) {
      BaseType::operator=(std::move(rhs));
      return *this;
    }

    template <class Arg> SpawnTargetPositionWrapper& operator=(Arg&& other) {
      static_assert(
        std::is_base_of_v<BaseType, std::decay_t<Arg>> == false
          || std::is_same_v<SpawnTargetPositionWrapper, std::decay_t<Arg>>,
        "SpawnTargetPosition can not implicitly be assigned from Vec2F-derived classes or strong Vec2F typedefs");

      BaseType::operator=(std::forward<Arg>(other));
      return *this;
    }
  };
  using SpawnTargetPosition = SpawnTargetPositionWrapper<Vec2F>;
  using SpawnTargetX = StrongTypedefBuiltin<float, struct SpawnTargetXTag>;
  using SpawnTarget = std::variant<std::monostate, SpawnTargetUniqueEntity, SpawnTargetPosition, SpawnTargetX>;

  Json spawnTargetToJson(SpawnTarget spawnTarget);
  [[nodiscard]] std::expected<SpawnTarget, StarException> spawnTargetFromJson(Json v);

  String print_spawn_target(SpawnTarget spawnTarget);

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

template <typename BaseType>
struct WarpToPlayerWrapper : BaseType {
  using BaseType::BaseType;


  WarpToPlayerWrapper(WarpToPlayerWrapper const& nt) : BaseType(nt) {}

  WarpToPlayerWrapper(WarpToPlayerWrapper&& nt) : BaseType(std::move(nt)) {}

  explicit WarpToPlayerWrapper(BaseType const& bt) : BaseType(bt) {}

  explicit WarpToPlayerWrapper(BaseType&& bt) : BaseType(std::move(bt)) {}

  WarpToPlayerWrapper& operator=(WarpToPlayerWrapper const& rhs) {
    BaseType::operator=(rhs);
    return *this;
  }

  WarpToPlayerWrapper& operator=(WarpToPlayerWrapper&& rhs) {
    BaseType::operator=(std::move(rhs));
    return *this;
  }

  template <class Arg>
  WarpToPlayerWrapper& operator=(Arg&& other) {
    static_assert(std::is_base_of_v<BaseType, std::decay_t<Arg>> == false
                    || std::is_same_v<WarpToPlayerWrapper, std::decay_t<Arg>>,
                  "WarpToPlayer can not implicitly be assigned from Uuid-derived classes or strong Uuid typedefs");

    BaseType::operator=(std::forward<Arg>(other));
    return *this;
  }
};
using WarpToPlayer = WarpToPlayerWrapper<Uuid>;

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
[[nodiscard]] std::expected<ClientShipWorldId, StarException> DataStream::read<ClientShipWorldId>();
template <>
[[nodiscard]] std::expected<ClientCustomWorldId, StarException> DataStream::read<ClientCustomWorldId>();
template <>
[[nodiscard]] std::expected<WarpToPlayer, StarException> DataStream::read<WarpToPlayer>();

}

export module star.warping;

export namespace Star {
  template <>
  struct hash<InstanceWorldId>;
  template <>
  struct hash<ClientCustomWorldId>;
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
  using ::Star::CelestialWorldIdWrapper;
  using ::Star::CelestialWorldId;
  using ::Star::ClientShipWorldIdWrapper;
  using ::Star::ClientShipWorldId;
  using ::Star::CustomWorldIdWrapper;
  using ::Star::CustomWorldId;
  using ::Star::WorldId;
  using ::Star::print_world_id;
  using ::Star::write_text;
  using ::Star::parseWorldId;
  using ::Star::SpawnTargetUniqueEntityWrapper;
  using ::Star::SpawnTargetUniqueEntity;
  using ::Star::SpawnTargetPositionWrapper;
  using ::Star::SpawnTargetPosition;
  using ::Star::SpawnTargetXTag;
  using ::Star::SpawnTargetX;
  using ::Star::SpawnTarget;
  using ::Star::spawnTargetToJson;
  using ::Star::spawnTargetFromJson;
  using ::Star::print_spawn_target;
  using ::Star::WarpToWorld;
  using ::Star::WarpToPlayerWrapper;
  using ::Star::WarpToPlayer;
  using ::Star::WarpAlias;
  using ::Star::WarpAction;
  using ::Star::parseWarpAction;
  using ::Star::print_warp_action;
  using ::Star::warpActionToJson;
}
