module;
import star.byte_array;

import std;
import star.string;
import star.format;
import star.json;
import star.thread;
import star.net_compatibility;

namespace Star {

class Root;
class Rebuilder;
class Vehicle;

struct VehicleDatabaseExceptionTag {
  static constexpr std::string_view name() { return "VehicleDatabaseException"; }
};
using VehicleDatabaseException = StarError<VehicleDatabaseExceptionTag, StarException>;

class VehicleDatabase {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<VehicleDatabase>, StarException> create(Root& root);

  [[nodiscard]] std::expected<std::shared_ptr<Vehicle>, StarException> create(String const& vehicleName, Json const& extraConfig = Json()) const;

  [[nodiscard]] std::expected<ByteArray, StarException> net_store(std::shared_ptr<Vehicle> const& vehicle, NetCompatibilityRules rules) const;
  [[nodiscard]] std::expected<std::shared_ptr<Vehicle>, StarException> net_load(ByteArray const& net_store, NetCompatibilityRules rules) const;

  [[nodiscard]] std::expected<Json, StarException> diskStore(std::shared_ptr<Vehicle> const& vehicle) const;
  [[nodiscard]] std::expected<std::shared_ptr<Vehicle>, StarException> diskLoad(Json const& diskStore) const;

private:
  explicit VehicleDatabase(Root& root) : m_root(root) {}
  Root& m_root;

  StringMap<std::pair<String, Json>> m_vehicles;

  mutable std::recursive_mutex m_luaMutex;
  std::shared_ptr<Rebuilder> m_rebuilder;
};

}

export module star.vehicle_database;

export namespace Star {
  using ::Star::VehicleDatabaseExceptionTag;
  using ::Star::VehicleDatabaseException;
  using ::Star::Rebuilder;
  using ::Star::Vehicle;
  using ::Star::VehicleDatabase;
}
