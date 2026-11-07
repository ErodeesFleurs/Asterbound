#include <benchmark/benchmark.h>

import std;
import star.chat_bubble_separation;
import star.file;
import star.format;
import star.input_event;
import star.json;
import star.key_bindings;
import star.list;
import star.logging;
import star.native_runtime;
import star.rect;
import star.root;
import star.root_loader;
import star.set;
import star.string;
import star.vector;

using namespace Star;

namespace {

struct BenchmarkLifetime {
  ~BenchmarkLifetime() {
    benchmark::ClearRegisteredBenchmarks();
    benchmark::Shutdown();
  }
};

class CheckedReporter : public benchmark::ConsoleReporter {
public:
  void ReportRuns(std::vector<Run> const& reports) override {
    for (auto const& run : reports)
      failed |= run.skipped == benchmark::internal::SkippedWithError;
    benchmark::ConsoleReporter::ReportRuns(reports);
  }
  bool failed = false;
};

std::expected<void, StarException> requirePrivateStorage(String const& storage) {
  for (std::string_view name : {"player", "universe"}) {
    auto exists = File::exists(File::relative_to(storage, name));
    if (!exists)
      return std::unexpected(std::move(exists.error()));
    if (*exists)
      return std::unexpected(StarException("Frontend benchmarks require private storage without player/universe directories"));
  }
  return {};
}

bool bubblePreflight() {
  auto check = [](List<RectF> const& obstacles, RectF input, RectF expected) {
    BubbleSeparator<int> separator(1.0f);
    int id = 0;
    for (auto const& obstacle : obstacles)
      separator.addBubble(obstacle.center(), obstacle, id++);
    auto prepared = separator.forEach([](BubbleState<int>&, int&) -> std::expected<void, StarException> {
      return {};
    });
    if (!prepared)
      return false;
    separator.addBubble(input.center(), input, -1);
    auto matches = separator.filtered([](BubbleState<int> const&, int id) { return id == -1; });
    return matches.size() == 1 && std::ranges::all_of(matches, [&](auto const& bubble) {
      return bubble.separatedBox == expected;
    });
  };
  return check({{-100, 0, 100, 10}, {-20, 20, -10, 30}}, {0, 2, 10, 8}, {0, 10, 10, 16})
      && check({{-100, 0, 100, 10}, {20, 20, 30, 30}}, {0, 2, 10, 8}, {0, 10, 10, 16})
      && check({{0, 5, 10, 15}}, {0, 0, 10, 10}, {0, 0, 10, 10})
      && check({{0, -5, 10, 5}}, {0, 0, 10, 10}, {0, 10, 10, 20})
      && check({{10, 0, 20, 10}, {0, 10, 10, 20}}, {0, 0, 10, 10}, {0, 0, 10, 10})
      && check({{-5, 0, 15, 10}, {-2, 12, 13, 22}, {0, 24, 10, 44}}, {0, 4, 10, 14}, {0, 44, 10, 54});
}

std::expected<bool, StarException> keybindingPreflight(Root& root) {
  auto config = Json::parseJson(R"JSON({
    "PlayerLeft": [{"type":"key","value":"A","mods":[]}],
    "PlayerRight": [{"type":"key","value":"A","mods":["LShift"]}],
    "PlayerUp": [{"type":"key","value":"A","mods":["LCtrl"]}],
    "PlayerDown": [{"type":"key","value":"A","mods":["LShift","LCtrl"]}],
    "PlayerJump": [{"type":"key","value":"A","mods":["LShift","LCtrl"]}]
  })JSON");
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto bindings = KeyBindings::create(root, *config);
  if (!bindings)
    return std::unexpected(std::move(bindings.error()));
  auto both = KeyMod::LShift | KeyMod::LCtrl;
  if (bindings->actions(Key::A) != Set<InterfaceAction>{InterfaceAction::PlayerLeft}
      || bindings->actions(KeyChord{Key::A, KeyMod::LShift}) != Set<InterfaceAction>{InterfaceAction::PlayerRight}
      || bindings->actions(KeyChord{Key::A, KeyMod::LCtrl}) != Set<InterfaceAction>{InterfaceAction::PlayerUp}
      || bindings->actions(InputEvent{KeyDownEvent{Key::A, both | KeyMod::LAlt}})
          != Set<InterfaceAction>{InterfaceAction::PlayerDown, InterfaceAction::PlayerJump}
      || !bindings->actions(InputEvent{KeyUpEvent{Key::A}}).empty()
      || !bindings->actions(Key::B).empty()
      || !bindings->actionsForKey(Key::B).empty()
      || bindings->actionsForKey(Key::A) != Set<InterfaceAction>{InterfaceAction::PlayerLeft,
          InterfaceAction::PlayerRight, InterfaceAction::PlayerUp, InterfaceAction::PlayerDown, InterfaceAction::PlayerJump})
    return false;
  auto tiedConfig = Json::parseJson(R"JSON({
    "PlayerRight": [{"type":"key","value":"A","mods":["LShift"]}],
    "PlayerUp": [{"type":"key","value":"A","mods":["LCtrl"]}]
  })JSON");
  if (!tiedConfig)
    return std::unexpected(std::move(tiedConfig.error()));
  auto tied = KeyBindings::create(root, *tiedConfig);
  if (!tied)
    return std::unexpected(std::move(tied.error()));
  return tied->actions(KeyChord{Key::A, both})
      == Set<InterfaceAction>{InterfaceAction::PlayerRight, InterfaceAction::PlayerUp};
}

struct BubbleFixture {
  List<RectF> boxes;
};

BubbleFixture bubbleFixture() {
  BubbleFixture fixture;
  for (int i = 0; i < 64; ++i) {
    float x = float((i % 8) * 9 - 32);
    float y = float((i / 8) * 8 - 12);
    fixture.boxes.append(RectF{x, y, x + 16 + (i % 3) * 8, y + 10 + (i % 4) * 6});
  }
  for (int i = 0; i < 16; ++i) {
    float x = float(i * 3 - 24);
    fixture.boxes.append(RectF{x, 0, x + 10 + (i % 3) * 5, 10.f + (i % 2) * 5});
  }
  return fixture;
}

void separateBubbles(benchmark::State& state, BubbleFixture const& fixture) {
  BubbleSeparator<int> separator(0.5f);
  int id = 0;
  for (auto const& box : fixture.boxes)
    separator.addBubble(box.center(), box, id++);
  for (auto _ : state) {
    (void)_;
    auto separated = separator.forEach([](BubbleState<int>&, int&) -> std::expected<void, StarException> {
      return {};
    });
    if (!separated) {
      state.SkipWithError(separated.error().what());
      return;
    }
    separator.update(1.0f / 60.0f);
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(fixture.boxes.size()));
}

void resolveGameplayChords(benchmark::State& state, KeyBindings const& bindings) {
  // Measure the production resolver, not a copy of ClientApplication's cache.
  state.SetLabel("Explicit representative fixture: 8 base bindings + 4 modified overrides; not client defaults");
  std::array<KeyChord, 12> chords{{
      {Key::W, KeyMod::NoMod}, {Key::A, KeyMod::NoMod}, {Key::S, KeyMod::NoMod}, {Key::D, KeyMod::NoMod},
      {Key::Space, KeyMod::NoMod}, {Key::E, KeyMod::NoMod}, {Key::Q, KeyMod::NoMod}, {Key::LShift, KeyMod::LShift},
      {Key::W, KeyMod::LShift}, {Key::A, KeyMod::LCtrl}, {Key::Space, KeyMod::LShift | KeyMod::LCtrl},
      {Key::E, KeyMod::LShift | KeyMod::LAlt}
  }};
  for (auto _ : state) {
    (void)_;
    for (auto chord : chords) {
      auto actions = bindings.actions(chord);
      benchmark::DoNotOptimize(actions);
    }
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(chords.size()));
}

}

int main(int argc, char** argv) {
  auto nativeRuntime = NativeRuntime::create();
  if (!nativeRuntime) {
    std::fprintf(stderr, "%s\n", nativeRuntime.error().what());
    return 1;
  }
  benchmark::Initialize(&argc, argv);
  BenchmarkLifetime benchmarkLifetime;
  auto loader = RootLoader::create({{}, {}, {}, LogLevel::Error, true, {}});
  if (!loader) {
    std::fprintf(stderr, "%s\n", loader.error().what());
    return 1;
  }
  auto parsed = loader->commandParseOrDie(argc, argv);
  if (!parsed) {
    std::fprintf(stderr, "%s\n", parsed.error().what());
    return 1;
  }
  auto& settings = parsed->first;
  if (auto storage = requirePrivateStorage(settings.storageDirectory); !storage) {
    std::fprintf(stderr, "%s\n", storage.error().what());
    return 1;
  }
  settings.runtimeConfigFile.reset();
  settings.logFile.reset();
  auto root = Root::create(std::move(settings));
  if (!root) {
    std::fprintf(stderr, "%s\n", root.error().what());
    return 1;
  }
  auto keysValid = keybindingPreflight(**root);
  if (!keysValid) {
    std::fprintf(stderr, "%s\n", keysValid.error().what());
    return 1;
  }
  if (!*keysValid || !bubblePreflight()) {
    std::fprintf(stderr, "Frontend semantic preflight failed\n");
    return 1;
  }
  // RootLoader's standalone defaults intentionally do not contain the client's
  // bindings. Keep this workload explicit and identical on old/new binaries.
  auto config = Json::parseJson(R"JSON({
    "PlayerUp": [{"type":"key","value":"W","mods":[]}],
    "PlayerLeft": [{"type":"key","value":"A","mods":[]}],
    "PlayerDown": [{"type":"key","value":"S","mods":[]}],
    "PlayerRight": [{"type":"key","value":"D","mods":[]}],
    "PlayerJump": [{"type":"key","value":"Space","mods":[]}],
    "PlayerInteract": [{"type":"key","value":"E","mods":[]}],
    "PlayerDropItem": [{"type":"key","value":"Q","mods":[]}],
    "PlayerShifting": [{"type":"key","value":"LShift","mods":[]}],
    "PlayerTechAction1": [{"type":"key","value":"W","mods":["LShift"]}],
    "PlayerTechAction2": [{"type":"key","value":"A","mods":["LCtrl"]}],
    "PlayerTechAction3": [{"type":"key","value":"Space","mods":["LShift","LCtrl"]}],
    "EmoteHappy": [{"type":"key","value":"E","mods":["LShift","LAlt"]}]
  })JSON");
  if (!config) {
    std::fprintf(stderr, "%s\n", config.error().what());
    return 1;
  }
  auto bindings = KeyBindings::create(**root, *config);
  if (!bindings) {
    std::fprintf(stderr, "%s\n", bindings.error().what());
    return 1;
  }
  auto bubbles = bubbleFixture();
  benchmark::RegisterBenchmark("ChatBubble/SeparateDenseMixedSizes", [&](benchmark::State& state) {
    separateBubbles(state, bubbles);
  })->MinTime(0.2)->UseRealTime();
  benchmark::RegisterBenchmark("KeyBindings/ResolveRepresentativeGameplayChords", [&](benchmark::State& state) {
    resolveGameplayChords(state, *bindings);
  })->MinTime(0.2)->UseRealTime();
  CheckedReporter reporter;
  auto matched = benchmark::RunSpecifiedBenchmarks(&reporter);
  benchmark::ClearRegisteredBenchmarks();
  return reporter.failed || matched == 0 ? 1 : 0;
}
