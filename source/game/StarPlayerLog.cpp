import star.format;
import std;
import star.string;
import star.set;
import star.json;
import star.json_extra;

import star.player_log;

namespace Star {

PlayerLog::PlayerLog() {
  m_deathCount = 0;
  m_playTime = 0;
  m_introComplete = false;
  m_scannedObjects = StringSet();
  m_radio_messages = StringSet();
  m_cinematics = StringSet();
}

std::expected<PlayerLog, StarException> PlayerLog::create(Json const& json) {
  PlayerLog log;
  auto deaths = json.get_int("deathCount");
  if (!deaths)
    return std::unexpected(std::move(deaths.error()));
  log.m_deathCount = *deaths;
  auto playTime = json.get_double("playTime");
  if (!playTime)
    return std::unexpected(std::move(playTime.error()));
  log.m_playTime = *playTime;
  auto introComplete = json.get_bool("introComplete");
  if (!introComplete)
    return std::unexpected(std::move(introComplete.error()));
  log.m_introComplete = *introComplete;
  for (auto const& field : {
      std::pair{std::string_view{"scannedObjects"}, std::ref(log.m_scannedObjects)},
      std::pair{std::string_view{"radioMessages"}, std::ref(log.m_radio_messages)},
      std::pair{std::string_view{"cinematics"}, std::ref(log.m_cinematics)}}) {
    auto value = json.get(field.first);
    if (!value)
      return std::unexpected(std::move(value.error()));
    auto strings = json_to_string_set(*value);
    if (!strings)
      return std::unexpected(std::move(strings.error()));
    field.second.get() = std::move(*strings);
  }
  auto collections = json.get("collections").and_then([](Json const& value) { return value.iterate_object(); });
  if (!collections)
    return std::unexpected(std::move(collections.error()));
  for (auto const& entry : *collections) {
    auto strings = json_to_string_set(entry.second);
    if (!strings)
      return std::unexpected(std::move(strings.error()));
    log.m_collections.try_emplace(entry.first, std::move(*strings));
  }
  return log;
}

Json PlayerLog::toJson() const {
  JsonObject collections;
  for (auto const& [name, entries] : m_collections)
    collections.emplace(name, json_from_string_set(entries));

  return JsonObject{{"deathCount", m_deathCount},
      {"playTime", m_playTime},
      {"introComplete", m_introComplete},
      {"scannedObjects", json_from_string_set(m_scannedObjects)},
      {"radioMessages", json_from_string_set(m_radio_messages)},
      {"cinematics", json_from_string_set(m_cinematics)},
      {"collections", std::move(collections)}};
}

int PlayerLog::deathCount() const {
  return m_deathCount;
}

void PlayerLog::addDeathCount(int deaths) {
  m_deathCount += deaths;
}

double PlayerLog::playTime() const {
  return m_playTime;
}

void PlayerLog::addPlayTime(double elapsedTime) {
  m_playTime += elapsedTime;
}

bool PlayerLog::introComplete() const {
  return m_introComplete;
}

void PlayerLog::setIntroComplete(bool complete) {
  m_introComplete = complete;
}

StringSet PlayerLog::scannedObjects() const {
  return m_scannedObjects;
}

bool PlayerLog::addScannedObject(String const& objectName) {
  return m_scannedObjects.add(objectName);
}

void PlayerLog::removeScannedObject(String const& objectName) {
  m_scannedObjects.remove(objectName);
}

void PlayerLog::clearScannedObjects() {
  m_scannedObjects.clear();
}

bool PlayerLog::has_radio_message(String const& message_name) const {
  return m_radio_messages.contains(message_name);
}

bool PlayerLog::add_radio_message(String const& message_name) {
  return m_radio_messages.add(message_name);
}

void PlayerLog::clear_radio_messages() {
  m_radio_messages.clear();
}

StringSet PlayerLog::cinematics() const {
  return m_cinematics;
}

bool PlayerLog::addCinematic(String const& cinematic) {
  return m_cinematics.add(cinematic);
}

void PlayerLog::clearCinematics() {
  m_cinematics.clear();
}

StringList PlayerLog::collections() const {
  return m_collections.keys();
}

StringSet PlayerLog::collectables(String const& collection) const {
  if (auto collectables = m_collections.find_ref(collection))
    return collectables->get();
  return {};
}

bool PlayerLog::has_collectable(String const& collection, String const& id) const {
  auto entries = m_collections.find_ref(collection);
  return entries && entries->get().contains(id);
}

bool PlayerLog::add_collectable(String const& collection, String const& collectable) {
  return m_collections[collection].add(collectable);
}

void PlayerLog::clear_collectables(String const& collection) {
  m_collections.remove(collection);
}

}
