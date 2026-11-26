module;
import star.format;
import std;
import star.string;
import star.list;
import star.set;
import star.json;

import star.vector;
import star.game_types;

import star.net_element_basic_fields;
import star.net_element_group;

namespace Star {

class RenderCallback;
class EffectSource;
class Root;

class EffectEmitter : public NetElementGroup {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<EffectEmitter>, StarException> create(Root& root);
  ~EffectEmitter() override;

  void add_effect_sources(String const& position, StringSet const& effect_sources);
  void set_source_position(String name, Vec2F const& position);
  void set_direction(Direction direction);
  void set_base_velocity(Vec2F const& velocity);

  [[nodiscard]] std::expected<void, StarException> tick(float dt, EntityMode mode);
  void reset();

  [[nodiscard]] std::expected<void, StarException> collect_render_events(RenderCallback& render_callback);

  [[nodiscard]] std::expected<Json, StarException> to_json() const;
  [[nodiscard]] std::expected<void, StarException> from_json(Json const& disk_store);

private:
  explicit EffectEmitter(Root& root);
  using SourceKey = std::pair<String, String>;
  struct RunningSource {
    SourceKey key;
    std::shared_ptr<EffectSource> source;
  };
  struct PendingEvents;
  [[nodiscard]] std::expected<void, StarException> reconcile_sources();
  Root& m_root;
  Set<SourceKey> m_new_sources;
  List<RunningSource> m_sources;
  NetElementData<Set<SourceKey>> m_active_sources;
  std::unique_ptr<PendingEvents> m_pending_events;
  bool m_reconcile = true;

  StringMap<Vec2F> m_positions;
  Direction m_direction = Direction::Right;
  Vec2F m_base_velocity;

  bool m_collects_events = false;
};

}

export module star.effect_emitter;

export namespace Star {
using ::Star::RenderCallback;
using ::Star::EffectEmitter;
using ::Star::EffectSource;
}
