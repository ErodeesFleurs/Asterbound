module;

import std;

import star.format;
import star.flat_hash_table;
import star.hash;

namespace Star {

template <typename Key, typename Mapped, typename... Args>
concept flat_map_value_constructible = flat_value_constructible<std::pair<Key, Mapped>, Args...>;

// Only genuine pairs with an unchanged key can be probed before conversion.
// Arbitrary converting inputs may produce a different key during construction.
template <typename Pair, typename Key>
concept flat_map_exact_key_pair = requires {
  typename std::remove_cvref_t<Pair>::first_type;
  typename std::remove_cvref_t<Pair>::second_type;
} && std::same_as<std::remove_cvref_t<Pair>,
    std::pair<typename std::remove_cvref_t<Pair>::first_type,
        typename std::remove_cvref_t<Pair>::second_type>>
  && std::same_as<std::remove_cvref_t<typename std::remove_cvref_t<Pair>::first_type>, Key>
  && requires(Pair&& pair) {
    { pair.first } -> std::convertible_to<Key const&>;
  };

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
  // References bind the genuine mutable-key storage without pretending it is
  // a different pair specialization. Only mapped values are publicly mutable.
  using reference = std::pair<key_type const&, mapped_type&>;
  using const_reference = std::pair<key_type const&, mapped_type const&>;

  template <typename Reference>
    requires (std::same_as<Reference, reference> || std::same_as<Reference, const_reference>)
  struct arrow_proxy {
    Reference entry;
    Reference const* operator->() const { return std::addressof(entry); }
  };

  using pointer = arrow_proxy<reference>;
  using const_pointer = arrow_proxy<const_reference>;

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
    using value_type = typename FlatHashMap::value_type;
    using difference_type = std::ptrdiff_t;
    using pointer = typename FlatHashMap::const_pointer;
    using reference = typename FlatHashMap::const_reference;

    bool operator==(const_iterator const& rhs) const;
    bool operator!=(const_iterator const& rhs) const;

    const_iterator& operator++();
    const_iterator operator++(int);

    reference operator*() const;
    pointer operator->() const;

    typename Table::const_iterator inner{};
  };

  struct iterator {
    using iterator_category = std::forward_iterator_tag;
    using value_type = typename FlatHashMap::value_type;
    using difference_type = std::ptrdiff_t;
    using pointer = typename FlatHashMap::pointer;
    using reference = typename FlatHashMap::reference;

    bool operator==(iterator const& rhs) const;
    bool operator!=(iterator const& rhs) const;

    iterator& operator++();
    iterator operator++(int);

    reference operator*() const;
    pointer operator->() const;

    operator const_iterator() const;

    typename Table::iterator inner{};
  };

  FlatHashMap();
  explicit FlatHashMap(std::size_t bucketCount, hasher const& hash = hasher(),
      key_equal const& equal = key_equal(), allocator_type const& alloc = allocator_type());
  FlatHashMap(std::size_t bucketCount, allocator_type const& alloc);
  FlatHashMap(std::size_t bucketCount, hasher const& hash, allocator_type const& alloc);
  explicit FlatHashMap(allocator_type const& alloc);

  template <std::input_iterator InputIt, std::sentinel_for<InputIt> Sentinel = InputIt>
    requires flat_map_value_constructible<Key, Mapped, std::iter_reference_t<InputIt>>
  FlatHashMap(InputIt first, Sentinel last, std::size_t bucketCount = 0,
      hasher const& hash = hasher(), key_equal const& equal = key_equal(),
      allocator_type const& alloc = allocator_type());
  template <std::input_iterator InputIt, std::sentinel_for<InputIt> Sentinel = InputIt>
    requires flat_map_value_constructible<Key, Mapped, std::iter_reference_t<InputIt>>
  FlatHashMap(InputIt first, Sentinel last, std::size_t bucketCount, allocator_type const& alloc);
  template <std::input_iterator InputIt, std::sentinel_for<InputIt> Sentinel = InputIt>
    requires flat_map_value_constructible<Key, Mapped, std::iter_reference_t<InputIt>>
  FlatHashMap(InputIt first, Sentinel last, std::size_t bucketCount,
      hasher const& hash, allocator_type const& alloc);

  template <std::ranges::input_range Range>
    requires flat_map_value_constructible<Key, Mapped, std::ranges::range_reference_t<Range>>
  FlatHashMap(std::from_range_t, Range&& range, std::size_t bucketCount = 0,
      hasher const& hash = hasher(), key_equal const& equal = key_equal(),
      allocator_type const& alloc = allocator_type())
    : FlatHashMap(bucketCount, hash, equal, alloc) {
    if constexpr (std::ranges::sized_range<Range>)
      m_table.reserve(std::ranges::size(range));
    insert_range(std::forward<Range>(range));
  }
  template <std::ranges::input_range Range>
    requires flat_map_value_constructible<Key, Mapped, std::ranges::range_reference_t<Range>>
  FlatHashMap(std::from_range_t, Range&& range, std::size_t bucketCount, allocator_type const& alloc)
    : FlatHashMap(std::from_range, std::forward<Range>(range), bucketCount, hasher(), key_equal(), alloc) {}
  template <std::ranges::input_range Range>
    requires flat_map_value_constructible<Key, Mapped, std::ranges::range_reference_t<Range>>
  FlatHashMap(std::from_range_t, Range&& range, std::size_t bucketCount,
      hasher const& hash, allocator_type const& alloc)
    : FlatHashMap(std::from_range, std::forward<Range>(range), bucketCount, hash, key_equal(), alloc) {}

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

  bool empty() const noexcept;
  std::size_t size() const;
  void clear();

  std::pair<iterator, bool> insert(value_type const& value)
    requires std::is_constructible_v<TableValue, value_type const&>;
  template <typename T>
    requires flat_map_value_constructible<Key, Mapped, T&&>
  std::pair<iterator, bool> insert(T&& value);
  iterator insert(const_iterator hint, value_type const& value)
    requires std::is_constructible_v<TableValue, value_type const&>;
  template <typename T>
    requires flat_map_value_constructible<Key, Mapped, T&&>
  iterator insert(const_iterator hint, T&& value);
  template <std::input_iterator InputIt, std::sentinel_for<InputIt> Sentinel = InputIt>
    requires flat_map_value_constructible<Key, Mapped, std::iter_reference_t<InputIt>>
  void insert(InputIt first, Sentinel last);
  template <std::ranges::input_range Range>
    requires flat_map_value_constructible<Key, Mapped, std::ranges::range_reference_t<Range>>
  void insert_range(Range&& range) {
    insert(std::ranges::begin(range), std::ranges::end(range));
  }
  void insert(std::initializer_list<value_type> init);

  template <typename... Args>
    requires flat_map_value_constructible<Key, Mapped, Args&&...>
  std::pair<iterator, bool> emplace(Args&&... args);
  template <typename KeyArg, typename ValueArg>
    requires (flat_map_value_constructible<Key, Mapped, KeyArg&&, ValueArg&&>
      && std::same_as<std::remove_cvref_t<KeyArg>, key_type>
      && std::convertible_to<KeyArg&&, key_type const&>)
  std::pair<iterator, bool> emplace(KeyArg&& key, ValueArg&& value) {
    auto result = m_table.insert_with(key, [&] {
      return TableValue(std::forward<KeyArg>(key), std::forward<ValueArg>(value));
    });
    return {iterator{result.first}, result.second};
  }
  template <typename... Args>
    requires flat_map_value_constructible<Key, Mapped, Args&&...>
  iterator emplace_hint(const_iterator hint, Args&&... args);

  template <typename... Args>
    requires (flat_value_constructible<key_type, key_type const&>
      && flat_value_constructible<mapped_type, Args&&...>)
  std::pair<iterator, bool> try_emplace(key_type const& key, Args&&... args) {
    auto result = m_table.insert_with(key, [&] {
      return TableValue(std::piecewise_construct, std::forward_as_tuple(key),
          std::forward_as_tuple(std::forward<Args>(args)...));
    });
    return {iterator{result.first}, result.second};
  }

  template <typename... Args>
    requires (flat_value_constructible<key_type, key_type&&>
      && flat_value_constructible<mapped_type, Args&&...>)
  std::pair<iterator, bool> try_emplace(key_type&& key, Args&&... args) {
    auto result = m_table.insert_with(key, [&] {
      return TableValue(std::piecewise_construct, std::forward_as_tuple(std::move(key)),
          std::forward_as_tuple(std::forward<Args>(args)...));
    });
    return {iterator{result.first}, result.second};
  }

  template <typename Value>
    requires (flat_value_constructible<key_type, key_type const&>
      && flat_value_constructible<mapped_type, Value&&>
      && flat_value_assignable<mapped_type, Value&&>)
  std::pair<iterator, bool> insert_or_assign(key_type const& key, Value&& value) {
    auto result = try_emplace(key, std::forward<Value>(value));
    if (!result.second)
      result.first->second = std::forward<Value>(value);
    return result;
  }

  template <typename Value>
    requires (flat_value_constructible<key_type, key_type&&>
      && flat_value_constructible<mapped_type, Value&&>
      && flat_value_assignable<mapped_type, Value&&>)
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
    requires transparent_hash_lookup<hasher, key_equal, key_type, LookupKey>
  const_iterator find(LookupKey const& key) const {
    return const_iterator{m_table.find(key)};
  }

  template <typename LookupKey>
    requires transparent_hash_lookup<hasher, key_equal, key_type, LookupKey>
  iterator find(LookupKey const& key) {
    return iterator{m_table.find(key)};
  }
  std::pair<iterator, iterator> equal_range(key_type const& key);
  std::pair<const_iterator, const_iterator> equal_range(key_type const& key) const;

  void reserve(std::size_t capacity);
  hasher hash_function() const noexcept(std::is_nothrow_copy_constructible_v<hasher>) { return m_table.hash_function(); }
  key_equal key_eq() const noexcept(std::is_nothrow_copy_constructible_v<key_equal>) { return m_table.key_eq(); }
  allocator_type get_allocator() const noexcept { return m_table.get_allocator(); }

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
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::const_iterator::operator*() const -> reference {
  return {inner->first, inner->second};
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::const_iterator::operator->() const -> pointer {
  return pointer{**this};
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
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::iterator::operator*() const -> reference {
  return {inner->first, inner->second};
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::iterator::operator->() const -> pointer {
  return pointer{**this};
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
template <std::input_iterator InputIt, std::sentinel_for<InputIt> Sentinel>
  requires flat_map_value_constructible<Key, Mapped, std::iter_reference_t<InputIt>>
FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::FlatHashMap(InputIt first, Sentinel last, std::size_t bucketCount,
    hasher const& hash, key_equal const& equal, allocator_type const& alloc)
  : FlatHashMap(bucketCount, hash, equal, alloc) {
  // Only fresh owners may reserve ahead of insertion: a borrowed self-range
  // must remain valid when every input is already present.
  if constexpr (std::sized_sentinel_for<Sentinel, InputIt>)
    m_table.reserve(static_cast<size_type>(last - first));
  insert(std::move(first), std::move(last));
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
template <std::input_iterator InputIt, std::sentinel_for<InputIt> Sentinel>
  requires flat_map_value_constructible<Key, Mapped, std::iter_reference_t<InputIt>>
FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::FlatHashMap(InputIt first, Sentinel last, std::size_t bucketCount,
    allocator_type const& alloc)
  : FlatHashMap(std::move(first), std::move(last), bucketCount, hasher(), key_equal(), alloc) {}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
template <std::input_iterator InputIt, std::sentinel_for<InputIt> Sentinel>
  requires flat_map_value_constructible<Key, Mapped, std::iter_reference_t<InputIt>>
FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::FlatHashMap(InputIt first, Sentinel last, std::size_t bucketCount,
    hasher const& hash, allocator_type const& alloc)
  : FlatHashMap(std::move(first), std::move(last), bucketCount, hash, key_equal(), alloc) {}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::FlatHashMap(FlatHashMap const& other) : m_table(other.m_table) {}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::FlatHashMap(FlatHashMap const& other, allocator_type const& alloc)
    : m_table(other.m_table, alloc) {}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::FlatHashMap(FlatHashMap&& other)
    : m_table(std::move(other.m_table)) {}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::FlatHashMap(FlatHashMap&& other, allocator_type const& alloc)
    : m_table(std::move(other.m_table), alloc) {}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::FlatHashMap(std::initializer_list<value_type> init, std::size_t bucketCount, hasher const& hash,
    key_equal const& equal, allocator_type const& alloc)
  : FlatHashMap(init.begin(), init.end(), bucketCount, hash, equal, alloc) {}

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
  m_table = other.m_table;
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
bool FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::empty() const noexcept {
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
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::insert(value_type const& value) -> std::pair<iterator, bool>
  requires std::is_constructible_v<TableValue, value_type const&> {
  auto result = m_table.insert_with(value.first, [&] { return TableValue(value); });
  return {iterator{result.first}, result.second};
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
template <typename T>
  requires flat_map_value_constructible<Key, Mapped, T&&>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::insert(T&& value) -> std::pair<iterator, bool> {
  if constexpr (flat_map_exact_key_pair<T, key_type>) {
    auto result = m_table.insert_with(value.first, [&] { return TableValue(std::forward<T>(value)); });
    return {iterator{result.first}, result.second};
  } else {
    // Conversion may change the key or expose no pair members at all.
    auto result = m_table.insert(TableValue(std::forward<T>(value)));
    return {iterator{result.first}, result.second};
  }
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::insert(const_iterator, value_type const& value) -> iterator
  requires std::is_constructible_v<TableValue, value_type const&> {
  return insert(value).first;
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
template <typename T>
  requires flat_map_value_constructible<Key, Mapped, T&&>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::insert(const_iterator, T&& value) -> iterator {
  return insert(std::forward<T>(value)).first;
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
template <std::input_iterator InputIt, std::sentinel_for<InputIt> Sentinel>
  requires flat_map_value_constructible<Key, Mapped, std::iter_reference_t<InputIt>>
void FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::insert(InputIt first, Sentinel last) {
  // Do not pre-count or reserve borrowed inputs. Exact-key proxy self-ranges
  // contain only duplicates and must neither construct values nor invalidate.
  for (; first != last; ++first)
    insert(*first);
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
void FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::insert(std::initializer_list<value_type> init) {
  insert(init.begin(), init.end());
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
template <typename... Args>
  requires flat_map_value_constructible<Key, Mapped, Args&&...>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::emplace(Args&&... args) -> std::pair<iterator, bool> {
  if constexpr (sizeof...(Args) == 1) {
    return insert(std::forward<Args>(args)...);
  } else {
    auto result = m_table.insert(TableValue(std::forward<Args>(args)...));
    return {iterator{result.first}, result.second};
  }
}

template <typename Key, typename Mapped, typename Hash, typename Equals, typename Allocator>
template <typename... Args>
  requires flat_map_value_constructible<Key, Mapped, Args&&...>
auto FlatHashMap<Key, Mapped, Hash, Equals, Allocator>::emplace_hint(const_iterator, Args&&... args) -> iterator {
  return emplace(std::forward<Args>(args)...).first;
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
