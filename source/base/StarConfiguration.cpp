import std;
import star.format;
import star.json;
import star.map;
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

namespace {

std::expected<void, StarException> replace_member(JsonObject& object,
    std::span<std::reference_wrapper<String const> const> keys, Json value) {
  auto const& key = keys.front().get();
  if (keys.size() == 1) {
    if (value.is_null())
      object.erase(key);
    else
      object[key] = std::move(value);
    return {};
  }
  auto existing = object.find_ref(key);
  JsonObject child;
  if (existing && existing->get().is_type(Json::Type::Object)) {
    auto members = existing->get().to_object();
    if (!members) return std::unexpected(std::move(members.error()));
    child = std::move(*members);
  } else if (value.is_null()) {
    return {};
  }
  if (auto result = replace_member(child, keys.subspan(1), std::move(value)); !result)
    return result;
  if (existing)
    existing->get() = Json(std::move(child));
  else
    object.emplace(key, Json(std::move(child)));
  return {};
}

}

std::expected<void, StarException> Configuration::set_object_path(
    std::span<std::reference_wrapper<String const> const> keys, Json value) {
  if (keys.empty())
    return std::unexpected(ConfigurationException("cannot replace or erase the configuration root"));
  if (keys.front().get() == "configurationVersion")
    return std::unexpected(ConfigurationException("cannot set configurationVersion"));
  std::lock_guard locker(m_mutex);
  auto object = m_current.value.to_object();
  if (!object) return std::unexpected(std::move(object.error()));
  if (auto result = replace_member(*object, keys, std::move(value)); !result)
    return result;
  m_current.value = Json(std::move(*object));
  ++m_current.revision;
  return {};
}

}
