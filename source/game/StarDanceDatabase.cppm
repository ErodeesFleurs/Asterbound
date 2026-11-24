module;
import star.format;
import star.game_timers;
import star.list;
import std;
import star.string;
import star.vector;

namespace Star {
class Assets;
class Logger;
class Root;

struct DanceStep {
  std::optional<String> body_frame;
  std::optional<String> front_arm_frame;
  std::optional<String> back_arm_frame;
  Vec2F head_offset;
  Vec2F front_arm_offset;
  Vec2F back_arm_offset;
  float front_arm_rotation = 0;
  float back_arm_rotation = 0;
};

struct Dance {
  String name;
  List<String> states;
  float cycle = 0;
  bool cyclic = false;
  float duration = 0;
  List<DanceStep> steps;

  [[nodiscard]] std::optional<std::reference_wrapper<DanceStep const>> step_at(float timer) const&;
  std::optional<std::reference_wrapper<DanceStep const>> step_at(float) const&& = delete;
};

struct TimedDance {
  std::optional<String> name;
  GameTimer timer;

  [[nodiscard]] std::expected<void, StarException> set(Root& root, std::optional<String> const& requested);
  void tick(float dt);
};

class DanceDatabase {
  struct ConstructionKey { explicit ConstructionKey() = default; };

public:
  DanceDatabase(ConstructionKey, Logger& logger) : m_logger(logger) {}
  [[nodiscard]] static std::expected<std::shared_ptr<DanceDatabase>, StarException> create(Assets const& assets, Logger& logger);

  [[nodiscard]] std::expected<std::shared_ptr<Dance const>, StarException> get_dance(String const& name) const;

private:
  Logger& m_logger;
  StringMap<std::shared_ptr<Dance const>> m_dances;
};

}

export module star.dance_database;

export namespace Star {
  using ::Star::DanceStep;
  using ::Star::Dance;
  using ::Star::TimedDance;
  using ::Star::DanceDatabase;
}
