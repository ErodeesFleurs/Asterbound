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
      requires (std::is_copy_constructible_v<Value> && std::is_copy_assignable_v<Value>);
    Bucket& operator=(Bucket&& rhs);

    void setFilled(std::size_t hash, Value value);
    void setEmpty();
    void setEnd();

    Value const* valuePtr() const;
    Value* valuePtr();
    bool isEmpty() const;
    bool isEnd() const;

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

  iterator begin();
  iterator end();

  const_iterator begin() const;
  const_iterator end() const;

  std::size_t empty() const;
  std::size_t size() const;
  void clear();

  std::pair<iterator, bool> insert(Value value);
  template <typename Factory>
  std::pair<iterator, bool> insertWith(Key const& key, Factory&& factory) {
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
  Allocator getAllocator() const;

  bool operator==(FlatHashTable const& rhs) const;
  bool operator!=(FlatHashTable const& rhs) const;

private:
  static constexpr std::size_t MinCapacity = 8;
  static constexpr double MaxFillLevel = 0.7;

  // Scans for the next bucket value that is non-empty
  static Bucket* scan(Bucket* p);
  static Bucket const* scan(Bucket const* p);

  std::size_t hashBucket(std::size_t hash) const;
  std::size_t bucketError(std::size_t current, std::size_t target) const;
  void checkCapacity(std::size_t additionalCapacity);

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
  if (auto s = valuePtr())
    s->~Value();
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Bucket::Bucket(Bucket const& rhs)
  requires std::is_copy_constructible_v<Value> {
  this->hash = rhs.hash;
  if (auto o = rhs.valuePtr())
    new (&this->storage.value) Value(*o);
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Bucket::Bucket(Bucket&& rhs)
  noexcept(std::is_nothrow_move_constructible_v<Value>) {
  this->hash = rhs.hash;
  if (auto o = rhs.valuePtr())
    new (&this->storage.value) Value(std::move(*o));
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Bucket::operator=(Bucket const& rhs) -> Bucket&
  requires (std::is_copy_constructible_v<Value> && std::is_copy_assignable_v<Value>) {
  if (auto o = rhs.valuePtr()) {
    if (auto s = valuePtr())
      *s = *o;
    else
      new (&this->storage.value) Value(*o);
  } else {
    if (auto s = valuePtr())
      s->~Value();
  }
  this->hash = rhs.hash;
  return *this;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Bucket::operator=(Bucket&& rhs) -> Bucket& {
  if (auto o = rhs.valuePtr()) {
    if (auto s = valuePtr())
      *s = std::move(*o);
    else
      new (&this->storage.value) Value(std::move(*o));
  } else {
    if (auto s = valuePtr())
      s->~Value();
  }
  this->hash = rhs.hash;
  return *this;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
void FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Bucket::setFilled(std::size_t hash, Value value) {
  if (auto s = valuePtr())
    *s = std::move(value);
  else
    new (&this->storage.value) Value(std::move(value));
  this->hash = hash | FilledHashBit;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
void FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Bucket::setEmpty() {
  if (auto s = valuePtr())
    s->~Value();
  this->hash = EmptyHashValue;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
void FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Bucket::setEnd() {
  if (auto s = valuePtr())
    s->~Value();
  this->hash = EndHashValue;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
Value const* FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Bucket::valuePtr() const {
  if (hash & FilledHashBit)
    return &this->storage.value;
  return nullptr;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
Value* FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Bucket::valuePtr() {
  if (hash & FilledHashBit)
    return &this->storage.value;
  return nullptr;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
bool FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Bucket::isEmpty() const {
  return this->hash == EmptyHashValue;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
bool FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::Bucket::isEnd() const {
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
  return current->valuePtr();
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
  return current->valuePtr();
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
    checkCapacity(bucketCount);
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
    m_buckets[i].setEmpty();
  m_filledCount = 0;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::insert(Value value) -> std::pair<iterator, bool> {
  if (m_buckets.empty() || m_filledCount + 1 > (m_buckets.size() - 1) * MaxFillLevel)
    checkCapacity(1);

  std::size_t hash = m_hash(m_getKey(value)) | FilledHashBit;
  std::size_t targetBucket = hashBucket(hash);
  std::size_t currentBucket = targetBucket;
  std::size_t insertedBucket = NPos;

  while (true) {
    auto& target = m_buckets[currentBucket];
    if (auto entryValue = target.valuePtr()) {
      if (target.hash == hash && m_equals(m_getKey(*entryValue), m_getKey(value)))
        return std::make_pair(iterator{m_buckets.data() + currentBucket}, false);

      std::size_t entryTargetBucket = hashBucket(target.hash);
      std::size_t entryError = bucketError(currentBucket, entryTargetBucket);
      std::size_t addError = bucketError(currentBucket, targetBucket);
      if (addError > entryError) {
        if (insertedBucket == NPos)
          insertedBucket = currentBucket;

        if constexpr (requires { swap(value, *entryValue); })
          swap(value, *entryValue);
        else
          std::swap(value, *entryValue);
        std::swap(hash, target.hash);
        targetBucket = entryTargetBucket;
      }
      currentBucket = hashBucket(currentBucket + 1);

    } else {
      target.setFilled(hash, std::move(value));
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
    std::size_t nextBucketIndex = hashBucket(currentBucketIndex + 1);
    auto nextBucket = &m_buckets[nextBucketIndex];
    if (auto nextPtr = nextBucket->valuePtr()) {
      if (bucketError(nextBucketIndex, nextBucket->hash) > 0) {
        currentBucket->hash = nextBucket->hash;
        *currentBucket->valuePtr() = std::move(*nextPtr);
        currentBucketIndex = nextBucketIndex;
        currentBucket = nextBucket;
      } else {
        break;
      }
    } else {
      break;
    }
  }

  m_buckets[currentBucketIndex].setEmpty();
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
  std::size_t targetBucket = hashBucket(hash);
  std::size_t currentBucket = targetBucket;
  while (true) {
    auto& bucket = m_buckets[currentBucket];
    if (auto value = bucket.valuePtr()) {
      if (bucket.hash == hash && m_equals(m_getKey(*value), key))
        return iterator{m_buckets.data() + currentBucket};

      std::size_t entryError = bucketError(currentBucket, bucket.hash);
      std::size_t findError = bucketError(currentBucket, targetBucket);

      if (findError > entryError)
        return end();

      currentBucket = hashBucket(currentBucket + 1);

    } else {
      return end();
    }
  }
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
void FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::reserve(std::size_t capacity) {
  if (capacity > m_filledCount)
    checkCapacity(capacity - m_filledCount);
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
Allocator FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::getAllocator() const {
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
  while (p->isEmpty())
    ++p;
  return p;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
auto FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::scan(Bucket const* p) -> Bucket const* {
  while (p->isEmpty())
    ++p;
  return p;
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
std::size_t FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::hashBucket(std::size_t hash) const {
  return hash & (m_buckets.size() - 2);
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
std::size_t FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::bucketError(std::size_t current, std::size_t target) const {
  return hashBucket(current - target);
}

template <typename Value, typename Key, typename GetKey, typename Hash, typename Equals, typename Allocator>
void FlatHashTable<Value, Key, GetKey, Hash, Equals, Allocator>::checkCapacity(std::size_t additionalCapacity) {
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

  Buckets oldBuckets;
  std::swap(m_buckets, oldBuckets);

  // Leave an extra end entry when allocating buckets, so that iterators are
  // simpler and can simply iterate until they find something that is not an
  // empty entry.
  m_buckets.resize(newSize + 1);
  while (m_buckets.capacity() > newSize * 2 + 1) {
    newSize *= 2;
    m_buckets.resize(newSize + 1);
  }
  m_buckets[newSize].setEnd();

  m_filledCount = 0;

  for (auto& entry : oldBuckets) {
    if (auto ptr = entry.valuePtr())
      insert(std::move(*ptr));
  }
}

}

export module star.flat_hash_table;

export namespace Star {
  using ::Star::FlatHashTable;
}
