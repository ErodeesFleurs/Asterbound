module;

import std;
import star.string;
import star.map;
import star.format;
import star.json;
import star.bi_map;
import star.game_types;

import star.tech_database;

namespace Star {

class Root;

struct PlayerTechExceptionTag {
  static constexpr std::string_view name() { return "PlayerTechException"; }
};
using PlayerTechException = StarError<PlayerTechExceptionTag, StarException>;

// Set of player techs, techs can be either unavailable, available but not
// enabled, enabled but not equipped, or equipped.
class PlayerTech {
public:
  PlayerTech(Root& root);
  [[nodiscard]] static std::expected<std::shared_ptr<PlayerTech>, StarException> create(Root& root, Json const& json);

  [[nodiscard]] std::expected<Json, StarException> toJson() const;

  bool isAvailable(String const& techModule) const;
  void makeAvailable(String const& techModule);
  void makeUnavailable(String const& techModule);

  bool isEnabled(String const& techModule) const;
  [[nodiscard]] std::expected<void, StarException> enable(String const& techModule);
  void disable(String const& techModule);

  bool isEquipped(String const& techModule) const;
  [[nodiscard]] std::expected<void, StarException> equip(String const& techModule);
  void unequip(String const& techModule);

  StringSet const& availableTechs() const;
  StringSet const& enabledTechs() const;
  HashMap<TechType, String> const& equipped_techs() const;
  StringList techModules() const;

private:
  Root& m_root;
  StringSet m_availableTechs;
  StringSet m_enabledTechs;
  HashMap<TechType, String> m_equipped_techs;
};

}

export module star.player_tech;

export namespace Star {
using ::Star::PlayerTech;
using ::Star::PlayerTechExceptionTag;
using ::Star::PlayerTechException;
}
