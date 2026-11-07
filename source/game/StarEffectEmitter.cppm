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

  void addEffectSources(String const& position, StringSet effectSources);
  void setSourcePosition(String name, Vec2F const& position);
  void setDirection(Direction direction);
  void setBaseVelocity(Vec2F const& velocity);

  [[nodiscard]] std::expected<void, StarException> tick(float dt, EntityMode mode);
  void reset();

  [[nodiscard]] std::expected<void, StarException> collectRenderEvents(RenderCallback& renderCallback);

  [[nodiscard]] std::expected<Json, StarException> toJson() const;
  [[nodiscard]] std::expected<void, StarException> fromJson(Json const& diskStore);

private:
  explicit EffectEmitter(Root& root) : m_root(root) {}
  Root& m_root;
  Set<std::pair<String, String>> m_newSources;
  List<std::shared_ptr<EffectSource>> m_sources;
  NetElementData<Set<std::pair<String, String>>> m_activeSources;

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
