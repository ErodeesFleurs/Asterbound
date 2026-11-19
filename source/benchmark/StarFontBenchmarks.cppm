module;
#include "GL/glew.h"
#include <benchmark/benchmark.h>

import std;
import star.assets;
import star.font;
import star.font_texture_group;
import star.format;
import star.image;
import star.list;
import star.performance_benchmarks;
import star.rect;
import star.ref_ptr;
import star.renderer;
import star.root;
import star.string;
import star.string_view;
import star.text_painter;
import star.vector;
import star.xx_hash;

namespace Star {
namespace {
using Result = std::expected<void, StarException>;
constexpr auto character = U'W';
constexpr unsigned pixel_size = 24;

std::expected<std::shared_ptr<Font>, StarException> load_face(Root& root) {
  auto assets = root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto source = (*assets)->font("/hobo.ttf");
  if (!source) return std::unexpected(std::move(source.error()));
  return (*source)->clone();
}

Result cold_raster(benchmark::State& state, PerformanceBenchmarkSession& session, std::shared_ptr<Renderer> const&) {
  auto face = load_face(session.root);
  if (!face) return std::unexpected(std::move(face.error()));
  auto sized = (*face)->set_pixel_size(pixel_size);
  if (!sized) return sized;
  auto first = (*face)->render(character);
  if (!first) return std::unexpected(std::move(first.error()));
  auto const& [image, offset, colored] = *first;
  if (colored || image.pixel_format() != PixelFormat::BGRA32 || image.width() < 3 || image.height() < 3)
    return std::unexpected(StarException("Expected a bordered grayscale W"));
  bool ink = false;
  for (unsigned y = 0; y < image.height(); ++y) {
    for (unsigned x = 0; x < image.width(); ++x) {
      auto pixel = image.get32(x, y);
      bool edge = x == 0 || y == 0 || x + 1 == image.width() || y + 1 == image.height();
      if (pixel[0] != 255 || pixel[1] != 255 || pixel[2] != 255 || (edge && pixel[3] != 0))
        return std::unexpected(StarException("Incorrect grayscale W border or channels"));
      ink |= pixel[3] != 0;
    }
  }
  if (!ink) return std::unexpected(StarException("No glyph coverage"));
  for (auto _ : state) {
    auto glyph = (*face)->render(character);
    if (!glyph) return std::unexpected(std::move(glyph.error()));
    benchmark::DoNotOptimize(*glyph);
  }
  auto last = (*face)->render(character);
  if (!last) return std::unexpected(std::move(last.error()));
  auto const& [last_image, last_offset, last_colored] = *last;
  if (image.size() != last_image.size() || last_colored != colored || last_offset != offset
      || !std::ranges::equal(image.data(), last_image.data()))
    return std::unexpected(StarException("Uncached glyph raster changed after timing"));
  auto bytes = image.data();
  state.SetLabel(std::format("hobo W 24px; warm face, uncached FT raster + image lifetime; {}x{} offset={},{} pixels={:016x}",
      image.width(), image.height(), offset.x(), offset.y(), xx_hash3(std::span(reinterpret_cast<char const*>(bytes.data()), bytes.size()))));
  state.SetItemsProcessed(state.iterations());
  return {};
}

Result configure_group(FontTextureGroup& group, std::shared_ptr<Font> const& face) {
  FontTextureGroup::FontSet fonts;
  auto inserted = fonts.insert_or_assign("hobo", face);
  if (!inserted) return std::unexpected(std::move(inserted.error()));
  return group.replace_fonts(std::move(fonts), "hobo", "", "");
}

Result hot_texture(benchmark::State& state, PerformanceBenchmarkSession& session, std::shared_ptr<Renderer> const& renderer) {
  auto face = load_face(session.root);
  if (!face) return std::unexpected(std::move(face.error()));
  auto atlas = renderer->createTextureGroup(TextureGroupSize::Small);
  if (!atlas) return std::unexpected(std::move(atlas.error()));
  FontTextureGroup group(session.root, *atlas);
  auto configured = configure_group(group, *face);
  if (!configured) return configured;
  auto first = group.glyph_texture(character, pixel_size);
  if (!first) return std::unexpected(std::move(first.error()));
  auto texture = first->get().texture;
  auto offset = first->get().offset;
  if (!texture || texture->size().x() < 3 || texture->size().y() < 3 || first->get().colored)
    return std::unexpected(StarException("Texture miss did not upload the expected grayscale glyph"));
  for (auto _ : state) {
    auto hit = group.glyph_texture(character, pixel_size);
    if (!hit) return std::unexpected(std::move(hit.error()));
    benchmark::DoNotOptimize(hit->get());
  }
  auto last = group.glyph_texture(character, pixel_size);
  if (!last) return std::unexpected(std::move(last.error()));
  if (last->get().texture != texture || last->get().colored || last->get().offset != offset)
    return std::unexpected(StarException("Warm glyph no longer refers to the original upload"));
  state.SetLabel(std::format("hobo W 24px; same atlas upload; {}x{} offset={},{}", texture->size().x(), texture->size().y(), offset.x(), offset.y()));
  state.SetItemsProcessed(state.iterations());
  return {};
}

Result alternating_width(benchmark::State& state, PerformanceBenchmarkSession& session, std::shared_ptr<Renderer> const& renderer) {
  auto face = load_face(session.root);
  if (!face) return std::unexpected(std::move(face.error()));
  auto atlas = renderer->createTextureGroup(TextureGroupSize::Small);
  if (!atlas) return std::unexpected(std::move(atlas.error()));
  FontTextureGroup group(session.root, *atlas);
  auto configured = configure_group(group, *face);
  if (!configured) return configured;
  auto small = group.glyph_width(character, 12);
  if (!small) return std::unexpected(std::move(small.error()));
  auto large = group.glyph_width(character, 36);
  if (!large) return std::unexpected(std::move(large.error()));
  if (*small == 0 || *large <= *small)
    return std::unexpected(StarException("Width fixture did not distinguish the two sizes"));
  unsigned last_small = 0, last_large = 0;
  for (auto _ : state) {
    auto a = group.glyph_width(character, 12);
    if (!a) return std::unexpected(std::move(a.error()));
    auto b = group.glyph_width(character, 36);
    if (!b) return std::unexpected(std::move(b.error()));
    last_small = *a;
    last_large = *b;
    benchmark::DoNotOptimize(last_small);
    benchmark::DoNotOptimize(last_large);
  }
  if (last_small != *small || last_large != *large)
    return std::unexpected(StarException("Alternating glyph width returned stale metrics"));
  state.SetLabel(std::format("two hot hobo W metrics, alternating 12/36px; advances {},{}", *small, *large));
  state.SetItemsProcessed(2 * state.iterations());
  return {};
}

constexpr std::string_view line = "Asterbound0123456789";
using Pixel = std::array<unsigned char, 4>;
std::expected<std::vector<Pixel>, StarException> read_line(TextPainter& painter, Renderer& renderer,
    TextPositioning const& position, RectF const& expected) {
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  auto bounds = painter.renderLine(StringView(line), position);
  if (!bounds) return std::unexpected(std::move(bounds.error()));
  if (*bounds != expected || renderer.immediatePrimitives().size() != line.size() * 3)
    return std::unexpected(StarException("Layered line bounds or shadow/back/front quad count changed"));
  auto drawn = renderer.flush();
  if (!drawn) return std::unexpected(std::move(drawn.error()));
  std::vector<Pixel> pixels(640 * 64);
  glReadPixels(0, 0, 640, 64, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
  if (glGetError() != GL_NO_ERROR) return std::unexpected(StarException("Layered text framebuffer read failed"));
  return pixels;
}

Result layered_line(benchmark::State& state, PerformanceBenchmarkSession& session, std::shared_ptr<Renderer> const& renderer) {
  auto atlas = renderer->createTextureGroup(TextureGroupSize::Small);
  if (!atlas) return std::unexpected(std::move(atlas.error()));
  auto painter = TextPainter::create(session.root, renderer, *atlas);
  if (!painter) return std::unexpected(std::move(painter.error()));
  auto font = (*painter)->setFont("hobo");
  if (!font) return font;
  (*painter)->setFontSize(pixel_size);
  (*painter)->setFontColor({255, 255, 255, 255});
  auto shadow = (*painter)->applyCommands("shadow=#0000ff");
  if (!shadow) return shadow;
  auto back = (*painter)->setProcessingDirectives("?border=1;ff0000;ff0000", true);
  if (!back) return back;
  TextPositioning position(Vec2F(16, 16));
  auto expected = (*painter)->determineLineSize(StringView(line), position);
  if (!expected) return std::unexpected(std::move(expected.error()));
  auto before = read_line(**painter, *renderer, position, *expected);
  if (!before) return std::unexpected(std::move(before.error()));
  bool red = false, blue = false, white = false;
  for (auto const& p : *before) {
    red |= p[0] > p[1] && p[0] > p[2];
    blue |= p[2] > p[1] && p[2] > p[0];
    white |= p[0] == 255 && p[1] == 255 && p[2] == 255;
  }
  if (!red || !blue || !white) return std::unexpected(StarException("Missing red outline, blue shadow or white text in readback"));
  // Time hot layout, selection and primitive assembly. Submit real draws in
  // bounded batches outside timing; do not attribute GL stalls or swap to text.
  constexpr unsigned batch = 8;
  for (auto _ : state) {
    for (unsigned i = 0; i < batch; ++i) {
      auto bounds = (*painter)->renderLine(StringView(line), position);
      if (!bounds) return std::unexpected(std::move(bounds.error()));
      benchmark::DoNotOptimize(*bounds);
    }
    state.PauseTiming();
    auto drawn = renderer->flush();
    state.ResumeTiming();
    if (!drawn) return drawn;
  }
  auto after = read_line(**painter, *renderer, position, *expected);
  if (!after) return std::unexpected(std::move(after.error()));
  if (*before != *after) return std::unexpected(StarException("Hot and cold layered text pixels differ"));
  state.SetLabel(std::format("8 hot lines, shadow + red border + front; GL flush untimed; {}x{} pixels={:016x}",
      expected->width(), expected->height(), xx_hash3(std::span(reinterpret_cast<char const*>(before->data()), before->size() * sizeof(Pixel)))));
  state.SetItemsProcessed(batch * state.iterations());
  return {};
}
} // namespace

void register_font_benchmarks(PerformanceBenchmarkSession& session, std::shared_ptr<Renderer> const& renderer) {
  using Operation = Result(benchmark::State&, PerformanceBenchmarkSession&, std::shared_ptr<Renderer> const&);
  auto add = [&](std::string_view name, Operation& run) {
    benchmark::RegisterBenchmark(name.data(), [&session, renderer, &run](benchmark::State& state) {
      if (auto result = run(state, session, renderer); !result) session.fail(state, result.error());
    })->MinTime(0.2)->UseRealTime();
  };
  add("Font/ColdRaster", cold_raster);
  add("Font/HotTextureHit", hot_texture);
  add("Font/AlternatingSizeWidth", alternating_width);
  add("Font/LayeredTextPainter", layered_line);
}
} // namespace Star

export module star.font_benchmarks;
export namespace Star {
using ::Star::register_font_benchmarks;
}
