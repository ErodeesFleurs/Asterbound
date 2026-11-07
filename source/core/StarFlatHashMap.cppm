module;

import std;

import star.format;
import star.flat_hash_table;
import star.hash;

namespace Star {

template <typename Key, typename Mapped, typename Hash = hash<Key>, typename Equals = std::equal_to<>, typename Allocator = std::allocator<Key>>
class FlatHashMap {
public:
  using key_type = Key;
  using mapped_type = Mapped;
  using value_type = std::pair<key_type const, mapped_type>;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using hasher = Hash;
  using key_equal = Equals;
  using allocator_type = Allocator;
  using reference = value_type&;
  using const_reference = value_type const&;
  using pointer = value_type*;
  using const_pointer = value_type const*;

private:
  using TableValue = std::pair<key_type, mapped_type>;

  struct GetKey {
    key_type const& operator()(TableValue const& value) const;
  };

  using Table = FlatHashTable<TableValue, key_type, GetKey, Hash, Equals,
                              typename std::allocator_traits<Allocator>::template rebind_alloc<TableValue>>;

public:
  struct const_iterator {
    using iterator_category = std::forward_iterator_tag;
    using value_type = typename FlatHashMap::value_type const;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;

    bool operator==(const_iterator const& rhs) const;
    bool operator!=(const_iterator const& rhs) const;

    const_iterator& operator++();
    const_iterator operator++(int);

    value_type& operator*() const;
    value_type* operator->() const;

    typename Table::const_iterator inner;
  };

  struct iterator {
    using iterator_category = std::forward_iterator_tag;
    using value_type = typename FlatHashMap::value_type;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;

    bool operator==(iterator const& rhs) const;
    bool operator!=(iterator const& rhs) const;

    iterator& operator++();
    iterator operator++(int);

    value_type& operator*() const;
    value_type* operator->() const;

    operator const_iterator() const;

    typename Table::iterator inner;
  };

  FlatHashMap();
  explicit FlatHashMap(std::size_t bucketCount, hasher const& hash = hasher(),
      key_equal const& equal = key_equal(), allocator_type const& alloc = allocator_type());
  FlatHashMap(std::size_t bucketCount, allocator_type const& alloc);
  FlatHashMap(std::size_t bucketCount, hasher const& hash, allocator_type const& alloc);
  explicit FlatHashMap(allocator_type const& alloc);

  template <typename InputIt>
  FlatHashMap(InputIt first, InputIt last, std::size_t bucketCount = 0,
      hasher const& hash = hasher(), key_equal const& equal = key_equal(),
      allocator_type const& alloc = allocator_type());
  template <typename InputIt>
  FlatHashMap(InputIt first, InputIt last, std::size_t bucketCount, allocator_type const& alloc);
  template <typename InputIt>
  FlatHashMap(InputIt first, InputIt last, std::size_t bucketCount,
      hasher const& hash, allocator_type const& alloc);

  FlatHashMap(FlatHashMap const& other);
  FlatHashMap(FlatHashMap const& other, allocator_type const& alloc);
  FlatHashMap(FlatHashMap&& other);
  FlatHashMap(FlatHashMap&& other, allocator_type const& alloc);

  FlatHashMap(std::initializer_list<value_type> init, std::size_t bucketCount = 0,
      hasher const& hash = hasher(), key_equal const& equal = key_equal(),
      allocator_type const& alloc = allocator_type());
  FlatHashMap(std::initializer_list<value_type> init, std::size_t bucketCount, allocator_type const& alloc);
  FlatHashMap(std::initializer_list<value_type> init, std::size_t bucketCount, hasher const& hash,
      allocator_type const& alloc);

  FlatHashMap& operator=(FlatHashMap const& other);
  FlatHashMap& operator=(FlatHashMap&& other);
  FlatHashMap& operator=(std::initializer_list<value_type> init);

  iterator begin();
  iterator end();

  const_iterator begin() const;
  const_iterator end() const;

  const_iterator cbegin() const;
  const_iterator cend() const;

  std::size_t empty() const;
  std::size_t size() const;
  void clear();

  std::pair<iterator, bool> insert(value_type const& value);
  template <typename T, typename = std::enable_if_t<std::is_constructible_v<TableValue, T&&>>>
  std::pair<iterator, bool> insert(T&& value);
  iterator insert(const_iterator hint, value_type const& value);
  template <typename T, typename = std::enable_if_t<std::is_constructible_v<TableValue, T&&>>>
  iterator insert(const_iterator hint, T&& value);
  template <typename InputIt>
  void insert(InputIt first, InputIt last);
  void insert(std::initializer_list<value_type> init);

  template <typename... Args>
  std::pair<iterator, bool> emplace(Args&&... args);
  template <typename... Args>
  iterator emplace_hint(const_iterator hint, Args&&... args);

  template <typename... Args>
  std::pair<iterator, bool> try_emplace(key_type const& key, Args&&... args) {
    auto result = m_table.insertWith(key, [&] {
      return TableValue(std::piecewise_construct, std::forward_as_tuple(key),
          std::forward_as_tuple(std::forward<Args>(args)...));
    });
    return {iterator{result.first}, result.second};
  }

  template <typename... Args>
  std::pair<iterator, bool> try_emplace(key_type&& key, Args&&... args) {
    auto result = m_table.insertWith(key, [&] {
      return TableValue(std::piecewise_construct, std::forward_as_tuple(std::move(key)),
          std::forward_as_tuple(std::forward<Args>(args)...));
    });
    return {iterator{result.first}, result.second};
  }

  template <typename Value>
  std::pair<iterator, bool> insert_or_assign(key_type const& key, Value&& value) {
    auto result = try_emplace(key, std::forward<Value>(value));
    if (!result.second)
      result.first->second = std::forward<Value>(value);
    return result;
  }

  template <typename Value>
  std::pair<iterator, bool> insert_or_assign(key_type&& key, Value&& value) {
    auto result = try_emplace(std::move(key), std::forward<Value>(value));
    if (!result.second)
      result.first->second = std::forward<Value>(value);
    return result;
  }

  iterator erase(const_iterator pos);
  iterator erase(const_iterator first, const_iterator last);
  std::size_t erase(key_type const& key);

  [[nodiscard]] std::expected<std::reference_wrapper<mapped_type>, StarException> at(key_type const& key);
  [[nodiscard]] std::expected<std::reference_wrapper<mapped_type const>, StarException> at(key_type const& key) const;

  mapped_type& operator[](key_type const& key);
  mapped_type& operator[](key_type&& key);

  std::size_t count(key_type const& key) const;
  const_iterator find(key_type const& key) const;
  iterator find(key_type const& key);

  template <typename LookupKey>
    requires requires(hasher const& h, key_equal const& equal, key_type const& stored, LookupKey const& query) {
      typename hasher::is_transparent;
      typename key_equal::is_transparent;
      { h(query) } -> std::convertible_to<std::size_t>;
      { equal(stored, query) } -> std::convertible_to<bool>;
    }
  const_iterator find(LookupKey const& key) const { return const_iterator{m_table.find(key)}; }

  template <typename LookupKey>
    requires requires(hasher const& h, key_equal const& equal, key_type const& stored, LookupKey const& query) {
      typename hasher::is_transparent;
      typename key_equal::is_transparent;
      { h(query) } -> std::convertible_to<std::size_t>;
      { equal(stored, query) } -> std::convertible_to<bool>;
    }
  iterator find(LookupKey const& key) { return iterator{m_table.find(key)}; }
  std::pair<iterator, iterator> equal_range(key_type const& key);
  std::pair<const_iterator, const_iterator> equal_range(key_type const& key) const;

  void reserve(std::size_t capacity);

  bool operator==(FlatHashMap const& rhs) const;
  bool operator!=(FlatHashMap const& rhs) const;

private:
  Table m_table;
};

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::GetKey::operator()(TableValue const& value) const -> key_type const& {
  return value.first;
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
bool FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::const_iterator::operator==(const_iterator const& rhs) const {
  return inner == rhs.inner;
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
bool FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::const_iterator::operator!=(const_iterator const& rhs) const {
  return inner != rhs.inner;
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::const_iterator::operator++() -> const_iterator& {
  ++inner;
  return *this;
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::const_iterator::operator++(int) -> const_iterator {
  const_iterator copy(*this);
  ++*this;
  return copy;
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::const_iterator::operator*() const -> value_type& {
  return *operator->();
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::const_iterator::operator->() const -> value_type* {
  return reinterpret_cast<value_type*>(&*inner);
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
bool FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::iterator::operator==(iterator const& rhs) const {
  return inner == rhs.inner;
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
bool FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::iterator::operator!=(iterator const& rhs) const {
  return inner != rhs.inner;
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::iterator::operator++() -> iterator& {
  ++inner;
  return *this;
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::iterator::operator++(int) -> iterator {
  iterator copy(*this);
  operator++();
  return copy;
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::iterator::operator*() const -> value_type& {
  return *operator->();
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::iterator::operator->() const -> value_type* {
  return reinterpret_cast<value_type*>(&*inner);
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::iterator::operator typename FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::const_iterator() const {
  return const_iterator{inner};
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::FlatHashMap()
  : FlatHashMap(0) {}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::FlatHashMap(std::size_t bucketCount, hasher const& hash,
    key_equal const& equal, allocator_type const& alloc)
  : m_table(bucketCount, GetKey(), hash, equal, alloc) {}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::FlatHashMap(std::size_t bucketCount, allocator_type const& alloc)
  : FlatHashMap(bucketCount, hasher(), key_equal(), alloc) {}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::FlatHashMap(std::size_t bucketCount, hasher const& hash,
    allocator_type const& alloc)
  : FlatHashMap(bucketCount, hash, key_equal(), alloc) {}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::FlatHashMap(allocator_type const& alloc)
  : FlatHashMap(0, hasher(), key_equal(), alloc) {}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
template <typename InputIt>
FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::FlatHashMap(InputIt first, InputIt last, std::size_t bucketCount,
    hasher const& hash, key_equal const& equal, allocator_type const& alloc)
  : FlatHashMap(bucketCount, hash, equal, alloc) {
  insert(first, last);
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
template <typename InputIt>
FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::FlatHashMap(InputIt first, InputIt last, std::size_t bucketCount,
    allocator_type const& alloc)
  : FlatHashMap(first, last, bucketCount, hasher(), key_equal(), alloc) {}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
template <typename InputIt>
FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::FlatHashMap(InputIt first, InputIt last, std::size_t bucketCount,
    hasher const& hash, allocator_type const& alloc)
  : FlatHashMap(first, last, bucketCount, hash, key_equal(), alloc) {}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::FlatHashMap(FlatHashMap const& other)
  : FlatHashMap(other, other.m_table.getAllocator()) {}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::FlatHashMap(FlatHashMap const& other, allocator_type const& alloc)
  : FlatHashMap(alloc) {
  operator=(other);
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::FlatHashMap(FlatHashMap&& other)
  : FlatHashMap(std::move(other), other.m_table.getAllocator()) {}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::FlatHashMap(FlatHashMap&& other, allocator_type const& alloc)
  : FlatHashMap(alloc) {
  operator=(std::move(other));
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::FlatHashMap(std::initializer_list<value_type> init, std::size_t bucketCount, hasher const& hash,
    key_equal const& equal, allocator_type const& alloc)
  : FlatHashMap(bucketCount, hash, equal, alloc) {
  operator=(init);
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::FlatHashMap(std::initializer_list<value_type> init, std::size_t bucketCount,
    allocator_type const& alloc)
  : FlatHashMap(init, bucketCount, hasher(), key_equal(), alloc) {}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::FlatHashMap(std::initializer_list<value_type> init, std::size_t bucketCount, hasher const& hash,
    allocator_type const& alloc)
  : FlatHashMap(init, bucketCount, hash, key_equal(), alloc) {}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::operator=(FlatHashMap const& other) -> FlatHashMap& {
  m_table.clear();
  m_table.reserve(other.size());
  for (auto const& p : other)
    m_table.insert(p);
  return *this;
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::operator=(FlatHashMap&& other) -> FlatHashMap& {
  m_table = std::move(other.m_table);
  return *this;
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::operator=(std::initializer_list<value_type> init) -> FlatHashMap& {
  clear();
  insert(init.begin(), init.end());
  return *this;
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::begin() -> iterator {
  return iterator{m_table.begin()};
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::end() -> iterator {
  return iterator{m_table.end()};
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::begin() const -> const_iterator {
  return const_iterator{m_table.begin()};
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::end() const -> const_iterator {
  return const_iterator{m_table.end()};
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::cbegin() const -> const_iterator {
  return begin();
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::cend() const -> const_iterator {
  return end();
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
std::size_t FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::empty() const {
  return m_table.empty();
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
std::size_t FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::size() const {
  return m_table.size();
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
void FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::clear() {
  m_table.clear();
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::insert(value_type const& value) -> std::pair<iterator, bool> {
  auto res = m_table.insert(TableValue(value));
  return {iterator{res.first}, res.second};
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
template <typename T, typename>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::insert(T&& value) -> std::pair<iterator, bool> {
  auto res = m_table.insert(TableValue(std::forward<T&&>(value)));
  return {iterator{res.first}, res.second};
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::insert(const_iterator hint, value_type const& value) -> iterator {
  return insert(hint, TableValue(value));
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
template <typename T, typename>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::insert(const_iterator, T&& value) -> iterator {
  return insert(std::forward<T&&>(value)).first;
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
template <typename InputIt>
void FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::insert(InputIt first, InputIt last) {
  if constexpr (std::forward_iterator<InputIt>)
    m_table.reserve(m_table.size() + std::distance(first, last));
  for (auto i = first; i != last; ++i)
    m_table.insert(*i);
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
void FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::insert(std::initializer_list<value_type> init) {
  insert(init.begin(), init.end());
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
template <typename... Args>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::emplace(Args&&... args) -> std::pair<iterator, bool> {
  return insert(TableValue(std::forward<Args>(args)...));
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
template <typename... Args>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::emplace_hint(const_iterator hint, Args&&... args) -> iterator {
  return insert(hint, TableValue(std::forward<Args>(args)...));
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::erase(const_iterator pos) -> iterator {
  return iterator{m_table.erase(pos.inner)};
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::erase(const_iterator first, const_iterator last) -> iterator {
  return iterator{m_table.erase(first.inner, last.inner)};
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
std::size_t FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::erase(key_type const& key) {
  auto i = m_table.find(key);
  if (i != m_table.end()) {
    m_table.erase(i);
    return 1;
  }
  return 0;
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::at(key_type const& key) -> std::expected<std::reference_wrapper<mapped_type>, StarException> {
  auto i = m_table.find(key);
  if (i == m_table.end())
    return std::unexpected(StarException("no such key in FlatHashMap"));
  return std::reference_wrapper<Mapped>(i->second);
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::at(key_type const& key) const -> std::expected<std::reference_wrapper<mapped_type const>, StarException> {
  auto i = m_table.find(key);
  if (i == m_table.end())
    return std::unexpected(StarException("no such key in FlatHashMap"));
  return std::reference_wrapper<Mapped const>(i->second);
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::operator[](key_type const& key) -> mapped_type& {
  return try_emplace(key).first->second;
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::operator[](key_type&& key) -> mapped_type& {
  return try_emplace(std::move(key)).first->second;
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
std::size_t FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::count(key_type const& key) const {
  if (m_table.find(key) != m_table.end())
    return 1;
  else
    return 0;
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::find(key_type const& key) const -> const_iterator {
  return const_iterator{m_table.find(key)};
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::find(key_type const& key) -> iterator {
  return iterator{m_table.find(key)};
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::equal_range(key_type const& key) -> std::pair<iterator, iterator> {
  auto i = find(key);
  if (i != end()) {
    auto j = i;
    ++j;
    return {i, j};
  } else {
    return {i, i};
  }
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::equal_range(key_type const& key) const -> std::pair<const_iterator, const_iterator> {
  auto i = find(key);
  if (i != end()) {
    auto j = i;
    ++j;
    return {i, j};
  } else {
    return {i, i};
  }
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
void FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::reserve(std::size_t capacity) {
  m_table.reserve(capacity);
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
bool FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::operator==(FlatHashMap const& rhs) const {
  return m_table == rhs.m_table;
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
bool FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::operator!=(FlatHashMap const& rhs) const {
  return m_table != rhs.m_table;
}

}

export module star.flat_hash_map;

export namespace Star {
  using ::Star::FlatHashMap;
}
