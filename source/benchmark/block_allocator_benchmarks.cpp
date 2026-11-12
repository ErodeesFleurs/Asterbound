#include <benchmark/benchmark.h>
#include <cstdio>

import std;
import star.block_allocator;
import star.small_vector;
import star.format;
import star.native_runtime;

namespace {
namespace allocator_api = Star;
namespace vector_api = Star;

constexpr std::size_t small_batch = 64;
constexpr std::uint64_t guard_value = 0x94e7a9f10c38bd62ull;

struct tiny_value {
  std::uint8_t byte;
  bool operator==(tiny_value const&) const = default;
};
struct odd_value {
  std::array<std::uint8_t, 3> bytes;
  bool operator==(odd_value const&) const = default;
};
struct alignas(64) aligned_value {
  std::array<std::uint64_t, 8> words;
  aligned_value() = default;
  aligned_value(std::uint64_t number) {
    for (std::size_t i = 0; i != words.size(); ++i)
      words[i] = number ^ ((i + 1) * 0x9e3779b97f4a7c15ull);
  }
  bool operator==(aligned_value const&) const = default;
};
static_assert(sizeof(tiny_value) == 1 && sizeof(odd_value) == 3 && alignof(aligned_value) == 64);

template <typename T>
concept benchmark_value = std::same_as<T, tiny_value> || std::same_as<T, odd_value> || std::same_as<T, std::uint64_t>
  || std::same_as<T, aligned_value>;
template <typename T>
concept vector_value = std::same_as<T, std::uint64_t> || std::same_as<T, aligned_value>;

constexpr std::uint32_t number(std::size_t index) { return static_cast<std::uint32_t>(1009 + index * 17); }

template <benchmark_value T> T make_value(std::uint64_t value) {
  if constexpr (std::same_as<T, tiny_value>)
    return {static_cast<std::uint8_t>(value)};
  else if constexpr (std::same_as<T, odd_value>)
    return {{{static_cast<std::uint8_t>(value), static_cast<std::uint8_t>(value >> 8),
              static_cast<std::uint8_t>(value >> 16)}}};
  else
    return T(value);
}

struct fingerprint {
  std::uint64_t value = 14695981039346656037ull;
  void add(std::uint64_t word) {
    for (unsigned shift = 0; shift != 64; shift += 8)
      value = (value ^ ((word >> shift) & 255)) * 1099511628211ull;
  }
  template <benchmark_value T> void add_value(T const& object) {
    if constexpr (std::same_as<T, tiny_value>)
      add(object.byte);
    else if constexpr (std::same_as<T, odd_value>) {
      for (auto byte : object.bytes)
        add(byte);
    } else if constexpr (std::same_as<T, aligned_value>) {
      for (auto word : object.words)
        add(word);
    } else
      add(object);
  }
};

template <typename T>
concept guard_comparable = requires(T const& left, T const& right) { (left == right) ? true : false; };

template <guard_comparable T> bool guard_equal(T const& left, T const& right) { return (left == right) ? true : false; }

template <benchmark_value T>
bool guard_equal(std::optional<std::reference_wrapper<T>> const& left,
                 std::optional<std::reference_wrapper<T>> const& right) {
  return left.has_value() == right.has_value()
    && (!left || std::addressof(left->get()) == std::addressof(right->get()));
}

// Guard elements live in the same allocation as the fixture/output array.
// They detect indexing overruns; they are not claims about allocator redzones.
// Allocator-owned objects are borrowed only through reference_wrapper below.
template <typename T>
concept guarded_value = std::default_initializable<T> && std::copyable<T> && requires(T const& left, T const& right) {
  { guard_equal(left, right) } -> std::same_as<bool>;
};

template <guarded_value T> class guarded_array {
public:
  guarded_array(std::size_t size, T sentinel) : m_storage(size + 2), m_sentinel(std::move(sentinel)) {
    m_storage.front() = m_sentinel;
    m_storage.back() = m_sentinel;
  }
  std::span<T> elements() { return std::span(m_storage).subspan(1, m_storage.size() - 2); }
  std::span<T const> elements() const { return std::span(m_storage).subspan(1, m_storage.size() - 2); }
  bool intact() const {
    return guard_equal(m_storage.front(), m_sentinel) && guard_equal(m_storage.back(), m_sentinel);
  }

private:
  std::vector<T> m_storage;
  T m_sentinel;
};

template <benchmark_value T> struct fixture {
  guarded_array<T> values;
  guarded_array<std::size_t> order;
  fingerprint hash;

  explicit fixture(std::size_t count)
      : values(count * 2, make_value<T>(guard_value)), order(count, static_cast<std::size_t>(guard_value)) {
    for (std::size_t i = 0; i != values.elements().size(); ++i) {
      values.elements()[i] = make_value<T>(number(i));
      hash.add_value(values.elements()[i]);
    }
    std::iota(order.elements().begin(), order.elements().end(), 0);
    // Fixed Fisher-Yates permutation, independent of libc++ random engines.
    std::uint64_t random = 0xd6e8feb86659fd93ull;
    for (std::size_t i = count; i > 1; --i) {
      random ^= random << 13;
      random ^= random >> 7;
      random ^= random << 17;
      std::swap(order.elements()[i - 1], order.elements()[random % i]);
    }
    for (auto index : order.elements())
      hash.add(index);
  }
  bool intact() const { return values.intact() && order.intact(); }
};

bool require(benchmark::State& state, bool condition) {
  if (!condition)
    state.SkipWithError("complete values/order, alignment, family identity or boundary sentinel differs");
  return condition;
}

void label(benchmark::State& state, fingerprint input, fingerprint output, std::size_t batch, std::string_view detail) {
  state.SetLabel(
    std::format("fixture-fnv1a={:016x}; complete-content-fnv1a={:016x}; batch={}; ns/batch (divide by batch for "
                "amortized ns/item); fixture allocation, complete validation and fingerprints excluded; {}",
                input.value, output.value, batch, detail));
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(batch));
}

enum class metadata_operation { empty, rebind };
template <metadata_operation Op>
concept metadata_workload = Op == metadata_operation::empty || Op == metadata_operation::rebind;

template <benchmark_value T, metadata_operation Op>
  requires metadata_workload<Op>
void measure_metadata(benchmark::State& state) {
  using allocator_type = allocator_api::BlockAllocator<T, 1024>;
  using base_type = allocator_api::BlockAllocator<std::uint32_t, 1024>;
  std::array<std::optional<base_type>, small_batch> bases;
  std::array<std::optional<allocator_type>, small_batch> output;
  auto reset = [&] {
    for (auto& item : output)
      item.reset();
    if constexpr (Op == metadata_operation::rebind)
      for (auto& base : bases) {
        base.reset();
        base.emplace();
      }
  };
  auto apply = [&] {
    for (std::size_t i = 0; i != small_batch; ++i) {
      if constexpr (Op == metadata_operation::empty)
        output[i].emplace();
      else
        output[i].emplace(*bases[i]);
      benchmark::DoNotOptimize(*output[i]);
    }
    benchmark::ClobberMemory();
  };
  auto validate = [&] {
    for (std::size_t i = 0; i != small_batch; ++i) {
      if (!output[i])
        return false;
      auto copy = *output[i];
      if (!(copy == *output[i]))
        return false;
      if constexpr (Op == metadata_operation::rebind) {
        base_type back(*output[i]);
        if (!(back == *bases[i]))
          return false;
      }
    }
    return true;
  };
  reset();
  apply();
  if (!require(state, validate()))
    return;
  reset();
  for (auto _ : state) {
    (void)_;
    apply();
    state.PauseTiming();
    bool valid = validate();
    reset();
    state.ResumeTiming();
    if (!require(state, valid))
      return;
  }
  fingerprint input;
  input.add(sizeof(T));
  input.add(alignof(T));
  fingerprint output_hash;
  for (std::size_t i = 0; i != small_batch; ++i) {
    output_hash.add(i);
    output_hash.add(1);
  }
  label(state, input, output_hash, small_batch,
        Op == metadata_operation::empty
          ? "64 independent allocator constructions included; allocation(1), destruction and reset excluded"
          : "64 first cross-type rebind constructions included; fresh source families, destruction and reset excluded");
}

enum class pool_operation { cold_single, reuse, random_release };
template <pool_operation Op>
concept pool_workload =
  Op == pool_operation::cold_single || Op == pool_operation::reuse || Op == pool_operation::random_release;

template <benchmark_value T, std::size_t BlockSize, pool_operation Op>
  requires pool_workload<Op> && (BlockSize > 0)
void measure_pool(benchmark::State& state) {
  using allocator_type = allocator_api::BlockAllocator<T, BlockSize>;
  using traits = std::allocator_traits<allocator_type>;
  auto count = Op == pool_operation::cold_single ? small_batch : static_cast<std::size_t>(state.range(0));
  fixture<T> input(count);
  // The pool owns storage; these optional references own no allocations.
  T sentinel = make_value<T>(guard_value);
  using borrow = std::optional<std::reference_wrapper<T>>;
  guarded_array<borrow> live(count, std::ref(sentinel));
  guarded_array<std::uintptr_t> addresses(count, static_cast<std::uintptr_t>(guard_value));
  auto owners = std::vector<std::optional<allocator_type>>(Op == pool_operation::cold_single ? count : 1);
  auto reset = [&] {
    for (auto& owner : owners) {
      owner.reset();
      owner.emplace();
    }
  };
  auto allocator = [&](std::size_t index) -> allocator_type& {
    if constexpr (Op == pool_operation::cold_single)
      return *owners[index];
    else
      return *owners[0];
  };
  auto allocate = [&] {
    benchmark::DoNotOptimize(input.values.elements().data());
    benchmark::ClobberMemory();
    for (std::size_t i = 0; i != count; ++i) {
      auto& owner = allocator(i);
      auto address = traits::allocate(owner, 1);
      traits::construct(owner, address, input.values.elements()[i]);
      live.elements()[i] = std::ref(*address);
      benchmark::DoNotOptimize(address);
    }
    benchmark::ClobberMemory();
  };
  auto release = [&] {
    for (std::size_t position = 0; position != count; ++position) {
      auto index = Op == pool_operation::random_release ? input.order.elements()[position] : count - 1 - position;
      auto& owner = allocator(index);
      auto address = std::addressof(live.elements()[index]->get());
      traits::destroy(owner, address);
      traits::deallocate(owner, address, 1);
      live.elements()[index].reset();
      benchmark::DoNotOptimize(owner);
    }
    benchmark::ClobberMemory();
  };
  auto validate = [&](fingerprint& result) {
    if (!input.intact() || !live.intact() || !addresses.intact() || sentinel != make_value<T>(guard_value))
      return false;
    for (std::size_t i = 0; i != count; ++i) {
      if (!live.elements()[i])
        return false;
      auto const& object = live.elements()[i]->get();
      if (object != make_value<T>(number(i))
          || reinterpret_cast<std::uintptr_t>(std::addressof(object)) % alignof(T) != 0)
        return false;
      result.add_value(object);
      addresses.elements()[i] = reinterpret_cast<std::uintptr_t>(std::addressof(object));
    }
    std::ranges::sort(addresses.elements());
    for (std::size_t i = 1; i != count; ++i)
      if (addresses.elements()[i] - addresses.elements()[i - 1] < sizeof(T))
        return false;
    return true;
  };
  reset();
  allocate();
  fingerprint content;
  bool valid = validate(content);
  release();
  if (!require(state, valid))
    return;
  if constexpr (Op == pool_operation::cold_single)
    reset();
  for (auto _ : state) {
    (void)_;
    allocate();
    state.PauseTiming();
    fingerprint ignored;
    valid = validate(ignored);
    if constexpr (Op == pool_operation::cold_single) {
      release();
      reset();
      state.ResumeTiming();
    } else {
      state.ResumeTiming();
      release();
    }
    if (!require(state, valid))
      return;
  }
  if (!require(state, input.intact() && live.intact() && std::ranges::all_of(live.elements(), [](borrow const& item) {
                        return !item;
                      })))
    return;
  label(state, input.hash, content, count,
        Op == pool_operation::cold_single ? "64 fresh pools; allocate(1)+allocator_traits::construct included; "
                                            "destroy/deallocate, pool destruction/reset excluded"
          : Op == pool_operation::reuse   ? "warm retained block; allocate(1)+construct+reverse destroy/deallocate "
                                            "included; pool destruction/reset excluded"
                                        : "allocate(1)+construct+fixed random destroy/deallocate included; first sweep "
                                          "untimed; pool destruction/reset excluded");
}

template <bool Ordered>
using node_container =
  std::conditional_t<Ordered,
                     std::map<std::uint64_t, std::uint64_t, std::less<std::uint64_t>,
                              allocator_api::BlockAllocator<std::pair<std::uint64_t const, std::uint64_t>, 1024>>,
                     std::list<std::uint64_t, allocator_api::BlockAllocator<std::uint64_t, 1024>>>;

enum class node_operation { cold, churn };
template <node_operation Op>
concept node_workload = Op == node_operation::cold || Op == node_operation::churn;

template <bool Ordered, node_operation Op>
  requires node_workload<Op>
void measure_nodes(benchmark::State& state) {
  auto count = static_cast<std::size_t>(state.range(0));
  fixture<std::uint64_t> input(count);
  std::optional<node_container<Ordered>> nodes;
  auto fill = [&] {
    nodes.emplace();
    for (auto index : input.order.elements()) {
      if constexpr (Ordered)
        nodes->emplace(index, input.values.elements()[index]);
      else
        nodes->push_back(input.values.elements()[index]);
    }
    benchmark::DoNotOptimize(*nodes);
    benchmark::ClobberMemory();
  };
  auto churn = [&](std::size_t window) {
    for (auto index : input.order.elements()) {
      auto replacement = window * count + index;
      if constexpr (Ordered) {
        nodes->erase((window ^ 1) * count + index);
        nodes->emplace(replacement, input.values.elements()[replacement]);
      } else {
        nodes->pop_front();
        nodes->push_back(input.values.elements()[replacement]);
      }
    }
    benchmark::DoNotOptimize(*nodes);
    benchmark::ClobberMemory();
  };
  auto validate = [&](std::size_t window, fingerprint& result) {
    if (!input.intact() || !nodes || nodes->size() != count)
      return false;
    std::size_t position = 0;
    for (auto const& item : *nodes) {
      auto index = window * count + (Ordered ? position : input.order.elements()[position]);
      if constexpr (Ordered) {
        if (item.first != index || item.second != number(index))
          return false;
        result.add(item.first);
        result.add(item.second);
      } else {
        if (item != number(index))
          return false;
        result.add(item);
      }
      ++position;
    }
    return true;
  };
  fill();
  std::size_t window = 0;
  if constexpr (Op == node_operation::churn) {
    window = 1;
    churn(window);
  }
  fingerprint content;
  if (!require(state, validate(window, content)))
    return;
  nodes.reset();
  window = 0;
  if constexpr (Op == node_operation::churn)
    fill();
  for (auto _ : state) {
    (void)_;
    if constexpr (Op == node_operation::cold)
      fill();
    else {
      window ^= 1;
      churn(window);
    }
    state.PauseTiming();
    fingerprint ignored;
    bool valid = validate(window, ignored);
    if constexpr (Op == node_operation::cold)
      nodes.reset();
    state.ResumeTiming();
    if (!require(state, valid))
      return;
  }
  // The timed iteration count may differ across builds. Canonicalize the
  // alternating churn state off-clock before producing a comparable hash.
  if constexpr (Op == node_operation::churn) {
    if (window == 0)
      churn(1);
    content = {};
    if (!require(state, validate(1, content)))
      return;
  }
  label(state, input.hash, content, count,
        Op == node_operation::cold ? "default std node allocator; container construction+node insert/allocation "
                                     "included; clear/destruction/reset excluded"
                                   : "warm real std nodes; full erase+insert sweep included (map lookup included); "
                                     "clear/destruction/reset excluded");
}

enum class vector_operation { push, initializer, cross_copy, range, fill, copy };
template <vector_operation Op>
concept vector_workload =
  Op == vector_operation::push || Op == vector_operation::initializer || Op == vector_operation::cross_copy
  || Op == vector_operation::range || Op == vector_operation::fill || Op == vector_operation::copy;

template <vector_value T, vector_operation Op>
  requires vector_workload<Op>
void measure_vector(benchmark::State& state) {
  using vector_type = vector_api::SmallVector<T, 8>;
  auto count = Op == vector_operation::initializer ? std::size_t(9) : static_cast<std::size_t>(state.range(0));
  fixture<T> input(count);
  vector_type source(input.values.elements().begin(), input.values.elements().begin() + count);
  vector_api::SmallVector<std::uint32_t, 8> cross_source;
  for (std::size_t i = 0; i != count; ++i)
    cross_source.push_back(number(i));
  struct guarded_vector {
    std::uint64_t before = guard_value;
    std::optional<vector_type> object;
    std::uint64_t after = guard_value;
  };
  std::array<guarded_vector, small_batch> output;
  auto construct = [&] {
    benchmark::DoNotOptimize(input.values.elements().data());
    benchmark::DoNotOptimize(source);
    benchmark::DoNotOptimize(cross_source);
    benchmark::ClobberMemory();
    for (auto& holder : output) {
      auto& slot = holder.object;
      if constexpr (Op == vector_operation::push) {
        slot.emplace();
        for (std::size_t i = 0; i != count; ++i)
          slot->push_back(input.values.elements()[i]);
      } else if constexpr (Op == vector_operation::initializer) {
        auto values = input.values.elements();
        slot.emplace(std::initializer_list<T>{values[0], values[1], values[2], values[3], values[4], values[5],
                                              values[6], values[7], values[8]});
      } else if constexpr (Op == vector_operation::cross_copy)
        slot.emplace(cross_source);
      else if constexpr (Op == vector_operation::range)
        slot.emplace(input.values.elements().begin(), input.values.elements().begin() + count);
      else if constexpr (Op == vector_operation::fill)
        slot.emplace(count, input.values.elements()[0]);
      else
        slot.emplace(source);
      benchmark::DoNotOptimize(*slot);
    }
    benchmark::ClobberMemory();
  };
  auto reset = [&] {
    for (auto& holder : output)
      holder.object.reset();
  };
  auto validate = [&](fingerprint& result) {
    if (!input.intact())
      return false;
    for (auto const& holder : output) {
      if (holder.before != guard_value || holder.after != guard_value)
        return false;
      auto const& slot = holder.object;
      if (!slot || slot->size() != count || static_cast<std::size_t>(slot->end() - slot->begin()) != count
          || reinterpret_cast<std::uintptr_t>(slot->begin()) % alignof(T) != 0)
        return false;
      for (std::size_t i = 0; i != count; ++i) {
        auto expected = make_value<T>(number(Op == vector_operation::fill ? 0 : i));
        if ((*slot)[i] != expected)
          return false;
        result.add_value((*slot)[i]);
      }
    }
    // Check every source value as well as the destination and its boundaries.
    if (source.size() != count || cross_source.size() != count)
      return false;
    for (std::size_t i = 0; i != count; ++i)
      if (source[i] != make_value<T>(number(i)) || cross_source[i] != number(i))
        return false;
    return true;
  };
  construct();
  fingerprint content;
  bool valid = validate(content);
  reset();
  if (!require(state, valid))
    return;
  for (auto _ : state) {
    (void)_;
    construct();
    state.PauseTiming();
    fingerprint ignored;
    valid = validate(ignored);
    reset();
    state.ResumeTiming();
    if (!require(state, valid))
      return;
  }
  if (!require(state, std::ranges::all_of(output, [](guarded_vector const& holder) {
                 return holder.before == guard_value && holder.after == guard_value && !holder.object;
               })))
    return;
  label(state, input.hash, content, small_batch,
        "SmallVector<T,8>; 64 complete vector constructions, element copies/moves and spill allocations included; "
        "clear/destruction/reset excluded; amortized ns/vector, not ns/element");
}

template <benchmark_value T> void register_pool(std::string_view type) {
  benchmark::RegisterBenchmark(std::format("BlockAllocator/{}/Empty", type).c_str(),
                               measure_metadata<T, metadata_operation::empty>);
  benchmark::RegisterBenchmark(std::format("BlockAllocator/{}/RebindCold", type).c_str(),
                               measure_metadata<T, metadata_operation::rebind>);
  benchmark::RegisterBenchmark(std::format("BlockAllocator/{}/AllocateCold", type).c_str(),
                               measure_pool<T, 1024, pool_operation::cold_single>);
  benchmark::RegisterBenchmark(std::format("BlockAllocator/{}/Reuse/1024", type).c_str(),
                               measure_pool<T, 1024, pool_operation::reuse>)
    ->Arg(64);
  benchmark::RegisterBenchmark(std::format("BlockAllocator/{}/RandomRelease/4", type).c_str(),
                               measure_pool<T, 4, pool_operation::random_release>)
    ->Arg(257);
}

template <vector_value T> void register_vectors(std::string_view type) {
  benchmark::RegisterBenchmark(std::format("SmallVector/{}/Push", type).c_str(),
                               measure_vector<T, vector_operation::push>)
    ->Arg(8)
    ->Arg(9)
    ->Arg(64);
  benchmark::RegisterBenchmark(std::format("SmallVector/{}/Initializer", type).c_str(),
                               measure_vector<T, vector_operation::initializer>);
  benchmark::RegisterBenchmark(std::format("SmallVector/{}/CrossCopy", type).c_str(),
                               measure_vector<T, vector_operation::cross_copy>)
    ->Arg(64);
  benchmark::RegisterBenchmark(std::format("SmallVector/{}/KnownLengthRange", type).c_str(),
                               measure_vector<T, vector_operation::range>)
    ->Arg(64);
  benchmark::RegisterBenchmark(std::format("SmallVector/{}/KnownLengthFill", type).c_str(),
                               measure_vector<T, vector_operation::fill>)
    ->Arg(64);
  benchmark::RegisterBenchmark(std::format("SmallVector/{}/Copy", type).c_str(),
                               measure_vector<T, vector_operation::copy>)
    ->Arg(64);
}

class checked_reporter final : public benchmark::ConsoleReporter {
public:
  bool failed = false;
  void ReportRuns(std::vector<Run> const& reports) override {
    for (auto const& report : reports)
      failed |= report.skipped == benchmark::internal::SkippedWithError;
    ConsoleReporter::ReportRuns(reports);
  }
};
}// namespace

int main(int argc, char** argv) {
  auto runtime = Star::NativeRuntime::create();
  if (!runtime) {
    (void)Star::print(stderr, "{}\n", runtime.error().what());
    return 1;
  }
  register_pool<tiny_value>("Tiny1");
  register_pool<odd_value>("Odd3");
  register_pool<std::uint64_t>("U64");
  register_pool<aligned_value>("Align64");
  benchmark::RegisterBenchmark("BlockAllocator/U64/RandomRelease/1024",
                               measure_pool<std::uint64_t, 1024, pool_operation::random_release>)
    ->Arg(1024)
    ->Arg(1025)
    ->Arg(20000);
  benchmark::RegisterBenchmark("BlockAllocator/Align64/RandomRelease/1024",
                               measure_pool<aligned_value, 1024, pool_operation::random_release>)
    ->Arg(1024)
    ->Arg(1025)
    ->Arg(20000);
  benchmark::RegisterBenchmark("StdList/Cold", measure_nodes<false, node_operation::cold>)->Arg(64)->Arg(1025);
  benchmark::RegisterBenchmark("StdList/Churn", measure_nodes<false, node_operation::churn>)->Arg(64)->Arg(1025);
  benchmark::RegisterBenchmark("StdMap/Cold", measure_nodes<true, node_operation::cold>)->Arg(64)->Arg(1025);
  benchmark::RegisterBenchmark("StdMap/Churn", measure_nodes<true, node_operation::churn>)->Arg(64)->Arg(1025);
  register_vectors<std::uint64_t>("U64");
  register_vectors<aligned_value>("Align64");
  benchmark::Initialize(&argc, argv);
  if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
    benchmark::Shutdown();
    return 1;
  }
  checked_reporter reporter;
  auto matched = benchmark::RunSpecifiedBenchmarks(&reporter);
  benchmark::Shutdown();
  return reporter.failed || matched == 0 ? 1 : 0;
}
