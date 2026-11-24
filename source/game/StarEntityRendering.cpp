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

std::optional<std::reference_wrapper<TileDrawer>> RenderCallback::tile_drawer() const {
  return std::nullopt;
}

std::expected<void, StarException> RenderCallback::add_drawables(List<Drawable> drawables, EntityRenderLayer render_layer, Vec2F translate) {
  for (auto& drawable : drawables) {
    if (translate != Vec2F())
      drawable.translate(translate);
    auto result = add_drawable(std::move(drawable), render_layer);
    if (!result)
      return std::unexpected(std::move(result.error()));
  }
  return {};
}

std::expected<void, StarException> RenderCallback::add_light_sources(std::span<LightSource const> light_sources, Vec2F translate) {
  for (auto light_source : light_sources) {
    if (translate != Vec2F())
      light_source.translate(translate);
    auto result = add_light_source(std::move(light_source));
    if (!result)
      return std::unexpected(std::move(result.error()));
  }
  return {};
}

std::expected<void, StarException> RenderCallback::add_particles(List<Particle> particles, Vec2F translate) {
  for (auto& particle : particles) {
    if (translate != Vec2F())
      particle.translate(translate);
    auto result = add_particle(std::move(particle));
    if (!result)
      return std::unexpected(std::move(result.error()));
  }
  return {};
}

std::expected<void, StarException> RenderCallback::add_audios(List<std::shared_ptr<AudioInstance>> audios, Vec2F translate) {
  for (auto& audio : audios) {
    audio->translate(translate);
    auto result = add_audio(std::move(audio));
    if (!result)
      return std::unexpected(std::move(result.error()));
  }
  return {};
}

std::expected<void, StarException> RenderCallback::add_tile_previews(List<PreviewTile> previews) {
  for (auto& preview : previews) {
    auto result = add_tile_preview(std::move(preview));
    if (!result)
      return std::unexpected(std::move(result.error()));
  }
  return {};
}

std::expected<void, StarException> RenderCallback::add_overhead_bars(std::span<OverheadBar const> bars, Vec2F translate) {
  for (auto bar : bars) {
    if (translate != Vec2F())
      bar.entity_position += translate;
    auto result = add_overhead_bar(std::move(bar));
    if (!result)
      return std::unexpected(std::move(result.error()));
  }
  return {};
}

}
