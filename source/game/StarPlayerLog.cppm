module;
import star.format;

import std;
import star.string;
import star.set;
import star.json;

namespace Star {

class PlayerLog {
public:
  PlayerLog();
  [[nodiscard]] static std::expected<PlayerLog, StarException> create(Json const& json);

  Json toJson() const;

  int deathCount() const;
  void addDeathCount(int deaths);

  double playTime() const;
  void addPlayTime(double elapsedTime);

  bool introComplete() const;
  void setIntroComplete(bool complete);

  StringSet scannedObjects() const;
  bool addScannedObject(String const& objectName);
  void removeScannedObject(String const& objectName);
  void clearScannedObjects();

  StringSet radioMessages() const;
  bool addRadioMessage(String const& messageName);
  void clearRadioMessages();

  StringSet cinematics() const;
  bool addCinematic(String const& cinematic);
  void clearCinematics();

  StringList collections() const;
  StringSet collectables(String const& collection) const;
  bool has_collectable(String const& collection, String const& id) const;
  bool add_collectable(String const& collection, String const& collectable);
  void clear_collectables(String const& collection);

private:
  int m_deathCount;
  double m_playTime;
  bool m_introComplete;
  StringSet m_scannedObjects;
  StringSet m_radioMessages;
  StringSet m_cinematics;
  StringMap<StringSet> m_collections;
};

}

export module star.player_log;

export namespace Star {
using ::Star::PlayerLog;
}
