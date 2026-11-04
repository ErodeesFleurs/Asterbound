module;
import std;
import star.json;
import star.format;

import star.audio;
import star.thread;
import star.list;
import star.map;
import star.set;
import star.vector;
import star.bi_map;
import star.mixer;
import star.color;
import star.string;
import star.data_stream;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.drawable;
import star.game_types;
import star.entity_rendering_types;
import star.animation;
import star.particle;
import star.drawable;
import star.game_types;
import star.world_geometry;

import star.light_source;

namespace Star {

class TileDrawer;

// The two logical endpoints of one display interval. Identity and explicit
// discontinuities are supplied by the owning world.
struct PresentationPosition {
  Vec2F previous;
  Vec2F current;

  void reset(Vec2F position) { previous = current = position; }
  void advance(Vec2F position) { previous = current; current = position; }
  bool finish(Vec2F position, WorldGeometry const& geometry, float maximumContinuousDistance) {
    if (geometry.diff(position, previous).magnitudeSquared() > maximumContinuousDistance * maximumContinuousDistance) {
      reset(position);
      return false;
    }
    current = position;
    return true;
  }
  Vec2F sample(WorldGeometry const& geometry, float interpolation) const {
    return current + geometry.diff(previous, current) * (1.0f - std::clamp(interpolation, 0.0f, 1.0f));
  }
};

// Synchronous output sink. Tick-owned event collection submits particles and
// sounds; display sampling submits geometry and lights. Positions supplied to
// the sink are already in world space.
class RenderCallback {
public:
  virtual ~RenderCallback();

  virtual std::optional<std::reference_wrapper<TileDrawer>> tileDrawer() const;
  virtual PresentationContext presentation() const { return PresentationContext(); }
  virtual float timestep() const { return 0.0f; }


  [[nodiscard]] virtual std::expected<void, StarException> addDrawable(Drawable drawable, EntityRenderLayer renderLayer) = 0;
  [[nodiscard]] virtual std::expected<void, StarException> addLightSource(LightSource lightSource) = 0;
  [[nodiscard]] virtual std::expected<void, StarException> addParticle(Particle particle) = 0;
  [[nodiscard]] virtual std::expected<void, StarException> addAudio(std::shared_ptr<AudioInstance> audio) = 0;
  [[nodiscard]] virtual std::expected<void, StarException> addTilePreview(PreviewTile preview) = 0;
  [[nodiscard]] virtual std::expected<void, StarException> addOverheadBar(OverheadBar bar) = 0;

  // Convenience non-virtuals

  [[nodiscard]] std::expected<void, StarException> addDrawables(List<Drawable> drawables, EntityRenderLayer renderLayer, Vec2F translate = Vec2F());
  [[nodiscard]] std::expected<void, StarException> addLightSources(List<LightSource> lightSources, Vec2F translate = Vec2F());
  [[nodiscard]] std::expected<void, StarException> addParticles(List<Particle> particles, Vec2F translate = Vec2F());
  [[nodiscard]] std::expected<void, StarException> addAudios(List<std::shared_ptr<AudioInstance>> audios, Vec2F translate = Vec2F());
  [[nodiscard]] std::expected<void, StarException> addTilePreviews(List<PreviewTile> previews);
  [[nodiscard]] std::expected<void, StarException> addOverheadBars(List<OverheadBar> bars, Vec2F translate = Vec2F());
};

// Consume hidden entity events on the same tick as visible entities without
// replaying an accumulated audio/particle burst when they become visible.
class NullRenderCallback : public RenderCallback {
public:
  explicit NullRenderCallback(List<std::shared_ptr<AudioInstance>>* deferredLoops = nullptr) : m_deferredLoops(deferredLoops) {}
  std::expected<void, StarException> addDrawable(Drawable, EntityRenderLayer) override { return {}; }
  std::expected<void, StarException> addLightSource(LightSource) override { return {}; }
  std::expected<void, StarException> addParticle(Particle) override { return {}; }
  std::expected<void, StarException> addAudio(std::shared_ptr<AudioInstance> audio) override {
    if (m_deferredLoops && audio->loops() != 0)
      m_deferredLoops->append(std::move(audio));
    return {};
  }
  std::expected<void, StarException> addTilePreview(PreviewTile) override { return {}; }
  std::expected<void, StarException> addOverheadBar(OverheadBar) override { return {}; }
private:
  List<std::shared_ptr<AudioInstance>>* m_deferredLoops;
};

}

export module star.entity_rendering;

export namespace Star {
  using ::Star::RenderCallback;
  using ::Star::PresentationPosition;
  using ::Star::NullRenderCallback;
}
