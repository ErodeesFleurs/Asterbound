#include <cstdio>
#include "GL/glew.h"
#include "SDL3/SDL.h"
#include <benchmark/benchmark.h>

import std;
import star.collision_block;
import star.tile_damage;
import star.file;
import star.format;
import star.font_benchmarks;
import star.liquid_types;
import star.liquids_database;
import star.logging;
import star.material_database;
import star.material_render_profile;
import star.material_types;
import star.native_runtime;
import star.benchmark_runner;
import star.native_ui_context;
import star.performance_benchmarks;
import star.renderer;
import star.renderer_opengl;
import star.root;
import star.root_loader;
import star.string;
import star.tile_painter;
import star.vector;
import star.world_camera;
import star.world_geometry;
import star.world_render_data;
import star.world_tiles;
import star.xx_hash;

namespace Star {

// Private benchmark access observes the production key, never a replacement
// hasher or a rendererless TilePainter. There is no runtime observer state.
struct TilePainterPerformanceAccess {
  static std::expected<std::pair<std::uint64_t, std::uint64_t>, StarException>
  hashes(TilePainter& painter, WorldRenderData& data, Vec2I chunk) {
    return painter.chunkHashes(data, chunk);
  }
  static std::expected<void, StarException> failSetupPreservingFrame(
      TilePainter& painter, WorldCamera const& camera, WorldRenderData& data) {
    auto terrain = painter.m_pendingTerrainChunks;
    auto liquid = painter.m_pendingLiquidChunks;
    auto textures = painter.m_textureCache.size();
    auto failed = painter.setup(camera, data);
    if (failed)
      return std::unexpected(StarException("Tile failure fixture unexpectedly succeeded"));
    if (painter.m_textureCache.size() <= textures)
      return std::unexpected(StarException("Tile failure occurred before exercising a texture miss"));
    if (!painter.m_setupTextures.empty() || painter.m_pendingTerrainChunks != terrain
        || painter.m_pendingLiquidChunks != liquid)
      return std::unexpected(StarException("Failed tile setup retained uploads or replaced the pending frame"));
    return {};
  }
};

namespace {

Vec2I const FixtureMin{384, 384};
Vec2U const FixtureSize{208, 208};
Vec2F const CameraCenter{480, 480};

enum class Change { Stable, Pan, Foreground, Background, Mod, Hue, NeighborEdge, Liquid, Reload };

struct TileFixture {
  WorldRenderData data;
  WorldCamera camera;
  MaterialId dirt;
  MaterialId stone;
  ModId grass;
  LiquidId water;

  RenderTile& tile(Vec2I position) {
    return data.tiles(static_cast<std::size_t>(position.x() - FixtureMin.x()),
                      static_cast<std::size_t>(position.y() - FixtureMin.y()));
  }

  void reset() {
    data.geometry = WorldGeometry(Vec2U{4096, 2048});
    data.tileMinPosition = FixtureMin;
    auto resized = data.tiles.set_size({FixtureSize.x(), FixtureSize.y()});
    if (!resized) Star::fatal_error(resized.error().what(), false);
    for (unsigned x = 0; x < FixtureSize.x(); ++x) {
      for (unsigned y = 0; y < FixtureSize.y(); ++y) {
        int worldX = FixtureMin.x() + x;
        int worldY = FixtureMin.y() + y;
        RenderTile tile{};
        tile.foreground = worldY < 480 ? dirt : EmptyMaterialId;
        tile.background = worldY < 484 ? stone : EmptyMaterialId;
        tile.foregroundMod = worldY == 479 && worldX % 3 == 0 ? grass : NoModId;
        tile.backgroundMod = NoModId;
        tile.liquidId = water;
        tile.liquidLevel = worldY >= 480 && worldY < 484 && worldX % 32 < 16 ? 160 : 0;
        data.tiles(x, y) = tile;
      }
    }
    camera.setWorldGeometry(data.geometry);
    camera.setScreenSize({640, 360});
    camera.setPixelRatio(1.0f);
    camera.setCenterWorldPosition(CameraCenter, true);
  }

  void change(Change change) {
    switch (change) {
    case Change::Stable: break;
    case Change::Pan: camera.setCenterWorldPosition(CameraCenter + Vec2F{16, 0}, true); break;
    case Change::Foreground: tile({480, 479}).foreground = stone; break;
    case Change::Background: tile({480, 481}).background = dirt; break;
    case Change::Mod: tile({481, 479}).foregroundMod = grass; break;
    case Change::Hue:
      tile({480, 479}).foregroundHueShift = 37;
      tile({480, 481}).backgroundHueShift = 53;
      break;
    case Change::NeighborEdge:
      // On both sides of a chunk boundary, including its padded neighbor.
      tile({479, 479}).foreground = EmptyMaterialId;
      tile({480, 479}).foreground = stone;
      break;
    case Change::Liquid:
      tile({480, 480}).liquidLevel = 240;
      tile({481, 480}).liquidLevel = 0;
      break;
    case Change::Reload: break;
    }
  }
};

std::expected<TileFixture, StarException> makeFixture(Root& root) {
  auto materials = root.materialDatabase();
  if (!materials)
    return std::unexpected(std::move(materials.error()));
  auto liquids = root.liquidsDatabase();
  if (!liquids)
    return std::unexpected(std::move(liquids.error()));
  auto dirt = (*materials)->materialId("dirt");
  if (!dirt)
    return std::unexpected(std::move(dirt.error()));
  auto stone = (*materials)->materialId("cobblestone");
  if (!stone)
    return std::unexpected(std::move(stone.error()));
  auto grass = (*materials)->modId("grass");
  if (!grass)
    return std::unexpected(std::move(grass.error()));
  auto water = (*liquids)->liquidId("water");
  if (!water)
    return std::unexpected(std::move(water.error()));
  TileFixture fixture{{}, {}, *dirt, *stone, *grass, *water};
  fixture.reset();
  return fixture;
}

std::expected<void, StarException> advance_native_frame(OpenGlRenderer& renderer) {
  auto finished = renderer.finishFrame();
  if (!finished)
    return finished;
  if (!SDL_GL_SwapWindow(SDL_GL_GetCurrentWindow()))
    return std::unexpected(StarException(Star::format("Could not present tile benchmark frame: {}", SDL_GetError())));
  return renderer.startFrame();
}

std::expected<std::uint64_t, StarException> imageHash(TilePainter& painter, TileFixture& fixture, OpenGlRenderer& renderer,
                                                      bool liquidOnly = false, bool prepare = true) {
  if (prepare) {
    auto setup = painter.setup(fixture.camera, fixture.data);
    if (!setup)
      return std::unexpected(std::move(setup.error()));
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glClear(GL_COLOR_BUFFER_BIT);
  if (!liquidOnly) {
    auto background = painter.renderBackground(fixture.camera);
    if (!background)
      return std::unexpected(std::move(background.error()));
    auto midground = painter.renderMidground(fixture.camera);
    if (!midground)
      return std::unexpected(std::move(midground.error()));
  }
  auto liquid = painter.renderLiquid(fixture.camera);
  if (!liquid)
    return std::unexpected(std::move(liquid.error()));
  if (!liquidOnly) {
    auto foreground = painter.renderForeground(fixture.camera);
    if (!foreground)
      return std::unexpected(std::move(foreground.error()));
  }
  std::vector<char> pixels(640 * 360 * 4);
  glReadPixels(0, 0, 640, 360, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
  if (auto error = glGetError(); error != GL_NO_ERROR)
    return std::unexpected(StarException(Star::format("Tile fixture framebuffer read failed: {}", error)));
  painter.cleanup();
  auto advanced = advance_native_frame(renderer);
  if (!advanced)
    return std::unexpected(std::move(advanced.error()));
  return xx_hash3(std::span<char const>(pixels));
}

std::expected<void, StarException> validateFixture(Root& root, std::shared_ptr<OpenGlRenderer> const& renderer) {
  auto fixture = makeFixture(root);
  if (!fixture)
    return std::unexpected(std::move(fixture.error()));
  for (auto change : {Change::Stable, Change::Pan, Change::Foreground, Change::Background, Change::Mod, Change::Hue,
                      Change::NeighborEdge, Change::Liquid, Change::Reload}) {
    fixture->reset();
    auto cached = TilePainter::create(root, renderer);
    if (!cached)
      return std::unexpected(std::move(cached.error()));
    auto before = imageHash(**cached, *fixture, *renderer);
    if (!before)
      return std::unexpected(std::move(before.error()));
    fixture->change(change);
    if (change == Change::Reload) {
      cached = TilePainter::create(root, renderer);
      if (!cached)
        return std::unexpected(std::move(cached.error()));
    }
    auto observed = imageHash(**cached, *fixture, *renderer);
    if (!observed)
      return std::unexpected(std::move(observed.error()));
    auto fresh = TilePainter::create(root, renderer);
    if (!fresh)
      return std::unexpected(std::move(fresh.error()));
    auto expected = imageHash(**fresh, *fixture, *renderer);
    if (!expected)
      return std::unexpected(std::move(expected.error()));
    auto hashes = TilePainterPerformanceAccess::hashes(**cached, fixture->data, {30, 29});
    if (!hashes)
      return std::unexpected(std::move(hashes.error()));
    auto reported = Star::print("TILE_FIXTURE change={} before={:016x} cached={:016x} fresh={:016x} terrain={:016x} liquid={:016x}\n",
        static_cast<int>(change), *before, *observed, *expected, hashes->first, hashes->second);
    if (!reported)
      return std::unexpected(std::move(reported.error()));
    if (*observed != *expected)
      return std::unexpected(StarException("Cached tile output differs from fresh TilePainter output"));
  }

  // Separate consumer-visible regression: a bottom foreground edit changes the
  // geometry of the partial liquid above it, even when liquid bytes do not move.
  fixture->reset();
  fixture->tile({480, 479}).foreground = EmptyMaterialId;
  fixture->tile({480, 479}).liquidLevel = 0;
  fixture->tile({480, 480}).liquidLevel = 160;
  auto cached = TilePainter::create(root, renderer);
  if (!cached)
    return std::unexpected(std::move(cached.error()));
  auto before = imageHash(**cached, *fixture, *renderer, true);
  if (!before)
    return std::unexpected(std::move(before.error()));
  fixture->tile({480, 479}).foreground = fixture->dirt;
  auto observed = imageHash(**cached, *fixture, *renderer, true);
  if (!observed)
    return std::unexpected(std::move(observed.error()));
  auto fresh = TilePainter::create(root, renderer);
  if (!fresh)
    return std::unexpected(std::move(fresh.error()));
  auto expected = imageHash(**fresh, *fixture, *renderer, true);
  if (!expected)
    return std::unexpected(std::move(expected.error()));
  bool matches = *observed == *expected;
  auto reported = Star::print("TILE_LIQUID_BOTTOM_INVALIDATION before={:016x} cached={:016x} fresh={:016x} matches={}\n",
      *before, *observed, *expected, static_cast<int>(matches));
  if (!reported)
    return std::unexpected(std::move(reported.error()));
  if (*before == *expected)
    return std::unexpected(StarException("Liquid-bottom fixture does not distinguish the two geometries"));
  if (!matches)
    return std::unexpected(StarException("Foreground edit left cached liquid geometry stale"));
  auto materials = root.materialDatabase();
  if (!materials)
    return std::unexpected(std::move(materials.error()));
  for (bool damage : {true, false}) {
    fixture->reset();
    auto painter = TilePainter::create(root, renderer);
    if (!painter) return std::unexpected(std::move(painter.error()));
    auto before = imageHash(**painter, *fixture, *renderer);
    if (!before) return std::unexpected(std::move(before.error()));
    if (damage) {
      fixture->tile({480, 479}).foregroundDamageLevel = 180;
      fixture->tile({480, 479}).foregroundDamageType = TileDamageType::Blockish;
      fixture->tile({480, 481}).backgroundDamageLevel = 170;
      fixture->tile({480, 481}).backgroundDamageType = TileDamageType::Blockish;
    } else {
      std::optional<MaterialId> nonBlock;
      for (auto const& name : (*materials)->materialNames()) {
        auto id = (*materials)->materialId(name);
        if (!id) return std::unexpected(std::move(id.error()));
        auto collision = (*materials)->materialCollisionKind(*id);
        if (!collision) return std::unexpected(std::move(collision.error()));
        if (!BlockCollisionSet.contains(*collision) && (*materials)->materialRenderProfile(*id)) {
          nonBlock = *id;
          break;
        }
      }
      if (!nonBlock)
        return std::unexpected(StarException("Tile fixture found no rendered non-block material"));
      fixture->tile({480, 479}).foreground = *nonBlock;
    }
    auto observed = imageHash(**painter, *fixture, *renderer);
    if (!observed) return std::unexpected(std::move(observed.error()));
    auto reference = TilePainter::create(root, renderer);
    if (!reference) return std::unexpected(std::move(reference.error()));
    auto expected = imageHash(**reference, *fixture, *renderer);
    if (!expected) return std::unexpected(std::move(expected.error()));
    auto reported = Star::print("TILE_EXTRA damage={} before={:016x} cached={:016x} fresh={:016x}\n",
        static_cast<int>(damage), *before, *observed, *expected);
    if (!reported)
      return std::unexpected(std::move(reported.error()));
    if (*observed != *expected || *before == *expected)
      return std::unexpected(StarException("Damage or midground fixture failed its visible pixel oracle"));
  }
  fixture->reset();
  auto recovery = TilePainter::create(root, renderer);
  if (!recovery) return std::unexpected(std::move(recovery.error()));
  auto normal = imageHash(**recovery, *fixture, *renderer);
  if (!normal) return std::unexpected(std::move(normal.error()));
  auto pending = (*recovery)->setup(fixture->camera, fixture->data);
  if (!pending) return std::unexpected(std::move(pending.error()));
  MaterialId missing = FirstMetaMaterialId - 1;
  while ((*materials)->materialCollisionKind(missing)) {
    if (missing == 0)
      return std::unexpected(StarException("Tile fixture found no absent real material"));
    --missing;
  }
  fixture->tile({448, 464}).foregroundHueShift = 37;
  fixture->tile({512, 464}).foreground = missing;
  auto failed = TilePainterPerformanceAccess::failSetupPreservingFrame(
      **recovery, fixture->camera, fixture->data);
  if (!failed) return std::unexpected(std::move(failed.error()));
  auto retained = imageHash(**recovery, *fixture, *renderer, false, false);
  if (!retained) return std::unexpected(std::move(retained.error()));
  fixture->reset();
  auto restored = imageHash(**recovery, *fixture, *renderer);
  if (!restored) return std::unexpected(std::move(restored.error()));
  if (*retained != *normal || *restored != *normal)
    return std::unexpected(StarException("Failed setup did not retain and restore its visible frame"));
  auto retained_report = Star::print("TILE_FAILED_SETUP_RETAINED normal={:016x} retained={:016x} restored={:016x} uploadsCleared=1\n",
      *normal, *retained, *restored);
  if (!retained_report)
    return std::unexpected(std::move(retained_report.error()));
  return {};
}

}// namespace

std::expected<void, StarException> registerTilePerformanceBenchmarks(PerformanceBenchmarkSession& session,
                                                                     std::shared_ptr<OpenGlRenderer> renderer) {
  auto validated = validateFixture(session.root, renderer);
  if (!validated)
    return validated;
  for (auto const& [name, change] :
       std::array<std::pair<char const*, Change>, 9>{{{"Stable", Change::Stable},
                                                      {"PanChunk", Change::Pan},
                                                      {"Foreground", Change::Foreground},
                                                      {"Background", Change::Background},
                                                      {"Mod", Change::Mod},
                                                      {"Hue", Change::Hue},
                                                      {"NeighborEdge", Change::NeighborEdge},
                                                      {"Liquid", Change::Liquid},
                                                      {"Reload", Change::Reload}}}) {
    auto benchmarkName = std::string("TilePainter/Setup/") + name;
    benchmark::RegisterBenchmark(
      benchmarkName.c_str(),
      [&session, renderer, change](benchmark::State& state) {
        auto advanced = advance_native_frame(*renderer);
        if (!advanced) {
          session.fail(state, advanced.error());
          return;
        }
        auto fixture = makeFixture(session.root);
        if (!fixture) {
          session.fail(state, fixture.error());
          return;
        }
        auto painter = TilePainter::create(session.root, renderer);
        if (!painter) {
          session.fail(state, painter.error());
          return;
        }
        auto warm = (*painter)->setup(fixture->camera, fixture->data);
        if (!warm) {
          session.fail(state, warm.error());
          return;
        }
        (*painter)->cleanup();
        for (auto _ : state) {
          (void)_;
          state.PauseTiming();
          if (change != Change::Stable) {
            fixture->reset();
            painter->reset();
            advanced = advance_native_frame(*renderer);
            if (!advanced) {
              state.ResumeTiming();
              session.fail(state, advanced.error());
              break;
            }
            painter = TilePainter::create(session.root, renderer);
            if (!painter) {
              state.ResumeTiming();
              session.fail(state, painter.error());
              break;
            }
            if (change != Change::Reload) {
              auto prepared = (*painter)->setup(fixture->camera, fixture->data);
              if (!prepared) {
                state.ResumeTiming();
                session.fail(state, prepared.error());
                break;
              }
              (*painter)->cleanup();
            }
            fixture->change(change);
          }
          state.ResumeTiming();
          auto setup = (*painter)->setup(fixture->camera, fixture->data);
          if (!setup) {
            session.fail(state, setup.error());
            break;
          }
          benchmark::ClobberMemory();
          state.PauseTiming();
          (*painter)->cleanup();
          advanced = advance_native_frame(*renderer);
          if (!advanced) {
            state.ResumeTiming();
            session.fail(state, advanced.error());
            break;
          }
          state.ResumeTiming();
        }
        if (painter)
          painter->reset();
        advanced = advance_native_frame(*renderer);
        if (!advanced)
          session.fail(state, advanced.error());
        state.SetItemsProcessed(state.iterations());
        state.SetLabel("real SDL/OpenGL TilePainter; 640x360; 208x208 padded tiles; reset and cleanup untimed");
      })
      ->MinTime(0.1)
      ->UseRealTime();
  }
  return {};
}

namespace {

struct SdlLifetime {
  ~SdlLifetime() { SDL_Quit(); }
};

struct SdlWindowDeleter {
  void operator()(SDL_Window* window) const { SDL_DestroyWindow(window); }
};

struct SdlGlContextDeleter {
  void operator()(SDL_GLContext context) const { (void)SDL_GL_DestroyContext(context); }
};

struct BenchmarkRegistrations {
  ~BenchmarkRegistrations() { benchmark::ClearRegisteredBenchmarks(); }
};

std::expected<int, StarException> run_native_benchmarks(Root& root) {
  // This is a benchmark-owned direct-backend session on main, not an
  // Application render callback: that callback receives a threaded facade.
  SdlLifetime sdl_lifetime;
  if (!SDL_Init(SDL_INIT_VIDEO))
    return std::unexpected(StarException(Star::format("Could not initialize benchmark SDL video: {}", SDL_GetError())));
  if (!SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3)
      || !SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3)
      || !SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE)
      || !SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1))
    return std::unexpected(StarException(Star::format("Could not configure benchmark OpenGL 3.3 context: {}", SDL_GetError())));

  std::unique_ptr<SDL_Window, SdlWindowDeleter> window{
      SDL_CreateWindow("Asterbound TilePainter benchmark", 640, 360, SDL_WINDOW_OPENGL)};
  if (!window)
    return std::unexpected(StarException(Star::format("Could not create benchmark SDL window: {}", SDL_GetError())));
  std::unique_ptr<std::remove_pointer_t<SDL_GLContext>, SdlGlContextDeleter> gl_context;
  // Capture before context creation makes it current. Declaration order keeps
  // the scope's restoration after renderer teardown and before context deletion.
  auto native_scope = NativeUiContextScope::create(nullptr, nullptr, nullptr);
  if (!native_scope)
    return std::unexpected(std::move(native_scope.error()));
  gl_context.reset(SDL_GL_CreateContext(window.get()));
  if (!gl_context)
    return std::unexpected(StarException(Star::format("Could not create benchmark OpenGL context: {}", SDL_GetError())));

  auto renderer = OpenGlRenderer::create(root.logger());
  if (!renderer)
    return std::unexpected(std::move(renderer.error()));
  auto screen = (*renderer)->setScreenSize({640, 360});
  if (!screen)
    return std::unexpected(std::move(screen.error()));
  auto effect = (*renderer)->switchEffectConfig("internal");
  if (!effect)
    return std::unexpected(std::move(effect.error()));
  if (!*effect)
    return std::unexpected(StarException("Tile benchmark internal renderer effect is unavailable"));
  auto started = (*renderer)->startFrame();
  if (!started)
    return std::unexpected(std::move(started.error()));

  PerformanceBenchmarkSession session{root};
  // Registered closures borrow session/Root and retain the backend. Clear them
  // on every exit before any of those owners or the native scope are released.
  BenchmarkRegistrations registrations;
  auto registered = registerTilePerformanceBenchmarks(session, *renderer);
  if (!registered)
    return std::unexpected(std::move(registered.error()));
  register_font_benchmarks(session, *renderer);
  auto result = Star::run_registered_benchmarks();
  auto finished = (*renderer)->finishFrame();
  if (!finished)
    return std::unexpected(std::move(finished.error()));
  benchmark::ClearRegisteredBenchmarks();
  renderer->reset();
  auto restored = native_scope->restore();
  if (!restored)
    return std::unexpected(std::move(restored.error()));
  return session.failed || result != 0 ? 1 : 0;
}

}// namespace
}// namespace Star

int main(int argc, char** argv) {
  auto native = Star::NativeRuntime::create();
  if (!native) {
    if (!Star::print(*stderr, "{}\n", native.error().what()))
      return 1;
    return 1;
  }
  Star::BenchmarkLifetime benchmark_lifetime(argc, argv);
  auto loader = Star::RootLoader::create({{}, {}, {}, Star::LogLevel::Error, true, {}});
  if (!loader) {
    (void)Star::print(*stderr, "{}\n", loader.error().what());
    return 1;
  }
  auto parsed = loader->command_parse_or_die({argv, static_cast<std::size_t>(argc)});
  if (!parsed) {
    (void)Star::print(*stderr, "{}\n", parsed.error().what());
    return 1;
  }
  auto& settings = parsed->first;
  for (std::string_view directory : {"player", "universe"}) {
    auto exists = Star::File::exists(Star::File::relative_to(settings.storageDirectory, directory));
    if (!exists) {
      (void)Star::print(*stderr, "{}\n", exists.error().what());
      return 1;
    }
    if (*exists) {
      Star::StarException error("Tile benchmarks require private storage without player/universe saves");
      (void)Star::print(*stderr, "{}\n", error.what());
      return 1;
    }
  }
  settings.runtimeConfigFile.reset();
  settings.logFile.reset();
  auto root = Star::Root::create(std::move(settings));
  if (!root) {
    (void)Star::print(*stderr, "{}\n", root.error().what());
    return 1;
  }
  auto result = Star::run_native_benchmarks(**root);
  if (!result) {
    (void)Star::print(*stderr, "{}\n", result.error().what());
    return 1;
  }
  return *result;
}
