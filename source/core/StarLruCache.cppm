module;
import std;
import star.format;
import star.list;
import star.hash;
import star.ordered_map;
import star.block_allocator;

import star.algorithm;
namespace Star {

template <typename OrderedMapType>
class LruCacheBase {
public:
  using Key = typename OrderedMapType::key_type;
  using Value = typename OrderedMapType::mapped_type;

  LruCacheBase();
  LruCacheBase(std::size_t maxSize);

  // Max size cannot be zero, it will be clamped to at least 1 in order to hold
  // the most recent element returned by get.
  std::size_t maxSize() const;
  void setMaxSize(std::size_t maxSize);

  std::size_t currentSize() const;

  List<Key> keys() const;
  List<Value> values() const;

  // A mutable lookup marks the entry as accessed; const lookup only observes it.
  std::optional<std::reference_wrapper<Value>> maybeRef(Key const& key);
  std::optional<std::reference_wrapper<Value const>> maybeRef(Key const& key) const;

  // Put the given value into the cache.
  void set(Key const& key, Value value);
  // Removes the given value from the cache.  If found and removed, returns
  // true.
  bool remove(Key const& key);

  // Remove all key / value pairs matching a filter.
  template <typename Filter>
    requires std::is_invocable_r_v<bool, Filter&, Key const&, Value&>
  void removeWhere(Filter&& filter);

  // If the value for the key is not found in the cache, produce it with the
  // given producer.  Producer shold take the key as an argument and return the
  // value.
  template <typename Producer>
    requires std::invocable<Producer&, Key const&>
  [[nodiscard]] std::expected<std::reference_wrapper<Value>, StarException> get(Key const& key, Producer&& producer);

  // Clear all cached entries.
  void clear();

private:
  OrderedMapType m_map;
  std::size_t m_maxSize;
};

template <typename Key, typename Value, typename Compare = std::less<Key>, typename Allocator = BlockAllocator<std::pair<Key const, Value>, 1024>>
using LruCache = LruCacheBase<OrderedMap<Key, Value, Compare, Allocator>>;

template <typename Key, typename Value, typename Hash = Star::hash<Key>, typename Equals = std::equal_to<Key>, typename Allocator = BlockAllocator<std::pair<Key const, Value>, 1024>>
using HashLruCache = LruCacheBase<OrderedHashMap<Key, Value, Hash, Equals, Allocator>>;

template <typename OrderedMapType>
LruCacheBase<OrderedMapType>::LruCacheBase()
  : LruCacheBase(256) {}

template <typename OrderedMapType>
LruCacheBase<OrderedMapType>::LruCacheBase(std::size_t maxSize) {
  setMaxSize(maxSize);
}

template <typename OrderedMapType>
std::size_t LruCacheBase<OrderedMapType>::maxSize() const {
  return m_maxSize;
}

template <typename OrderedMapType>
void LruCacheBase<OrderedMapType>::setMaxSize(std::size_t maxSize) {
  m_maxSize = std::max<std::size_t>(maxSize, 1);

  while (m_map.size() > m_maxSize)
    m_map.erase(m_map.begin());
}

template <typename OrderedMapType>
std::size_t LruCacheBase<OrderedMapType>::currentSize() const {
  return m_map.size();
}

template <typename OrderedMapType>
auto LruCacheBase<OrderedMapType>::keys() const -> List<Key> {
  return m_map.keys();
}

template <typename OrderedMapType>
auto LruCacheBase<OrderedMapType>::values() const -> List<Value> {
  return m_map.values();
}

template <typename OrderedMapType>
auto LruCacheBase<OrderedMapType>::maybeRef(Key const& key) -> std::optional<std::reference_wrapper<Value>> {
  auto i = m_map.find(key);
  if (i == m_map.end())
    return std::nullopt;
  i = m_map.toBack(i);
  return std::reference_wrapper<Value>(i->second);
}

template <typename OrderedMapType>
auto LruCacheBase<OrderedMapType>::maybeRef(Key const& key) const -> std::optional<std::reference_wrapper<Value const>> {
  return m_map.maybeRef(key);
}

template <typename OrderedMapType>
void LruCacheBase<OrderedMapType>::set(Key const& key, Value value) {
  auto i = m_map.find(key);
  if (i == m_map.end()) {
    m_map.insert({key, std::move(value)});
  } else {
    i->second = std::move(value);
    m_map.toBack(i);
  }
}

template <typename OrderedMapType>
bool LruCacheBase<OrderedMapType>::remove(Key const& key) {
  return m_map.remove(key);
}

template <typename OrderedMapType>
template <typename Filter>
  requires std::is_invocable_r_v<bool, Filter&, typename OrderedMapType::key_type const&, typename OrderedMapType::mapped_type&>
void LruCacheBase<OrderedMapType>::removeWhere(Filter&& filter) {
  eraseWhere(m_map, [&filter](auto& p) {
      return std::invoke(filter, p.first, p.second);
    });
}

template <typename OrderedMapType>
template <typename Producer>
  requires std::invocable<Producer&, typename OrderedMapType::key_type const&>
[[nodiscard]] auto LruCacheBase<OrderedMapType>::get(Key const& key, Producer&& producer) -> std::expected<std::reference_wrapper<Value>, StarException> {
  auto i = m_map.find(key);
  if (i == m_map.end()) {
    while (m_map.size() > m_maxSize - 1)
      m_map.erase(m_map.begin());
    if constexpr (!std::is_function_v<std::remove_reference_t<Producer>> && requires { producer == nullptr; }) {
      if (producer == nullptr)
        return std::unexpected(StarException("Cannot invoke an empty cache producer"));
    }
    if constexpr (std::same_as<std::invoke_result_t<Producer&, Key const&>, std::expected<Value, StarException>>) {
      auto value = std::invoke(producer, key);
      if (!value)
        return std::unexpected(std::move(value.error()));
      i = m_map.insert({key, std::move(*value)}).first;
    } else {
      i = m_map.insert({key, std::invoke(producer, key)}).first;
    }
  } else {
    i = m_map.toBack(i);
  }

  return std::reference_wrapper<Value>(i->second);
}

template <typename OrderedMapType>
void LruCacheBase<OrderedMapType>::clear() {
  m_map.clear();
}

}

export module star.lru_cache;

export namespace Star {
  using ::Star::LruCacheBase;
  using ::Star::LruCache;
  using ::Star::HashLruCache;
}
