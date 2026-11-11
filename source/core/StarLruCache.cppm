module;
import std;
import star.format;
import star.list;
import star.hash;
import star.ordered_map;
import star.block_allocator;

import star.algorithm;
namespace Star {

template <typename Value>
concept cache_value = std::is_object_v<Value> && !std::is_array_v<Value>;

template <typename Producer, typename Key, typename Value>
concept cache_producer = std::invocable<Producer&, Key const&>
  && (std::same_as<std::invoke_result_t<Producer&, Key const&>, std::expected<Value, StarException>>
      || std::constructible_from<Value, std::invoke_result_t<Producer&, Key const&>>);

template <typename Filter, typename Key, typename Value>
concept cache_filter = std::is_invocable_r_v<bool, Filter&, Key const&, Value&>;

template <typename Compare, typename Key>
concept cache_compare = std::strict_weak_order<Compare const&, Key const&, Key const&>;

template <typename Hash, typename Key>
concept cache_hash = requires(Hash& hash, Key const& key) {
  { std::invoke(hash, key) } -> std::convertible_to<std::size_t>;
};

template <typename Equals, typename Key>
concept cache_equal =
  requires(Equals& equal, Key const& left, Key const& right) { std::invoke(equal, left, right) ? 0 : 0; };

template <typename Allocator, typename Value>
concept cache_allocator_for = std::copy_constructible<Allocator> && requires(Allocator& allocator, std::size_t count) {
  requires std::same_as<typename Allocator::value_type, Value>;
  allocator.allocate(count);
  { allocator.deallocate(allocator.allocate(count), count) } -> std::same_as<void>;
};

template <typename Storage>
concept lru_storage = requires(Storage& storage, Storage const& observed, typename Storage::key_type const& key,
                               typename Storage::mapped_type&& value, typename Storage::iterator position) {
  typename Storage::value_type;
  { storage.begin() } -> std::same_as<typename Storage::iterator>;
  { storage.end() } -> std::same_as<typename Storage::iterator>;
  { observed.size() } -> std::convertible_to<std::size_t>;
  { storage.find(key) } -> std::same_as<typename Storage::iterator>;
  {
    observed.find_ref(key)
  } -> std::same_as<std::optional<std::reference_wrapper<typename Storage::mapped_type const>>>;
  { storage.try_emplace(key, std::move(value)) } -> std::same_as<std::pair<typename Storage::iterator, bool>>;
  { storage.move_to_back(position) } -> std::same_as<typename Storage::iterator>;
  { storage.erase(position) } -> std::same_as<typename Storage::iterator>;
  { storage.erase(key) } -> std::convertible_to<std::size_t>;
  { storage.clear() } -> std::same_as<void>;
  { observed.keys() } -> std::same_as<List<typename Storage::key_type>>;
  { observed.values() } -> std::same_as<List<typename Storage::mapped_type>>;
};

template <lru_storage Storage> class LruCacheBase {
public:
  using key_type = typename Storage::key_type;
  using mapped_type = typename Storage::mapped_type;

  explicit LruCacheBase(std::size_t capacity = 256) : m_capacity(std::max(capacity, std::size_t{1})) {
    trim_to(m_capacity);
  }

  // At least one slot keeps a newly produced value alive until the next mutation.
  std::size_t capacity() const noexcept { return m_capacity; }
  void set_capacity(std::size_t capacity) {
    m_capacity = std::max(capacity, std::size_t{1});
    trim_to(m_capacity);
  }

  std::size_t size() const { return m_map.size(); }
  List<key_type> keys() const
    requires std::constructible_from<key_type, key_type const&>
  {
    return m_map.keys();
  }
  List<mapped_type> values() const
    requires std::constructible_from<mapped_type, mapped_type const&>
  {
    return m_map.values();
  }

  // A mutable lookup touches MRU; const lookup only observes the entry.
  std::optional<std::reference_wrapper<mapped_type>> find_ref(key_type const& key) {
    auto position = m_map.find(key);
    if (position == m_map.end())
      return std::nullopt;
    m_map.move_to_back(position);
    return std::ref(position->second);
  }

  std::optional<std::reference_wrapper<mapped_type const>> find_ref(key_type const& key) const {
    return m_map.find_ref(key);
  }

  // Upsert does not evict: callers may retain a reference across another insertion.
  void insert_or_assign(key_type const& key, mapped_type value)
    requires std::is_assignable_v<mapped_type&, mapped_type&&>
  {
    // Keep duplicate updates off the allocation-capable insertion path.
    auto position = m_map.find(key);
    if (position == m_map.end()) {
      m_map.try_emplace(key, std::move(value));
    } else {
      position->second = std::move(value);
      m_map.move_to_back(position);
    }
  }

  bool erase(key_type const& key) { return m_map.erase(key) != 0; }

  template <typename Filter>
    requires cache_filter<Filter, key_type, mapped_type>
  void erase_if(Filter&& filter) {
    eraseWhere(
      m_map, [&filter](typename Storage::value_type& entry) { return std::invoke(filter, entry.first, entry.second); });
  }

  template <typename Producer>
    requires cache_producer<Producer, key_type, mapped_type>
  [[nodiscard]] std::expected<std::reference_wrapper<mapped_type>, StarException> get(key_type const& key,
                                                                                      Producer&& producer) {
    auto position = m_map.find(key);
    if (position != m_map.end()) {
      m_map.move_to_back(position);
      return std::ref(position->second);
    }

    // Failure is not transactional: eviction precedes checking/calling the producer.
    trim_to(m_capacity - 1);
    if constexpr (!std::is_function_v<std::remove_reference_t<Producer>> && requires { producer == nullptr; }) {
      if (producer == nullptr)
        return std::unexpected(StarException("Cannot invoke an empty cache producer"));
    }
    if constexpr (std::same_as<std::invoke_result_t<Producer&, key_type const&>,
                               std::expected<mapped_type, StarException>>) {
      auto value = std::invoke(producer, key);
      if (!value)
        return std::unexpected(std::move(value.error()));
      position = m_map.try_emplace(key, std::move(*value)).first;
    } else {
      position = m_map.try_emplace(key, std::invoke(producer, key)).first;
    }
    // A producer may have inserted this key while the initial miss was in flight.
    return std::ref(position->second);
  }

  void clear() { m_map.clear(); }

private:
  void trim_to(std::size_t limit) {
    while (m_map.size() > limit)
      m_map.erase(m_map.begin());
  }

  Storage m_map;
  std::size_t m_capacity;
};

template <typename Key, cache_value Value, typename Compare = std::less<Key>,
          typename Allocator = BlockAllocator<std::pair<Key const, Value>, 1024>>
  requires cache_compare<Compare, Key> && cache_allocator_for<Allocator, std::pair<Key const, Value>>
using LruCache = LruCacheBase<OrderedMap<Key, Value, Compare, Allocator>>;

template <typename Key, cache_value Value, typename Hash = Star::hash<Key>, typename Equals = std::equal_to<Key>,
          typename Allocator = BlockAllocator<std::pair<Key const, Value>, 1024>>
  requires cache_hash<Hash, Key> && cache_equal<Equals, Key>
  && cache_allocator_for<Allocator, std::pair<Key const, Value>>
using HashLruCache = LruCacheBase<OrderedHashMap<Key, Value, Hash, Equals, Allocator>>;
}

export module star.lru_cache;

export namespace Star {
using ::Star::cache_allocator_for;
using ::Star::cache_compare;
using ::Star::cache_equal;
using ::Star::cache_filter;
using ::Star::cache_hash;
using ::Star::cache_producer;
using ::Star::cache_value;
using ::Star::HashLruCache;
using ::Star::lru_storage;
using ::Star::LruCache;
using ::Star::LruCacheBase;
}
