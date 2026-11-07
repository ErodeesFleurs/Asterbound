import std;
import star.string;
import star.format;
import star.hash;
import star.json;
import star.json_extra;
import star.strong_typedef;
import star.data_stream;
import star.uuid;
import star.vector;
import star.celestial_coordinate;
import star.bi_map;
import star.game_types;
import star.warping;
import star.data_stream_extra;
import star.lexical_cast;

namespace Star {

extern std::expected<EnumMap<WarpMode>, StarException> const WarpModeNames = EnumMap<WarpMode>::create({
  {WarpMode::None, "None"},
  {WarpMode::BeamOnly, "BeamOnly"},
  {WarpMode::DeployOnly, "DeployOnly"},
  {WarpMode::BeamOrDeploy, "BeamOrDeploy"}
});

template <>
[[nodiscard]] std::expected<ClientShipWorldId, StarException> DataStream::read<ClientShipWorldId>() {
  auto uuid = read<Uuid>();
  if (!uuid)
    return std::unexpected(std::move(uuid.error()));
  return ClientShipWorldId(std::move(*uuid));
}

template <>
[[nodiscard]] std::expected<ClientCustomWorldId, StarException> DataStream::read<ClientCustomWorldId>() {
  auto uuid = read<Uuid>();
  if (!uuid)
    return std::unexpected(std::move(uuid.error()));
  auto name = read<String>();
  if (!name)
    return std::unexpected(std::move(name.error()));
  return ClientCustomWorldId(std::move(*uuid), std::move(*name));
}

template <>
[[nodiscard]] std::expected<WarpToPlayer, StarException> DataStream::read<WarpToPlayer>() {
  auto uuid = read<Uuid>();
  if (!uuid)
    return std::unexpected(std::move(uuid.error()));
  return WarpToPlayer(std::move(*uuid));
}

InstanceWorldId::InstanceWorldId() = default;

InstanceWorldId::InstanceWorldId(String instance, std::optional<Uuid> uuid, std::optional<float> level)
  : instance(std::move(instance)), uuid(std::move(uuid)), level(std::move(level)) {}

bool InstanceWorldId::operator==(InstanceWorldId const& rhs) const {
  return std::tie(instance, uuid, level) == std::tie(rhs.instance, rhs.uuid, rhs.level);
}

bool InstanceWorldId::operator<(InstanceWorldId const& rhs) const {
  return std::tie(instance, uuid, rhs.level) < std::tie(rhs.instance, rhs.uuid, rhs.level);
}

std::size_t hash<InstanceWorldId>::operator()(InstanceWorldId const& id) const {
  return hashOf(id.instance, id.uuid, id.level);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, InstanceWorldId& instanceWorldId) {
  InstanceWorldId decoded;
  auto result = ds >> decoded.instance >> decoded.uuid >> decoded.level;
  if (!result)
    return std::unexpected(std::move(result.error()));
  instanceWorldId = std::move(decoded);
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, InstanceWorldId const& instanceWorldId) {
  return ds << instanceWorldId.instance << instanceWorldId.uuid << instanceWorldId.level;
}


ClientCustomWorldId::ClientCustomWorldId(Uuid uuid, String name)
  : uuid(std::move(uuid)), name(std::move(name)) {}

bool ClientCustomWorldId::operator==(ClientCustomWorldId const& rhs) const {
  return std::tie(uuid, name) == std::tie(rhs.uuid, rhs.name);
}

bool ClientCustomWorldId::operator<(ClientCustomWorldId const& rhs) const {
  return std::tie(uuid, name) < std::tie(rhs.uuid, rhs.name);
}

std::size_t hash<ClientCustomWorldId>::operator()(ClientCustomWorldId const& id) const {
  return hashOf(id.uuid, id.name);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, ClientCustomWorldId& clientWorldId) {
  auto uuid = ds.read<Uuid>();
  if (!uuid)
    return std::unexpected(std::move(uuid.error()));
  auto name = ds.read<String>();
  if (!name)
    return std::unexpected(std::move(name.error()));
  clientWorldId = ClientCustomWorldId(std::move(*uuid), std::move(*name));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, ClientCustomWorldId const& clientWorldId) {
  return ds << clientWorldId.uuid << clientWorldId.name;
}

std::expected<String, StarException> printWorldId(WorldId const& worldId) {
  if (std::holds_alternative<InstanceWorldId>(worldId)) {
    auto const& instanceWorldId = std::get<InstanceWorldId>(worldId);
    if (instanceWorldId.level && *instanceWorldId.level < 0.0f)
      return std::unexpected(StarException::format("InstanceWorldId level component cannot be negative"));

    String uuidPart = instanceWorldId.uuid ? instanceWorldId.uuid->hex() : "-";
    String levelPart = instanceWorldId.level ? toString(*instanceWorldId.level) : "-";
    return strf("InstanceWorld:{}:{}:{}", instanceWorldId.instance, uuidPart, levelPart);
  } else if (std::holds_alternative<CelestialWorldId>(worldId)) {
    return strf("CelestialWorld:{}", std::get<CelestialWorldId>(worldId));
  } else if (std::holds_alternative<ClientShipWorldId>(worldId)) {
    return strf("ClientShipWorld:{}", std::get<ClientShipWorldId>(worldId).hex());
  } else if (std::holds_alternative<CustomWorldId>(worldId)) {
    return strf("CustomWorld:{}", std::get<CustomWorldId>(worldId));
  } else if (std::holds_alternative<ClientCustomWorldId>(worldId)) {
    auto const& clientCustomWorldId = std::get<ClientCustomWorldId>(worldId);
    return strf("ClientCustomWorld:{}:{}", clientCustomWorldId.uuid.hex(), clientCustomWorldId.name);
  } else {
    return "Nowhere";
  }
}

std::expected<WorldId, StarException> parseWorldId(String const& printedId) {
  if (printedId.empty())
    return WorldId();
  auto parts = printedId.split(':', 1);
  if (!parts)
    return std::unexpected(std::move(parts.error()));
  auto const& type = (*parts)[0];
  enum class Kind { Instance, Celestial, Ship, Custom, ClientCustom, Nowhere };
  auto kind = [&]() -> std::expected<Kind, StarException> {
    constexpr std::pair<std::string_view, Kind> names[] = {
        {"InstanceWorld", Kind::Instance}, {"CelestialWorld", Kind::Celestial},
        {"ClientShipWorld", Kind::Ship}, {"CustomWorld", Kind::Custom},
        {"ClientCustomWorld", Kind::ClientCustom}, {"Nowhere", Kind::Nowhere}};
    for (auto const& [name, value] : names) {
      auto matches = type.equalsIgnoreCase(name);
      if (!matches)
        return std::unexpected(std::move(matches.error()));
      if (*matches)
        return value;
    }
    return std::unexpected(StarException::format("Improper WorldId type '{}'", type));
  }();
  if (!kind)
    return std::unexpected(std::move(kind.error()));
  if (*kind == Kind::Nowhere)
    return WorldId();
  if (parts->size() < 2)
    return std::unexpected(StarException::format("Missing WorldId body for '{}'", type));
  auto const& body = (*parts)[1];
  if (*kind == Kind::Instance) {
    auto rest = body.split(":", 2);
    if (!rest)
      return std::unexpected(std::move(rest.error()));
    if (rest->empty() || rest->size() > 3)
      return std::unexpected(StarException::format("Wrong number of parts in InstanceWorldId"));
    InstanceWorldId result{(*rest)[0], {}, {}};
    if (rest->size() > 1 && !(*rest)[1].empty() && (*rest)[1] != "-") {
      auto uuid = Uuid::create((*rest)[1]);
      if (!uuid)
        return std::unexpected(std::move(uuid.error()));
      result.uuid = std::move(*uuid);
    }
    if (rest->size() > 2 && !(*rest)[2].empty() && (*rest)[2] != "-") {
      auto level = lexicalCast<float>((*rest)[2]);
      if (!level)
        return std::unexpected(std::move(level.error()));
      if (*level < 0)
        return std::unexpected(StarException::format("InstanceWorldId level component cannot be negative"));
      result.level = *level;
    }
    return result;
  } else if (*kind == Kind::Celestial) {
    auto coordinate = CelestialCoordinate::create(body);
    if (!coordinate)
      return std::unexpected(std::move(coordinate.error()));
    return CelestialWorldId(std::move(*coordinate));
  } else if (*kind == Kind::Ship) {
    auto uuid = Uuid::create(body);
    if (!uuid)
      return std::unexpected(std::move(uuid.error()));
    return ClientShipWorldId(std::move(*uuid));
  } else if (*kind == Kind::Custom) {
    return CustomWorldId(body);
  } else {
    auto rest = body.split(":", 1);
    if (!rest)
      return std::unexpected(std::move(rest.error()));
    if (rest->size() != 2)
      return std::unexpected(StarException::format("Wrong number of parts in ClientCustomWorldId"));
    auto uuid = Uuid::create((*rest)[0]);
    if (!uuid)
      return std::unexpected(std::move(uuid.error()));
    return ClientCustomWorldId(std::move(*uuid), (*rest)[1]);
  }
}

std::ostream& operator<<(std::ostream& os, CelestialWorldId const& worldId) {
  os << static_cast<CelestialCoordinate>(worldId);
  return os;
}


std::ostream& operator<<(std::ostream& os, ClientShipWorldId const& worldId) {
  os << static_cast<Uuid>(worldId).hex();
  return os;
}


std::ostream& operator<<(std::ostream& os, CustomWorldId const& worldId) {
  os << static_cast<String>(worldId);
  return os;
}

std::ostream& operator<<(std::ostream& os, ClientCustomWorldId const& worldId) {
  format(os, "ClientCustomWorld:{}:{}", worldId.uuid.hex(), worldId.name);
  return os;
}

Json spawnTargetToJson(SpawnTarget spawnTarget) {
  if (std::holds_alternative<SpawnTargetUniqueEntity>(spawnTarget))
    return std::get<SpawnTargetUniqueEntity>(spawnTarget);
  else if (std::holds_alternative<SpawnTargetPosition>(spawnTarget))
    return jsonFromVec2F(std::get<SpawnTargetPosition>(spawnTarget));
  else if (std::holds_alternative<SpawnTargetX>(spawnTarget))
    return Json(std::get<SpawnTargetX>(spawnTarget));
  else
    return Json();
}

std::expected<SpawnTarget, StarException> spawnTargetFromJson(Json v) {
  if (v.isNull()) {
    return SpawnTarget();
  } else if (v.isType(Json::Type::String)) {
    auto text = v.toString();
    if (!text)
      return std::unexpected(std::move(text.error()));
    return SpawnTargetUniqueEntity(std::move(*text));
  } else if (v.isType(Json::Type::Float)) {
    auto x = v.toFloat();
    if (!x)
      return std::unexpected(std::move(x.error()));
    return SpawnTargetX(*x);
  } else {
    auto position = jsonToVec2F(v);
    if (!position)
      return std::unexpected(std::move(position.error()));
    return SpawnTargetPosition(*position);
  }
}

String printSpawnTarget(SpawnTarget spawnTarget) {
  if (std::holds_alternative<SpawnTargetUniqueEntity>(spawnTarget))
    return std::get<SpawnTargetUniqueEntity>(spawnTarget);
  else if (std::holds_alternative<SpawnTargetPosition>(spawnTarget)) {
    auto const& pos = std::get<SpawnTargetPosition>(spawnTarget);
    return strf("{}.{}", pos[0], pos[1]);
  } else if (std::holds_alternative<SpawnTargetX>(spawnTarget))
    return toString(std::get<SpawnTargetX>(spawnTarget).t);
  else
    return "";
}

WarpToWorld::WarpToWorld() = default;

WarpToWorld::WarpToWorld(WorldId world, SpawnTarget target) : world(std::move(world)), target(std::move(target)) {}

std::expected<WarpToWorld, StarException> WarpToWorld::create(Json v) {
  WarpToWorld result;
  if (v) {
    auto worldText = v.getString("world");
    if (!worldText)
      return std::unexpected(std::move(worldText.error()));
    auto world = parseWorldId(*worldText);
    if (!world)
      return std::unexpected(std::move(world.error()));
    auto targetJson = v.get("target");
    if (!targetJson)
      return std::unexpected(std::move(targetJson.error()));
    auto target = spawnTargetFromJson(*targetJson);
    if (!target)
      return std::unexpected(std::move(target.error()));
    result.world = std::move(*world);
    result.target = std::move(*target);
  }
  return result;
}

bool WarpToWorld::operator==(WarpToWorld const& rhs) const {
  return std::tie(world, target) == std::tie(rhs.world, rhs.target);
}

WarpToWorld::operator bool() const {
  return !std::holds_alternative<std::monostate>(world);
}

std::expected<Json, StarException> WarpToWorld::toJson() const {
  auto printed = printWorldId(world);
  if (!printed)
    return std::unexpected(std::move(printed.error()));
  return JsonObject{{"world", std::move(*printed)}, {"target", spawnTargetToJson(target)}};
}

std::expected<WarpAction, StarException> parseWarpAction(String const& warpString) {
  auto isReturn = warpString.equalsIgnoreCase("Return");
  if (!isReturn)
    return std::unexpected(std::move(isReturn.error()));
  if (*isReturn)
    return WarpAlias::Return;
  auto isOrbited = warpString.equalsIgnoreCase("OrbitedWorld");
  if (!isOrbited)
    return std::unexpected(std::move(isOrbited.error()));
  if (*isOrbited)
    return WarpAlias::OrbitedWorld;
  auto isShip = warpString.equalsIgnoreCase("OwnShip");
  if (!isShip)
    return std::unexpected(std::move(isShip.error()));
  if (*isShip)
    return WarpAlias::OwnShip;
  auto isPlayer = warpString.beginsWith("Player:", String::CaseSensitivity::CaseInsensitive);
  if (!isPlayer)
    return std::unexpected(std::move(isPlayer.error()));
  if (*isPlayer) {
    auto parts = warpString.split(':', 1);
    if (!parts)
      return std::unexpected(std::move(parts.error()));
    if (parts->size() != 2)
      return std::unexpected(StarException("Missing player warp UUID"));
    auto uuid = Uuid::create((*parts)[1]);
    if (!uuid)
      return std::unexpected(std::move(uuid.error()));
    return WarpToPlayer(std::move(*uuid));
  }
  auto parts = warpString.split('=', 1);
  if (!parts)
    return std::unexpected(std::move(parts.error()));
  auto world = parseWorldId((*parts)[0]);
  if (!world)
    return std::unexpected(std::move(world.error()));
  SpawnTarget target;
  if (parts->size() == 2) {
    auto const& targetPart = (*parts)[1];
    auto positionMatch = targetPart.regexMatch("\\d+.\\d+");
    if (!positionMatch)
      return std::unexpected(std::move(positionMatch.error()));
    if (*positionMatch) {
      auto pos = targetPart.split(".", 1);
      if (!pos)
        return std::unexpected(std::move(pos.error()));
      if (pos->size() != 2)
        return std::unexpected(StarException("Malformed positional warp target"));
      auto x = lexicalCast<int>((*pos)[0]);
      if (!x)
        return std::unexpected(std::move(x.error()));
      auto y = lexicalCast<int>((*pos)[1]);
      if (!y)
        return std::unexpected(std::move(y.error()));
      target = SpawnTargetPosition(Vec2F(*x, *y));
    } else {
      auto xMatch = targetPart.regexMatch("\\d+");
      if (!xMatch)
        return std::unexpected(std::move(xMatch.error()));
      if (*xMatch) {
        auto x = lexicalCast<int>(targetPart);
        if (!x)
          return std::unexpected(std::move(x.error()));
        target = SpawnTargetX(*x);
      } else {
        target = SpawnTargetUniqueEntity(targetPart);
      }
    }
  }
  return WarpToWorld(std::move(*world), std::move(target));
}

std::expected<String, StarException> printWarpAction(WarpAction const& warpAction) {
  if (std::holds_alternative<WarpAlias>(warpAction)) {
    auto warpAlias = std::get<WarpAlias>(warpAction);
    if (warpAlias == WarpAlias::Return)
      return "Return";
    else if (warpAlias == WarpAlias::OrbitedWorld)
      return "OrbitedWorld";
    else if (warpAlias == WarpAlias::OwnShip)
      return "OwnShip";
  } else if (std::holds_alternative<WarpToPlayer>(warpAction)) {
    return strf("Player:{}", std::get<WarpToPlayer>(warpAction).hex());
  } else if (std::holds_alternative<WarpToWorld>(warpAction)) {
    auto const& warpToWorld = std::get<WarpToWorld>(warpAction);
    auto toWorldString = printWorldId(warpToWorld.world);
    if (!toWorldString)
      return std::unexpected(std::move(toWorldString.error()));
    if (!std::holds_alternative<std::monostate>(warpToWorld.target))
      return strf("{}={}", *toWorldString, printSpawnTarget(warpToWorld.target));
    return std::move(*toWorldString);
  }

  return "UnknownWarpAction";
}

std::expected<JsonObject, StarException> warpActionToJson(WarpAction const& warpAction) {
  if (std::holds_alternative<WarpAlias>(warpAction)) {
    auto warpAlias = std::get<WarpAlias>(warpAction);
    auto out = JsonObject{
      {"actionKind", "Alias"}
    };
    if (warpAlias == WarpAlias::Return)
      out.set("actionAlias","Return");
    else if (warpAlias == WarpAlias::OrbitedWorld)
      out.set("actionAlias","OrbitedWorld");
    else if (warpAlias == WarpAlias::OwnShip)
      out.set("actionAlias","OwnShip");
    return out;
  } else if (std::holds_alternative<WarpToPlayer>(warpAction)) {
    return JsonObject{
      {"actionKind", "Player"},
      {"uuid", std::get<WarpToPlayer>(warpAction).hex()}
    };
  } else if (std::holds_alternative<WarpToWorld>(warpAction)) {
    auto const& warpToWorld = std::get<WarpToWorld>(warpAction);
    auto printed = printWorldId(warpToWorld.world);
    if (!printed)
      return std::unexpected(std::move(printed.error()));
    auto out = JsonObject{
      {"actionKind", "World"},
      {"worldId", std::move(*printed)}
    };
    if (!std::holds_alternative<std::monostate>(warpToWorld.target))
      out.set("spawnTarget", spawnTargetToJson(warpToWorld.target));
    return out;
  }

  return JsonObject{
      {"actionKind", "UnknownWarpAction"}
  };
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, WarpToWorld& warpToWorld) {
  WarpToWorld decoded;
  auto result = ds >> decoded.world >> decoded.target;
  if (!result)
    return std::unexpected(std::move(result.error()));
  warpToWorld = std::move(decoded);
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, WarpToWorld const& warpToWorld) {
  return ds << warpToWorld.world << warpToWorld.target;
}

}
