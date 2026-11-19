#include <cstdio>
#include <benchmark/benchmark.h>

import std;
import star.bi_map;
import star.hash;
import star.json;
import star.list;
import star.map;
import star.benchmark_runner;
import star.perlin;
import star.string;
import star.string_view;
import star.lua;
import star.lua_converters;
import star.vector;
import star.format;
import star.asset_path;
import star.text_formatter;
import star.a_star;
import star.encode;
import star.byte_array;
import star.curve25519;
import star.dynamic_lib;

namespace DynamicLibBenchmarks {

using Star::DynamicLib;
using Star::String;

enum class Operation { Open, Reopen, Find };

static std::expected<int, Star::StarException> lookup_and_call(DynamicLib const& library, String const& name) {
  auto symbol = library.find<int(int)>(name);
  if (!symbol)
    return std::unexpected(std::move(symbol.error()));
  return (*symbol)(7);
}

static void measure(benchmark::State& state, Operation operation) {
  String const path(STAR_DYNAMIC_LIB_FIXTURE);
  String const name("asterbound_fixture_transform");
  // Check the same exported ABI and result before timing either implementation.
  auto resident = DynamicLib::open(path);
  if (!resident) {
    state.SkipWithError(resident.error().what());
    return;
  }
  auto result = lookup_and_call(*resident, name);
  if (!result || *result != 22) {
    state.SkipWithError(result ? "unexpected fixture result" : result.error().what());
    return;
  }
  if (operation == Operation::Open) {
    // Release the validation reference before measuring open/last-close.
    auto release = std::move(*resident);
  }
  for (auto _ : state) {
    if (operation == Operation::Find) {
      auto value = lookup_and_call(*resident, name);
      if (!value || *value != 22) {
        state.SkipWithError(value ? "unexpected fixture result" : value.error().what());
        break;
      }
    } else {
      auto library = DynamicLib::open(path);
      if (!library) {
        state.SkipWithError(library.error().what());
        break;
      }
    }
  }
  state.SetItemsProcessed(state.iterations());
}

BENCHMARK_CAPTURE(measure, Open, Operation::Open)->Name("Core/DynamicLib/OpenClose")->UseRealTime();
BENCHMARK_CAPTURE(measure, Reopen, Operation::Reopen)->Name("Core/DynamicLib/ResidentOpenClose")->UseRealTime();
BENCHMARK_CAPTURE(measure, Find, Operation::Find)->Name("Core/DynamicLib/FindAndCall")->UseRealTime();

} // namespace DynamicLibBenchmarks

namespace Ed25519Benchmarks {

struct Vector {
  std::string_view seed;
  std::string_view public_key;
  std::string_view message;
  std::string_view signature;
};

// RFC 8032 section 7.1: independent complete key/signature oracles.
constexpr std::array vectors{
    Vector{"9d61b19deffd5a60ba844af492ec2cc4" "4449c5697b326919703bac031cae7f60",
           "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a", "",
           "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e06522490155"
           "5fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b"},
    Vector{"4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb",
           "3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c", "72",
           "92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da"
           "085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00"},
    Vector{"833fe62409237b9d62ec77587520911e9a759cec1d19755b7da901b96dca3d42",
           "ec172b93ad5e563bf4932c70e1245034c35467ef2efd4d64ebf819683467e2bf",
           "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
           "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f",
           "dc2a4459e7369633a52b1bf277839a00201009a3efbf3ecb69bea2186c26b589"
           "09351fc9ac90b3ecfdfbc7c66431e0303dca179c138ac17ad9bef1177331a704"}};

template <std::size_t extent>
std::array<std::uint8_t, extent> decode_fixed(std::string_view text) {
  if (text.size() != extent * 2)
    Star::fatal_error("Invalid Ed25519 benchmark hex length", false);
  auto bytes = Star::hex_decode(text);
  if (bytes.size() != extent)
    Star::fatal_error("Invalid Ed25519 benchmark vector", false);
  std::array<std::uint8_t, extent> result;
  std::ranges::copy(bytes, result.begin());
  return result;
}

// Only these adapters differ from the frozen original native implementation;
// fixtures, full-result validation and measured operations stay identical.
class Identity {
public:
  explicit Identity(std::span<std::uint8_t const, 32> seed) : m_key(make_key(seed)) {}
  auto const& public_key() const { return m_key.public_key(); }
  auto sign(std::span<char const> message) const { return m_key.sign(message); }
private:
  static Star::Curve25519::KeySet make_key(std::span<std::uint8_t const, 32> seed) {
    auto key = Star::Curve25519::KeySet::from_seed(seed);
    if (!key)
      Star::fatal_error(key.error().what(), false);
    return std::move(*key);
  }
  Star::Curve25519::KeySet m_key;
};

class Verifier {
public:
  explicit Verifier(std::span<std::uint8_t const, 32> public_key) : m_key(make_key(public_key)) {}
  bool verify(std::span<std::uint8_t const, 64> signature, std::span<char const> message) const {
    return m_key.verify(signature, message);
  }
private:
  static Star::Curve25519::Verifier make_key(std::span<std::uint8_t const, 32> public_key) {
    auto key = Star::Curve25519::Verifier::create(public_key);
    if (!key)
      Star::fatal_error(key.error().what(), false);
    return std::move(*key);
  }
  Star::Curve25519::Verifier m_key;
};

bool verify(std::span<std::uint8_t const, 64> signature, std::span<std::uint8_t const, 32> public_key,
            std::span<char const> message) {
  return Star::Curve25519::verify(signature, public_key, message);
}

enum class Operation { Keygen, Sign, Verify, ReusedVerifier };

void measure(benchmark::State& state, Operation operation) {
  auto const& vector = vectors[state.range(0)];
  auto seed = decode_fixed<32>(vector.seed);
  auto public_key = decode_fixed<32>(vector.public_key);
  auto signature = decode_fixed<64>(vector.signature);
  auto message = Star::hex_decode(vector.message);
  Identity identity(seed);
  Verifier verifier(public_key);
  if (identity.public_key() != public_key || identity.sign(message.span()) != signature
      || !verify(signature, public_key, message.span()) || !verifier.verify(signature, message.span())) {
    state.SkipWithError("Ed25519 differs from independent RFC 8032 vector");
    return;
  }
  auto corrupt = signature;
  corrupt[0] ^= 1;
  if (verify(corrupt, public_key, message.span()) || verifier.verify(corrupt, message.span())) {
    state.SkipWithError("Ed25519 accepts a corrupted RFC signature");
    return;
  }
  state.SetLabel(std::format("RFC8032/7.1; seed={}; pk={}; signature={}; message={}; operations/batch=1; deterministic seed, not OS RNG",
      vector.seed, vector.public_key, vector.signature, vector.message));
  for (auto _ : state) {
    (void)_;
    if (operation == Operation::Keygen) {
      Identity generated(seed);
      benchmark::DoNotOptimize(generated.public_key());
    } else if (operation == Operation::Sign) {
      auto result = identity.sign(message.span());
      benchmark::DoNotOptimize(result);
    } else {
      auto result = operation == Operation::Verify ? verify(signature, public_key, message.span())
                                                 : verifier.verify(signature, message.span());
      benchmark::DoNotOptimize(result);
    }
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations());
}

BENCHMARK_CAPTURE(measure, Keygen, Operation::Keygen)->Name("Core/Ed25519/Keygen")->Arg(0)->Arg(1)->Arg(2);
BENCHMARK_CAPTURE(measure, Sign, Operation::Sign)->Name("Core/Ed25519/Sign")->Arg(0)->Arg(1)->Arg(2);
BENCHMARK_CAPTURE(measure, Verify, Operation::Verify)->Name("Core/Ed25519/Verify")->Arg(0)->Arg(1)->Arg(2);
BENCHMARK_CAPTURE(measure, ReusedVerifier, Operation::ReusedVerifier)->Name("Core/Ed25519/ReusedVerifier")->Arg(0)->Arg(1)->Arg(2);
}

namespace AStarBenchmarks {

struct Edge {
  double cost;
  unsigned source;
  unsigned target;

  friend bool operator==(Edge const&, Edge const&) = default;
};

using Search = Star::AStar::Search<Edge, unsigned>;

enum class Shape { Chain, Tree, UnreachableBest };

struct Fixture {
  std::vector<std::vector<Edge>> adjacency;
  std::vector<Edge> expected;
  unsigned goal;
  bool success;
  bool return_best;
};

void fingerprint(std::uint64_t& value, std::uint64_t word) {
  for (unsigned shift = 0; shift != 64; shift += 8) {
    value ^= (word >> shift) & 255;
    value *= 1099511628211ULL;
  }
}

void fingerprint_edge(std::uint64_t& value, Edge const& edge) {
  fingerprint(value, std::bit_cast<std::uint64_t>(edge.cost));
  fingerprint(value, edge.source);
  fingerprint(value, edge.target);
}

Fixture make_fixture(Shape shape, unsigned node_count) {
  Fixture fixture{std::vector<std::vector<Edge>>(node_count),
                  {},
                  shape == Shape::UnreachableBest ? node_count + 17 : node_count - 1,
                  shape != Shape::UnreachableBest,
                  shape == Shape::UnreachableBest};
  for (unsigned node = 0; node < node_count; ++node) {
    if (shape == Shape::Chain) {
      if (node + 1 < node_count)
        fixture.adjacency[node].push_back({1.0, node, node + 1});
    } else {
      for (unsigned child : {2 * node + 1, 2 * node + 2}) {
        if (child < node_count)
          fixture.adjacency[node].push_back({1.0, node, child});
      }
    }
  }
  // A chain has exactly one path. A heap-indexed directed tree has exactly
  // one parent per child: the oracle is arithmetic, not a second graph search.
  // UnreachableBest's |goal-node| heuristic uniquely selects the largest leaf.
  for (unsigned node = node_count - 1; node != 0;) {
    unsigned parent = shape == Shape::Chain ? node - 1 : (node - 1) / 2;
    fixture.expected.push_back({1.0, parent, node});
    node = parent;
  }
  std::ranges::reverse(fixture.expected);
  return fixture;
}

Search make_search(Fixture const& fixture) {
  return Search(
    [fixture = std::cref(fixture)](unsigned node, unsigned goal) -> std::expected<double, Star::StarException> {
      return fixture.get().return_best ? static_cast<double>(goal - node) : 0.0;
    },
    [fixture = std::cref(fixture)](unsigned node,
                                   Star::List<Edge>& neighbors) -> std::expected<void, Star::StarException> {
      for (auto const& edge : fixture.get().adjacency[node])
        neighbors.push_back(edge);
      return {};
    },
    [goal = fixture.goal](unsigned node) -> std::expected<bool, Star::StarException> { return node == goal; },
    fixture.return_best);
}

std::expected<bool, Star::StarException> run_search(Search& search, Fixture const& fixture, unsigned budget) {
  auto started = search.start(0, fixture.goal);
  if (!started)
    return std::unexpected(std::move(started.error()));
  // Unique-parent graphs enqueue at most node_count candidates. This bound
  // catches a stalled incremental search without timing an unbounded loop.
  for (std::size_t call = 0; call < fixture.adjacency.size() + 2; ++call) {
    auto status = search.explore(budget ? std::optional<unsigned>(budget) : std::nullopt);
    if (!status)
      return std::unexpected(std::move(status.error()));
    if (status->has_value())
      return **status;
  }
  return std::unexpected(Star::StarException("AStar benchmark exceeded its bounded exploration calls"));
}

void record_diagnostics() {
  auto enabled = std::getenv("ASTERBOUND_ASTAR_DIAGNOSTICS");
  if (!enabled || std::string_view(enabled) != "1")
    return;
  // Opt-in, once per process, before State timing. These deliberately broken
  // old domains are observations, never equivalent performance/oracle rows.
  static bool const recorded = [] {
    auto print_outcome = [](char const* name, std::expected<std::optional<bool>, Star::StarException> const& status,
                            Search const& search) {
      if (!status) {
        std::fprintf(stderr, "AStar diagnostic %s: error=%s\n", name, status.error().what());
        return;
      }
      std::fprintf(stderr, "AStar diagnostic %s: status=%s; path=", name,
                   !status->has_value() ? "pending"
                     : **status         ? "true"
                                        : "false");
      auto const& path = search.result();
      if (!path) {
        std::fprintf(stderr, "none\n");
        return;
      }
      double cost = 0.0;
      for (auto const& edge : *path) {
        std::fprintf(stderr, "[%u>%u:%.17g]", edge.source, edge.target, edge.cost);
        cost += edge.cost;
      }
      std::fprintf(stderr, "; edges=%zu; cost=%.17g\n", path->size(), cost);
    };
    Fixture rediscovery{{{{100.0, 0, 1}, {1.0, 0, 2}}, {{1.0, 1, 3}}, {{1.0, 2, 1}}, {}}, {}, 3, true, false};
    for (bool pruned : {false, true}) {
      // Construct explicitly to change only maxFScore, not the graph/callbacks.
      Search search([](unsigned, unsigned) -> std::expected<double, Star::StarException> { return 0.0; },
                    [fixture = std::cref(rediscovery)](
                      unsigned node, Star::List<Edge>& neighbors) -> std::expected<void, Star::StarException> {
                      for (auto const& edge : fixture.get().adjacency[node])
                        neighbors.push_back(edge);
                      return {};
                    },
                    [](unsigned node) -> std::expected<bool, Star::StarException> { return node == 3; }, false, {},
                    pruned ? std::optional<double>(3.0) : std::nullopt);
      auto started = search.start(0, 3);
      if (!started)
        std::fprintf(stderr, "AStar diagnostic start: error=%s\n", started.error().what());
      else
        print_outcome(pruned ? "pruned_rediscovery" : "cheaper_rediscovery", search.explore(), search);
    }
    auto partial_fixture = make_fixture(Shape::UnreachableBest, 3);
    auto partial = make_search(partial_fixture);
    auto started = partial.start(0, partial_fixture.goal);
    if (!started)
      std::fprintf(stderr, "AStar diagnostic start: error=%s\n", started.error().what());
    else {
      print_outcome("partial_first", partial.explore(), partial);
      print_outcome("partial_repeat", partial.explore(), partial);
    }
    return true;
  }();
  (void)recorded;
}

bool validate(benchmark::State& state, Fixture const& fixture, unsigned budget, unsigned restarts,
              std::uint64_t& input_fingerprint, std::uint64_t& output_fingerprint) {
  fingerprint(input_fingerprint, fixture.adjacency.size());
  fingerprint(input_fingerprint, fixture.goal);
  fingerprint(input_fingerprint, fixture.success);
  fingerprint(input_fingerprint, fixture.return_best);
  fingerprint(input_fingerprint, budget);
  fingerprint(input_fingerprint, restarts);
  for (auto const& neighbors : fixture.adjacency) {
    fingerprint(input_fingerprint, neighbors.size());
    for (auto const& edge : neighbors)
      fingerprint_edge(input_fingerprint, edge);
  }
  auto search = make_search(fixture);
  for (unsigned restart = 0; restart < restarts; ++restart) {
    auto status = run_search(search, fixture, budget);
    if (!status) {
      state.SkipWithError(status.error().what());
      return false;
    }
    auto const& path = search.result();
    if (*status != fixture.success || !path || path->size() != fixture.expected.size()) {
      state.SkipWithError("AStar status/path size differs from independent unique-parent oracle");
      return false;
    }
    fingerprint(output_fingerprint, *status);
    fingerprint(output_fingerprint, path.has_value());
    fingerprint(output_fingerprint, path->size());
    for (std::size_t index = 0; index < path->size(); ++index) {
      if ((*path)[index] != fixture.expected[index]) {
        state.SkipWithError("AStar complete path differs from independent unique-parent oracle");
        return false;
      }
      fingerprint_edge(output_fingerprint, (*path)[index]);
    }
    // Deliberately stop at the first terminal answer: the old returnBest
    // terminal-repeat bug is a regression-test domain, not comparable timing.
  }
  return true;
}

void benchmark_astar(benchmark::State& state, Shape shape, unsigned node_count, unsigned budget, unsigned restarts) {
  record_diagnostics();
  auto fixture = make_fixture(shape, node_count);
  std::uint64_t input_fingerprint = 14695981039346656037ULL;
  std::uint64_t output_fingerprint = 14695981039346656037ULL;
  if (!validate(state, fixture, budget, restarts, input_fingerprint, output_fingerprint))
    return;
  state.SetLabel(std::format("nodes={}; searches/batch={}; budget={} (0=unlimited); input={:016x}; output={:016x}",
                             node_count, restarts, budget, input_fingerprint, output_fingerprint));
  // Fixture/oracle construction and complete checks are untimed. One timed
  // batch owns one Search, restarts it 1 or 4 times, and destroys it (including
  // its final result). Earlier results are destroyed by the next start().
  // Thus restart rows amortize callback construction but do not leak the last
  // result's destruction outside timing. No result copies/extraction are used.
  for (auto _ : state) {
    (void)_;
    auto search = make_search(fixture);
    for (unsigned restart = 0; restart < restarts; ++restart) {
      auto status = run_search(search, fixture, budget);
      if (!status) {
        state.SkipWithError(status.error().what());
        return;
      }
      benchmark::DoNotOptimize(*status);
      benchmark::DoNotOptimize(search.result());
    }
  }
  std::uint64_t checked_input = 14695981039346656037ULL;
  std::uint64_t checked_output = 14695981039346656037ULL;
  if (validate(state, fixture, budget, restarts, checked_input, checked_output)
      && (checked_input != input_fingerprint || checked_output != output_fingerprint))
    state.SkipWithError("AStar fixture or complete output fingerprint changed after timing");
  state.SetItemsProcessed(state.iterations() * restarts);
}

BENCHMARK_CAPTURE(benchmark_astar, Chain128, Shape::Chain, 128, 0, 1)
  ->Name("Core/AStar/chain128")
  ->UseRealTime()
  ->Unit(benchmark::kMicrosecond);
BENCHMARK_CAPTURE(benchmark_astar, Chain1024, Shape::Chain, 1024, 0, 1)
  ->Name("Core/AStar/chain1024")
  ->UseRealTime()
  ->Unit(benchmark::kMicrosecond);
BENCHMARK_CAPTURE(benchmark_astar, Tree255, Shape::Tree, 255, 0, 1)
  ->Name("Core/AStar/branch255")
  ->UseRealTime()
  ->Unit(benchmark::kMicrosecond);
BENCHMARK_CAPTURE(benchmark_astar, Best255, Shape::UnreachableBest, 255, 0, 1)
  ->Name("Core/AStar/unreachable_best255")
  ->UseRealTime()
  ->Unit(benchmark::kMicrosecond);
BENCHMARK_CAPTURE(benchmark_astar, Chain128Restart, Shape::Chain, 128, 1, 4)
  ->Name("Core/AStar/chain128_restart4_budget1")
  ->UseRealTime()
  ->Unit(benchmark::kMicrosecond);
BENCHMARK_CAPTURE(benchmark_astar, Chain1024Restart, Shape::Chain, 1024, 8, 4)
  ->Name("Core/AStar/chain1024_restart4_budget8")
  ->UseRealTime()
  ->Unit(benchmark::kMicrosecond);
BENCHMARK_CAPTURE(benchmark_astar, Tree255Restart, Shape::Tree, 255, 8, 4)
  ->Name("Core/AStar/branch255_restart4_budget8")
  ->UseRealTime()
  ->Unit(benchmark::kMicrosecond);
BENCHMARK_CAPTURE(benchmark_astar, Best255Restart, Shape::UnreachableBest, 255, 8, 4)
  ->Name("Core/AStar/unreachable_best255_restart4_budget8")
  ->UseRealTime()
  ->Unit(benchmark::kMicrosecond);

}// namespace AStarBenchmarks

namespace {

namespace AssetPathBenchmarks {

using Star::String;

constexpr std::size_t batch_size = 64;

enum class Operation {
  Split,
  Construct,
  Join,
  Filename,
  Extension,
  Directory,
  SetSubPath,
  RemoveSubPath,
  AddDirectives,
  RelativeTo,
  WriteText
};

struct Fixture {
  String m_input;
  String m_base;
  String m_replacement;
  String m_added_directives;
  String m_relative;
  Star::AssetPath m_locator;
  std::string m_original;
  std::string m_canonical;
  std::string m_original_subpath;
  std::array<std::string, 11> m_expected;
};

std::vector<Fixture> make_fixtures(std::size_t directory_bytes) {
  std::vector<Fixture> fixtures;
  fixtures.reserve(batch_size);
  for (std::size_t index = 0; index < batch_size; ++index) {
    std::string directory = "/";
    while (directory.size() + std::string_view("星/").size() <= directory_bytes)
      directory += "星/";
    while (directory.size() < directory_bytes)
      directory += '/';
    auto filename = std::format("image{:02}.png", index);
    auto base = directory + filename;
    auto subpath = std::format("frame:{}:星", index);
    std::string directives = "?flipx?multiply=ffffff";
    auto original = base + ":" + subpath + "??flipx???multiply=ffffff?";
    auto canonical = base + ":" + subpath + directives;
    auto replacement = std::format("other:{}:月", index);
    std::string added = "?flipy";
    auto relative = std::format("next{:02}.png:idle??flipx", index);
    Fixture fixture{String(original),
                    String(base),
                    String(replacement),
                    String(added),
                    String(relative),
                    Star::AssetPath::split(String(original)),
                    original,
                    canonical,
                    subpath,
                    {canonical, canonical, canonical, filename, "png", directory, base + ":" + replacement + directives,
                     base + directives, original + "flipy", directory + relative, canonical}};
    fixtures.push_back(std::move(fixture));
  }
  return fixtures;
}

template <Operation operation>
concept asset_path_operation = operation >= Operation::Split && operation <= Operation::WriteText;

template <Operation operation>
  requires asset_path_operation<operation>
auto execute(Fixture const& fixture) {
  using Star::AssetPath;
  if constexpr (operation == Operation::Split)
    return AssetPath::split(fixture.m_input);
  else if constexpr (operation == Operation::Construct)
    return AssetPath(std::string_view(fixture.m_input.utf8()));
  else if constexpr (operation == Operation::Join)
    return AssetPath::join(fixture.m_locator);
  else if constexpr (operation == Operation::Filename)
    return AssetPath::filename(fixture.m_base);
  else if constexpr (operation == Operation::Extension)
    return AssetPath::extension(fixture.m_base);
  else if constexpr (operation == Operation::Directory)
    return AssetPath::directory(fixture.m_input);
  else if constexpr (operation == Operation::SetSubPath)
    return AssetPath::set_sub_path(fixture.m_input, fixture.m_replacement);
  else if constexpr (operation == Operation::RemoveSubPath)
    return AssetPath::remove_sub_path(fixture.m_input);
  else if constexpr (operation == Operation::AddDirectives)
    return AssetPath::add_directives(fixture.m_input, fixture.m_added_directives);
  else if constexpr (operation == Operation::RelativeTo)
    return AssetPath::relative_to(fixture.m_input, fixture.m_relative);
  else {
    Star::TextBuffer output;
    Star::write_text(output, fixture.m_locator);
    return output;
  }
}

// Stable byte fingerprints, not std::hash: labels remain comparable across builds.
void fingerprint(std::uint64_t& value, std::string_view text) {
  for (unsigned char byte : text) {
    value ^= byte;
    value *= 1099511628211ULL;
  }
  value ^= 255;
  value *= 1099511628211ULL;
}

template <Operation operation>
  requires asset_path_operation<operation>
bool validate(benchmark::State& state, std::vector<Fixture> const& fixtures, std::uint64_t& fixture_fingerprint,
              std::uint64_t& result_fingerprint) {
  for (auto const& fixture : fixtures) {
    auto result = execute<operation>(fixture);
    auto text = [&]() -> std::string {
      if constexpr (operation == Operation::Split || operation == Operation::Construct) {
        if (result.m_base_path != fixture.m_base || !result.m_sub_path
            || result.m_sub_path->utf8() != fixture.m_original_subpath)
          return {};
        return Star::AssetPath::join(result).utf8();
      } else if constexpr (operation == Operation::WriteText)
        return std::string(result.view());
      else
        return result.utf8();
    }();
    if (text != fixture.m_expected[static_cast<std::size_t>(operation)] || fixture.m_input.utf8() != fixture.m_original
        || Star::AssetPath::join(fixture.m_locator).utf8() != fixture.m_canonical) {
      state.SkipWithError("AssetPath output or original fixture differs from independent oracle");
      return false;
    }
    fingerprint(fixture_fingerprint, fixture.m_original);
    fingerprint(fixture_fingerprint, fixture.m_base.utf8());
    fingerprint(fixture_fingerprint, fixture.m_replacement.utf8());
    fingerprint(fixture_fingerprint, fixture.m_added_directives.utf8());
    fingerprint(fixture_fingerprint, fixture.m_relative.utf8());
    fingerprint(fixture_fingerprint, fixture.m_canonical);
    fingerprint(result_fingerprint, text);
  }
  return true;
}

template <Operation operation>
  requires asset_path_operation<operation>
void benchmark_asset_path(benchmark::State& state) {
  auto fixtures = make_fixtures(static_cast<std::size_t>(state.range(0)));
  std::uint64_t fixture_fingerprint = 14695981039346656037ULL;
  std::uint64_t result_fingerprint = 14695981039346656037ULL;
  if (!validate<operation>(state, fixtures, fixture_fingerprint, result_fingerprint))
    return;
  state.SetLabel(std::format("batch=64; directory_bytes={}; fixture={:016x}; result={:016x}", state.range(0),
                             fixture_fingerprint, result_fingerprint));
  // Fixtures, independent oracles and full validation are untimed. Each result
  // is destroyed inside the timed batch; join/write_text use prebuilt locators.
  for (auto _ : state) {
    (void)_;
    for (auto const& fixture : fixtures) {
      benchmark::DoNotOptimize(fixture);
      auto result = execute<operation>(fixture);
      benchmark::DoNotOptimize(result);
    }
  }
  fixture_fingerprint = 14695981039346656037ULL;
  result_fingerprint = 14695981039346656037ULL;
  validate<operation>(state, fixtures, fixture_fingerprint, result_fingerprint);
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(batch_size));
}

BENCHMARK_TEMPLATE(benchmark_asset_path, Operation::Split)->Name("Core/AssetPath/split")->Arg(32)->Arg(1024);
BENCHMARK_TEMPLATE(benchmark_asset_path, Operation::Construct)
  ->Name("Core/AssetPath/constructor_string_view")
  ->Arg(32)
  ->Arg(1024);
BENCHMARK_TEMPLATE(benchmark_asset_path, Operation::Join)->Name("Core/AssetPath/join")->Arg(32)->Arg(1024);
BENCHMARK_TEMPLATE(benchmark_asset_path, Operation::Filename)
  ->Name("Core/AssetPath/filename_base_only")
  ->Arg(32)
  ->Arg(1024);
BENCHMARK_TEMPLATE(benchmark_asset_path, Operation::Extension)
  ->Name("Core/AssetPath/extension_base_only")
  ->Arg(32)
  ->Arg(1024);
BENCHMARK_TEMPLATE(benchmark_asset_path, Operation::Directory)->Name("Core/AssetPath/directory")->Arg(32)->Arg(1024);
BENCHMARK_TEMPLATE(benchmark_asset_path, Operation::SetSubPath)->Name("Core/AssetPath/setSubPath")->Arg(32)->Arg(1024);
BENCHMARK_TEMPLATE(benchmark_asset_path, Operation::RemoveSubPath)
  ->Name("Core/AssetPath/removeSubPath")
  ->Arg(32)
  ->Arg(1024);
BENCHMARK_TEMPLATE(benchmark_asset_path, Operation::AddDirectives)
  ->Name("Core/AssetPath/addDirectives")
  ->Arg(32)
  ->Arg(1024);
BENCHMARK_TEMPLATE(benchmark_asset_path, Operation::RelativeTo)->Name("Core/AssetPath/relative_to")->Arg(32)->Arg(1024);
BENCHMARK_TEMPLATE(benchmark_asset_path, Operation::WriteText)->Name("Core/AssetPath/write_text")->Arg(32)->Arg(1024);

}// namespace AssetPathBenchmarks

using Star::EnumMap;
using Star::Json;
using Star::JsonArray;
using Star::JsonObject;
using Star::PerlinType;
using Star::String;

// Fixture construction and semantic validation precede the State iteration
// range, so Google Benchmark excludes them from timing. Only the explicitly
// named ConstructDestroy case times table construction and destruction.
constexpr auto FixedPerlinNames = Star::sortedEnumNames(std::array{
    std::pair{PerlinType::Uninitialized, std::string_view("uninitialized")},
    std::pair{PerlinType::Perlin, std::string_view("perlin")},
    std::pair{PerlinType::Billow, std::string_view("billow")},
    std::pair{PerlinType::RidgedMulti, std::string_view("ridgedMulti")},
});

auto makePerlinMap() {
  std::array<std::pair<PerlinType, String>, FixedPerlinNames.size()> entries;
  for (std::size_t i = 0; i < entries.size(); ++i)
    entries[i] = {FixedPerlinNames[i].first, String(FixedPerlinNames[i].second)};
  return EnumMap<PerlinType>::from(entries);
}

// A borrowed optional array access candidate, not a replacement enumName API:
// enumName is checked and constructs a diagnostic on a missing enum value.
std::optional<std::string_view> arrayPerlinName(PerlinType value) {
  auto found = std::lower_bound(FixedPerlinNames.begin(), FixedPerlinNames.end(), value,
      [](auto const& pair, auto key) { return pair.first < key; });
  if (found == FixedPerlinNames.end() || found->first != value)
    return std::nullopt;
  return found->second;
}

struct PerlinQueries {
  std::array<PerlinType, 4> hits{
      PerlinType::Uninitialized, PerlinType::Perlin, PerlinType::Billow, PerlinType::RidgedMulti};
  std::array<PerlinType, 4> misses{
      static_cast<PerlinType>(4), static_cast<PerlinType>(5),
      static_cast<PerlinType>(6), static_cast<PerlinType>(7)};
  std::array<String, 12> reverseHits{
      "uninitialized", "perlin", "billow", "ridgedMulti",
      "UNINITIALIZED", "PERLIN", "BILLOW", "RIDGEDMULTI",
      "uNiNiTiAlIzEd", "pErLiN", "bIlLoW", "rIdGeDmUlTi"};
  std::array<String, 4> reverseMisses{"", "perlin-extra", "ridgedMult", "日本語"};
};

bool validatePerlin(benchmark::State& state, EnumMap<PerlinType> const& dynamic, PerlinQueries const& queries) {
  if (dynamic.size() != FixedPerlinNames.size()) {
    state.SkipWithError("Perlin table size differs from the four-entry candidate");
    return false;
  }
  for (auto const& [value, spelling] : FixedPerlinNames) {
    auto dynamicOptional = dynamic.maybe_right(value);
    auto dynamicChecked = dynamic.get_right(value);
    auto fixedChecked = Star::enumName<FixedPerlinNames>(value);
    auto fixedOptional = arrayPerlinName(value);
    if (!dynamicOptional || !dynamicChecked || !fixedChecked || !fixedOptional
        || dynamicOptional->utf8() != spelling || dynamicChecked->get().utf8() != spelling
        || *fixedChecked != spelling || *fixedOptional != spelling) {
      state.SkipWithError("Perlin forward candidates disagree");
      return false;
    }
  }
  for (auto value : queries.misses) {
    if (dynamic.maybe_right(value) || arrayPerlinName(value)) {
      state.SkipWithError("Perlin optional forward miss unexpectedly matched");
      return false;
    }
  }
  for (std::size_t i = 0; i < queries.reverseHits.size(); ++i) {
    auto dynamicValue = dynamic.maybe_left(queries.reverseHits[i]);
    auto fixedValue = Star::maybeEnumValue<FixedPerlinNames>(Star::StringView(queries.reverseHits[i]));
    auto expected = queries.hits[i % queries.hits.size()];
    if (!dynamicValue || !fixedValue || !*dynamicValue || !*fixedValue
        || **dynamicValue != expected || **fixedValue != expected) {
      state.SkipWithError("Perlin reverse candidates disagree on case-insensitive hits");
      return false;
    }
  }
  for (auto const& text : queries.reverseMisses) {
    auto dynamicValue = dynamic.maybe_left(text);
    auto fixedValue = Star::maybeEnumValue<FixedPerlinNames>(Star::StringView(text));
    if (!dynamicValue || !fixedValue || *dynamicValue || *fixedValue) {
      state.SkipWithError("Perlin reverse optional miss returned an error or a match");
      return false;
    }
  }
  return true;
}

template <bool Fixed, bool Hit>
void BM_PerlinForwardOptional(benchmark::State& state) {
  auto fixture = makePerlinMap();
  if (!fixture) {
    state.SkipWithError("Perlin EnumMap fixture construction failed");
    return;
  }
  auto const& dynamic = *fixture;
  PerlinQueries queries;
  if (!validatePerlin(state, dynamic, queries))
    return;
  auto const& values = Hit ? queries.hits : queries.misses;
  state.SetLabel(Fixed ? "borrowed string_view optional; prebuilt array"
                       : "owning String optional; existing EnumMap");
  for (auto _ : state) {
    (void)_;
    for (auto value : values) {
      benchmark::DoNotOptimize(value);
      if constexpr (Fixed) {
        auto result = arrayPerlinName(value);
        benchmark::DoNotOptimize(result);
      } else {
        auto result = dynamic.maybe_right(value);
        benchmark::DoNotOptimize(result);
      }
    }
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(values.size()));
}

template <bool Fixed>
void BM_PerlinForwardCheckedHit(benchmark::State& state) {
  auto fixture = makePerlinMap();
  if (!fixture) {
    state.SkipWithError("Perlin EnumMap fixture construction failed");
    return;
  }
  auto const& dynamic = *fixture;
  PerlinQueries queries;
  if (!validatePerlin(state, dynamic, queries))
    return;
  state.SetLabel(Fixed ? "checked borrowed string_view; enumName helper"
                       : "checked borrowed String reference; EnumMap::get_right");
  for (auto _ : state) {
    (void)_;
    for (auto value : queries.hits) {
      benchmark::DoNotOptimize(value);
      if constexpr (Fixed) {
        auto result = Star::enumName<FixedPerlinNames>(value);
        if (!result) {
          state.SkipWithError("enumName failed for a validated hit");
          return;
        }
        benchmark::DoNotOptimize(*result);
      } else {
        auto result = dynamic.get_right(value);
        if (!result) {
          state.SkipWithError("EnumMap::get_right failed for a validated hit");
          return;
        }
        benchmark::DoNotOptimize(result->get().utf8());
      }
    }
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(queries.hits.size()));
}

template <bool Fixed, bool Hit>
void BM_PerlinReverseOptional(benchmark::State& state) {
  auto fixture = makePerlinMap();
  if (!fixture) {
    state.SkipWithError("Perlin EnumMap fixture construction failed");
    return;
  }
  auto const& dynamic = *fixture;
  PerlinQueries queries;
  if (!validatePerlin(state, dynamic, queries))
    return;
  auto const& texts = [&]() -> auto const& {
    if constexpr (Hit)
      return queries.reverseHits;
    else
      return queries.reverseMisses;
  }();
  state.SetLabel(Hit ? "prebuilt String queries; canonical/upper/mixed ASCII case"
                     : "prebuilt String queries; successful empty optionals, no diagnostics");
  for (auto _ : state) {
    (void)_;
    for (auto const& text : texts) {
      benchmark::DoNotOptimize(text);
      auto result = [&] {
        if constexpr (Fixed)
          return Star::maybeEnumValue<FixedPerlinNames>(Star::StringView(text));
        else
          return dynamic.maybe_left(text);
      }();
      if (!result) {
        state.SkipWithError("Perlin reverse lookup failed for validated UTF-8");
        return;
      }
      benchmark::DoNotOptimize(*result);
    }
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(texts.size()));
}

void BM_PerlinDynamicConstructDestroy(benchmark::State& state) {
  std::array<std::pair<PerlinType, String>, 4> entries;
  for (std::size_t i = 0; i < entries.size(); ++i)
    entries[i] = {FixedPerlinNames[i].first, String(std::string(FixedPerlinNames[i].second))};
  auto validation = EnumMap<PerlinType>::from(entries);
  PerlinQueries queries;
  if (!validation || !validatePerlin(state, *validation, queries)) {
    state.SkipWithError("EnumMap construction validation failed");
    return;
  }
  state.SetLabel("EnumMap::from prebuilt owning Strings + destruction; NOT process startup");
  for (auto _ : state) {
    (void)_;
    auto result = EnumMap<PerlinType>::from(entries);
    if (!result) {
      state.SkipWithError("EnumMap::from failed for validated unique entries");
      return;
    }
    benchmark::DoNotOptimize(*result);
  }
  state.SetItemsProcessed(state.iterations());
}

void measure_bimap_move(benchmark::State& state) {
  Star::BiMap<int, int> table;
  for (int i = 0; i < 128; ++i)
    table.insert(i, i + 1);
  for (auto _ : state) {
    Star::BiMap<int, int> moved(std::move(table));
    benchmark::DoNotOptimize(moved);
    table = std::move(moved);
    benchmark::DoNotOptimize(table);
  }
  if (table.size() != 128 || table.maybe_left(128) != 127)
    state.SkipWithError("Move lost a mapping");
}

void measure_bimap_conflict(benchmark::State& state, bool left_conflict) {
  Star::BiMap<int, int> table;
  for (int i = 0; i < 128; ++i)
    table.insert(i, i + 1);
  for (auto _ : state) {
    auto result = left_conflict ? table.insert({64, 200}) : table.insert({200, 65});
    if (result.second || (*result.first).first != 64) {
      state.SkipWithError("Duplicate returned the wrong mapping");
      break;
    }
    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK(measure_bimap_move)->Name("Core/BiMap/MoveRoundTrip128");
BENCHMARK_CAPTURE(measure_bimap_conflict, Left, true)->Name("Core/BiMap/LeftConflict128");
BENCHMARK_CAPTURE(measure_bimap_conflict, Right, false)->Name("Core/BiMap/RightConflict128");

Json numericArray(std::size_t count) {
  JsonArray values;
  values.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    auto value = static_cast<std::int64_t>((i * 7919) % 2000001) - 1000000;
    if (i % 2)
      values.push_back(Json(static_cast<double>(value) / 8.0));
    else
      values.push_back(Json(value));
  }
  return Json(std::move(values));
}

Json unicodeDocument(std::size_t count) {
  JsonArray rows;
  rows.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    rows.push_back(Json(JsonObject{
      {"index", Json(i)},
      {"日本語", Json("星の世界 — café é 😀")},
      {"escapes", Json("quote: \"; slash: \\; newline:\n tab:\t control:\x01")},
      {"active", Json(i % 2 == 0)},
      {"nested", Json(JsonObject{{"nothing", Json()}, {"numbers", numericArray(8)}})},
    }));
  }
  return Json(JsonObject{{"title", Json("Unicode mixed document / Ελληνικά")}, {"rows", Json(std::move(rows))}});
}

template <bool Unicode>
void BM_JsonSerialize(benchmark::State& state) {
  auto count = static_cast<std::size_t>(state.range(0));
  auto document = Unicode ? unicodeDocument(count) : numericArray(count);
  auto reference = document.repr(0, false);
  if (!reference) {
    state.SkipWithError("JSON fixture serialization failed");
    return;
  }
  auto parsed = Json::parseJson(*reference);
  if (!parsed || *parsed != document) {
    state.SkipWithError("JSON serialization did not round-trip the deterministic fixture");
    return;
  }
  auto repeated = document.repr(0, false);
  if (!repeated || *repeated != *reference) {
    state.SkipWithError("JSON serialization output is inconsistent");
    return;
  }
  auto bytes = static_cast<std::int64_t>(reference->utf8().size());
  state.SetLabel(Unicode ? "repr(0,false); nested Unicode/escaped strings/numbers/bools/null; rows"
                         : "repr(0,false); alternating signed integers and exact eighths; elements");
  for (auto _ : state) {
    (void)_;
    auto result = document.repr(0, false);
    if (!result) {
      state.SkipWithError("JSON serialization failed for a validated fixture");
      return;
    }
    benchmark::DoNotOptimize(result->utf8().data());
    benchmark::DoNotOptimize(result->utf8().size());
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(count));
  state.SetBytesProcessed(state.iterations() * bytes);
}

using FlatLookupMap = Star::HashMap<String, std::uint64_t, Star::hash<String>, std::equal_to<>>;
using StandardLookupMap = std::unordered_map<String, std::uint64_t, Star::hash<String>, std::equal_to<>>;

struct MapQueries {
  std::vector<String> hits;
  std::vector<String> misses;

  explicit MapQueries(std::size_t count) {
    hits.reserve(count);
    misses.reserve(count);
    // Counts are powers of two. An odd multiplier permutes the complete set,
    // independently of map layout, without timing RNG or key construction.
    for (std::size_t i = 0; i < count; ++i) {
      auto key = (i * 4051) & (count - 1);
      hits.emplace_back(std::format("asset/{:08x}/deterministic-key", key));
      misses.emplace_back(std::format("absent/{:08x}/deterministic-key", key));
    }
  }
};

template <typename Map, bool Hit>
void BM_MapLookup(benchmark::State& state) {
  auto count = static_cast<std::size_t>(state.range(0));
  MapQueries queries(count);
  Map map;
  map.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    auto inserted = map.emplace(queries.hits[i], static_cast<std::uint64_t>(i + 1));
    if (!inserted.second) {
      state.SkipWithError("Deterministic map keys are not unique");
      return;
    }
  }
  if (map.size() != count) {
    state.SkipWithError("Map fixture cardinality differs from the dataset");
    return;
  }
  for (std::size_t i = 0; i < count; ++i) {
    auto found = map.find(queries.hits[i]);
    if (found == map.end() || found->second != i + 1 || map.find(queries.misses[i]) != map.end()) {
      state.SkipWithError("Map hit/miss semantics differ from the deterministic dataset");
      return;
    }
  }
  auto const& lookup = std::as_const(map);
  auto const& keys = Hit ? queries.hits : queries.misses;
  state.SetLabel("Star::hash<String>/std::equal_to<>; reserve(count); prebuilt permuted String queries");
  for (auto _ : state) {
    (void)_;
    std::uint64_t checksum = 0;
    for (auto const& key : keys) {
      benchmark::DoNotOptimize(key);
      auto found = lookup.find(key);
      if (found != lookup.end())
        checksum += found->second;
    }
    benchmark::DoNotOptimize(checksum);
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(count));
}

BENCHMARK_TEMPLATE(BM_JsonSerialize, false)->Name("Core/Json/NumericArraySerialize")
    ->Arg(16)->Arg(256)->Arg(4096)->Arg(65536);
BENCHMARK_TEMPLATE(BM_JsonSerialize, true)->Name("Core/Json/UnicodeMixedSerialize")
    ->Arg(1)->Arg(16)->Arg(256);

BENCHMARK_TEMPLATE(BM_PerlinForwardOptional, false, true)->Name("Core/Perlin/DynamicMaybeRight/Hit");
BENCHMARK_TEMPLATE(BM_PerlinForwardOptional, false, false)->Name("Core/Perlin/DynamicMaybeRight/Miss");
BENCHMARK_TEMPLATE(BM_PerlinForwardOptional, true, true)->Name("Core/Perlin/ArrayOptionalName/Hit");
BENCHMARK_TEMPLATE(BM_PerlinForwardOptional, true, false)->Name("Core/Perlin/ArrayOptionalName/Miss");
BENCHMARK_TEMPLATE(BM_PerlinForwardCheckedHit, false)->Name("Core/Perlin/DynamicGetRight/Hit");
BENCHMARK_TEMPLATE(BM_PerlinForwardCheckedHit, true)->Name("Core/Perlin/EnumNameHelper/Hit");
BENCHMARK_TEMPLATE(BM_PerlinReverseOptional, false, true)->Name("Core/Perlin/DynamicMaybeLeft/Hit");
BENCHMARK_TEMPLATE(BM_PerlinReverseOptional, false, false)->Name("Core/Perlin/DynamicMaybeLeft/Miss");
BENCHMARK_TEMPLATE(BM_PerlinReverseOptional, true, true)->Name("Core/Perlin/MaybeEnumValueHelper/Hit");
BENCHMARK_TEMPLATE(BM_PerlinReverseOptional, true, false)->Name("Core/Perlin/MaybeEnumValueHelper/Miss");
BENCHMARK(BM_PerlinDynamicConstructDestroy)->Name("Core/Perlin/DynamicFromPrebuiltStringsConstructDestroy");

BENCHMARK_TEMPLATE(BM_MapLookup, FlatLookupMap, true)->Name("Core/Map/HashMap/Hit")
    ->Arg(4)->Arg(32)->Arg(4096)->Arg(65536);
BENCHMARK_TEMPLATE(BM_MapLookup, FlatLookupMap, false)->Name("Core/Map/HashMap/Miss")
    ->Arg(4)->Arg(32)->Arg(4096)->Arg(65536);
BENCHMARK_TEMPLATE(BM_MapLookup, StandardLookupMap, true)->Name("Core/Map/UnorderedMap/Hit")
    ->Arg(4)->Arg(32)->Arg(4096)->Arg(65536);
BENCHMARK_TEMPLATE(BM_MapLookup, StandardLookupMap, false)->Name("Core/Map/UnorderedMap/Miss")
    ->Arg(4)->Arg(32)->Arg(4096)->Arg(65536);

}

namespace LuaBenchmarks {
using namespace Star;

// Run core_benchmarks --benchmark_filter=Lua/ against both engine versions.
template <typename T> bool checked(benchmark::State& state, std::expected<T, StarException> const& result) {
  if (result)
    return true;
  std::string message(result.error().what());
  state.SkipWithError(message.c_str());
  return false;
}

bool require(benchmark::State& state, bool condition, char const* message) {
  if (!condition)
    state.SkipWithError(message);
  return condition;
}

std::expected<LuaContext, StarException> makeContext() {
  auto engine = LuaEngine::create();
  if (!engine)
    return std::unexpected(std::move(engine.error()));
  return (*engine)->createContext();
}

template <typename Function> bool installCallback(benchmark::State& state, LuaContext& context, Function&& callback) {
  auto function = context.createFunction(std::forward<Function>(callback));
  return checked(state, function) && checked(state, context.set("native", *function))
    && checked(state, context.load(R"LUA(
        function entry(...)
          return native(...)
        end
      )LUA"));
}

template <typename Operation, typename Validate>
bool measure(benchmark::State& state, Operation&& operation, Validate&& validate) {
  // Retain only the final output for untimed validation. Moving the next result
  // over the previous one includes ordinary output release in each sample.
  auto result = operation();
  if (!checked(state, result) || !validate(result))
    return false;
  for (auto _ : state) {
    result = operation();
    if (!checked(state, result))
      return false;
    benchmark::DoNotOptimize(result);
  }
  if (!validate(result))
    return false;
  state.SetItemsProcessed(state.iterations());
  return true;
}

template <bool Path> void BM_Invoke(benchmark::State& state) {
  auto context = makeContext();
  if (!checked(state, context) || !checked(state, context->load(R"LUA(
        api = {nested = {}}
        function api.nested.add(a, b) return a + b end
      )LUA")))
    return;
  String const path = "api.nested.add";
  auto function = context->getPath<LuaFunction>(path);
  if (!checked(state, function))
    return;
  measure(
    state,
    [&] {
      if constexpr (Path)
        return context->invokePath<int>(path, 17, 25);
      else
        return function->invoke<int>(17, 25);
    },
    [&](auto const& result) { return require(state, *result == 42, "Lua invocation returned the wrong sum"); });
}

template <typename T> T payload();

template <> int payload<int>() { return 42; }

template <> String payload<String>() {
  static constexpr char Text[] = "native callback payload: UTF-8 \xc3\xa9 / \xe6\xb0\xb4 / embedded\0NUL";
  return String(std::string_view(Text, sizeof(Text) - 1));
}

template <> Vec2F payload<Vec2F>() { return Vec2F(1.25f, -3.5f); }

template <> Json payload<Json>() {
  return JsonObject{{"name", "nested callback"},
                    {"enabled", true},
                    {"rows",
                     JsonArray{JsonObject{{"id", 1}, {"samples", JsonArray{1, 2, 3}}},
                               JsonObject{{"id", 2}, {"samples", JsonArray{5, 8, 13}}}}},
                    {"metadata", JsonObject{{"empty", JsonArray{}}, {"missing", Json()}}}};
}

template <typename T> void BM_CallbackRoundTrip(benchmark::State& state) {
  auto context = makeContext();
  if (!checked(state, context) || !installCallback(state, *context, [](T value) { return value; }))
    return;
  auto input = payload<T>();
  auto function = context->get<LuaFunction>("entry");
  if (!checked(state, function))
    return;
  measure(
    state, [&] { return function->template invoke<T>(input); },
    [&](auto const& result) {
      return require(state, *result == input, "Native callback round-trip changed the payload");
    });
}

enum class ReturnKind { Void, Single, Tuple, Variadic };

template <ReturnKind Kind>
using ReturnType = std::conditional_t<
  Kind == ReturnKind::Void, void,
  std::conditional_t<Kind == ReturnKind::Single, int,
                     std::conditional_t<Kind == ReturnKind::Tuple, LuaTupleReturn<int, int>, LuaVariadic<int>>>>;

template <ReturnKind Kind> auto returnCallback(std::uint64_t& calls) {
  if constexpr (Kind == ReturnKind::Void) {
    return [&calls] { ++calls; };
  } else if constexpr (Kind == ReturnKind::Single) {
    return [&calls] {
      ++calls;
      return 11;
    };
  } else if constexpr (Kind == ReturnKind::Tuple) {
    return [&calls] {
      ++calls;
      return luaTupleReturn(11, 22);
    };
  } else {
    return [&calls] {
      ++calls;
      return LuaVariadic<int>{11, 22, 33};
    };
  }
}

template <ReturnKind Kind>
bool validateReturn(benchmark::State& state, std::expected<ReturnType<Kind>, StarException> const& result) {
  if constexpr (Kind == ReturnKind::Void) {
    return true;
  } else if constexpr (Kind == ReturnKind::Single) {
    return require(state, *result == 11, "Single Lua return changed");
  } else if constexpr (Kind == ReturnKind::Tuple) {
    int first = 0;
    int second = 0;
    luaTie(first, second) = *result;
    return require(state, first == 11 && second == 22, "Tuple Lua returns changed");
  } else {
    return require(state, *result == LuaVariadic<int>{11, 22, 33}, "Variadic Lua returns changed");
  }
}

template <ReturnKind Kind> void BM_CallbackReturn(benchmark::State& state) {
  // The callback's borrowed capture outlives the context and engine.
  std::uint64_t calls = 0;
  auto context = makeContext();
  if (!checked(state, context) || !installCallback(state, *context, returnCallback<Kind>(calls)))
    return;
  auto function = context->get<LuaFunction>("entry");
  if (!checked(state, function))
    return;
  if (measure(
        state, [&] { return function->template invoke<ReturnType<Kind>>(); },
        [&](auto const& result) { return validateReturn<Kind>(state, result); }))
    require(state, calls == static_cast<std::uint64_t>(state.iterations()) + 1,
            "Native return callback did not run exactly once per invocation");
}

template <ReturnKind Kind> void BM_DeclaredReturn(benchmark::State& state) {
  auto context = makeContext();
  if (!checked(state, context) || !checked(state, context->load("function entry() return 11, 22, 33 end")))
    return;
  auto function = context->get<LuaFunction>("entry");
  if (!checked(state, function))
    return;
  // Identical Lua producer: only the caller's declared return arity varies.
  measure(
    state, [&] { return function->template invoke<ReturnType<Kind>>(); },
    [&](auto const& result) { return validateReturn<Kind>(state, result); });
}

void BM_ContextCreateDestroy(benchmark::State& state) {
  auto engine = LuaEngine::create();
  if (!checked(state, engine))
    return;
  auto retained = (*engine)->createContext();
  if (!checked(state, retained) || !checked(state, retained->set("sentinel", 42)))
    return;
  {
    auto independent = (*engine)->createContext();
    if (!checked(state, independent))
      return;
    auto sentinel = independent->get<LuaValue>("sentinel");
    if (!checked(state, sentinel)
        || !require(state, std::holds_alternative<LuaNilType>(*sentinel), "Lua contexts share globals"))
      return;
  }
  state.SetLabel("context creation + host reference destruction; automatic Lua GC");
  if (!measure(
        state,
        [&]() -> std::expected<void, StarException> {
          auto context = (*engine)->createContext();
          if (!context)
            return std::unexpected(std::move(context.error()));
          benchmark::DoNotOptimize(*context);
          return {};
        },
        [](auto const&) { return true; }))
    return;
  auto sentinel = retained->get<int>("sentinel");
  if (checked(state, sentinel))
    require(state, *sentinel == 42, "Context creation changed a retained context");
}

void BM_CoroutineYieldResume(benchmark::State& state) {
  auto context = makeContext();
  if (!checked(state, context) || !checked(state, context->load(R"LUA(
        function run(value)
          while true do
            value = coroutine.yield(value * 2)
          end
        end
      )LUA")))
    return;
  auto function = context->get<LuaFunction>("run");
  auto thread = context->engine().createThread();
  if (!checked(state, function) || !checked(state, thread) || !checked(state, thread->pushFunction(*function)))
    return;
  if (measure(
        state, [&] { return thread->resume<int>(7); },
        [&](auto const& result) {
          return require(state, result->has_value() && **result == 14, "Coroutine yielded the wrong value");
        }))
    require(state, thread->status() == LuaThread::Status::Active, "Yielding coroutine stopped unexpectedly");
}

BENCHMARK_TEMPLATE(BM_Invoke, false)->Name("Lua/Invoke/Direct");
BENCHMARK_TEMPLATE(BM_Invoke, true)->Name("Lua/Invoke/Path");
BENCHMARK_TEMPLATE(BM_CallbackRoundTrip, int)->Name("Lua/Callback/Scalar");
BENCHMARK_TEMPLATE(BM_CallbackRoundTrip, String)->Name("Lua/Callback/String");
BENCHMARK_TEMPLATE(BM_CallbackRoundTrip, Vec2F)->Name("Lua/Callback/Vector");
BENCHMARK_TEMPLATE(BM_CallbackRoundTrip, Json)->Name("Lua/Callback/NestedJson");
BENCHMARK_TEMPLATE(BM_CallbackReturn, ReturnKind::Void)->Name("Lua/CallbackReturn/Void");
BENCHMARK_TEMPLATE(BM_CallbackReturn, ReturnKind::Single)->Name("Lua/CallbackReturn/Single");
BENCHMARK_TEMPLATE(BM_CallbackReturn, ReturnKind::Tuple)->Name("Lua/CallbackReturn/Tuple");
BENCHMARK_TEMPLATE(BM_CallbackReturn, ReturnKind::Variadic)->Name("Lua/CallbackReturn/Variadic");
BENCHMARK_TEMPLATE(BM_DeclaredReturn, ReturnKind::Void)->Name("Lua/DeclaredReturn/Void");
BENCHMARK_TEMPLATE(BM_DeclaredReturn, ReturnKind::Single)->Name("Lua/DeclaredReturn/Single");
BENCHMARK_TEMPLATE(BM_DeclaredReturn, ReturnKind::Tuple)->Name("Lua/DeclaredReturn/Tuple");
BENCHMARK_TEMPLATE(BM_DeclaredReturn, ReturnKind::Variadic)->Name("Lua/DeclaredReturn/Variadic");
BENCHMARK(BM_ContextCreateDestroy)->Name("Lua/Context/CreateDestroy");
BENCHMARK(BM_CoroutineYieldResume)->Name("Lua/Coroutine/YieldResume");
}// namespace LuaBenchmarks

int main(int argc, char** argv) {
  return Star::run_benchmarks(argc, argv);
}
