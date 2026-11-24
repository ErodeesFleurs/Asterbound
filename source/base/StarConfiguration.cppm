module;

import std;
import star.json;
import star.string;
import star.format;

namespace Star {

struct ConfigurationExceptionTag {
  static constexpr std::string_view name() { return "ConfigurationException"; }
};
using ConfigurationException = StarError<ConfigurationExceptionTag, StarException>;

struct ConfigurationSnapshot {
  Json value;
  std::uint64_t revision = 0;
};

class Configuration {
public:
  Configuration(Json default_configuration, Json current_configuration);

  [[nodiscard]] Json current_configuration() const;
  [[nodiscard]] ConfigurationSnapshot snapshot() const;

  [[nodiscard]] std::expected<Json, StarException> get(String const& key, Json def = {}) const;
  [[nodiscard]] std::expected<Json, StarException> get_path(String const& path, Json def = {}) const;

  [[nodiscard]] std::expected<Json, StarException> get_default(String const& key) const;

  [[nodiscard]] std::expected<void, StarException> set(String const& key, Json const& value);
  [[nodiscard]] std::expected<void, StarException> set_path(String const& path, Json const& value);
  // Literal object keys, not a query expression. Create object parents for writes;
  // null erases the leaf. The entire read-modify-write advances one revision.
  [[nodiscard]] std::expected<void, StarException> set_object_path(
      std::span<std::reference_wrapper<String const> const> keys, Json value);

private:
  mutable std::mutex m_mutex;

  Json const m_defaults;
  ConfigurationSnapshot m_current;
};

}

export module star.configuration;

export namespace Star {
  using ::Star::ConfigurationExceptionTag;
  using ::Star::ConfigurationException;
  using ::Star::ConfigurationSnapshot;
  using ::Star::Configuration;
}
