module;

import star.memory;
import std;

namespace Star {

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
  FlatHashTable(FlatHashTable const&) = default;
  FlatHashTable(FlatHashTable const& other, Allocator const& alloc)
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
  FlatHashTable& operator=(FlatHashTable const&) = default;
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

  std::size_t empty() const;
  std::size_t size() const;
  void clear();

  std::pair<iterator, bool> insert(Value value);
  template <typename Factory> std::pair<iterator, bool> insert_with(Key const& key, Factory&& factory) {
    auto position = find(key);
    if (position != end())
      return {position, false};
    return insert(std::invoke(std::forward<Factory>(factory)));
  }

  iterator erase(const_iterator pos);
  iterator erase(const_iterator first, const_iterator last);

  template <typename LookupKey>
  const_iterator find(LookupKey const& key) const;
  template <typename LookupKey>
  iterator find(LookupKey const& key);

  void reserve(std::size_t capacity);
  Allocator get_allocator() const;
  Hash hash_function() const { return m_hash; }
  Equals key_eq() const { return m_equals; }

  bool operator==(FlatHashTable const& rhs) const;
  bool operator!=(FlatHashTable const& rhs) const;

private:
  static constexpr std::size_t MinCapacity = 8;
  static constexpr double MaxFillLevel = 0.7;

  // Scans for the next bucket value that is non-empty
  static Bucket* scan(Bucket* p);
  static Bucket const* scan(Bucket const* p);

  std::size_t hash_bucket(std::size_t hash) const;
  std::size_t bucket_error(std::size_t current, std::size_t target) const;
  void check_capacity(std::size_t additionalCapacity);

  Buckets m_buckets;
  std::size_t m_filledCount = 0;

  GetKey m_getKey;
  Hash m_hash;
  Equals m_equals;
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
std::size_t FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::empty() const {
  return m_filledCount == 0;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
std::size_t FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::size() const {
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
  if (m_buckets.empty() || m_filledCount + 1 > (m_buckets.size() - 1) * MaxFillLevel)
    check_capacity(1);

  std::size_t hash = m_hash(m_getKey(value)) | FilledHashBit;
  std::size_t targetBucket = hash_bucket(hash);
  std::size_t currentBucket = targetBucket;
  std::size_t insertedBucket = NPos;

  while (true) {
    auto& target = m_buckets[currentBucket];
    if (auto entryValue = target.value_ptr()) {
      if (target.hash == hash && m_equals(m_getKey(*entryValue), m_getKey(value)))
        return std::make_pair(iterator{m_buckets.data() + currentBucket}, false);

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
  std::size_t bucketIndex = pos.current - m_buckets.data();
  std::size_t currentBucketIndex = bucketIndex;
  auto currentBucket = &m_buckets[currentBucketIndex];

  while (true) {
    std::size_t nextBucketIndex = hash_bucket(currentBucketIndex + 1);
    auto nextBucket = &m_buckets[nextBucketIndex];
    if (auto nextPtr = nextBucket->value_ptr()) {
      if (bucket_error(nextBucketIndex, nextBucket->hash) > 0) {
        currentBucket->set_filled(nextBucket->hash, std::move(*nextPtr));
        currentBucketIndex = nextBucketIndex;
        currentBucket = nextBucket;
      } else {
        break;
      }
    } else {
      break;
    }
  }

  m_buckets[currentBucketIndex].set_empty();
  --m_filledCount;

  return iterator{scan(m_buckets.data() + bucketIndex)};
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::erase(const_iterator first, const_iterator last) -> iterator {
  while (first != last)
    first = erase(first);
  return iterator{const_cast<Bucket*>(first.current)};
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
template <typename LookupKey>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::find(LookupKey const& key) const -> const_iterator {
  return const_cast<FlatHashTable*>(this)->find(key);
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
template <typename LookupKey>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::find(LookupKey const& key) -> iterator {
  if (m_buckets.empty())
    return end();

  std::size_t hash = m_hash(key) | FilledHashBit;
  std::size_t targetBucket = hash_bucket(hash);
  std::size_t currentBucket = targetBucket;
  while (true) {
    auto& bucket = m_buckets[currentBucket];
    if (auto value = bucket.value_ptr()) {
      if (bucket.hash == hash && m_equals(m_getKey(*value), key))
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
Allocator FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::get_allocator() const {
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
constexpr std::size_t FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::MinCapacity;

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
constexpr double FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::MaxFillLevel;

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

  std::size_t newSize;
  if (!m_buckets.empty())
    newSize = m_buckets.size() - 1;
  else
    newSize = MinCapacity;

  while (static_cast<double>((m_filledCount + additionalCapacity)) / static_cast<double>(newSize) > MaxFillLevel)
    newSize *= 2;

  if (newSize == m_buckets.size() - 1)
    return;

  Buckets oldBuckets(m_buckets.get_allocator());
  m_buckets.swap(oldBuckets);

  // Leave an extra end entry when allocating buckets, so that iterators are
  // simpler and can simply iterate until they find something that is not an
  // empty entry.
  m_buckets.resize(newSize + 1);
  while (m_buckets.capacity() > newSize * 2 + 1) {
    newSize *= 2;
    m_buckets.resize(newSize + 1);
  }
  m_buckets[newSize].set_end();

  m_filledCount = 0;

  for (auto& entry : oldBuckets) {
    if (auto ptr = entry.value_ptr())
      insert(std::move(*ptr));
  }
}
}

export module star.flat_hash_table;

export namespace Star {
  using ::Star::FlatHashTable;
}
