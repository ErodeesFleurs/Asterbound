module;

import std;
import star.format;
import star.memory;
import star.list;
import star.math_common;
import star.random;
import star.static_random;

namespace Star {

struct WeightedPoolExceptionTag {
  static constexpr std::string_view name() { return "WeightedPoolException"; }
};
using WeightedPoolException = StarError<WeightedPoolExceptionTag, StarException>;

template <typename Item, typename Source>
concept weighted_pool_item_source = std::is_constructible_v<Item, Source>
  && (std::is_constructible_v<Item, Item&&> || std::is_constructible_v<Item, Item const&>);

template <typename Item>
concept weighted_pool_copyable_item = std::is_constructible_v<Item, Item const&>;

template <typename Item>
concept weighted_pool_selectable_item = weighted_pool_copyable_item<Item> && std::is_default_constructible_v<Item>;

// The owning List<Item> result must also support vector reserve relocation.
template <typename Item>
concept weighted_pool_unique_item = weighted_pool_copyable_item<Item> && std::is_constructible_v<Item, Item&&>;

namespace WeightedPoolDetail {
  using std::get;

  template <std::size_t Index, typename Entry>
    requires requires(Entry&& entry) { get<Index>(std::forward<Entry>(entry)); }
  decltype(auto) entry_get(Entry&& entry) {
    return get<Index>(std::forward<Entry>(entry));
  }

  // Keep the exponential race in log space: fast-math must not turn this
  // difference into a ratio that can overflow for a tiny positive weight.
#pragma float_control(precise, on, push)
  inline double unique_race_key(double weight, std::uint64_t random_bits) {
    double unit = (static_cast<double>(random_bits >> 12) + 0.5) * 0x1p-52;
    return std::log(-std::log(unit)) - std::log(weight);
  }
#pragma float_control(pop)
}

template <typename Range>
  requires std::ranges::input_range<Range>
using weighted_pool_source_reference =
  std::conditional_t<std::is_rvalue_reference_v<Range&&>, std::ranges::range_rvalue_reference_t<Range>,
                     std::ranges::range_reference_t<Range>>;

template <typename Entry, typename Item>
concept weighted_pool_entry_source = requires(Entry&& entry) {
  static_cast<double>(WeightedPoolDetail::entry_get<0>(std::forward<Entry>(entry)));
  requires weighted_pool_item_source<Item,
    decltype(WeightedPoolDetail::entry_get<1>(std::forward<Entry>(entry)))>;
};

template <typename Range, typename Item>
concept weighted_pool_source = std::ranges::input_range<Range>
  && weighted_pool_entry_source<weighted_pool_source_reference<Range>, Item>;

template <typename Item>
class WeightedPool {
public:
  using value_type = std::pair<double, Item>;
  using container_type = List<value_type>;
  using size_type = typename container_type::size_type;

  WeightedPool() = default;
  WeightedPool(WeightedPool const&) requires weighted_pool_copyable_item<Item> = default;
  WeightedPool& operator=(WeightedPool const& other) requires weighted_pool_copyable_item<Item> {
    if (this != std::addressof(other)) {
      WeightedPool copy(other);
      *this = std::move(copy);
    }
    return *this;
  }

  WeightedPool(WeightedPool&& other) noexcept(std::is_nothrow_move_constructible_v<container_type>)
    : m_items(std::move(other.m_items)), m_total_weight(std::exchange(other.m_total_weight, 0.0)) {
    other.m_items.clear();
  }

  WeightedPool& operator=(WeightedPool&& other) noexcept(std::is_nothrow_move_assignable_v<container_type>) {
    if (this != std::addressof(other)) {
      m_items = std::move(other.m_items);
      m_total_weight = std::exchange(other.m_total_weight, 0.0);
      other.m_items.clear();
    }
    return *this;
  }

  // Rvalue ranges transfer their entries; lvalue ranges retain their payloads.
  // Only sized ranges are reserved, so input ranges are traversed exactly once.
  template <typename Range>
    requires weighted_pool_source<Range, Item>
  [[nodiscard]] static std::expected<WeightedPool, StarException> from_range(Range&& range) {
    WeightedPool pool;
    if constexpr (std::ranges::sized_range<Range>)
      pool.m_items.reserve(std::ranges::size(range));
    auto position = std::ranges::begin(range);
    auto end = std::ranges::end(range);
    for (; position != end; ++position) {
      auto&& entry = [&]() -> decltype(auto) {
        if constexpr (std::is_rvalue_reference_v<Range&&>)
          return std::ranges::iter_move(position);
        else
          return *position;
      }();
      double weight = static_cast<double>(
        WeightedPoolDetail::entry_get<0>(std::forward<decltype(entry)>(entry)));
      auto total = pool.checked_total(weight);
      if (!total)
        return std::unexpected(std::move(total.error()));
      if (weight <= 0.0)
        continue;
      // Validate before even projecting the payload: a proxy get may itself move.
      pool.m_items.emplace_back(weight,
        WeightedPoolDetail::entry_get<1>(std::forward<decltype(entry)>(entry)));
      pool.m_total_weight = *total;
    }
    return pool;
  }

  template <typename Value>
    requires weighted_pool_item_source<Item, Value&&>
  [[nodiscard]] std::expected<void, StarException> insert(double weight, Value&& value) {
    auto total = checked_total(weight);
    if (!total)
      return std::unexpected(std::move(total.error()));
    if (weight <= 0.0)
      return {};
    m_items.emplace_back(weight, std::forward<Value>(value));
    m_total_weight = *total;
    return {};
  }

  void clear() {
    m_items.clear();
    m_total_weight = 0.0;
  }

  container_type const& items() const & { return m_items; }
  container_type const& items() && = delete;
  container_type const& items() const && = delete;

  size_type size() const { return m_items.size(); }
  bool empty() const { return m_items.empty(); }

  [[nodiscard]] std::expected<std::reference_wrapper<value_type const>, StarException> at(size_type index) const & {
    return m_items.at(index);
  }
  std::expected<std::reference_wrapper<value_type const>, StarException> at(size_type) && = delete;
  std::expected<std::reference_wrapper<value_type const>, StarException> at(size_type) const && = delete;

  [[nodiscard]] std::expected<double, StarException> weight(size_type index) const {
    auto entry = at(index);
    if (!entry)
      return std::unexpected(std::move(entry.error()));
    return entry->get().first;
  }

  [[nodiscard]] std::expected<std::reference_wrapper<Item const>, StarException> item(size_type index) const & {
    auto entry = at(index);
    if (!entry)
      return std::unexpected(std::move(entry.error()));
    return std::reference_wrapper<Item const>(entry->get().second);
  }
  std::expected<std::reference_wrapper<Item const>, StarException> item(size_type) && = delete;
  std::expected<std::reference_wrapper<Item const>, StarException> item(size_type) const && = delete;

  // Owning selection copies the chosen item. An empty pool consumes no randomness.
  Item select(RandomSource& rand) const requires weighted_pool_selectable_item<Item> {
    if (m_items.empty())
      return Item{};
    return Item(m_items[select_index(rand)].second);
  }

  Item select(RandomService& rand) const requires weighted_pool_selectable_item<Item> {
    if (m_items.empty())
      return Item{};
    return Item(m_items[select_index(rand)].second);
  }

  Item select(std::uint64_t seed) const requires weighted_pool_selectable_item<Item> {
    if (m_items.empty())
      return Item{};
    return Item(m_items[select_index(seed)].second);
  }

  // Bounded weighted sampling without replacement, unique by original index.
  // Results retain original pool order, independently of item equality.
  List<Item> select_unique(size_type desired_count, RandomService& rand) const
    requires weighted_pool_unique_item<Item> {
    return select_unique(desired_count, rand.next_u64());
  }

  List<Item> select_unique(size_type desired_count, std::uint64_t seed) const
    requires weighted_pool_unique_item<Item> {
    size_type target_count = std::min(desired_count, size());
    List<Item> result;
    result.reserve(target_count);
    if (target_count == 0)
      return result;
    if (target_count == size()) {
      for (auto const& entry : m_items)
        result.emplace_back(entry.second);
      return result;
    }
    if (target_count == 1) {
      result.emplace_back(m_items[select_index(++seed)].second);
      return result;
    }

    struct Candidate {
      size_type index;
      double key;
    };
    List<Candidate> candidates;
    candidates.reserve(size());
    // Independent exponential clocks give weighted sampling without replacement.
    // Midpoint uniforms exclude both endpoints, keeping every log-space key finite.
    for (size_type index = 0; index < size(); ++index)
      candidates.emplace_back(index,
        WeightedPoolDetail::unique_race_key(m_items[index].first, static_random_u64(++seed)));
    auto key_less = [](Candidate const& left, Candidate const& right) {
      if (left.key != right.key)
        return left.key < right.key;
      return left.index < right.index;
    };
    auto selected_end = candidates.begin() + target_count;
    std::ranges::nth_element(candidates, selected_end, key_less);
    std::ranges::sort(candidates.begin(), selected_end, std::ranges::less{}, &Candidate::index);
    for (auto position = candidates.begin(); position != selected_end; ++position)
      result.emplace_back(m_items[position->index].second);
    return result;
  }

  // Index selection consumes one uniform_double even when the pool is empty.
  size_type select_index(RandomSource& rand) const { return select_target(rand.uniform_double()); }
  size_type select_index(RandomService& rand) const { return select_target(rand.uniform_double()); }
  size_type select_index(std::uint64_t seed) const { return select_target(static_random_double(seed)); }

private:
  [[nodiscard]] std::expected<double, StarException> checked_total(double weight) const {
    // is_finite uses an integer-bit, non-inlined reference boundary, which is
    // required under production fast-math flags for both inputs and overflow.
    if (!is_finite(weight))
      return std::unexpected(WeightedPoolException("Weighted pool weights must be finite"));
    if (weight <= 0.0)
      return m_total_weight;
    double total = m_total_weight + weight;
    if (!is_finite(total))
      return std::unexpected(WeightedPoolException("Weighted pool total weight overflow"));
    return total;
  }

  size_type select_target(double target) const {
    if (m_items.empty())
      return NPos;
    // Preserve the original normalized scan and final-index rounding fallback.
    double accumulated_weight = 0.0;
    for (size_type index = 0; index < m_items.size(); ++index) {
      accumulated_weight += m_items[index].first / m_total_weight;
      if (target <= accumulated_weight)
        return index;
    }
    return m_items.size() - 1;
  }

  container_type m_items;
  double m_total_weight = 0.0;
};

}

export module star.weighted_pool;

export namespace Star {
  using ::Star::WeightedPool;
  using ::Star::WeightedPoolException;
  using ::Star::WeightedPoolExceptionTag;
  using ::Star::weighted_pool_copyable_item;
  using ::Star::weighted_pool_entry_source;
  using ::Star::weighted_pool_item_source;
  using ::Star::weighted_pool_selectable_item;
  using ::Star::weighted_pool_source;
  using ::Star::weighted_pool_source_reference;
  using ::Star::weighted_pool_unique_item;
}
