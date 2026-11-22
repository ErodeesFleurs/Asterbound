import std;
import star.format;
import star.json;
import star.json_path;
import star.unicode;
import star.string;
import star.configuration;

namespace Star {

Configuration::Configuration(Json default_configuration, Json current_configuration)
    : m_defaults(std::move(default_configuration)), m_current{std::move(current_configuration)} {}

Json Configuration::current_configuration() const {
  std::lock_guard locker(m_mutex);
  return m_current.value;
}

ConfigurationSnapshot Configuration::snapshot() const {
  std::lock_guard locker(m_mutex);
  return m_current;
}

std::expected<Json, StarException> Configuration::get(String const& key, Json def) const {
  std::lock_guard locker(m_mutex);
  return m_current.value.get(key, std::move(def));
}

std::expected<Json, StarException> Configuration::get_path(String const& path, Json def) const {
  std::lock_guard locker(m_mutex);
  return m_current.value.query(path, std::move(def));
}

std::expected<Json, StarException> Configuration::get_default(String const& key) const {
  return m_defaults.get(key, {});
}

std::expected<void, StarException> Configuration::set(String const& key, Json const& value) {
  if (key == "configurationVersion")
    return std::unexpected(ConfigurationException("cannot set configurationVersion"));

  std::lock_guard locker(m_mutex);
  auto updated = value ? m_current.value.set(key, value) : m_current.value.erase(key);
  if (!updated)
    return std::unexpected(std::move(updated.error()));
  m_current.value = std::move(*updated);
  ++m_current.revision;
  return {};
}

std::expected<void, StarException> Configuration::set_path(String const& path, Json const& value) {
  if (path.empty())
    return std::unexpected(ConfigurationException("cannot replace or erase the configuration root"));

  auto parse_component = [](String& component, String const& query, Utf8Cursor& cursor)
      -> std::expected<JsonPath::TypeHint, StarException> {
    bool first = cursor.offset() == 0;
    auto parsed = JsonPath::parse_query_path(component, query, cursor);
    if (parsed && first && component == "configurationVersion")
      return std::unexpected(ConfigurationException("cannot set configurationVersion"));
    return parsed;
  };
  std::lock_guard locker(m_mutex);
  auto updated = value ? JsonPath::path_set(m_current.value, parse_component, path, value)
                       : JsonPath::path_remove(m_current.value, parse_component, path);
  if (!updated)
    return std::unexpected(std::move(updated.error()));
  m_current.value = std::move(*updated);
  ++m_current.revision;
  return {};
}

}
