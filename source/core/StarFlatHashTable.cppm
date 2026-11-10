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
  static std::size_t const EmptyHashValue = 0;
  static std::size_t const EndHashValue = 1;
  static std::size_t const FilledHashBit = static_cast<std::size_t>(1) << (sizeof(std::size_t) * 8 - 1);

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

    Value const* value_ptr() const;
    Value* value_ptr();
    bool is_empty() const;
    bool is_end() const;

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
  struct const_iterator {
    bool operator==(const_iterator const& rhs) const;
    bool operator!=(const_iterator const& rhs) const;

    const_iterator& operator++();
    const_iterator operator++(int);

    Value const& operator*() const;
    Value const* operator->() const;

    Bucket const* current;
  };

  struct iterator {
    bool operator==(iterator const& rhs) const;
    bool operator!=(iterator const& rhs) const;

    iterator& operator++();
    iterator operator++(int);

    Value& operator*() const;
    Value* operator->() const;

    operator const_iterator() const;

    Bucket* current;
  };

  FlatHashTable(std::size_t bucketCount, GetKey const& getKey, Hash const& hash, Equals const& equal, Allocator const& alloc);
  FlatHashTable(FlatHashTable const&)
    requires (std::is_copy_constructible_v<GetKey>
      && std::is_copy_constructible_v<Hash> && std::is_copy_constructible_v<Equals>) = default;
  FlatHashTable(FlatHashTable const& other, Allocator const& alloc)
    requires (std::is_copy_constructible_v<Value> && std::is_copy_constructible_v<GetKey>
      && std::is_copy_constructible_v<Hash> && std::is_copy_constructible_v<Equals>)
      : m_buckets(other.m_buckets, alloc), m_filledCount(other.m_filledCount), m_getKey(other.m_getKey),
        m_hash(other.m_hash), m_equals(other.m_equals) {}
  FlatHashTable(FlatHashTable&& other)
      : m_buckets(std::move(other.m_buckets)), m_filledCount(std::exchange(other.m_filledCount, 0)),
        m_getKey(std::move(other.m_getKey)), m_hash(std::move(other.m_hash)), m_equals(std::move(other.m_equals)) {
    other.clear();
  }
  FlatHashTable(FlatHashTable&& other, Allocator const& alloc)
      : m_buckets(std::move(other.m_buckets), alloc), m_filledCount(std::exchange(other.m_filledCount, 0)),
        m_getKey(std::move(other.m_getKey)), m_hash(std::move(other.m_hash)), m_equals(std::move(other.m_equals)) {
    other.clear();
  }
  FlatHashTable& operator=(FlatHashTable const&)
    requires (std::is_copy_assignable_v<GetKey>
      && std::is_copy_assignable_v<Hash> && std::is_copy_assignable_v<Equals>) = default;
  FlatHashTable& operator=(FlatHashTable&& other) {
    if (this == &other)
      return *this;
    m_buckets = std::move(other.m_buckets);
    m_filledCount = std::exchange(other.m_filledCount, 0);
    m_getKey = std::move(other.m_getKey);
    m_hash = std::move(other.m_hash);
    m_equals = std::move(other.m_equals);
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
    std::size_t hash = m_hash(key) | FilledHashBit;
    auto position = find_prehashed(key, hash);
    if (position != end())
      return {position, false};

    Value value(std::invoke(std::forward<Factory>(factory)));
    // Growth invalidates probe positions, but not the marked hash.
    if (m_buckets.empty() || m_filledCount >= load_limit(m_buckets.size() - 1))
      check_capacity(1);
    return insert_prehashed(std::move(value), hash);
  }

  iterator erase(const_iterator pos);
  iterator erase(const_iterator first, const_iterator last);
  Value extract_value(const_iterator pos) requires std::is_move_constructible_v<Value> {
    Value value(std::move(*const_cast<Bucket*>(pos.current)->value_ptr()));
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
  Hash hash_function() const noexcept(std::is_nothrow_copy_constructible_v<Hash>) { return m_hash; }
  Equals key_eq() const noexcept(std::is_nothrow_copy_constructible_v<Equals>) { return m_equals; }

  bool operator==(FlatHashTable const& rhs) const;
  bool operator!=(FlatHashTable const& rhs) const;

private:
  static constexpr std::size_t MinCapacity = 8;

  static constexpr std::size_t load_limit(std::size_t buckets) noexcept {
    return buckets / 10 * 7 + buckets % 10 * 7 / 10;
  }

  // Scans for the next bucket value that is non-empty
  static Bucket* scan(Bucket* p);
  static Bucket const* scan(Bucket const* p);

  std::size_t hash_bucket(std::size_t hash) const;
  std::size_t bucket_error(std::size_t current, std::size_t target) const;
  void check_capacity(std::size_t additionalCapacity);
  // Rebuilding unique resident values must neither rehash nor compare keys.
  template <bool Unique = false>
  std::pair<iterator, bool> insert_prehashed(Value&& value, std::size_t hash);
  template <typename LookupKey>
    requires key_match<Equals, GetKey, Value, LookupKey>
  iterator find_prehashed(LookupKey const& key, std::size_t hash);
  template <typename Match>
    requires value_match<Match, Value>
  iterator find_prehashed_if(std::size_t hash, Match&& match);

  Buckets m_buckets;
  std::size_t m_filledCount = 0;

  [[no_unique_address]] GetKey m_getKey;
  [[no_unique_address]] Hash m_hash;
  [[no_unique_address]] Equals m_equals;
};

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Bucket::Bucket() {
  this->hash = EmptyHashValue;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Bucket::~Bucket() {
  if (auto s = value_ptr())
    s->~Value();
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Bucket::Bucket(Bucket const& rhs)
  requires std::is_copy_constructible_v<Value> {
  this->hash = rhs.hash;
  if (auto o = rhs.value_ptr())
    new (&this->storage.value) Value(*o);
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Bucket::Bucket(Bucket&& rhs)
  noexcept(std::is_nothrow_move_constructible_v<Value>) {
  this->hash = rhs.hash;
  if (auto o = rhs.value_ptr())
    new (&this->storage.value) Value(std::move(*o));
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Bucket::operator=(Bucket const& rhs) -> Bucket&
  requires std::is_copy_constructible_v<Value>
{
  if (this == &rhs)
    return *this;
  set_empty();
  if (auto source = rhs.value_ptr())
    std::construct_at(std::addressof(storage.value), *source);
  hash = rhs.hash;
  return *this;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Bucket::operator=(Bucket&& rhs) -> Bucket& {
  if (this == &rhs)
    return *this;
  set_empty();
  if (auto source = rhs.value_ptr())
    std::construct_at(std::addressof(storage.value), std::move(*source));
  hash = rhs.hash;
  return *this;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
void FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Bucket::set_filled(std::size_t hash, Value&& value) {
  if (auto destination = value_ptr())
    std::destroy_at(destination);
  std::construct_at(std::addressof(storage.value), std::move(value));
  this->hash = hash | FilledHashBit;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
void FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Bucket::exchange_value(Value& value) {
  if constexpr (std::is_swappable_v<Value>) {
    using std::swap;
    swap(value, *value_ptr());
  } else {
    // Robin Hood displacement needs construction, not assignment. This also
    // supports nonassignable move-only keys and mapped values.
    Value displaced(std::move(*value_ptr()));
    std::destroy_at(value_ptr());
    std::construct_at(std::addressof(storage.value), std::move(value));
    std::destroy_at(std::addressof(value));
    std::construct_at(std::addressof(value), std::move(displaced));
  }
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
void FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Bucket::set_empty() {
  if (auto s = value_ptr())
    s->~Value();
  this->hash = EmptyHashValue;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
void FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Bucket::set_end() {
  if (auto s = value_ptr())
    s->~Value();
  this->hash = EndHashValue;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
Value const* FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Bucket::value_ptr() const {
  if (hash & FilledHashBit)
    return &this->storage.value;
  return nullptr;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
Value* FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Bucket::value_ptr() {
  if (hash & FilledHashBit)
    return &this->storage.value;
  return nullptr;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
bool FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Bucket::is_empty() const {
  return this->hash == EmptyHashValue;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
bool FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Bucket::is_end() const {
  return this->hash == EndHashValue;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
bool FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::const_iterator::operator==(const_iterator const& rhs) const {
  return current == rhs.current;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
bool FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::const_iterator::operator!=(const_iterator const& rhs) const {
  return current != rhs.current;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::const_iterator::operator++() -> const_iterator& {
  current = scan(++current);
  return *this;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::const_iterator::operator++(int) -> const_iterator {
  const_iterator copy(*this);
  operator++();
  return copy;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::const_iterator::operator*() const -> Value const& {
  return *operator->();
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::const_iterator::operator->() const -> Value const* {
  return current->value_ptr();
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
bool FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::iterator::operator==(iterator const& rhs) const {
  return current == rhs.current;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
bool FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::iterator::operator!=(iterator const& rhs) const {
  return current != rhs.current;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::iterator::operator++() -> iterator& {
  current = scan(++current);
  return *this;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::iterator::operator++(int) -> iterator {
  iterator copy(*this);
  operator++();
  return copy;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::iterator::operator*() const -> Value& {
  return *operator->();
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::iterator::operator->() const -> Value* {
  return current->value_ptr();
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::iterator::operator typename FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::const_iterator() const {
  return const_iterator{current};
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::FlatHashTable(std::size_t bucketCount, GetKey const& getKey,
                                                                          Hash const& hash, Equals const& equal,
                                                                          Allocator const& alloc)
    : m_buckets(alloc), m_getKey(getKey), m_hash(hash), m_equals(equal) {
  if (bucketCount != 0)
    check_capacity(bucketCount);
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::begin() -> iterator {
  if (m_buckets.empty())
    return end();
  return iterator{scan(m_buckets.data())};
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::end() -> iterator {
  // An empty table has no sentinel bucket, and 'data() + size() - 1' would be a
  // pointer computed from a null pointer with a negative offset (UB, and UBSan
  // reports it for every begin()/end() on an empty table).
  if (m_buckets.empty())
    return iterator{nullptr};
  return iterator{m_buckets.data() + m_buckets.size() - 1};
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::begin() const -> const_iterator {
  return const_cast<FlatHashTable*>(this)->begin();
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::end() const -> const_iterator {
  return const_cast<FlatHashTable*>(this)->end();
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
bool FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::empty() const noexcept {
  return m_filledCount == 0;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
std::size_t FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::size() const noexcept {
  return m_filledCount;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
void FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::clear() {
  if (m_buckets.empty())
    return;

  for (std::size_t i = 0; i < m_buckets.size() - 1; ++i)
    m_buckets[i].set_empty();
  m_filledCount = 0;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::insert(Value value) -> std::pair<iterator, bool> {
  std::size_t hash = m_hash(m_getKey(value)) | FilledHashBit;
  // A duplicate must not reallocate: source ranges may borrow this table.
  if (m_buckets.empty() || m_filledCount >= load_limit(m_buckets.size() - 1)) {
    auto position = find_prehashed_if(hash, [&](Value& stored) { return m_equals(m_getKey(stored), m_getKey(value)); });
    if (position != end())
      return {position, false};
    check_capacity(1);
  }
  return insert_prehashed(std::move(value), hash);
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
template <bool Unique>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::insert_prehashed(Value&& value, std::size_t hash)
  -> std::pair<iterator, bool> {
  std::size_t targetBucket = hash_bucket(hash);
  std::size_t currentBucket = targetBucket;
  std::size_t insertedBucket = NPos;

  while (true) {
    auto& target = m_buckets[currentBucket];
    if (auto entryValue = target.value_ptr()) {
      if constexpr (!Unique) {
        // After the first displacement, every subsequent value came from
        // this table and is already known to be unique.
        if (target.hash == hash && insertedBucket == NPos && m_equals(m_getKey(*entryValue), m_getKey(value)))
          return std::make_pair(iterator{m_buckets.data() + currentBucket}, false);
      }

      std::size_t entryTargetBucket = hash_bucket(target.hash);
      std::size_t entryError = bucket_error(currentBucket, entryTargetBucket);
      std::size_t addError = bucket_error(currentBucket, targetBucket);
      if (addError > entryError) {
        if (insertedBucket == NPos)
          insertedBucket = currentBucket;

        target.exchange_value(value);
        std::swap(hash, target.hash);
        targetBucket = entryTargetBucket;
      }
      currentBucket = hash_bucket(currentBucket + 1);

    } else {
      target.set_filled(hash, std::move(value));
      ++m_filledCount;
      if (insertedBucket == NPos)
        insertedBucket = currentBucket;

      return std::make_pair(iterator{m_buckets.data() + insertedBucket}, true);
    }
  }
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::erase(const_iterator pos) -> iterator {
  auto const bucket_index = static_cast<std::size_t>(pos.current - m_buckets.data());
  auto current = bucket_index;
  while (true) {
    auto next = hash_bucket(current + 1);
    auto& bucket = m_buckets[next];
    auto value = bucket.value_ptr();
    if (!value || bucket_error(next, bucket.hash) == 0)
      break;
    m_buckets[current].set_filled(bucket.hash, std::move(*value));
    current = next;
  }
  m_buckets[current].set_empty();
  --m_filledCount;
  // A wrapped prefix is not the successor of the last physical bucket.
  if (bucket_index == m_buckets.size() - 2)
    return end();
  return iterator{scan(m_buckets.data() + bucket_index)};
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::erase(const_iterator first, const_iterator last) -> iterator {
  if (first == last)
    return iterator{const_cast<Bucket*>(last.current)};

  auto const first_index = static_cast<std::size_t>(first.current - m_buckets.data());
  auto const last_index = static_cast<std::size_t>(last.current - m_buckets.data());
  auto result_index = last_index;

  // Clear the original interval before relocating anything. Repeated erasure
  // loses both interval boundaries when a cluster wraps through bucket zero.
  for (auto index = first_index; index != last_index; ++index) {
    if (m_buckets[index].value_ptr()) {
      m_buckets[index].set_empty();
      --m_filledCount;
    }
  }

  auto hole = first_index;
  auto current = hash_bucket(last_index);
  while (current != first_index && m_buckets[current].value_ptr()) {
    auto& bucket = m_buckets[current];
    auto const displacement = bucket_error(current, bucket.hash);
    if (displacement == 0)
      break;
    // Robin Hood clusters have nondecreasing home buckets in cyclic order.
    // Holes before this value's home cannot be filled by later values either.
    while (hole != current
        && (bucket_error(current, hole) > displacement || m_buckets[hole].value_ptr()))
      hole = hash_bucket(hole + 1);
    if (hole != current) {
      m_buckets[hole].set_filled(bucket.hash, std::move(*bucket.value_ptr()));
      bucket.set_empty();
      if (current == last_index)
        result_index = hole;
      hole = hash_bucket(hole + 1);
    }
    current = hash_bucket(current + 1);
  }
  return iterator{m_buckets.data() + result_index};
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
template <typename LookupKey>
  requires hash_lookup<Hash, Equals, GetKey, Value, LookupKey>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::find(LookupKey const& key) const -> const_iterator {
  return const_cast<FlatHashTable*>(this)->find(key);
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
template <typename LookupKey>
  requires hash_lookup<Hash, Equals, GetKey, Value, LookupKey>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::find(LookupKey const& key) -> iterator {
  if (m_buckets.empty())
    return end();
  return find_prehashed(key, m_hash(key) | FilledHashBit);
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
template <typename LookupKey>
  requires key_match<Equals, GetKey, Value, LookupKey>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::find_prehashed(LookupKey const& key, std::size_t hash)
  -> iterator {
  return find_prehashed_if(hash, [&](Value& stored) { return m_equals(m_getKey(stored), key); });
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
template <typename Match>
  requires value_match<Match, Value>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::find_prehashed_if(std::size_t hash, Match&& match)
  -> iterator {
  if (m_buckets.empty())
    return end();

  std::size_t targetBucket = hash_bucket(hash);
  std::size_t currentBucket = targetBucket;
  while (true) {
    auto& bucket = m_buckets[currentBucket];
    if (auto value = bucket.value_ptr()) {
      if (bucket.hash == hash && std::invoke(match, *value))
        return iterator{m_buckets.data() + currentBucket};

      std::size_t entryError = bucket_error(currentBucket, bucket.hash);
      std::size_t findError = bucket_error(currentBucket, targetBucket);

      if (findError > entryError)
        return end();

      currentBucket = hash_bucket(currentBucket + 1);

    } else {
      return end();
    }
  }
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
void FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::reserve(std::size_t capacity) {
  if (capacity > m_filledCount)
    check_capacity(capacity - m_filledCount);
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
Allocator FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::get_allocator() const noexcept {
  return m_buckets.get_allocator();
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
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::scan(Bucket* p) -> Bucket* {
  while (p->is_empty())
    ++p;
  return p;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::scan(Bucket const* p) -> Bucket const* {
  while (p->is_empty())
    ++p;
  return p;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
std::size_t FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::hash_bucket(std::size_t hash) const {
  return hash & (m_buckets.size() - 2);
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
std::size_t FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::bucket_error(std::size_t current,
                                                                                     std::size_t target) const {
  return hash_bucket(current - target);
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
void FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::check_capacity(std::size_t additionalCapacity) {
  if (additionalCapacity == 0)
    return;

  auto maximum = m_buckets.max_size();
  auto maxBuckets = std::bit_floor(maximum == 0 ? 0 : maximum - 1);
  auto maxElements = load_limit(maxBuckets);
  if (maxBuckets < MinCapacity || m_filledCount > maxElements || additionalCapacity > maxElements - m_filledCount) {
    Star::fatal_error("FlatHashTable capacity exceeds max_size", false);
    return;
  }

  auto required = m_filledCount + additionalCapacity;
  auto newSize = m_buckets.empty() ? MinCapacity : m_buckets.size() - 1;
  while (required > load_limit(newSize))
    newSize *= 2;
  if (!m_buckets.empty() && newSize == m_buckets.size() - 1)
    return;

  // Allocate before changing the live table. The extra bucket terminates scans.
  Buckets oldBuckets(newSize + 1, m_buckets.get_allocator());
  // Use allocator-provided spare capacity without another allocation.
  newSize = std::bit_floor(oldBuckets.capacity() - 1);
  oldBuckets.resize(newSize + 1);
  oldBuckets[newSize].set_end();
  m_buckets.swap(oldBuckets);
  m_filledCount = 0;

  for (auto& entry : oldBuckets) {
    if (auto ptr = entry.value_ptr())
      insert_prehashed<true>(std::move(*ptr), entry.hash);
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
