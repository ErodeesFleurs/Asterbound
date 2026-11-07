module;

import std;
import star.format;
import star.memory;
import star.set;
import star.list;
import star.random;

import star.static_random;
namespace Star {

template <typename Item>
struct WeightedPool {
public:
  using ItemsType = std::pair<double, Item>;
  using ItemsList = List<ItemsType>;

  WeightedPool();

  template <typename Container>
  explicit WeightedPool(Container container);

  void add(double weight, Item item);
  void clear();

  ItemsList const& items() const;

  std::size_t size() const;
  [[nodiscard]] std::expected<std::reference_wrapper<std::pair<double, Item> const>, StarException> at(std::size_t index) const;
  [[nodiscard]] std::expected<double, StarException> weight(std::size_t index) const;
  [[nodiscard]] std::expected<std::reference_wrapper<Item const>, StarException> item(std::size_t index) const;
  bool empty() const;

  // Return item using the given randomness source
  Item select(RandomSource& rand) const;
  // Return item using the owning service's randomness source
  Item select(RandomService& rand) const;
  // Return item using fast static randomness from the given seed
  Item select(std::uint64_t seed) const;

  // Return a list of n items which are selected uniquely (by index), where
  // n is the lesser of the desiredCount and the size of the pool.
  // This INFLUENCES PROBABILITIES so it should not be used where a
  // correct statistical distribution is required.
  List<Item> selectUniques(std::size_t desiredCount, RandomService& rand) const;
  List<Item> selectUniques(std::size_t desiredCount, std::uint64_t seed) const;

  std::size_t selectIndex(RandomSource& rand) const;
  std::size_t selectIndex(RandomService& rand) const;
  std::size_t selectIndex(std::uint64_t seed) const;

private:
  std::size_t selectIndex(double target) const;

  ItemsList m_items;
  double m_totalWeight;
};

template <typename Item>
WeightedPool<Item>::WeightedPool()
  : m_totalWeight(0.0) {}

template <typename Item>
template <typename Container>
WeightedPool<Item>::WeightedPool(Container container)
  : WeightedPool() {
  for (auto const& pair : container)
    add(std::get<0>(pair), std::get<1>(pair));
}

template <typename Item>
void WeightedPool<Item>::add(double weight, Item item) {
  if (weight <= 0.0)
    return;

  m_items.append({weight, std::move(item)});
  m_totalWeight += weight;
}

template <typename Item>
void WeightedPool<Item>::clear() {
  m_items.clear();
  m_totalWeight = 0.0;
}

template <typename Item>
auto WeightedPool<Item>::items() const -> ItemsList const & {
  return m_items;
}

template <typename Item>
std::size_t WeightedPool<Item>::size() const {
  return m_items.count();
}

template <typename Item>
std::expected<std::reference_wrapper<std::pair<double, Item> const>, StarException> WeightedPool<Item>::at(std::size_t index) const {
  return m_items.at(index);
}

template <typename Item>
std::expected<double, StarException> WeightedPool<Item>::weight(std::size_t index) const {
  auto entry = at(index);
  if (!entry)
    return std::unexpected(std::move(entry.error()));
  return entry->get().first;
}

template <typename Item>
std::expected<std::reference_wrapper<Item const>, StarException> WeightedPool<Item>::item(std::size_t index) const {
  auto entry = at(index);
  if (!entry)
    return std::unexpected(std::move(entry.error()));
  return std::reference_wrapper<Item const>(entry->get().second);
}

template <typename Item>
bool WeightedPool<Item>::empty() const {
  return m_items.empty();
}

template <typename Item>
Item WeightedPool<Item>::select(RandomSource& rand) const {
  if (m_items.empty())
    return Item();

  return m_items[selectIndex(rand)].second;
}

template <typename Item>
Item WeightedPool<Item>::select(RandomService& rand) const {
  if (m_items.empty())
    return Item();

  return m_items[selectIndex(rand)].second;
}

template <typename Item>
Item WeightedPool<Item>::select(std::uint64_t seed) const {
  if (m_items.empty())
    return Item();

  return m_items[selectIndex(seed)].second;
}

template <typename Item>
List<Item> WeightedPool<Item>::selectUniques(std::size_t desiredCount, RandomService& rand) const {
  return selectUniques(desiredCount, rand.randu64());
}

template <typename Item>
List<Item> WeightedPool<Item>::selectUniques(std::size_t desiredCount, std::uint64_t seed) const {
  std::size_t targetCount = std::min(desiredCount, size());
  Set<std::size_t> indices;
  while (indices.size() < targetCount)
    indices.add(selectIndex(++seed));
  List<Item> result;
  for (std::size_t i : indices)
    result.append(m_items[i].second);
  return result;
}

template <typename Item>
std::size_t WeightedPool<Item>::selectIndex(RandomSource& rand) const {
  return selectIndex(rand.randd());
}

template <typename Item>
std::size_t WeightedPool<Item>::selectIndex(RandomService& rand) const {
  return selectIndex(rand.randd());
}

template <typename Item>
std::size_t WeightedPool<Item>::selectIndex(std::uint64_t seed) const {
  return selectIndex(staticRandomDouble(seed));
}

template <typename Item>
std::size_t WeightedPool<Item>::selectIndex(double target) const {
  if (m_items.empty())
    return NPos;

  // Test a randomly generated target against each weighted item in turn, and
  // see if that weighted item's weight value crosses the target.  This way, a
  // random item is picked from the list, but (roughly) weighted to be
  // proportional to its weight over the weight of all entries.
  //
  // TODO: This is currently O(n), but can easily be made O(log(n)) by using a
  // tree.  If this shows up in performance measurements, this is an obvious
  // improvement.

  double accumulatedWeight = 0.0f;
  for (std::size_t i = 0; i < m_items.size(); ++i) {
    accumulatedWeight += m_items[i].first / m_totalWeight;
    if (target <= accumulatedWeight)
      return i;
  }

  // If we haven't crossed the target, just assume floating point error has
  // caused us to not quite make it to the last item.
  return m_items.size() - 1;
}

}

export module star.weighted_pool;

export namespace Star {
  using ::Star::WeightedPool;
}
