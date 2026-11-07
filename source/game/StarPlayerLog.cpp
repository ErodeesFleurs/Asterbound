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
  m_radioMessages = StringSet();
  m_cinematics = StringSet();
}

std::expected<PlayerLog, StarException> PlayerLog::create(Json const& json) {
  PlayerLog log;
  auto deaths = json.getInt("deathCount");
  if (!deaths)
    return std::unexpected(std::move(deaths.error()));
  log.m_deathCount = *deaths;
  auto playTime = json.getDouble("playTime");
  if (!playTime)
    return std::unexpected(std::move(playTime.error()));
  log.m_playTime = *playTime;
  auto introComplete = json.getBool("introComplete");
  if (!introComplete)
    return std::unexpected(std::move(introComplete.error()));
  log.m_introComplete = *introComplete;
  for (auto const& field : {
      std::pair{std::string_view{"scannedObjects"}, std::ref(log.m_scannedObjects)},
      std::pair{std::string_view{"radioMessages"}, std::ref(log.m_radioMessages)},
      std::pair{std::string_view{"cinematics"}, std::ref(log.m_cinematics)}}) {
    auto value = json.get(field.first);
    if (!value)
      return std::unexpected(std::move(value.error()));
    auto strings = jsonToStringSet(*value);
    if (!strings)
      return std::unexpected(std::move(strings.error()));
    field.second.get() = std::move(*strings);
  }
  auto collections = json.getObject("collections");
  if (!collections)
    return std::unexpected(std::move(collections.error()));
  for (auto const& entry : *collections) {
    auto strings = jsonToStringSet(entry.second);
    if (!strings)
      return std::unexpected(std::move(strings.error()));
    log.m_collections[entry.first] = std::move(*strings);
  }
  return log;
}

Json PlayerLog::toJson() const {
  auto collections = JsonObject();
  for (auto const& pair : m_collections)
    collections[pair.first] = jsonFromStringSet(pair.second);

  return JsonObject{{"deathCount", m_deathCount},
      {"playTime", m_playTime},
      {"introComplete", m_introComplete},
      {"scannedObjects", jsonFromStringSet(m_scannedObjects)},
      {"radioMessages", jsonFromStringSet(m_radioMessages)},
      {"cinematics", jsonFromStringSet(m_cinematics)},
      {"collections", collections}};
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

StringSet PlayerLog::radioMessages() const {
  return m_radioMessages;
}

bool PlayerLog::addRadioMessage(String const& messageName) {
  return m_radioMessages.add(messageName);
}

void PlayerLog::clearRadioMessages() {
  m_radioMessages.clear();
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
  if (auto collectables = m_collections.maybeRef(collection))
    return collectables->get();
  return {};
}

bool PlayerLog::addCollectable(String const& collection, String const& collectable) {
  if (!m_collections.contains(collection))
    m_collections[collection] = {};

  return m_collections[collection].add(collectable);
}

void PlayerLog::clearCollectables(String const& collection) {
  m_collections.remove(collection);
}

}
