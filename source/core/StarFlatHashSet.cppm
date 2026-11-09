module;

import std;
import star.flat_hash_table;
import star.hash;

namespace Star {

template <typename Key, typename Hash = hash<Key>, typename Equals = std::equal_to<Key>, typename Allocator = std::allocator<Key>>
class FlatHashSet {
public:
  using key_type = Key;
  using value_type = Key;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using hasher = Hash;
  using key_equal = Equals;
  using allocator_type = Allocator;
  using reference = value_type const&;
  using const_reference = value_type const&;
  using pointer = value_type const*;
  using const_pointer = value_type const*;

private:
  struct GetKey {
    key_type const& operator()(value_type const& value) const { return value; }
  };
  using Table = FlatHashTable<Key, Key, GetKey, Hash, Equals, Allocator>;

public:
  struct const_iterator {
    using iterator_category = std::forward_iterator_tag;
    using iterator_concept = std::forward_iterator_tag;
    using value_type = Key;
    using difference_type = std::ptrdiff_t;
    using pointer = Key const*;
    using reference = Key const&;

    bool operator==(const_iterator const& rhs) const { return inner == rhs.inner; }
    bool operator!=(const_iterator const& rhs) const { return inner != rhs.inner; }
    const_iterator& operator++() { ++inner; return *this; }
    const_iterator operator++(int) { auto copy = *this; ++*this; return copy; }
    reference operator*() const { return *inner; }
    pointer operator->() const { return std::addressof(*inner); }

    typename Table::const_iterator inner{};
  };
  using iterator = const_iterator;

  FlatHashSet() : FlatHashSet(0) {}
  explicit FlatHashSet(size_type bucket_count, hasher const& hash = hasher(),
      key_equal const& equal = key_equal(), allocator_type const& alloc = allocator_type())
    : m_table(bucket_count, GetKey(), hash, equal, alloc) {}
  FlatHashSet(size_type bucket_count, allocator_type const& alloc)
    : FlatHashSet(bucket_count, hasher(), key_equal(), alloc) {}
  FlatHashSet(size_type bucket_count, hasher const& hash, allocator_type const& alloc)
    : FlatHashSet(bucket_count, hash, key_equal(), alloc) {}
  explicit FlatHashSet(allocator_type const& alloc)
    : FlatHashSet(0, hasher(), key_equal(), alloc) {}

  template <std::input_iterator InputIt, std::sentinel_for<InputIt> Sentinel = InputIt>
    requires std::is_constructible_v<value_type, std::iter_reference_t<InputIt>>
  FlatHashSet(InputIt first, Sentinel last, size_type bucket_count = 0,
      hasher const& hash = hasher(), key_equal const& equal = key_equal(),
      allocator_type const& alloc = allocator_type())
    : FlatHashSet(bucket_count, hash, equal, alloc) {
    // A fresh owner cannot alias its input. Keep reservation here, not in
    // insert_range, where reallocation could invalidate a borrowed source.
    if constexpr (std::sized_sentinel_for<Sentinel, InputIt>)
      m_table.reserve(static_cast<size_type>(last - first));
    else if constexpr (std::forward_iterator<InputIt>)
      m_table.reserve(static_cast<size_type>(std::ranges::distance(first, last)));
    insert(std::move(first), std::move(last));
  }
  template <std::input_iterator InputIt, std::sentinel_for<InputIt> Sentinel = InputIt>
    requires std::is_constructible_v<value_type, std::iter_reference_t<InputIt>>
  FlatHashSet(InputIt first, Sentinel last, size_type bucket_count, allocator_type const& alloc)
    : FlatHashSet(std::move(first), std::move(last), bucket_count, hasher(), key_equal(), alloc) {}
  template <std::input_iterator InputIt, std::sentinel_for<InputIt> Sentinel = InputIt>
    requires std::is_constructible_v<value_type, std::iter_reference_t<InputIt>>
  FlatHashSet(InputIt first, Sentinel last, size_type bucket_count,
      hasher const& hash, allocator_type const& alloc)
    : FlatHashSet(std::move(first), std::move(last), bucket_count, hash, key_equal(), alloc) {}

  template <std::ranges::input_range Range>
    requires std::is_constructible_v<value_type, std::ranges::range_reference_t<Range>>
  FlatHashSet(std::from_range_t, Range&& range, size_type bucket_count = 0,
      hasher const& hash = hasher(), key_equal const& equal = key_equal(),
      allocator_type const& alloc = allocator_type())
    : FlatHashSet(bucket_count, hash, equal, alloc) {
    if constexpr (std::ranges::sized_range<Range>)
      m_table.reserve(std::ranges::size(range));
    else if constexpr (std::ranges::forward_range<Range>)
      m_table.reserve(static_cast<size_type>(std::ranges::distance(range)));
    insert_range(std::forward<Range>(range));
  }
  template <std::ranges::input_range Range>
    requires std::is_constructible_v<value_type, std::ranges::range_reference_t<Range>>
  FlatHashSet(std::from_range_t, Range&& range, size_type bucket_count, allocator_type const& alloc)
    : FlatHashSet(std::from_range, std::forward<Range>(range), bucket_count, hasher(), key_equal(), alloc) {}
  template <std::ranges::input_range Range>
    requires std::is_constructible_v<value_type, std::ranges::range_reference_t<Range>>
  FlatHashSet(std::from_range_t, Range&& range, size_type bucket_count,
      hasher const& hash, allocator_type const& alloc)
    : FlatHashSet(std::from_range, std::forward<Range>(range), bucket_count, hash, key_equal(), alloc) {}

  FlatHashSet(FlatHashSet const&)
    requires (std::is_copy_constructible_v<value_type> && std::is_copy_constructible_v<Table>) = default;
  FlatHashSet(FlatHashSet const& other, allocator_type const& alloc)
    requires (std::is_copy_constructible_v<value_type>
      && std::is_constructible_v<Table, Table const&, allocator_type const&>)
    : m_table(other.m_table, alloc) {}
  FlatHashSet(FlatHashSet&&) = default;
  FlatHashSet(FlatHashSet&& other, allocator_type const& alloc)
    : m_table(std::move(other.m_table), alloc) {}
  FlatHashSet& operator=(FlatHashSet const&)
    requires (std::is_copy_constructible_v<value_type> && std::is_copy_assignable_v<Table>) = default;
  FlatHashSet& operator=(FlatHashSet&&) = default;

  FlatHashSet(std::initializer_list<value_type> init, size_type bucket_count = 0,
      hasher const& hash = hasher(), key_equal const& equal = key_equal(),
      allocator_type const& alloc = allocator_type())
    : FlatHashSet(init.begin(), init.end(), bucket_count, hash, equal, alloc) {}
  FlatHashSet(std::initializer_list<value_type> init, size_type bucket_count, allocator_type const& alloc)
    : FlatHashSet(init, bucket_count, hasher(), key_equal(), alloc) {}
  FlatHashSet(std::initializer_list<value_type> init, size_type bucket_count,
      hasher const& hash, allocator_type const& alloc)
    : FlatHashSet(init, bucket_count, hash, key_equal(), alloc) {}
  FlatHashSet& operator=(std::initializer_list<value_type> init) {
    clear();
    insert(init.begin(), init.end());
    return *this;
  }

  iterator begin() { return iterator{m_table.begin()}; }
  iterator end() { return iterator{m_table.end()}; }
  const_iterator begin() const { return const_iterator{m_table.begin()}; }
  const_iterator end() const { return const_iterator{m_table.end()}; }
  const_iterator cbegin() const { return begin(); }
  const_iterator cend() const { return end(); }
  bool empty() const noexcept { return m_table.empty(); }
  size_type size() const noexcept { return m_table.size(); }
  void clear() { m_table.clear(); }

  std::pair<iterator, bool> insert(value_type const& value)
    requires std::is_copy_constructible_v<value_type> {
    auto result = m_table.insert(value);
    return {iterator{result.first}, result.second};
  }
  std::pair<iterator, bool> insert(value_type&& value)
    requires std::is_move_constructible_v<value_type> {
    auto result = m_table.insert(std::move(value));
    return {iterator{result.first}, result.second};
  }
  iterator insert(const_iterator, value_type const& value)
    requires std::is_copy_constructible_v<value_type> { return insert(value).first; }
  iterator insert(const_iterator, value_type&& value)
    requires std::is_move_constructible_v<value_type> { return insert(std::move(value)).first; }

  template <std::input_iterator InputIt, std::sentinel_for<InputIt> Sentinel = InputIt>
    requires std::is_constructible_v<value_type, std::iter_reference_t<InputIt>>
  void insert(InputIt first, Sentinel last) {
    // Do not pre-count or reserve: an input is single-pass, and a directly
    // aliased key range contains only duplicates and must remain valid.
    // Opaque projections producing new keys must not borrow this storage;
    // discovering that overlap would evaluate projections or require snapshots.
    for (; first != last; ++first)
      m_table.insert(value_type(*first));
  }
  template <std::ranges::input_range Range>
    requires std::is_constructible_v<value_type, std::ranges::range_reference_t<Range>>
  void insert_range(Range&& range) {
    insert(std::ranges::begin(range), std::ranges::end(range));
  }
  void insert(std::initializer_list<value_type> init) { insert(init.begin(), init.end()); }

  template <typename... Args>
    requires std::is_constructible_v<value_type, Args&&...>
  std::pair<iterator, bool> emplace(Args&&... args) {
    return insert(value_type(std::forward<Args>(args)...));
  }
  template <typename... Args>
    requires std::is_constructible_v<value_type, Args&&...>
  iterator emplace_hint(const_iterator, Args&&... args) { return emplace(std::forward<Args>(args)...).first; }

  iterator erase(const_iterator pos) { return iterator{m_table.erase(pos.inner)}; }
  iterator erase(const_iterator first, const_iterator last) { return iterator{m_table.erase(first.inner, last.inner)}; }
  size_type erase(key_type const& key) {
    auto position = find(key);
    if (position == end())
      return 0;
    erase(position);
    return 1;
  }
  // Like erase(pos), extraction requires a valid, dereferenceable iterator.
  value_type extract_value(const_iterator pos) requires std::is_move_constructible_v<value_type> {
    return m_table.extract_value(pos.inner);
  }

  size_type count(key_type const& key) const { return find(key) == end() ? 0 : 1; }
  bool contains(key_type const& key) const { return find(key) != end(); }
  const_iterator find(key_type const& key) const { return const_iterator{m_table.find(key)}; }
  iterator find(key_type const& key) { return iterator{m_table.find(key)}; }
  template <typename Query> requires transparent_hash_lookup<hasher, key_equal, key_type, Query>
  const_iterator find(Query const& key) const { return const_iterator{m_table.find(key)}; }
  template <typename Query> requires transparent_hash_lookup<hasher, key_equal, key_type, Query>
  iterator find(Query const& key) { return iterator{m_table.find(key)}; }
  template <typename Query> requires transparent_hash_lookup<hasher, key_equal, key_type, Query>
  bool contains(Query const& key) const { return find(key) != end(); }
  template <typename Query> requires transparent_hash_lookup<hasher, key_equal, key_type, Query>
  size_type count(Query const& key) const { return contains(key) ? 1 : 0; }

  std::pair<iterator, iterator> equal_range(key_type const& key) {
    auto first = find(key);
    auto last = first;
    if (last != end())
      ++last;
    return {first, last};
  }
  std::pair<const_iterator, const_iterator> equal_range(key_type const& key) const {
    auto first = find(key);
    auto last = first;
    if (last != end())
      ++last;
    return {first, last};
  }

  void reserve(size_type capacity) { m_table.reserve(capacity); }
  allocator_type get_allocator() const noexcept { return m_table.get_allocator(); }
  hasher hash_function() const noexcept(std::is_nothrow_copy_constructible_v<hasher>) { return m_table.hash_function(); }
  key_equal key_eq() const noexcept(std::is_nothrow_copy_constructible_v<key_equal>) { return m_table.key_eq(); }

  bool operator==(FlatHashSet const& rhs) const
    requires requires(value_type const& left, value_type const& right) { left != right ? 0 : 0; } {
    if (size() != rhs.size())
      return false;
    for (auto const& value : *this) {
      auto found = rhs.find(value);
      if (found == rhs.end())
        return false;
      if (value != *found)
        return false;
    }
    return true;
  }
  bool operator!=(FlatHashSet const& rhs) const
    requires requires(value_type const& left, value_type const& right) { left != right ? 0 : 0; } {
    return !(*this == rhs);
  }

private:
  Table m_table;
};

}

export module star.flat_hash_set;

export namespace Star {
  using ::Star::FlatHashSet;
}
