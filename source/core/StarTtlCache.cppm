module;
import std;
import star.format;
import star.memory;
import star.list;
import star.hash;
import star.ordered_map;
import star.block_allocator;
import star.lru_cache;
import star.random;

import star.algorithm;
namespace Star {

template <typename LruCacheType>
class TtlCacheBase {
public:
  using Key = typename LruCacheType::Key;
  using Value = typename LruCacheType::Value::second_type;

  explicit TtlCacheBase(RandomService& random);
  TtlCacheBase(RandomService& random, std::chrono::milliseconds timeToLive, std::chrono::milliseconds timeSmear = std::chrono::milliseconds(1000), std::size_t maxSize = NPos, bool ttlUpdateEnabled = true);

  std::chrono::milliseconds timeToLive() const;
  void setTimeToLive(std::chrono::milliseconds timeToLive);

  std::chrono::milliseconds timeSmear() const;
  void setTimeSmear(std::chrono::milliseconds timeSmear);

  // If a max size is set, this cache also acts as an LRU cache with the given
  // maximum size.
  std::size_t maxSize() const;
  void setMaxSize(std::size_t maxSize = NPos);

  std::size_t currentSize() const;

  List<Key> keys() const;
  List<Value> values() const;

  // If ttlUpdateEnabled is false, then the time to live for entries will not
  // be updated on access.
  bool ttlUpdateEnabled() const;
  void setTtlUpdateEnabled(bool enabled);

  // If the value is in the cache, returns it and updates the access time,
  // otherwise returns an empty optional.
  [[nodiscard]] std::expected<std::optional<std::reference_wrapper<Value>>, StarException> find_ref(Key const& key);
  std::optional<std::reference_wrapper<Value const>> find_ref(Key const& key) const;

  // Put the given value into the cache.
  [[nodiscard]] std::expected<void, StarException> set(Key const& key, Value value);
  // Removes the given value from the cache.  If found and removed, returns
  // true.
  bool remove(Key const& key);

  // Remove all key / value pairs matching a filter.
  template <typename Filter>
    requires std::is_invocable_r_v<bool, Filter&, Key const&, Value&>
  void removeWhere(Filter&& filter);

  // If the value for the key is not found in the cache, produce it with the
  // given producer.  Producer should take the key as an argument and return
  // the Value.
  template <typename Producer>
    requires std::invocable<Producer&, Key const&>
  [[nodiscard]] std::expected<std::reference_wrapper<Value>, StarException> get(Key const& key, Producer&& producer);

  void clear();

  // Cleanup any cached entries that are older than their time to live, if the
  // refreshFilter is given, things that match the refreshFilter instead have
  // their ttl refreshed rather than being removed.
  void cleanup(std::nullptr_t = nullptr);
  template <typename Filter>
    requires std::is_invocable_r_v<bool, Filter&, Key const&, Value const&>
  void cleanup(Filter&& refreshFilter);

private:
  template <typename Filter>
  static bool refreshFilterPresent(Filter const& filter) {
    if constexpr (std::is_pointer_v<Filter> || std::is_member_pointer_v<Filter>)
      return filter != nullptr;
    else
      return true;
  }

  template <typename Result, typename... Args>
  static bool refreshFilterPresent(std::function<Result(Args...)> const& filter) {
    return static_cast<bool>(filter);
  }

  // The owning service must outlive this cache.
  std::reference_wrapper<RandomService> m_random;
  LruCacheType m_cache;
  std::chrono::milliseconds m_timeToLive;
  // Sampling retains the historical int endpoint conversion for random smear.
  std::chrono::milliseconds m_timeSmear;
  bool m_ttlUpdateEnabled;
};

template <typename Key, typename Value, typename Compare = std::less<Key>, typename Allocator = BlockAllocator<std::pair<Key const, std::pair<std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds>, Value>>, 1024>>
using TtlCache = TtlCacheBase<LruCache<Key, std::pair<std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds>, Value>, Compare, Allocator>>;

template <typename Key, typename Value, typename Hash = Star::hash<Key>, typename Equals = std::equal_to<Key>, typename Allocator = BlockAllocator<std::pair<Key const, std::pair<std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds>, Value>>, 1024>>
using HashTtlCache = TtlCacheBase<HashLruCache<Key, std::pair<std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds>, Value>, Hash, Equals, Allocator>>;

template <typename LruCacheType>
TtlCacheBase<LruCacheType>::TtlCacheBase(RandomService& random)
  : TtlCacheBase(random, std::chrono::milliseconds(10000), std::chrono::milliseconds(1000), NPos, true) {}

template <typename LruCacheType>
TtlCacheBase<LruCacheType>::TtlCacheBase(RandomService& random, std::chrono::milliseconds timeToLive, std::chrono::milliseconds timeSmear, std::size_t maxSize, bool ttlUpdateEnabled)
  : m_random(random) {
  m_cache.setMaxSize(maxSize);
  m_timeToLive = timeToLive;
  m_timeSmear = timeSmear;
  m_ttlUpdateEnabled = ttlUpdateEnabled;
}

template <typename LruCacheType>
std::chrono::milliseconds TtlCacheBase<LruCacheType>::timeToLive() const {
  return m_timeToLive;
}

template <typename LruCacheType>
void TtlCacheBase<LruCacheType>::setTimeToLive(std::chrono::milliseconds timeToLive) {
  m_timeToLive = timeToLive;
}

template <typename LruCacheType>
std::chrono::milliseconds TtlCacheBase<LruCacheType>::timeSmear() const {
  return m_timeSmear;
}

template <typename LruCacheType>
void TtlCacheBase<LruCacheType>::setTimeSmear(std::chrono::milliseconds timeSmear) {
  m_timeSmear = timeSmear;
}

template <typename LruCacheType>
bool TtlCacheBase<LruCacheType>::ttlUpdateEnabled() const {
  return m_ttlUpdateEnabled;
}

template <typename LruCacheType>
std::size_t TtlCacheBase<LruCacheType>::maxSize() const {
  return m_cache.maxSize();
}

template <typename LruCacheType>
void TtlCacheBase<LruCacheType>::setMaxSize(std::size_t maxSize) {
  m_cache.setMaxSize(maxSize);
}

template <typename LruCacheType>
std::size_t TtlCacheBase<LruCacheType>::currentSize() const {
  return m_cache.currentSize();
}

template <typename LruCacheType>
auto TtlCacheBase<LruCacheType>::keys() const -> List<Key> {
  return m_cache.keys();
}

template <typename LruCacheType>
auto TtlCacheBase<LruCacheType>::values() const -> List<Value> {
  List<Value> values;
  for (auto& p : m_cache.values())
    values.append(std::move(p.second));
  return values;
}

template <typename LruCacheType>
void TtlCacheBase<LruCacheType>::setTtlUpdateEnabled(bool enabled) {
  m_ttlUpdateEnabled = enabled;
}

template <typename LruCacheType>
[[nodiscard]] auto TtlCacheBase<LruCacheType>::find_ref(Key const& key)
  -> std::expected<std::optional<std::reference_wrapper<Value>>, StarException> {
  if (auto p = m_cache.find_ref(key)) {
    if (m_ttlUpdateEnabled) {
      auto smear = m_random.get().randInt(-static_cast<int>(m_timeSmear.count()), static_cast<int>(m_timeSmear.count()));
      if (!smear)
        return std::unexpected(std::move(smear.error()));
      p->get().first = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500)) + std::chrono::milliseconds(*smear);
    }
    return std::optional{std::reference_wrapper<Value>(p->get().second)};
  }
  return std::nullopt;
}

template <typename LruCacheType>
auto TtlCacheBase<LruCacheType>::find_ref(Key const& key) const -> std::optional<std::reference_wrapper<Value const>> {
  if (auto value = m_cache.find_ref(key))
    return std::reference_wrapper<Value const>(value->get().second);
  return std::nullopt;
}

template <typename LruCacheType>
[[nodiscard]] std::expected<void, StarException> TtlCacheBase<LruCacheType>::set(Key const& key, Value value) {
  auto smear = m_random.get().randInt(-static_cast<int>(m_timeSmear.count()), static_cast<int>(m_timeSmear.count()));
  if (!smear)
    return std::unexpected(std::move(smear.error()));
  m_cache.set(key, std::make_pair(std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500)) + std::chrono::milliseconds(*smear), std::move(value)));
  return {};
}

template <typename LruCacheType>
bool TtlCacheBase<LruCacheType>::remove(Key const& key) {
  return m_cache.remove(key);
}

template <typename LruCacheType>
template <typename Filter>
  requires std::is_invocable_r_v<bool, Filter&, typename LruCacheType::Key const&, typename LruCacheType::Value::second_type&>
void TtlCacheBase<LruCacheType>::removeWhere(Filter&& filter) {
  m_cache.removeWhere([&filter](auto const& key, auto& value) { return std::invoke(filter, key, value.second); });
}

template <typename LruCacheType>
template <typename Producer>
  requires std::invocable<Producer&, typename LruCacheType::Key const&>
[[nodiscard]] auto TtlCacheBase<LruCacheType>::get(Key const& key, Producer&& producer) -> std::expected<std::reference_wrapper<Value>, StarException> {
  auto result = m_cache.get(key, [&producer](Key const& key) -> std::expected<std::pair<std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds>, Value>, StarException> {
      if constexpr (!std::is_function_v<std::remove_reference_t<Producer>> && requires { producer == nullptr; }) {
        if (producer == nullptr)
          return std::unexpected(StarException("Cannot invoke an empty cache producer"));
      }
      if constexpr (std::same_as<std::invoke_result_t<Producer&, Key const&>, std::expected<Value, StarException>>) {
        auto produced = std::invoke(producer, key);
        if (!produced)
          return std::unexpected(std::move(produced.error()));
        return std::pair<std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds>, Value>(std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds>{}, std::move(*produced));
      } else {
        return std::pair<std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds>, Value>(std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds>{}, std::invoke(producer, key));
      }
    });
  if (!result)
    return std::unexpected(std::move(result.error()));
  auto& value = result->get();
  if (value.first == std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds>{} || m_ttlUpdateEnabled) {
    auto smear = m_random.get().randInt(-static_cast<int>(m_timeSmear.count()), static_cast<int>(m_timeSmear.count()));
    if (!smear)
      return std::unexpected(std::move(smear.error()));
    value.first = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500)) + std::chrono::milliseconds(*smear);
  }
  return std::reference_wrapper<Value>(value.second);
}

template <typename LruCacheType>
void TtlCacheBase<LruCacheType>::clear() {
  m_cache.clear();
}

template <typename LruCacheType>
void TtlCacheBase<LruCacheType>::cleanup(std::nullptr_t) {
  cleanup([](Key const&, Value const&) { return false; });
}

template <typename LruCacheType>
template <typename Filter>
  requires std::is_invocable_r_v<bool, Filter&, typename LruCacheType::Key const&, typename LruCacheType::Value::second_type const&>
void TtlCacheBase<LruCacheType>::cleanup(Filter&& refreshFilter) {
  auto currentTime = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500));
  m_cache.removeWhere([&](auto const& key, auto& value) {
      if (refreshFilterPresent(refreshFilter) && std::invoke(refreshFilter, key, std::as_const(value.second))) {
        value.first = currentTime;
      } else {
        if (currentTime - value.first > m_timeToLive)
          return true;
      }
      return false;
    });
}

}

export module star.ttl_cache;

export namespace Star {
  using ::Star::TtlCacheBase;
  using ::Star::TtlCache;
  using ::Star::HashTtlCache;
}
