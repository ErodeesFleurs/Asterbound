module;

import star.memory;
import star.format;
import std;

namespace Star {

template <typename Hash, typename Equal, typename Key, typename Query>
concept transparent_hash_lookup = requires(Hash& hash, Equal& equal, Key const& key, Query const& query) {
  typename Hash::is_transparent;
  typename Equal::is_transparent;
  { hash(query) } -> std::convertible_to<std::size_t>;
  equal(key, query) ? 0 : 0;
};

// Keep construction checks local to operations: mapped values may be recursive,
// and constructible_from would additionally require a nothrow destructor.
template <typename Value, typename... Args>
concept flat_value_constructible = std::is_constructible_v<Value, Args...>;

template <typename Value, typename Argument>
concept flat_value_assignable = std::is_assignable_v<Value&, Argument>;

template <typename Equal, typename GetKey, typename Value, typename Query>
concept key_match = requires(Equal& equal, GetKey& get_key, Value& value, Query const& query) {
  equal(get_key(value), query) ? 0 : 0;
};

template <typename Hash, typename Equal, typename GetKey, typename Value, typename Query>
concept hash_lookup = key_match<Equal, GetKey, Value, Query>
    && requires(Hash& hash, Query const& query, std::size_t const& filled_hash_bit) {
      requires std::is_convertible_v<decltype(hash(query) | filled_hash_bit), std::size_t>;
    };

template <typename Factory, typename Value>
concept value_factory = requires(Factory&& factory) { Value(std::invoke(std::forward<Factory>(factory))); };

template <typename Match, typename Value>
concept value_match = requires(Match& match, Value& value) { std::invoke(match, value) ? 0 : 0; };

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
struct FlatHashTable {
private:
  static std::size_t const empty_hash_value = 0;
  static std::size_t const end_hash_value = 1;
  static std::size_t const filled_hash_bit = static_cast<std::size_t>(1) << (sizeof(std::size_t) * 8 - 1);

  struct Bucket {
    Bucket();
    ~Bucket();

    Bucket(Bucket const& rhs) requires std::is_copy_constructible_v<Value>;
    Bucket(Bucket&& rhs) noexcept(std::is_nothrow_move_constructible_v<Value>);

    Bucket& operator=(Bucket const& rhs)
      requires std::is_copy_constructible_v<Value>;
    Bucket& operator=(Bucket&& rhs);

    void set_filled(std::size_t hash, Value&& value);
    void exchange_value(Value& value);
    void set_empty();
    void set_end();

    bool is_filled() const { return (hash & filled_hash_bit) != 0; }
    bool is_empty() const;

    // Named storage avoids xclang's imported anonymous-union member IR failure.
    // Neither constructor nor destructor activates or destroys the Value.
    union ValueStorage {
      ValueStorage() {}
      ~ValueStorage() {}
      Value value;
    } storage;
    std::size_t hash;
  };

  using Buckets = std::vector<Bucket, typename std::allocator_traits<Allocator>::template rebind_alloc<Bucket>>;

public:
  struct ConstIterator;
  using const_iterator = ConstIterator;

  struct ConstIterator {
    bool operator==(const_iterator const& rhs) const;
    bool operator!=(const_iterator const& rhs) const;

    const_iterator& operator++();
    const_iterator operator++(int);

    Value const& operator*() const;
    Value const* operator->() const;

    typename Buckets::const_iterator current{};
  };

  struct Iterator;
  using iterator = Iterator;

  struct Iterator {
    bool operator==(iterator const& rhs) const;
    bool operator!=(iterator const& rhs) const;

    iterator& operator++();
    iterator operator++(int);

    Value& operator*() const;
    Value* operator->() const;

    operator const_iterator() const;

    typename Buckets::iterator current{};
  };

  FlatHashTable(std::size_t bucket_count, GetKey const& key_extractor, Hash const& hash, Equals const& equal, Allocator const& alloc);
  FlatHashTable(FlatHashTable const&)
    requires (std::is_copy_constructible_v<GetKey>
      && std::is_copy_constructible_v<Hash> && std::is_copy_constructible_v<Equals>) = default;
  FlatHashTable(FlatHashTable const& other, Allocator const& alloc)
    requires (std::is_copy_constructible_v<Value> && std::is_copy_constructible_v<GetKey>
      && std::is_copy_constructible_v<Hash> && std::is_copy_constructible_v<Equals>)
      : buckets(other.buckets, alloc), filled_count(other.filled_count), get_key(other.get_key),
        hasher(other.hasher), equals(other.equals) {}
  FlatHashTable(FlatHashTable&& other)
      : buckets(std::move(other.buckets)), filled_count(std::exchange(other.filled_count, 0)),
        get_key(std::move(other.get_key)), hasher(std::move(other.hasher)), equals(std::move(other.equals)) {
    other.clear();
  }
  FlatHashTable(FlatHashTable&& other, Allocator const& alloc)
      : buckets(std::move(other.buckets), alloc), filled_count(std::exchange(other.filled_count, 0)),
        get_key(std::move(other.get_key)), hasher(std::move(other.hasher)), equals(std::move(other.equals)) {
    other.clear();
  }
  FlatHashTable& operator=(FlatHashTable const&)
    requires (std::is_copy_assignable_v<GetKey>
      && std::is_copy_assignable_v<Hash> && std::is_copy_assignable_v<Equals>) = default;
  FlatHashTable& operator=(FlatHashTable&& other) {
    if (this == &other)
      return *this;
    buckets = std::move(other.buckets);
    filled_count = std::exchange(other.filled_count, 0);
    get_key = std::move(other.get_key);
    hasher = std::move(other.hasher);
    equals = std::move(other.equals);
    other.clear();
    return *this;
  }

  iterator begin();
  iterator end();

  const_iterator begin() const;
  const_iterator end() const;

  bool empty() const noexcept;
  std::size_t size() const noexcept;
  void clear();

  std::pair<iterator, bool> insert(Value value);
  template <typename Factory>
    requires value_factory<Factory, Value>
  std::pair<iterator, bool> insert_with(Key const& key, Factory&& factory) {
    std::size_t hash = hasher(key) | filled_hash_bit;
    auto position = find_prehashed(key, hash);
    if (position != end())
      return {position, false};

    Value value(std::invoke(std::forward<Factory>(factory)));
    // Growth invalidates probe positions, but not the marked hash.
    if (buckets.empty() || filled_count >= load_limit(buckets.size() - 1))
      check_capacity(1);
    return insert_prehashed(std::move(value), hash);
  }

  iterator erase(const_iterator pos);
  iterator erase(const_iterator first, const_iterator last);
  Value extract_value(const_iterator pos) requires std::is_move_constructible_v<Value> {
    Value value(std::move(buckets[pos.current - buckets.cbegin()].storage.value));
    erase(pos);
    return value;
  }

  template <typename LookupKey>
    requires hash_lookup<Hash, Equals, GetKey, Value, LookupKey>
  const_iterator find(LookupKey const& key) const;
  template <typename LookupKey>
    requires hash_lookup<Hash, Equals, GetKey, Value, LookupKey>
  iterator find(LookupKey const& key);

  void reserve(std::size_t capacity);
  Allocator get_allocator() const noexcept;
  Hash hash_function() const noexcept(std::is_nothrow_copy_constructible_v<Hash>) { return hasher; }
  Equals key_eq() const noexcept(std::is_nothrow_copy_constructible_v<Equals>) { return equals; }

  bool operator==(FlatHashTable const& rhs) const;
  bool operator!=(FlatHashTable const& rhs) const;

private:
  static constexpr std::size_t min_capacity = 8;

  static constexpr std::size_t load_limit(std::size_t buckets) noexcept {
    return buckets / 10 * 7 + buckets % 10 * 7 / 10;
  }

  // Scans for the next bucket value that is non-empty
  template <typename Position>
  static Position scan(Position position) {
    while (position->is_empty())
      ++position;
    return position;
  }

  std::size_t hash_bucket(std::size_t hash) const;
  std::size_t bucket_error(std::size_t current, std::size_t target) const;
  void check_capacity(std::size_t additional_capacity);
  // Rebuilding unique resident values must neither rehash nor compare keys.
  template <bool unique = false>
  std::pair<iterator, bool> insert_prehashed(Value&& value, std::size_t hash);
  template <typename LookupKey>
    requires key_match<Equals, GetKey, Value, LookupKey>
  iterator find_prehashed(LookupKey const& key, std::size_t hash);
  template <typename Match>
    requires value_match<Match, Value>
  iterator find_prehashed_if(std::size_t hash, Match&& match);

  Buckets buckets;
  std::size_t filled_count = 0;

  [[no_unique_address]] GetKey get_key;
  [[no_unique_address]] Hash hasher;
  [[no_unique_address]] Equals equals;
};

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Bucket::Bucket() {
  this->hash = empty_hash_value;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Bucket::~Bucket() {
  if (is_filled())
    std::destroy_at(std::addressof(storage.value));
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Bucket::Bucket(Bucket const& rhs)
  requires std::is_copy_constructible_v<Value> {
  this->hash = rhs.hash;
  if (rhs.is_filled())
    std::construct_at(std::addressof(storage.value), rhs.storage.value);
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Bucket::Bucket(Bucket&& rhs)
  noexcept(std::is_nothrow_move_constructible_v<Value>) {
  this->hash = rhs.hash;
  if (rhs.is_filled())
    std::construct_at(std::addressof(storage.value), std::move(rhs.storage.value));
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Bucket::operator=(Bucket const& rhs) -> Bucket&
  requires std::is_copy_constructible_v<Value>
{
  if (this == &rhs)
    return *this;
  set_empty();
  if (rhs.is_filled())
    std::construct_at(std::addressof(storage.value), rhs.storage.value);
  hash = rhs.hash;
  return *this;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Bucket::operator=(Bucket&& rhs) -> Bucket& {
  if (this == &rhs)
    return *this;
  set_empty();
  if (rhs.is_filled())
    std::construct_at(std::addressof(storage.value), std::move(rhs.storage.value));
  hash = rhs.hash;
  return *this;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
void FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Bucket::set_filled(std::size_t hash, Value&& value) {
  if (is_filled())
    std::destroy_at(std::addressof(storage.value));
  std::construct_at(std::addressof(storage.value), std::move(value));
  this->hash = hash | filled_hash_bit;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
void FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Bucket::exchange_value(Value& value) {
  if constexpr (std::is_swappable_v<Value>) {
    using std::swap;
    swap(value, storage.value);
  } else {
    // Robin Hood displacement needs construction, not assignment. This also
    // supports nonassignable move-only keys and mapped values.
    Value displaced(std::move(storage.value));
    std::destroy_at(std::addressof(storage.value));
    std::construct_at(std::addressof(storage.value), std::move(value));
    std::destroy_at(std::addressof(value));
    std::construct_at(std::addressof(value), std::move(displaced));
  }
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
void FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Bucket::set_empty() {
  if (is_filled())
    std::destroy_at(std::addressof(storage.value));
  this->hash = empty_hash_value;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
void FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Bucket::set_end() {
  if (is_filled())
    std::destroy_at(std::addressof(storage.value));
  this->hash = end_hash_value;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
bool FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Bucket::is_empty() const {
  return this->hash == empty_hash_value;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
bool FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::ConstIterator::operator==(const_iterator const& rhs) const {
  return current == rhs.current;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
bool FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::ConstIterator::operator!=(const_iterator const& rhs) const {
  return current != rhs.current;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::ConstIterator::operator++() -> const_iterator& {
  current = scan(++current);
  return *this;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::ConstIterator::operator++(int) -> const_iterator {
  const_iterator copy(*this);
  operator++();
  return copy;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::ConstIterator::operator*() const -> Value const& {
  return current->storage.value;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::ConstIterator::operator->() const -> Value const* {
  return std::addressof(**this);
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
bool FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Iterator::operator==(iterator const& rhs) const {
  return current == rhs.current;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
bool FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Iterator::operator!=(iterator const& rhs) const {
  return current != rhs.current;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Iterator::operator++() -> iterator& {
  current = scan(++current);
  return *this;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Iterator::operator++(int) -> iterator {
  iterator copy(*this);
  operator++();
  return copy;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Iterator::operator*() const -> Value& {
  return current->storage.value;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Iterator::operator->() const -> Value* {
  return std::addressof(**this);
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Iterator::operator typename FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::const_iterator() const {
  return const_iterator{current};
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::FlatHashTable(std::size_t bucket_count, GetKey const& key_extractor,
                                                                          Hash const& hash, Equals const& equal,
                                                                          Allocator const& alloc)
    : buckets(alloc), get_key(key_extractor), hasher(hash), equals(equal) {
  if (bucket_count != 0)
    check_capacity(bucket_count);
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::begin() -> iterator {
  if (buckets.empty())
    return end();
  return iterator{scan(buckets.begin())};
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::end() -> iterator {
  return iterator{buckets.empty() ? buckets.end() : std::prev(buckets.end())};
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::begin() const -> const_iterator {
  return buckets.empty() ? end() : const_iterator{scan(buckets.begin())};
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::end() const -> const_iterator {
  return const_iterator{buckets.empty() ? buckets.end() : std::prev(buckets.end())};
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
bool FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::empty() const noexcept {
  return filled_count == 0;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
std::size_t FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::size() const noexcept {
  return filled_count;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
void FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::clear() {
  if (buckets.empty())
    return;

  for (std::size_t i = 0; i < buckets.size() - 1; ++i)
    buckets[i].set_empty();
  filled_count = 0;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::insert(Value value) -> std::pair<iterator, bool> {
  std::size_t hash = hasher(get_key(value)) | filled_hash_bit;
  // A duplicate must not reallocate: source ranges may borrow this table.
  if (buckets.empty() || filled_count >= load_limit(buckets.size() - 1)) {
    auto position = find_prehashed_if(hash, [&](Value& stored) { return equals(get_key(stored), get_key(value)); });
    if (position != end())
      return {position, false};
    check_capacity(1);
  }
  return insert_prehashed(std::move(value), hash);
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
template <bool unique>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::insert_prehashed(Value&& value, std::size_t hash)
  -> std::pair<iterator, bool> {
  std::size_t target_bucket = hash_bucket(hash);
  std::size_t current_bucket = target_bucket;
  std::size_t inserted_bucket = npos;

  while (true) {
    auto& target = buckets[current_bucket];
    if (target.is_filled()) {
      if constexpr (!unique) {
        // After the first displacement, every subsequent value came from
        // this table and is already known to be unique.
        if (target.hash == hash && inserted_bucket == npos && equals(get_key(target.storage.value), get_key(value)))
          return std::make_pair(iterator{buckets.begin() + current_bucket}, false);
      }

      std::size_t entry_target_bucket = hash_bucket(target.hash);
      std::size_t entry_error = bucket_error(current_bucket, entry_target_bucket);
      std::size_t add_error = bucket_error(current_bucket, target_bucket);
      if (add_error > entry_error) {
        if (inserted_bucket == npos)
          inserted_bucket = current_bucket;

        target.exchange_value(value);
        std::swap(hash, target.hash);
        target_bucket = entry_target_bucket;
      }
      current_bucket = hash_bucket(current_bucket + 1);

    } else {
      target.set_filled(hash, std::move(value));
      ++filled_count;
      if (inserted_bucket == npos)
        inserted_bucket = current_bucket;

      return std::make_pair(iterator{buckets.begin() + inserted_bucket}, true);
    }
  }
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::erase(const_iterator pos) -> iterator {
  auto const bucket_index = static_cast<std::size_t>(pos.current - buckets.begin());
  auto current = bucket_index;
  while (true) {
    auto next = hash_bucket(current + 1);
    auto& bucket = buckets[next];
    if (!bucket.is_filled() || bucket_error(next, bucket.hash) == 0)
      break;
    buckets[current].set_filled(bucket.hash, std::move(bucket.storage.value));
    current = next;
  }
  buckets[current].set_empty();
  --filled_count;
  // A wrapped prefix is not the successor of the last physical bucket.
  if (bucket_index == buckets.size() - 2)
    return end();
  return iterator{scan(buckets.begin() + bucket_index)};
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::erase(const_iterator first, const_iterator last) -> iterator {
  if (first == last)
    return buckets.empty() ? end() : iterator{buckets.begin() + (last.current - buckets.cbegin())};

  auto const first_index = static_cast<std::size_t>(first.current - buckets.begin());
  auto const last_index = static_cast<std::size_t>(last.current - buckets.begin());
  auto result_index = last_index;

  // Clear the original interval before relocating anything. Repeated erasure
  // loses both interval boundaries when a cluster wraps through bucket zero.
  for (auto index = first_index; index != last_index; ++index) {
    if (buckets[index].is_filled()) {
      buckets[index].set_empty();
      --filled_count;
    }
  }

  auto hole = first_index;
  auto current = hash_bucket(last_index);
  while (current != first_index && buckets[current].is_filled()) {
    auto& bucket = buckets[current];
    auto const displacement = bucket_error(current, bucket.hash);
    if (displacement == 0)
      break;
    // Robin Hood clusters have nondecreasing home buckets in cyclic order.
    // Holes before this value's home cannot be filled by later values either.
    while (hole != current
        && (bucket_error(current, hole) > displacement || buckets[hole].is_filled()))
      hole = hash_bucket(hole + 1);
    if (hole != current) {
      buckets[hole].set_filled(bucket.hash, std::move(bucket.storage.value));
      bucket.set_empty();
      if (current == last_index)
        result_index = hole;
      hole = hash_bucket(hole + 1);
    }
    current = hash_bucket(current + 1);
  }
  return iterator{buckets.begin() + result_index};
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
template <typename LookupKey>
  requires hash_lookup<Hash, Equals, GetKey, Value, LookupKey>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::find(LookupKey const& key) const -> const_iterator {
  return const_cast<FlatHashTable&>(*this).find(key);
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
template <typename LookupKey>
  requires hash_lookup<Hash, Equals, GetKey, Value, LookupKey>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::find(LookupKey const& key) -> iterator {
  if (buckets.empty())
    return end();
  return find_prehashed(key, hasher(key) | filled_hash_bit);
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
template <typename LookupKey>
  requires key_match<Equals, GetKey, Value, LookupKey>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::find_prehashed(LookupKey const& key, std::size_t hash)
  -> iterator {
  return find_prehashed_if(hash, [&](Value& stored) { return equals(get_key(stored), key); });
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
template <typename Match>
  requires value_match<Match, Value>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::find_prehashed_if(std::size_t hash, Match&& match)
  -> iterator {
  if (buckets.empty())
    return end();

  std::size_t target_bucket = hash_bucket(hash);
  std::size_t current_bucket = target_bucket;
  while (true) {
    auto& bucket = buckets[current_bucket];
    if (bucket.is_filled()) {
      if (bucket.hash == hash && std::invoke(match, bucket.storage.value))
        return iterator{buckets.begin() + current_bucket};

      std::size_t entry_error = bucket_error(current_bucket, bucket.hash);
      std::size_t find_error = bucket_error(current_bucket, target_bucket);

      if (find_error > entry_error)
        return end();

      current_bucket = hash_bucket(current_bucket + 1);

    } else {
      return end();
    }
  }
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
void FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::reserve(std::size_t capacity) {
  if (capacity > filled_count)
    check_capacity(capacity - filled_count);
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
Allocator FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::get_allocator() const noexcept {
  return buckets.get_allocator();
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
bool FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::operator==(FlatHashTable const& rhs) const {
  if (size() != rhs.size())
    return false;

  auto i = begin();
  auto j = rhs.begin();
  auto e = end();

  while (i != e) {
    if (*i != *j)
      return false;
    ++i;
    ++j;
  }

  return true;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
bool FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::operator!=(FlatHashTable const& rhs) const {
  return !operator==(rhs);
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
std::size_t FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::hash_bucket(std::size_t hash) const {
  return hash & (buckets.size() - 2);
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
std::size_t FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::bucket_error(std::size_t current,
                                                                                     std::size_t target) const {
  return hash_bucket(current - target);
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
void FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::check_capacity(std::size_t additional_capacity) {
  if (additional_capacity == 0)
    return;

  auto maximum = buckets.max_size();
  auto max_buckets = std::bit_floor(maximum == 0 ? 0 : maximum - 1);
  auto max_elements = load_limit(max_buckets);
  if (max_buckets < min_capacity || filled_count > max_elements || additional_capacity > max_elements - filled_count) {
    Star::fatal_error("FlatHashTable capacity exceeds max_size", false);
    return;
  }

  auto required = filled_count + additional_capacity;
  auto new_size = buckets.empty() ? min_capacity : buckets.size() - 1;
  while (required > load_limit(new_size))
    new_size *= 2;
  if (!buckets.empty() && new_size == buckets.size() - 1)
    return;

  // Allocate before changing the live table. The extra bucket terminates scans.
  Buckets old_buckets(new_size + 1, buckets.get_allocator());
  // Use allocator-provided spare capacity without another allocation.
  new_size = std::bit_floor(old_buckets.capacity() - 1);
  old_buckets.resize(new_size + 1);
  old_buckets[new_size].set_end();
  buckets.swap(old_buckets);
  filled_count = 0;

  for (auto& entry : old_buckets) {
    if (entry.is_filled())
      insert_prehashed<true>(std::move(entry.storage.value), entry.hash);
  }
}
}

export module star.flat_hash_table;

export namespace Star {
  using ::Star::FlatHashTable;
  using ::Star::transparent_hash_lookup;
  using ::Star::flat_value_constructible;
  using ::Star::flat_value_assignable;
}
