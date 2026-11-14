#include <benchmark/benchmark.h>
#include <cstdio>

import std;
import star.casting;
import star.type_identity;
import star.ref_ptr;
import star.data_stream;
import star.data_stream_devices;
import star.net_element;
import star.net_element_group;
import star.net_element_sync_group;
import star.format;
import star.native_runtime;

namespace casting_api = Star;
namespace identity_api = Star;
namespace ref_api = Star;

namespace {
constexpr std::size_t batch_size = 64;
constexpr std::uint64_t guard_value = 0xd73a90b4c61e825full;

struct fingerprint {
  std::uint64_t value = 14695981039346656037ull;
  void add(std::uint64_t number) {
    for (unsigned shift = 0; shift != 64; shift += 8)
      value = (value ^ ((number >> shift) & 255)) * 1099511628211ull;
  }
  void add_text(std::string_view text) {
    add(text.size());
    for (unsigned char byte : text)
      value = (value ^ byte) * 1099511628211ull;
  }
};

bool require(benchmark::State& state, bool condition) {
  if (!condition)
    state.SkipWithError("casting result/adjustment/owner/lifetime/error/guard oracle failed");
  return condition;
}

struct lifetime {
  std::size_t created = 0;
  std::size_t destroyed = 0;
};
struct root : virtual ref_api::RefCounter {
  std::uint64_t before = guard_value;
  std::array<std::uint64_t, 5> payload{};
  std::uint64_t after = guard_value;
  std::shared_ptr<lifetime> lives;
  virtual ~root() {
    if (lives)
      ++lives->destroyed;
  }
  void const* query_type(Star::TypeKey key) const noexcept override {
    return identity_api::query_type_of<root, ref_api::RefCounter>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override { return identity_api::type_name<root>(); }
};

template <class T>
concept query_base = requires(T const& value, Star::TypeKey key) {
  { value.query_type(key) } noexcept -> std::same_as<void const*>;
};

template <unsigned Tag, query_base... Bases> struct node : Bases... {
  void const* query_type(Star::TypeKey key) const noexcept override {
    return identity_api::query_type_of<node, Bases...>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override { return identity_api::type_name<node>(); }
};
template <unsigned Tag> struct virtual_branch : virtual root {
  void const* query_type(Star::TypeKey key) const noexcept override {
    return identity_api::query_type_of<virtual_branch, root>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override { return identity_api::type_name<virtual_branch>(); }
};
using single = node<1, root>;
using deep = node<2, single>;
using deepest = node<3, deep>;
using left = virtual_branch<4>;
using right = virtual_branch<5>;
using multi = node<6, left, right>;
using other_single = node<7, root>;
using repeated = node<8, single, other_single>;
using absent = node<9, root>;
using multi_child = node<10, multi>;

enum class population { single_hit, deep_hit, multi_hit, mixed, virtual_diamond, nonvirtual_diamond };
enum class operation { identity, static_upcast, query, pointer, borrowed, predicate, checked, checked_ref };

struct fixture {
  std::shared_ptr<lifetime> lives = std::make_shared<lifetime>();
  std::array<std::shared_ptr<root>, batch_size> owners;
  std::array<std::optional<std::reference_wrapper<root>>, batch_size> alternate;
  std::array<std::optional<std::reference_wrapper<right>>, batch_size> adjusted_right;
  std::array<unsigned, batch_size> kinds{};

  static void initialize(root& value, std::size_t index, std::shared_ptr<lifetime> const& lives) {
    value.lives = lives;
    ++lives->created;
    for (std::size_t j = 0; j != value.payload.size(); ++j)
      value.payload[j] = (index + 1) * 7919 + j * 137;
  }

  explicit fixture(population family) {
    for (std::size_t i = 0; i != batch_size; ++i) {
      unsigned kind = family == population::single_hit                             ? 1
        : family == population::deep_hit                                           ? 3
        : family == population::multi_hit || family == population::virtual_diamond ? 6
        : family == population::nonvirtual_diamond                                 ? 8
                                                                                   : unsigned(i % 5 == 0     ? 0
                                                                                                : i % 5 == 1 ? 1
                                                                                                : i % 5 == 2 ? 3
                                                                                                : i % 5 == 3 ? 6
                                                                                                             : 8);
      // Hit-only populations still mix concrete runtime types, not final objects
      // with a single compiler-visible dynamic type.
      if (family == population::single_hit && i % 2)
        kind = 3;
      if (family == population::deep_hit && i % 2)
        kind = 2;
      kinds[i] = kind;
      switch (kind) {
      case 0: owners[i] = std::make_shared<root>(); break;
      case 1: owners[i] = std::make_shared<single>(); break;
      case 2: owners[i] = std::make_shared<deep>(); break;
      case 3: owners[i] = std::make_shared<deepest>(); break;
      case 6: {
        if (i % 2) {
          auto owner = std::make_shared<multi_child>();
          adjusted_right[i] = std::ref(static_cast<right&>(*owner));
          owners[i] = std::move(owner);
        } else {
          auto owner = std::make_shared<multi>();
          adjusted_right[i] = std::ref(static_cast<right&>(*owner));
          owners[i] = std::move(owner);
        }
        break;
      }
      default: {
        auto owner = std::make_shared<repeated>();
        owners[i] = std::shared_ptr<root>(owner, static_cast<single*>(owner.get()));
        alternate[i] = std::ref(static_cast<root&>(static_cast<other_single&>(*owner)));
        initialize(alternate[i]->get(), i + batch_size, lives);
        break;
      }
      }
      initialize(*owners[i], i, lives);
    }
  }

  bool guards() const {
    for (std::size_t i = 0; i != batch_size; ++i) {
      auto intact = [](root const& value, std::size_t index) {
        if (value.before != guard_value || value.after != guard_value)
          return false;
        for (std::size_t j = 0; j != value.payload.size(); ++j)
          if (value.payload[j] != (index + 1) * 7919 + j * 137)
            return false;
        return true;
      };
      if (!intact(*owners[i], i) || (alternate[i] && !intact(alternate[i]->get(), i + batch_size)))
        return false;
    }
    return lives->destroyed == 0;
  }

  fingerprint hash() const {
    fingerprint result;
    for (std::size_t i = 0; i != batch_size; ++i) {
      result.add(kinds[i]);
      auto add = [&](root const& value) {
        result.add(value.before);
        for (auto number : value.payload)
          result.add(number);
        result.add(value.after);
      };
      add(*owners[i]);
      result.add(bool(alternate[i]));
      if (alternate[i])
        add(alternate[i]->get());
    }
    return result;
  }
};

// Independent oracle uses construction tags and C++ static pointer adjustment,
// never the capability query being measured. Repeated roots are intentionally
// ambiguous even when the source is a particular root subobject.
template <query_base Target> std::uintptr_t expected_address(fixture const& data, std::size_t i) {
  auto const& source = data.owners[i];
  auto kind = data.kinds[i];
  if constexpr (std::same_as<Target, root>) {
    if (kind != 8)
      return reinterpret_cast<std::uintptr_t>(source.get());
  } else if constexpr (std::same_as<Target, single>) {
    if (kind == 1 || kind == 2 || kind == 3 || kind == 8)
      return reinterpret_cast<std::uintptr_t>(static_cast<single*>(source.get()));
  } else if constexpr (std::same_as<Target, deep>) {
    if (kind == 2 || kind == 3)
      return reinterpret_cast<std::uintptr_t>(static_cast<deep*>(source.get()));
  } else if constexpr (std::same_as<Target, right>) {
    if (data.adjusted_right[i])
      return reinterpret_cast<std::uintptr_t>(std::addressof(data.adjusted_right[i]->get()));
  }
  return 0;
}

struct observation {
  std::uintptr_t address = 0;
  bool error = false;
  std::optional<Star::StarException> diagnostic;
};

void label(benchmark::State& state, fingerprint input, fingerprint result, std::string_view boundary) {
  state.SetLabel(std::format("fixture-fnv1a={:016x}; complete-result-fnv1a={:016x}; batch={}; {}", input.value,
                             result.value, batch_size, boundary));
}

template <query_base Target, operation Operation, bool Const = false>
void measure_borrow(benchmark::State& state, population family) {
  fixture data(family);
  std::array<observation, batch_size> output{};
  using source_type = std::conditional_t<Const, root const, root>;
  auto evaluate = [&] {
    for (std::size_t i = 0; i != batch_size; ++i) {
      source_type& source = *data.owners[i];
      observation result;
      if constexpr (Operation == operation::identity || Operation == operation::static_upcast) {
        if constexpr (Operation == operation::identity)
          result.address = reinterpret_cast<std::uintptr_t>(casting_api::as<root>(&source));
        else
          result.address = reinterpret_cast<std::uintptr_t>(casting_api::as<ref_api::RefCounter>(&source));
      } else if constexpr (Operation == operation::query) {
        result.address = reinterpret_cast<std::uintptr_t>(source.query_type(identity_api::type_key<Target>()));
      } else if constexpr (Operation == operation::pointer) {
        result.address = reinterpret_cast<std::uintptr_t>(casting_api::as<Target>(&source));
      } else if constexpr (Operation == operation::predicate) {
        result.address = casting_api::is<Target>(&source);
      } else if constexpr (Operation == operation::borrowed) {
        auto value = casting_api::as_ref<Target>(source);
        if (value)
          result.address = reinterpret_cast<std::uintptr_t>(std::addressof(value->get()));
      } else if constexpr (Operation == operation::checked) {
        auto value = casting_api::convert<Target>(&source);
        if (value)
          result.address = reinterpret_cast<std::uintptr_t>(*value);
        else {
          result.error = true;
          result.diagnostic = std::move(value.error());
        }
      } else {
        auto value = casting_api::convert_ref<Target>(source);
        if (value)
          result.address = reinterpret_cast<std::uintptr_t>(std::addressof(value->get()));
        else {
          result.error = true;
          result.diagnostic = std::move(value.error());
        }
      }
      output[i] = std::move(result);
    }
  };
  auto validate = [&] {
    if (!require(state, data.guards()))
      return false;
    for (std::size_t i = 0; i != batch_size; ++i) {
      auto want = expected_address<Target>(data, i);
      if constexpr (Operation == operation::identity)
        want = reinterpret_cast<std::uintptr_t>(data.owners[i].get());
      if constexpr (Operation == operation::static_upcast)
        want = reinterpret_cast<std::uintptr_t>(static_cast<ref_api::RefCounter*>(data.owners[i].get()));
      // A static identity cast does not perform capability traversal.
      if constexpr (Operation != operation::query && Operation != operation::static_upcast
                    && std::same_as<Target, root>)
        want = reinterpret_cast<std::uintptr_t>(data.owners[i].get());
      if constexpr (Operation == operation::predicate)
        want = bool(want);
      bool error = (Operation == operation::checked || Operation == operation::checked_ref) && !want;
      bool correct_error = !error
        ? !output[i].diagnostic
        : output[i].diagnostic && output[i].diagnostic->template is<Star::PointerConvertException>();
      if (!require(state,
                   output[i].address == want && output[i].error == error && correct_error
                     && data.owners[i].use_count() == 1))
        return false;
    }
    return true;
  };
  evaluate();
  if (!validate())
    return;
  fingerprint complete;
  for (std::size_t i = 0; i != batch_size; ++i) {
    complete.add(bool(output[i].address));
    complete.add(output[i].error);
    complete.add(1);
    complete.add(i);
    if (output[i].diagnostic)
      complete.add_text(output[i].diagnostic->what());
    for (auto value : data.owners[i]->payload)
      complete.add(value);
    complete.add(data.owners[i].use_count());
  }
  label(state, data.hash(), complete,
        "64 queries/casts; checked failures include typed error construction; no setup/validation");
  for (auto _ : state) {
    (void)_;
    benchmark::DoNotOptimize(data.owners.data());
    benchmark::ClobberMemory();
    evaluate();
    benchmark::DoNotOptimize(output.data());
    benchmark::ClobberMemory();
  }
  if (!validate())
    return;
  auto lives = data.lives;
  data.alternate.fill(std::nullopt);
  data.adjusted_right.fill(std::nullopt);
  for (auto& owner : data.owners)
    owner.reset();
  if (!require(state, lives->created == lives->destroyed))
    return;
  state.SetItemsProcessed(state.iterations() * batch_size);
}

template <class Owner>
concept root_owner = requires(Owner const& owner) {
  typename Owner::element_type;
  { owner.get() } -> std::same_as<root*>;
  { *owner } -> std::same_as<root&>;
};

template <bool Intrusive, bool Move, bool Hit, bool Checked = false> void measure_owned(benchmark::State& state) {
  fixture data(Hit ? population::multi_hit : population::single_hit);
  using owner_type = std::conditional_t<Intrusive, ref_api::RefPtr<root>, std::shared_ptr<root>>;
  using result_type = std::conditional_t<Intrusive, ref_api::RefPtr<right>, std::shared_ptr<right>>;
  static_assert(root_owner<owner_type>);
  std::array<owner_type, batch_size> input;
  std::array<result_type, batch_size> output;
  std::array<std::optional<std::reference_wrapper<right>>, batch_size> adjusted_right;
  std::array<bool, batch_size> errors{};
  std::array<std::optional<Star::StarException>, batch_size> diagnostics;
  // Intrusive fixtures are separately allocated; never put shared and intrusive
  // owners on the same object. The shared fixture supplies only the oracle tags.
  for (std::size_t i = 0; i != batch_size; ++i) {
    if constexpr (Intrusive) {
      if constexpr (Hit) {
        if (i % 2) {
          auto owner = ref_api::make_ref<multi_child>();
          adjusted_right[i] = std::ref(static_cast<right&>(*owner));
          input[i] = std::move(owner);
        } else {
          auto owner = ref_api::make_ref<multi>();
          adjusted_right[i] = std::ref(static_cast<right&>(*owner));
          input[i] = std::move(owner);
        }
      } else if (i % 2)
        input[i] = ref_api::make_ref<deepest>();
      else
        input[i] = ref_api::make_ref<single>();
      fixture::initialize(*input[i], i, data.lives);
    } else
      input[i] = data.owners[i];
  }
  auto evaluate = [&] {
    for (std::size_t i = 0; i != batch_size; ++i) {
      errors[i] = false;
      if constexpr (Checked) {
        auto result = [&] {
          if constexpr (Move)
            return casting_api::convert<right>(std::move(input[i]));
          else
            return casting_api::convert<right>(input[i]);
        }();
        if (result)
          output[i] = std::move(*result);
        else {
          errors[i] = true;
          diagnostics[i] = std::move(result.error());
        }
      } else if constexpr (Move)
        output[i] = casting_api::as<right>(std::move(input[i]));
      else
        output[i] = casting_api::as<right>(input[i]);
    }
  };
  auto validate = [&] {
    if (!require(state, data.guards()))
      return false;
    for (std::size_t i = 0; i != batch_size; ++i) {
      if (!require(state, bool(output[i]) == Hit && (output[i] || input[i])))
        return false;
      auto const& live = output[i] ? static_cast<root&>(*output[i]) : *input[i];
      bool correct_error =
        errors[i] ? diagnostics[i] && diagnostics[i]->template is<Star::PointerConvertException>() : !diagnostics[i];
      if (!require(state,
                   errors[i] == (Checked && !Hit) && correct_error && live.before == guard_value
                     && live.after == guard_value && live.payload == data.owners[i]->payload))
        return false;
      if constexpr (!Intrusive) {
        if constexpr (Hit) {
          if (!require(state,
                       reinterpret_cast<std::uintptr_t>(output[i].get()) == expected_address<right>(data, i)
                         && !output[i].owner_before(data.owners[i]) && !data.owners[i].owner_before(output[i])
                         && data.owners[i].use_count() == (input[i] ? 3 : 2)))
            return false;
        } else if (!require(state, input[i].get() == data.owners[i].get() && input[i].use_count() == 2))
          return false;
      } else {
        if constexpr (Hit)
          if (!require(state, adjusted_right[i] && output[i].get() == std::addressof(adjusted_right[i]->get())))
            return false;
      }
      if constexpr (!Move || !Hit)
        if (!require(state, bool(input[i])))
          return false;
    }
    return true;
  };
  // Restore only outside timing. Old APIs copy rvalues; new APIs transfer them.
  // Both are benchmarked by the identical consumer body, with exact respective
  // owner counts checked. Labels fingerprint semantic results, not this policy.
  auto restore = [&] {
    for (std::size_t i = 0; i != batch_size; ++i) {
      if constexpr (Move && Hit) {
        if (!input[i])
          input[i] = std::move(output[i]);
      }
      output[i].reset();
      diagnostics[i].reset();
    }
  };
  evaluate();
  if (!validate())
    return;
  fingerprint complete;
  for (std::size_t i = 0; i != batch_size; ++i) {
    complete.add(Hit);
    complete.add(errors[i]);
    complete.add(1);
    complete.add(i);
    if (diagnostics[i])
      complete.add_text(diagnostics[i]->what());
    for (auto value : data.owners[i]->payload)
      complete.add(value);
  }
  restore();
  label(state, data.hash(), complete,
        "64 owner casts; output release/input restore excluded; checked failures include error construction");
  for (auto _ : state) {
    (void)_;
    benchmark::DoNotOptimize(input.data());
    benchmark::ClobberMemory();
    evaluate();
    benchmark::DoNotOptimize(output.data());
    benchmark::ClobberMemory();
    state.PauseTiming();
    if (!validate())
      break;
    restore();
    state.ResumeTiming();
  }
  // A fresh untimed pass also handles a zero-iteration calibration consistently.
  restore();
  evaluate();
  if (!validate())
    return;
  adjusted_right.fill(std::nullopt);
  data.adjusted_right.fill(std::nullopt);
  for (auto& owner : output)
    owner.reset();
  for (auto& owner : input)
    owner.reset();
  auto lives = data.lives;
  data.alternate.fill(std::nullopt);
  for (auto& owner : data.owners)
    owner.reset();
  if (!require(state, lives->created == lives->destroyed))
    return;
  state.SetItemsProcessed(state.iterations() * batch_size);
}

enum class production_operation {
  reader_base_query,
  reader_external,
  reader_self,
  reader_miss,
  stream_mixed,
  net_sync,
  net_callback,
  net_miss
};

template <production_operation Operation> void measure_production(benchmark::State& state) {
  std::array<std::array<char, 19>, batch_size> bytes;
  std::array<std::shared_ptr<Star::DataStream>, batch_size> streams;
  std::array<std::shared_ptr<Star::NetElement>, batch_size> nets;
  std::array<std::uintptr_t, batch_size> expected{};
  std::array<std::uintptr_t, batch_size> output{};
  fingerprint input_hash;
  for (std::size_t i = 0; i != batch_size; ++i) {
    for (std::size_t j = 0; j != bytes[i].size(); ++j) {
      bytes[i][j] = char((i * 37 + j * 11) & 127);
      input_hash.add(static_cast<unsigned char>(bytes[i][j]));
    }
    if constexpr (Operation <= production_operation::stream_mixed) {
      if constexpr (Operation == production_operation::stream_mixed) {
        if (i % 2)
          streams[i] = std::make_shared<Star::DataStreamBuffer>();
        else
          streams[i] = std::make_shared<Star::DataStreamReader>(std::span<char const>(bytes[i]));
      } else
        streams[i] = std::make_shared<Star::DataStreamReader>(std::span<char const>(bytes[i]));
      if constexpr (Operation == production_operation::reader_base_query)
        expected[i] = reinterpret_cast<std::uintptr_t>(streams[i].get());
      else if constexpr (Operation == production_operation::reader_external)
        expected[i] = reinterpret_cast<std::uintptr_t>(static_cast<Star::DataStreamExternalBuffer*>(streams[i].get()));
      else if constexpr (Operation != production_operation::reader_miss) {
        if (Operation != production_operation::stream_mixed || i % 2 == 0)
          expected[i] = reinterpret_cast<std::uintptr_t>(static_cast<Star::DataStreamReader*>(streams[i].get()));
      }
      input_hash.add(Operation == production_operation::stream_mixed && i % 2 ? 2 : 1);
    } else {
      if (i % 3 == 0)
        nets[i] = std::make_shared<Star::NetElementGroup>();
      else if (i % 3 == 1)
        nets[i] = std::make_shared<Star::NetElementSyncGroup>();
      else
        nets[i] = std::make_shared<Star::NetElementCallbackGroup>();
      if constexpr (Operation == production_operation::net_sync) {
        if (i % 3 != 0)
          expected[i] = reinterpret_cast<std::uintptr_t>(static_cast<Star::NetElementSyncGroup*>(nets[i].get()));
      } else if constexpr (Operation == production_operation::net_callback) {
        if (i % 3 == 2)
          expected[i] = reinterpret_cast<std::uintptr_t>(static_cast<Star::NetElementCallbackGroup*>(nets[i].get()));
      }
      input_hash.add(i % 3);
    }
  }
  auto evaluate = [&] {
    for (std::size_t i = 0; i != batch_size; ++i) {
      if constexpr (Operation == production_operation::reader_base_query)
        output[i] =
          reinterpret_cast<std::uintptr_t>(streams[i]->query_type(identity_api::type_key<Star::DataStream>()));
      else if constexpr (Operation == production_operation::reader_external)
        output[i] = reinterpret_cast<std::uintptr_t>(casting_api::as<Star::DataStreamExternalBuffer>(streams[i].get()));
      else if constexpr (Operation == production_operation::reader_self
                         || Operation == production_operation::stream_mixed)
        output[i] = reinterpret_cast<std::uintptr_t>(casting_api::as<Star::DataStreamReader>(streams[i].get()));
      else if constexpr (Operation == production_operation::reader_miss)
        output[i] = reinterpret_cast<std::uintptr_t>(casting_api::as<Star::DataStreamBuffer>(streams[i].get()));
      else if constexpr (Operation == production_operation::net_sync)
        output[i] = reinterpret_cast<std::uintptr_t>(casting_api::as<Star::NetElementSyncGroup>(nets[i].get()));
      else if constexpr (Operation == production_operation::net_callback)
        output[i] = reinterpret_cast<std::uintptr_t>(casting_api::as<Star::NetElementCallbackGroup>(nets[i].get()));
      else
        output[i] = reinterpret_cast<std::uintptr_t>(casting_api::as<Star::DataStream>(nets[i].get()));
    }
  };
  auto validate = [&] {
    if (!require(state, output == expected))
      return false;
    for (std::size_t i = 0; i != batch_size; ++i) {
      if constexpr (Operation <= production_operation::stream_mixed) {
        if (!require(state, streams[i].use_count() == 1))
          return false;
        if (Operation != production_operation::stream_mixed || i % 2 == 0) {
          auto& reader = static_cast<Star::DataStreamReader&>(*streams[i]);
          auto position = reader.pos();
          if (!require(state, position && *position == 0 && std::ranges::equal(reader.span(), bytes[i])))
            return false;
        } else if (!require(state, static_cast<Star::DataStreamBuffer const&>(*streams[i]).empty()))
          return false;
      } else {
        auto const& group = static_cast<Star::NetElementGroup const&>(*nets[i]);
        if (!require(state,
                     nets[i].use_count() == 1 && !group.netVersion() && !group.netInterpolationEnabled()
                       && group.netExtrapolationHint() == 0))
          return false;
      }
      for (std::size_t j = 0; j != bytes[i].size(); ++j)
        if (!require(state, bytes[i][j] == char((i * 37 + j * 11) & 127)))
          return false;
    }
    return true;
  };
  evaluate();
  if (!validate())
    return;
  fingerprint complete;
  for (std::size_t i = 0; i != batch_size; ++i) {
    complete.add(bool(output[i]));
    complete.add(1);
    complete.add(0);
    for (auto byte : bytes[i])
      complete.add(static_cast<unsigned char>(byte));
  }
  label(state, input_hash, complete,
        "real production module graphs; 64 queries; constructors/content/cursor/owner checks untimed; no assets");
  for (auto _ : state) {
    (void)_;
    benchmark::DoNotOptimize(streams.data());
    benchmark::DoNotOptimize(nets.data());
    benchmark::ClobberMemory();
    evaluate();
    benchmark::DoNotOptimize(output.data());
    benchmark::ClobberMemory();
  }
  if (!validate())
    return;
  state.SetItemsProcessed(state.iterations() * batch_size);
}

void measure_null_alias(benchmark::State& state, bool checked) {
  fixture data(population::single_hit);
  std::array<std::shared_ptr<root>, batch_size> aliases;
  std::array<std::shared_ptr<right>, batch_size> output;
  std::array<bool, batch_size> failed{};
  std::array<std::optional<Star::StarException>, batch_size> diagnostics;
  for (std::size_t i = 0; i != batch_size; ++i)
    aliases[i] = std::shared_ptr<root>(data.owners[i], nullptr);
  auto evaluate = [&] {
    for (std::size_t i = 0; i != batch_size; ++i) {
      if (checked) {
        auto result = casting_api::convert<right>(std::move(aliases[i]));
        failed[i] = !result;
        if (!result)
          diagnostics[i] = std::move(result.error());
      } else {
        output[i] = casting_api::as<right>(std::move(aliases[i]));
      }
    }
  };
  auto validate = [&] {
    if (!require(state, data.guards()))
      return false;
    for (std::size_t i = 0; i != batch_size; ++i) {
      bool valid = checked ? failed[i] && diagnostics[i] && diagnostics[i]->template is<Star::PointerConvertException>()
                           : !output[i] && output[i].use_count() == 0;
      if (!require(state, valid && !aliases[i] && aliases[i].use_count() == 2 && data.owners[i].use_count() == 2))
        return false;
    }
    return true;
  };
  evaluate();
  if (!validate())
    return;
  fingerprint complete;
  for (std::size_t i = 0; i != batch_size; ++i) {
    complete.add(1);
    complete.add(0);
    complete.add(2);
    if (diagnostics[i])
      complete.add_text(diagnostics[i]->what());
  }
  label(state, data.hash(), complete,
        checked ? "64 null-alias checked failures; typed nullptr errors included"
                : "64 null stored-pointer alias casts; empty result; source control block preserved");
  for (auto _ : state) {
    (void)_;
    benchmark::DoNotOptimize(aliases.data());
    benchmark::ClobberMemory();
    evaluate();
    benchmark::DoNotOptimize(failed.data());
    benchmark::DoNotOptimize(diagnostics.data());
    benchmark::DoNotOptimize(output.data());
    benchmark::ClobberMemory();
  }
  if (!validate())
    return;
  for (auto& owner : aliases)
    owner.reset();
  auto lives = data.lives;
  for (auto& owner : data.owners)
    owner.reset();
  if (!require(state, lives->created == lives->destroyed))
    return;
  state.SetItemsProcessed(state.iterations() * batch_size);
}

template <query_base Target, operation Operation, bool Const = false>
void add_borrow(char const* name, population family) {
  benchmark::RegisterBenchmark(
    name, [family](benchmark::State& state) { measure_borrow<Target, Operation, Const>(state, family); });
}

void register_cases() {
  add_borrow<root, operation::identity>("Casting/Control/Identity", population::mixed);
  add_borrow<root, operation::static_upcast>("Casting/Control/StaticUpcast", population::mixed);
  add_borrow<single, operation::pointer>("Casting/Single/PointerHit", population::single_hit);
  add_borrow<absent, operation::pointer>("Casting/Single/PointerMiss", population::single_hit);
  add_borrow<deep, operation::pointer>("Casting/Deep/PointerHit", population::deep_hit);
  add_borrow<absent, operation::pointer>("Casting/Deep/PointerMiss", population::deep_hit);
  add_borrow<right, operation::pointer>("Casting/Multi/AdjustedHit", population::multi_hit);
  add_borrow<absent, operation::pointer>("Casting/Multi/PointerMiss", population::multi_hit);
  add_borrow<root, operation::query>("Casting/Diamond/VirtualSameAddress", population::virtual_diamond);
  add_borrow<root, operation::query>("Casting/Diamond/NonvirtualAmbiguous", population::nonvirtual_diamond);
  add_borrow<single, operation::query>("Casting/Diamond/NonvirtualUniqueBranch", population::nonvirtual_diamond);
  add_borrow<right, operation::pointer>("Casting/Mixed/Pointer", population::mixed);
  add_borrow<right, operation::borrowed>("Casting/Mixed/Borrowed", population::mixed);
  add_borrow<right, operation::predicate>("Casting/Mixed/Is", population::mixed);
  add_borrow<right, operation::pointer, true>("Casting/Mixed/ConstPointer", population::mixed);
  add_borrow<right, operation::borrowed, true>("Casting/Mixed/ConstBorrowed", population::mixed);
  add_borrow<right, operation::checked>("Casting/Checked/PointerSuccess", population::multi_hit);
  add_borrow<absent, operation::checked>("Casting/Checked/PointerFailureTypedError", population::single_hit);
  add_borrow<right, operation::checked_ref>("Casting/Checked/ReferenceSuccess", population::multi_hit);
  add_borrow<absent, operation::checked_ref>("Casting/Checked/ReferenceFailureTypedError", population::single_hit);
  add_borrow<right, operation::checked_ref, true>("Casting/Checked/ConstReferenceSuccess", population::multi_hit);
  benchmark::RegisterBenchmark("Casting/Shared/LvalueHit", measure_owned<false, false, true>);
  benchmark::RegisterBenchmark("Casting/Shared/LvalueMiss", measure_owned<false, false, false>);
  benchmark::RegisterBenchmark("Casting/Shared/RvalueHit", measure_owned<false, true, true>);
  benchmark::RegisterBenchmark("Casting/Shared/RvalueMiss", measure_owned<false, true, false>);
  benchmark::RegisterBenchmark("Casting/Intrusive/LvalueHit", measure_owned<true, false, true>);
  benchmark::RegisterBenchmark("Casting/Intrusive/LvalueMiss", measure_owned<true, false, false>);
  benchmark::RegisterBenchmark("Casting/Intrusive/RvalueHit", measure_owned<true, true, true>);
  benchmark::RegisterBenchmark("Casting/Intrusive/RvalueMiss", measure_owned<true, true, false>);
  benchmark::RegisterBenchmark("Casting/Checked/SharedRvalueSuccess", measure_owned<false, true, true, true>);
  benchmark::RegisterBenchmark("Casting/Checked/SharedRvalueFailureTypedError",
                               measure_owned<false, true, false, true>);
  benchmark::RegisterBenchmark("Casting/Checked/IntrusiveRvalueSuccess", measure_owned<true, true, true, true>);
  benchmark::RegisterBenchmark("Casting/Checked/IntrusiveRvalueFailureTypedError",
                               measure_owned<true, true, false, true>);
  benchmark::RegisterBenchmark("Casting/Shared/NullStoredAlias",
                               [](benchmark::State& state) { measure_null_alias(state, false); });
  benchmark::RegisterBenchmark("Casting/Checked/NullStoredAliasTypedError",
                               [](benchmark::State& state) { measure_null_alias(state, true); });
  benchmark::RegisterBenchmark("Casting/Production/ReaderBaseQuery",
                               measure_production<production_operation::reader_base_query>);
  benchmark::RegisterBenchmark("Casting/Production/ReaderExternalHit",
                               measure_production<production_operation::reader_external>);
  benchmark::RegisterBenchmark("Casting/Production/ReaderSelfHit",
                               measure_production<production_operation::reader_self>);
  benchmark::RegisterBenchmark("Casting/Production/ReaderMiss", measure_production<production_operation::reader_miss>);
  benchmark::RegisterBenchmark("Casting/Production/StreamMixed",
                               measure_production<production_operation::stream_mixed>);
  benchmark::RegisterBenchmark("Casting/Production/NetSyncMixed", measure_production<production_operation::net_sync>);
  benchmark::RegisterBenchmark("Casting/Production/NetCallbackMixed",
                               measure_production<production_operation::net_callback>);
  benchmark::RegisterBenchmark("Casting/Production/NetMissingCapability",
                               measure_production<production_operation::net_miss>);
}

class CheckedReporter final : public benchmark::ConsoleReporter {
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
  register_cases();
  benchmark::Initialize(&argc, argv);
  if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
    benchmark::Shutdown();
    return 1;
  }
  CheckedReporter reporter;
  auto matched = benchmark::RunSpecifiedBenchmarks(&reporter);
  benchmark::Shutdown();
  return reporter.failed || matched == 0 ? 1 : 0;
}
