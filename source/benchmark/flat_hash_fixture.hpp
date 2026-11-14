#pragma once

namespace BenchmarkFixtures {

template <class Map, class Entries>
concept emplace_entry_range = std::ranges::input_range<Entries const>
  && requires(Map& map, std::ranges::range_reference_t<Entries const> entry) {
    { map.try_emplace(entry.first, entry.second).second } -> std::convertible_to<bool>;
  };

template <class Map, class Entries>
  requires emplace_entry_range<Map, Entries>
std::size_t fill_map(Map& map, Entries const& input) {
  std::size_t inserted = 0;
  for (auto const& entry : input)
    inserted += map.try_emplace(entry.first, entry.second).second;
  return inserted;
}

template <class Map, class Fill>
concept owning_map_fill = std::default_initializable<Map>
  && requires(Map& map, std::size_t count, Fill& fill) {
    map.reserve(count);
    { std::invoke(fill, map) } -> std::same_as<std::size_t>;
  };

// The caller keeps this returned owner and its full destruction inside its
// original timing boundary. Input owners stay borrowed and are never copied.
template <class Map, bool Reserved, class Fill>
  requires owning_map_fill<Map, Fill>
Map make_filled_map(std::size_t count, Fill&& fill) {
  Map result;
  if constexpr (Reserved)
    result.reserve(count);
  auto inserted = std::invoke(fill, result);
  benchmark::DoNotOptimize(inserted);
  return result;
}

}// namespace BenchmarkFixtures
