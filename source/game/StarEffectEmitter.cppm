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

  void addEffectSources(String const& position, StringSet const& effectSources);
  void setSourcePosition(String name, Vec2F const& position);
  void setDirection(Direction direction);
  void setBaseVelocity(Vec2F const& velocity);

  [[nodiscard]] std::expected<void, StarException> tick(float dt, EntityMode mode);
  void reset();

  [[nodiscard]] std::expected<void, StarException> collectRenderEvents(RenderCallback& renderCallback);

  [[nodiscard]] std::expected<Json, StarException> toJson() const;
  [[nodiscard]] std::expected<void, StarException> fromJson(Json const& diskStore);

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
  Set<SourceKey> m_newSources;
  List<RunningSource> m_sources;
  NetElementData<Set<SourceKey>> m_activeSources;
  std::unique_ptr<PendingEvents> m_pending_events;
  bool m_reconcile = true;

  StringMap<Vec2F> m_positions;
  Direction m_direction = Direction::Right;
  Vec2F m_baseVelocity;

  bool m_collectsEvents = false;
};

}

export module star.effect_emitter;

export namespace Star {
using ::Star::RenderCallback;
using ::Star::EffectEmitter;
using ::Star::EffectSource;
}
