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

using ttl_timestamp = std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds>;

template <typename Value>
concept ttl_cache_value = cache_value<Value>;

template <ttl_cache_value Value> using ttl_entry = std::pair<ttl_timestamp, Value>;

template <typename Storage>
concept ttl_storage =
  requires {
    typename Storage::key_type;
    typename Storage::mapped_type::second_type;
    requires ttl_cache_value<typename Storage::mapped_type::second_type>;
    requires std::same_as<typename Storage::mapped_type, ttl_entry<typename Storage::mapped_type::second_type>>;
  }
  && requires(Storage& storage, Storage const& const_storage, typename Storage::key_type const& key,
              std::size_t capacity,
              std::function<typename Storage::mapped_type(typename Storage::key_type const&)> producer,
              std::function<bool(typename Storage::key_type const&, typename Storage::mapped_type&)> filter) {
       Storage{};
       { const_storage.capacity() } -> std::same_as<std::size_t>;
       { storage.set_capacity(capacity) } -> std::same_as<void>;
       { const_storage.size() } -> std::same_as<std::size_t>;
       { storage.find_ref(key) } -> std::same_as<std::optional<std::reference_wrapper<typename Storage::mapped_type>>>;
       {
         const_storage.find_ref(key)
       } -> std::same_as<std::optional<std::reference_wrapper<typename Storage::mapped_type const>>>;
       {
         storage.get(key, producer)
       } -> std::same_as<std::expected<std::reference_wrapper<typename Storage::mapped_type>, StarException>>;
       { storage.erase(key) } -> std::same_as<bool>;
       { storage.erase_if(filter) } -> std::same_as<void>;
       { storage.clear() } -> std::same_as<void>;
     };

template <typename Storage>
concept ttl_key_snapshot =
  ttl_storage<Storage> && std::is_copy_constructible_v<typename Storage::key_type> && requires(Storage const& storage) {
    { storage.keys() } -> std::same_as<List<typename Storage::key_type>>;
  };

template <typename Storage>
concept ttl_value_snapshot = ttl_storage<Storage> && std::is_copy_constructible_v<typename Storage::mapped_type>
  && requires(Storage const& storage) {
       { storage.values() } -> std::same_as<List<typename Storage::mapped_type>>;
     };

template <typename Storage>
concept ttl_assignable_storage = ttl_storage<Storage>
  && requires(Storage& storage, typename Storage::key_type const& key, typename Storage::mapped_type entry) {
       { storage.insert_or_assign(key, std::move(entry)) } -> std::same_as<void>;
     };

template <ttl_storage LruCacheType> class TtlCacheBase {
public:
  using key_type = typename LruCacheType::key_type;
  using mapped_type = typename LruCacheType::mapped_type::second_type;

  explicit TtlCacheBase(RandomService& random);
  TtlCacheBase(RandomService& random, std::chrono::milliseconds time_to_live,
               std::chrono::milliseconds time_smear = std::chrono::milliseconds(1000), std::size_t capacity = NPos,
               bool ttl_update_enabled = true);

  std::chrono::milliseconds time_to_live() const;
  void set_time_to_live(std::chrono::milliseconds time_to_live);

  std::chrono::milliseconds time_smear() const;
  void set_time_smear(std::chrono::milliseconds time_smear);

  // A finite capacity also enables least-recently-used eviction on get misses.
  std::size_t capacity() const;
  void set_capacity(std::size_t capacity = NPos);

  std::size_t size() const;

  List<key_type> keys() const
    requires ttl_key_snapshot<LruCacheType>;
  List<mapped_type> values() const
    requires ttl_value_snapshot<LruCacheType>;

  // If disabled, accessing an entry does not refresh its time to live.
  bool ttl_update_enabled() const;
  void set_ttl_update_enabled(bool enabled);

  // A mutable lookup marks the entry as accessed and optionally refreshes TTL.
  [[nodiscard]] std::expected<std::optional<std::reference_wrapper<mapped_type>>, StarException>
  find_ref(key_type const& key);
  std::optional<std::reference_wrapper<mapped_type const>> find_ref(key_type const& key) const;

  // Insertion does not trim capacity; get misses and set_capacity do.
  [[nodiscard]] std::expected<void, StarException> insert_or_assign(key_type const& key, mapped_type value)
    requires ttl_assignable_storage<LruCacheType>;
  bool erase(key_type const& key);

  template <typename Filter>
    requires cache_filter<Filter, key_type, mapped_type>
  void erase_if(Filter&& filter);

  template <typename Producer>
    requires cache_producer<Producer, key_type, mapped_type>
  [[nodiscard]] std::expected<std::reference_wrapper<mapped_type>, StarException> get(key_type const& key,
                                                                                      Producer&& producer);

  void clear();

  // Expiration is enforced only here. Matching refresh filters refresh TTL
  // instead of removing entries, without sampling random smear.
  void cleanup();
  template <typename Filter>
    requires cache_filter<Filter, key_type, mapped_type const>
  void cleanup(Filter&& refresh_filter);

private:
  using entry_type = typename LruCacheType::mapped_type;

  template <typename Callback>
    requires cache_filter<Callback, key_type, mapped_type const>
  static bool callback_present(Callback const& callback) {
    if constexpr (std::is_pointer_v<Callback> || std::is_member_pointer_v<Callback>)
      return callback != nullptr;
    else
      return true;
  }

  template <typename Result, typename... Args>
    requires cache_filter<std::function<Result(Args...)>, key_type, mapped_type const>
  static bool callback_present(std::function<Result(Args...)> const& callback) {
    return static_cast<bool>(callback);
  }

  // The owning service must outlive this cache.
  std::reference_wrapper<RandomService> m_random;
  LruCacheType m_cache;
  std::chrono::milliseconds m_timeToLive;
  // Sampling retains the historical int endpoint conversion for random smear.
  std::chrono::milliseconds m_timeSmear;
  bool m_ttlUpdateEnabled;
};

template <typename Key, ttl_cache_value Value, typename Compare = std::less<Key>,
          typename Allocator = BlockAllocator<std::pair<Key const, ttl_entry<Value>>, 1024>>
  requires requires { typename LruCache<Key, ttl_entry<Value>, Compare, Allocator>; }
using TtlCache = TtlCacheBase<LruCache<Key, ttl_entry<Value>, Compare, Allocator>>;

template <typename Key, ttl_cache_value Value, typename Hash = Star::hash<Key>, typename Equals = std::equal_to<Key>,
          typename Allocator = BlockAllocator<std::pair<Key const, ttl_entry<Value>>, 1024>>
  requires requires { typename HashLruCache<Key, ttl_entry<Value>, Hash, Equals, Allocator>; }
using HashTtlCache = TtlCacheBase<HashLruCache<Key, ttl_entry<Value>, Hash, Equals, Allocator>>;

template <ttl_storage LruCacheType>
TtlCacheBase<LruCacheType>::TtlCacheBase(RandomService& random)
    : TtlCacheBase(random, std::chrono::milliseconds(10000), std::chrono::milliseconds(1000), NPos, true) {}

template <ttl_storage LruCacheType>
TtlCacheBase<LruCacheType>::TtlCacheBase(RandomService& random, std::chrono::milliseconds time_to_live,
                                         std::chrono::milliseconds time_smear, std::size_t capacity,
                                         bool ttl_update_enabled)
    : m_random(random) {
  m_cache.set_capacity(capacity);
  m_timeToLive = time_to_live;
  m_timeSmear = time_smear;
  m_ttlUpdateEnabled = ttl_update_enabled;
}

template <ttl_storage LruCacheType> std::chrono::milliseconds TtlCacheBase<LruCacheType>::time_to_live() const {
  return m_timeToLive;
}

template <ttl_storage LruCacheType>
void TtlCacheBase<LruCacheType>::set_time_to_live(std::chrono::milliseconds time_to_live) {
  m_timeToLive = time_to_live;
}

template <ttl_storage LruCacheType> std::chrono::milliseconds TtlCacheBase<LruCacheType>::time_smear() const {
  return m_timeSmear;
}

template <ttl_storage LruCacheType>
void TtlCacheBase<LruCacheType>::set_time_smear(std::chrono::milliseconds time_smear) {
  m_timeSmear = time_smear;
}

template <ttl_storage LruCacheType> bool TtlCacheBase<LruCacheType>::ttl_update_enabled() const {
  return m_ttlUpdateEnabled;
}

template <ttl_storage LruCacheType> std::size_t TtlCacheBase<LruCacheType>::capacity() const {
  return m_cache.capacity();
}

template <ttl_storage LruCacheType> void TtlCacheBase<LruCacheType>::set_capacity(std::size_t capacity) {
  m_cache.set_capacity(capacity);
}

template <ttl_storage LruCacheType> std::size_t TtlCacheBase<LruCacheType>::size() const { return m_cache.size(); }

template <ttl_storage LruCacheType>
auto TtlCacheBase<LruCacheType>::keys() const -> List<key_type>
  requires ttl_key_snapshot<LruCacheType>
{
  return m_cache.keys();
}

template <ttl_storage LruCacheType>
auto TtlCacheBase<LruCacheType>::values() const -> List<mapped_type>
  requires ttl_value_snapshot<LruCacheType>
{
  List<mapped_type> values;
  for (auto& entry : m_cache.values())
    values.push_back(std::move(entry.second));
  return values;
}

template <ttl_storage LruCacheType> void TtlCacheBase<LruCacheType>::set_ttl_update_enabled(bool enabled) {
  m_ttlUpdateEnabled = enabled;
}

template <ttl_storage LruCacheType>
[[nodiscard]] auto TtlCacheBase<LruCacheType>::find_ref(key_type const& key)
  -> std::expected<std::optional<std::reference_wrapper<mapped_type>>, StarException> {
  if (auto entry = m_cache.find_ref(key)) {
    if (m_ttlUpdateEnabled) {
      auto smear = m_random.get().randInt(-static_cast<int>(m_timeSmear.count()), static_cast<int>(m_timeSmear.count()));
      if (!smear)
        return std::unexpected(std::move(smear.error()));
      entry->get().first = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()
                                                                                   + std::chrono::microseconds(500))
        + std::chrono::milliseconds(*smear);
    }
    return std::optional{std::reference_wrapper<mapped_type>(entry->get().second)};
  }
  return std::nullopt;
}

template <ttl_storage LruCacheType>
auto TtlCacheBase<LruCacheType>::find_ref(key_type const& key) const
  -> std::optional<std::reference_wrapper<mapped_type const>> {
  if (auto entry = m_cache.find_ref(key))
    return std::reference_wrapper<mapped_type const>(entry->get().second);
  return std::nullopt;
}

template <ttl_storage LruCacheType>
[[nodiscard]] std::expected<void, StarException> TtlCacheBase<LruCacheType>::insert_or_assign(key_type const& key,
                                                                                              mapped_type value)
  requires ttl_assignable_storage<LruCacheType>
{
  auto smear = m_random.get().randInt(-static_cast<int>(m_timeSmear.count()), static_cast<int>(m_timeSmear.count()));
  if (!smear)
    return std::unexpected(std::move(smear.error()));
  m_cache.insert_or_assign(key,
                           entry_type(std::chrono::time_point_cast<std::chrono::milliseconds>(
                                        std::chrono::steady_clock::now() + std::chrono::microseconds(500))
                                        + std::chrono::milliseconds(*smear),
                                      std::move(value)));
  return {};
}

template <ttl_storage LruCacheType> bool TtlCacheBase<LruCacheType>::erase(key_type const& key) {
  return m_cache.erase(key);
}

template <ttl_storage LruCacheType>
template <typename Filter>
  requires cache_filter<Filter, typename LruCacheType::key_type, typename LruCacheType::mapped_type::second_type>
void TtlCacheBase<LruCacheType>::erase_if(Filter&& filter) {
  m_cache.erase_if(
    [&filter](key_type const& key, entry_type& entry) { return std::invoke(filter, key, entry.second); });
}

template <ttl_storage LruCacheType>
template <typename Producer>
  requires cache_producer<Producer, typename LruCacheType::key_type, typename LruCacheType::mapped_type::second_type>
[[nodiscard]] auto TtlCacheBase<LruCacheType>::get(key_type const& key, Producer&& producer)
  -> std::expected<std::reference_wrapper<mapped_type>, StarException> {
  auto result = m_cache.get(key, [&producer](key_type const& key) -> std::expected<entry_type, StarException> {
    if constexpr (!std::is_function_v<std::remove_reference_t<Producer>> && requires { producer == nullptr; }) {
      if (producer == nullptr)
        return std::unexpected(StarException("Cannot invoke an empty cache producer"));
    }
    if constexpr (std::same_as<std::invoke_result_t<Producer&, key_type const&>,
                               std::expected<mapped_type, StarException>>) {
      auto produced = std::invoke(producer, key);
      if (!produced)
        return std::unexpected(std::move(produced.error()));
      return entry_type(ttl_timestamp{}, std::move(*produced));
    } else {
      return entry_type(ttl_timestamp{}, std::invoke(producer, key));
    }
  });
  if (!result)
    return std::unexpected(std::move(result.error()));
  auto& entry = result->get();
  if (entry.first == ttl_timestamp{} || m_ttlUpdateEnabled) {
    auto smear = m_random.get().randInt(-static_cast<int>(m_timeSmear.count()), static_cast<int>(m_timeSmear.count()));
    if (!smear)
      return std::unexpected(std::move(smear.error()));
    entry.first = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()
                                                                          + std::chrono::microseconds(500))
      + std::chrono::milliseconds(*smear);
  }
  return std::reference_wrapper<mapped_type>(entry.second);
}

template <ttl_storage LruCacheType> void TtlCacheBase<LruCacheType>::clear() { m_cache.clear(); }

template <ttl_storage LruCacheType> void TtlCacheBase<LruCacheType>::cleanup() {
  cleanup([](key_type const&, mapped_type const&) { return false; });
}

template <ttl_storage LruCacheType>
template <typename Filter>
  requires cache_filter<Filter, typename LruCacheType::key_type, typename LruCacheType::mapped_type::second_type const>
void TtlCacheBase<LruCacheType>::cleanup(Filter&& refresh_filter) {
  auto current_time = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()
                                                                              + std::chrono::microseconds(500));
  m_cache.erase_if([&](key_type const& key, entry_type& entry) {
    if (callback_present(refresh_filter) && std::invoke(refresh_filter, key, std::as_const(entry.second))) {
      entry.first = current_time;
    } else {
      if (current_time - entry.first > m_timeToLive)
        return true;
    }
    return false;
  });
}
}

export module star.ttl_cache;

export namespace Star {
using ::Star::HashTtlCache;
using ::Star::ttl_cache_value;
using ::Star::ttl_storage;
using ::Star::TtlCache;
using ::Star::TtlCacheBase;
}
