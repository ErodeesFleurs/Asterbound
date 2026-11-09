import star.json;
import star.audio;
import star.thread;
import star.list;
import star.map;
import star.set;
import star.vector;
import star.bi_map;
import star.color;
import star.string;
import star.data_stream;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.game_types;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.format;
import star.version;

import star.mixer;
import star.drawable;
import star.entity_rendering_types;
import star.animation;
import star.particle;

import star.light_source;
import star.entity_rendering;

namespace Star {

class TileDrawer;

RenderCallback::~RenderCallback() = default;

std::optional<std::reference_wrapper<TileDrawer>> RenderCallback::tileDrawer() const {
  return std::nullopt;
}

std::expected<void, StarException> RenderCallback::addDrawables(List<Drawable> drawables, EntityRenderLayer renderLayer, Vec2F translate) {
  for (auto& drawable : drawables) {
    drawable.translate(translate);
    auto result = addDrawable(std::move(drawable), renderLayer);
    if (!result)
      return std::unexpected(std::move(result.error()));
  }
  return {};
}

std::expected<void, StarException> RenderCallback::addLightSources(List<LightSource> lightSources, Vec2F translate) {
  for (auto& lightSource : lightSources) {
    lightSource.translate(translate);
    auto result = addLightSource(std::move(lightSource));
    if (!result)
      return std::unexpected(std::move(result.error()));
  }
  return {};
}

std::expected<void, StarException> RenderCallback::addParticles(List<Particle> particles, Vec2F translate) {
  for (auto& particle : particles) {
    particle.translate(translate);
    auto result = addParticle(std::move(particle));
    if (!result)
      return std::unexpected(std::move(result.error()));
  }
  return {};
}

std::expected<void, StarException> RenderCallback::addAudios(List<std::shared_ptr<AudioInstance>> audios, Vec2F translate) {
  for (auto& audio : audios) {
    audio->translate(translate);
    auto result = addAudio(std::move(audio));
    if (!result)
      return std::unexpected(std::move(result.error()));
  }
  return {};
}

std::expected<void, StarException> RenderCallback::addTilePreviews(List<PreviewTile> previews) {
  for (auto& preview : previews) {
    auto result = addTilePreview(std::move(preview));
    if (!result)
      return std::unexpected(std::move(result.error()));
  }
  return {};
}

std::expected<void, StarException> RenderCallback::addOverheadBars(List<OverheadBar> bars, Vec2F translate) {
  for (auto& bar : bars) {
    bar.entityPosition += translate;
    auto result = addOverheadBar(std::move(bar));
    if (!result)
      return std::unexpected(std::move(result.error()));
  }
  return {};
}

}
