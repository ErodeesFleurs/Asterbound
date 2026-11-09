import std;
import star.format;
import star.json;
import star.string;
import star.configuration;

namespace Star {

Configuration::Configuration(Json defaultConfiguration, Json currentConfiguration)
    : m_defaultConfig(std::move(defaultConfiguration)), m_currentConfig(std::move(currentConfiguration)) {}

Json Configuration::defaultConfiguration() const {
  return m_defaultConfig;
}

Json Configuration::currentConfiguration() const {
  std::lock_guard locker(m_mutex);
  return m_currentConfig;
}

std::expected<String, StarException> Configuration::printConfiguration() const {
  std::lock_guard locker(m_mutex);
  return m_currentConfig.printJson(2, true);
}

std::expected<Json, StarException> Configuration::get(String const& key, Json def) const {
  std::lock_guard locker(m_mutex);
  return m_currentConfig.get(key, std::move(def));
}

std::expected<Json, StarException> Configuration::getPath(String const& path, Json def) const {
  std::lock_guard locker(m_mutex);
  return m_currentConfig.query(path, std::move(def));
}

std::expected<Json, StarException> Configuration::getDefault(String const& key) const {
  std::lock_guard locker(m_mutex);
  return m_defaultConfig.get(key, {});
}

std::expected<Json, StarException> Configuration::getDefaultPath(String const& path) const {
  std::lock_guard locker(m_mutex);
  return m_defaultConfig.query(path, {});
}

std::expected<void, StarException> Configuration::set(String const& key, Json const& value) {
  std::lock_guard locker(m_mutex);
  if (key == "configurationVersion")
    return std::unexpected(ConfigurationException("cannot set configurationVersion"));

  auto updated = value ? m_currentConfig.set(key, value) : m_currentConfig.eraseKey(key);
  if (!updated)
    return std::unexpected(std::move(updated.error()));
  m_currentConfig = std::move(*updated);
  return {};
}

std::expected<void, StarException> Configuration::setPath(String const& path, Json const& value) {
  std::lock_guard locker(m_mutex);
  auto components = path.split_any("[].");
  if (!components)
    return std::unexpected(std::move(components.error()));
  if (!components->empty() && (*components)[0] == "configurationVersion")
    return std::unexpected(ConfigurationException("cannot set configurationVersion"));

  auto updated = value ? m_currentConfig.setPath(path, value) : m_currentConfig.erasePath(path);
  if (!updated)
    return std::unexpected(std::move(updated.error()));
  m_currentConfig = std::move(*updated);
  return {};
}

}
