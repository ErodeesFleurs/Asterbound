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
