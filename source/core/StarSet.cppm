module;
import star.stream_formatter;

import std;
import star.flat_hash_set;
import star.list;
import star.optional;
import star.hash;
import star.format;

namespace Star {

struct SetExceptionTag {
  static constexpr std::string_view name() { return "SetException"; }
};
using SetException = StarError<SetExceptionTag, StarException>;

template <typename T>
concept SetContainer = requires { typename std::remove_cvref_t<T>::StarSetContainerTag; }
    && std::same_as<typename std::remove_cvref_t<T>::StarSetContainerTag, void>;

template <typename Value, typename Compare = std::less<Value>, typename Allocator = std::allocator<Value>>
class Set {
public:
  using Storage = std::set<Value, Compare, Allocator>;
  using StarSetContainerTag = void;
  using key_type = Value;
  using value_type = Value;
  using iterator = typename Storage::iterator;
  using const_iterator = typename Storage::const_iterator;
  using size_type = typename Storage::size_type;
  using difference_type = typename Storage::difference_type;
  using allocator_type = Allocator;
  using key_compare = Compare;
  using value_compare = Compare;

  Set() = default;
  template <typename... Args>
    requires (sizeof...(Args) > 0 && std::constructible_from<Storage, Args...>)
  explicit Set(Args&&... args) : m_storage(std::forward<Args>(args)...) {}
  template <typename InputIt, typename... Args>
    requires std::constructible_from<Storage, InputIt, InputIt, Args...>
  Set(InputIt first, InputIt last, Args&&... args)
    : m_storage(first, last, std::forward<Args>(args)...) {}
  Set(std::initializer_list<value_type> values) : m_storage(values) {}
  Set(std::initializer_list<value_type> values, Compare const& compare, allocator_type const& allocator = allocator_type())
    : m_storage(values, compare, allocator) {}
  Set(std::initializer_list<value_type> values, allocator_type const& allocator)
    : m_storage(values, allocator) {}
  Set(Set const& other, allocator_type const& allocator) : m_storage(other.m_storage, allocator) {}
  Set(Set&& other, allocator_type const& allocator) : m_storage(std::move(other.m_storage), allocator) {}
  Set& operator=(std::initializer_list<value_type> values) { m_storage = values; return *this; }
  template <typename Range>
  static Set from(Range const& range) { return Set(std::ranges::begin(range), std::ranges::end(range)); }

  iterator begin() { return m_storage.begin(); }
  const_iterator begin() const { return m_storage.begin(); }
  const_iterator cbegin() const { return m_storage.cbegin(); }
  iterator end() { return m_storage.end(); }
  const_iterator end() const { return m_storage.end(); }
  const_iterator cend() const { return m_storage.cend(); }
  bool empty() const { return m_storage.empty(); }
  size_type size() const { return m_storage.size(); }
  void clear() { m_storage.clear(); }
  void swap(Set& other) noexcept(std::is_nothrow_swappable_v<Storage>) { std::swap(m_storage, other.m_storage); }

  // values() is an owning snapshot. valueView() borrows, with storage invalidation rules.
  List<value_type> values() const {
    List<value_type> result;
    result.reserve(size());
    std::ranges::copy(valueView(), std::back_inserter(result));
    return result;
  }
  auto valueView() const& { return std::ranges::subrange(begin(), end()); }
  auto valueView() const&& = delete;
  template <typename LookupKey = key_type>
    requires requires(Storage& storage, LookupKey const& key) { storage.find(key); }
  iterator find(LookupKey const& key) { return m_storage.find(key); }
  template <typename LookupKey = key_type>
    requires requires(Storage const& storage, LookupKey const& key) { storage.find(key); }
  const_iterator find(LookupKey const& key) const { return m_storage.find(key); }
  template <typename LookupKey = key_type>
    requires requires(Storage const& storage, LookupKey const& key) { storage.find(key); }
  bool contains(LookupKey const& key) const { return find(key) != end(); }
  template <typename LookupKey = key_type>
    requires requires(Storage const& storage, LookupKey const& key) { storage.find(key); }
  size_type count(LookupKey const& key) const { return contains(key) ? 1 : 0; }
  std::pair<iterator, bool> insert(value_type const& value) { return m_storage.insert(value); }
  std::pair<iterator, bool> insert(value_type&& value) { return m_storage.insert(std::move(value)); }
  iterator insert(const_iterator hint, value_type const& value) { return m_storage.insert(hint, value); }
  iterator insert(const_iterator hint, value_type&& value) { return m_storage.insert(hint, std::move(value)); }
  template <typename InputIt>
  void insert(InputIt first, InputIt last) { m_storage.insert(first, last); }
  void insert(std::initializer_list<value_type> values) { m_storage.insert(values); }
  template <typename... Args>
  auto emplace(Args&&... args) { return m_storage.emplace(std::forward<Args>(args)...); }
  template <typename... Args>
  iterator emplace_hint(const_iterator hint, Args&&... args) { return m_storage.emplace_hint(hint, std::forward<Args>(args)...); }
  iterator erase(const_iterator position) { return m_storage.erase(position); }
  iterator erase(const_iterator first, const_iterator last) { return m_storage.erase(first, last); }
  size_type erase(key_type const& key) { return m_storage.erase(key); }
  auto equal_range(key_type const& key) { return m_storage.equal_range(key); }
  auto equal_range(key_type const& key) const { return m_storage.equal_range(key); }
  bool add(value_type const& value) { return insert(value).second; }
  bool add(value_type&& value) { return insert(std::move(value)).second; }
  // Replace even when equivalence does not imply identical stored state.
  bool replace(value_type value) {
    bool replaced = remove(value);
    insert(std::move(value));
    return replaced;
  }
  template <typename Range>
  void addAll(Range const& range) { insert(std::ranges::begin(range), std::ranges::end(range)); }
  bool remove(value_type const& value) { return erase(value) != 0; }
  template <typename Range>
  void removeAll(Range const& range) { for (auto const& value : range) remove(value); }

  [[nodiscard]] std::expected<value_type, StarException> first() const {
    if (empty()) return std::unexpected(SetException("first called on empty set"));
    return *begin();
  }
  std::optional<value_type> maybeFirst() const {
    if (empty()) return {};
    return std::optional<value_type>(std::in_place, *begin());
  }
  [[nodiscard]] std::expected<value_type, StarException> takeFirst() {
    if (empty()) return std::unexpected(SetException("takeFirst called on empty set"));
    return takeAt(begin());
  }
  std::optional<value_type> maybeTakeFirst() {
    if (empty()) return {};
    return std::optional<value_type>(std::in_place, takeAt(begin()));
  }
  [[nodiscard]] std::expected<value_type, StarException> last() const {
    if (empty()) return std::unexpected(SetException("last called on empty set"));
    return *lastPosition();
  }
  std::optional<value_type> maybeLast() const {
    if (empty()) return {};
    return std::optional<value_type>(std::in_place, *lastPosition());
  }
  [[nodiscard]] std::expected<value_type, StarException> takeLast() {
    if (empty()) return std::unexpected(SetException("takeLast called on empty set"));
    return takeAt(lastPosition());
  }
  std::optional<value_type> maybeTakeLast() {
    if (empty()) return {};
    return std::optional<value_type>(std::in_place, takeAt(lastPosition()));
  }
  bool hasIntersection(Set const& other) const {
    return std::ranges::any_of(other, [this](auto const& value) { return this->contains(value); });
  }
  Set intersection(Set const& other) const {
    return intersection(other, m_storage.key_comp());
  }
  template <typename CompareFunction>
    requires std::predicate<CompareFunction&, value_type const&, value_type const&>
  Set intersection(Set const& other, CompareFunction compare) const {
    Set result(m_storage.key_comp(), m_storage.get_allocator());
    std::ranges::set_intersection(*this, other, std::inserter(result, result.end()), std::move(compare));
    return result;
  }
  Set difference(Set const& other) const {
    return difference(other, m_storage.key_comp());
  }
  template <typename CompareFunction>
    requires std::predicate<CompareFunction&, value_type const&, value_type const&>
  Set difference(Set const& other, CompareFunction compare) const {
    Set result(m_storage.key_comp(), m_storage.get_allocator());
    std::ranges::set_difference(*this, other, std::inserter(result, result.end()), std::move(compare));
    return result;
  }
  Set combination(Set const& other) const {
    Set result(*this);
    result.addAll(other);
    return result;
  }
  bool operator==(Set const& other) const { return m_storage == other.m_storage; }
  auto lower_bound(key_type const& key) { return m_storage.lower_bound(key); }
  auto lower_bound(key_type const& key) const { return m_storage.lower_bound(key); }
  auto upper_bound(key_type const& key) { return m_storage.upper_bound(key); }
  auto upper_bound(key_type const& key) const { return m_storage.upper_bound(key); }
  auto key_comp() const { return m_storage.key_comp(); }
  auto value_comp() const { return m_storage.value_comp(); }
  auto rbegin() { return m_storage.rbegin(); }
  auto rbegin() const { return m_storage.rbegin(); }
  auto rend() { return m_storage.rend(); }
  auto rend() const { return m_storage.rend(); }
  auto get_allocator() const { return m_storage.get_allocator(); }
  auto max_size() const { return m_storage.max_size(); }

private:
  iterator lastPosition() {
    return std::prev(end());
  }
  const_iterator lastPosition() const {
    return std::prev(end());
  }
  value_type takeAt(iterator position) {
    auto node = m_storage.extract(position);
    return std::move(node.value());
  }
  Storage m_storage;
};

template <typename Value, typename Hash = hash<Value>, typename Equals = std::equal_to<Value>, typename Allocator = std::allocator<Value>>
class HashSet {
public:
  using Storage = FlatHashSet<Value, Hash, Equals, Allocator>;
  using StarSetContainerTag = void;
  using key_type = Value;
  using value_type = Value;
  using iterator = typename Storage::iterator;
  using const_iterator = typename Storage::const_iterator;
  using size_type = typename Storage::size_type;
  using difference_type = typename Storage::difference_type;
  using allocator_type = Allocator;
  using hasher = Hash;
  using key_equal = Equals;

  HashSet() = default;
  template <typename... Args>
    requires (sizeof...(Args) > 0 && std::constructible_from<Storage, Args...>)
  explicit HashSet(Args&&... args) : m_storage(std::forward<Args>(args)...) {}
  template <typename InputIt, typename... Args>
    requires std::constructible_from<Storage, InputIt, InputIt, Args...>
  HashSet(InputIt first, InputIt last, Args&&... args)
    : m_storage(first, last, std::forward<Args>(args)...) {}
  HashSet(std::initializer_list<value_type> values) : m_storage(values) {}
  HashSet(std::initializer_list<value_type> values, size_type bucketCount, hasher const& hash = hasher(),
      key_equal const& equal = key_equal(), allocator_type const& allocator = allocator_type())
    : m_storage(values, bucketCount, hash, equal, allocator) {}
  HashSet(std::initializer_list<value_type> values, allocator_type const& allocator)
    : m_storage(values, 0, hasher(), key_equal(), allocator) {}
  HashSet(std::initializer_list<value_type> values, size_type bucketCount, allocator_type const& allocator)
    : m_storage(values, bucketCount, allocator) {}
  HashSet(std::initializer_list<value_type> values, size_type bucketCount, hasher const& hash, allocator_type const& allocator)
    : m_storage(values, bucketCount, hash, allocator) {}
  HashSet(HashSet const& other, allocator_type const& allocator) : m_storage(other.m_storage, allocator) {}
  HashSet(HashSet&& other, allocator_type const& allocator) : m_storage(std::move(other.m_storage), allocator) {}
  HashSet& operator=(std::initializer_list<value_type> values) { m_storage = values; return *this; }
  template <typename Range>
  static HashSet from(Range const& range) { return HashSet(std::ranges::begin(range), std::ranges::end(range)); }

  iterator begin() { return m_storage.begin(); }
  const_iterator begin() const { return m_storage.begin(); }
  const_iterator cbegin() const { return m_storage.cbegin(); }
  iterator end() { return m_storage.end(); }
  const_iterator end() const { return m_storage.end(); }
  const_iterator cend() const { return m_storage.cend(); }
  bool empty() const { return m_storage.empty(); }
  size_type size() const { return m_storage.size(); }
  void clear() { m_storage.clear(); }
  void swap(HashSet& other) noexcept(std::is_nothrow_swappable_v<Storage>) { std::swap(m_storage, other.m_storage); }

  // values() is an owning snapshot. valueView() borrows, with storage invalidation rules.
  List<value_type> values() const {
    List<value_type> result;
    result.reserve(size());
    std::ranges::copy(valueView(), std::back_inserter(result));
    return result;
  }
  auto valueView() const& { return std::ranges::subrange(begin(), end()); }
  auto valueView() const&& = delete;
  template <typename LookupKey = key_type>
    requires requires(Storage& storage, LookupKey const& key) { storage.find(key); }
  iterator find(LookupKey const& key) { return m_storage.find(key); }
  template <typename LookupKey = key_type>
    requires requires(Storage const& storage, LookupKey const& key) { storage.find(key); }
  const_iterator find(LookupKey const& key) const { return m_storage.find(key); }
  template <typename LookupKey = key_type>
    requires requires(Storage const& storage, LookupKey const& key) { storage.find(key); }
  bool contains(LookupKey const& key) const { return find(key) != end(); }
  template <typename LookupKey = key_type>
    requires requires(Storage const& storage, LookupKey const& key) { storage.find(key); }
  size_type count(LookupKey const& key) const { return contains(key) ? 1 : 0; }
  std::pair<iterator, bool> insert(value_type const& value) { return m_storage.insert(value); }
  std::pair<iterator, bool> insert(value_type&& value) { return m_storage.insert(std::move(value)); }
  iterator insert(const_iterator hint, value_type const& value) { return m_storage.insert(hint, value); }
  iterator insert(const_iterator hint, value_type&& value) { return m_storage.insert(hint, std::move(value)); }
  template <typename InputIt>
  void insert(InputIt first, InputIt last) { m_storage.insert(first, last); }
  void insert(std::initializer_list<value_type> values) { m_storage.insert(values); }
  template <typename... Args>
  auto emplace(Args&&... args) { return m_storage.emplace(std::forward<Args>(args)...); }
  template <typename... Args>
  iterator emplace_hint(const_iterator hint, Args&&... args) { return m_storage.emplace_hint(hint, std::forward<Args>(args)...); }
  iterator erase(const_iterator position) { return m_storage.erase(position); }
  iterator erase(const_iterator first, const_iterator last) { return m_storage.erase(first, last); }
  size_type erase(key_type const& key) { return m_storage.erase(key); }

  bool add(value_type const& value) { return insert(value).second; }
  bool add(value_type&& value) { return insert(std::move(value)).second; }
  // Replace even when equivalence does not imply identical stored state.
  bool replace(value_type value) {
    bool replaced = remove(value);
    insert(std::move(value));
    return replaced;
  }
  template <typename Range>
  void addAll(Range const& range) { insert(std::ranges::begin(range), std::ranges::end(range)); }
  bool remove(value_type const& value) { return erase(value) != 0; }
  template <typename Range>
  void removeAll(Range const& range) { for (auto const& value : range) remove(value); }

  [[nodiscard]] std::expected<value_type, StarException> first() const {
    if (empty()) return std::unexpected(SetException("first called on empty set"));
    return *begin();
  }
  std::optional<value_type> maybeFirst() const {
    if (empty()) return {};
    return std::optional<value_type>(std::in_place, *begin());
  }
  [[nodiscard]] std::expected<value_type, StarException> takeFirst() {
    if (empty()) return std::unexpected(SetException("takeFirst called on empty set"));
    return takeAt(begin());
  }
  std::optional<value_type> maybeTakeFirst() {
    if (empty()) return {};
    return std::optional<value_type>(std::in_place, takeAt(begin()));
  }
  [[nodiscard]] std::expected<value_type, StarException> last() const {
    if (empty()) return std::unexpected(SetException("last called on empty set"));
    return *lastPosition();
  }
  std::optional<value_type> maybeLast() const {
    if (empty()) return {};
    return std::optional<value_type>(std::in_place, *lastPosition());
  }
  [[nodiscard]] std::expected<value_type, StarException> takeLast() {
    if (empty()) return std::unexpected(SetException("takeLast called on empty set"));
    return takeAt(lastPosition());
  }
  std::optional<value_type> maybeTakeLast() {
    if (empty()) return {};
    return std::optional<value_type>(std::in_place, takeAt(lastPosition()));
  }
  bool hasIntersection(HashSet const& other) const {
    return std::ranges::any_of(other, [this](auto const& value) { return this->contains(value); });
  }
  HashSet intersection(HashSet const& other) const {
    // Hash iteration is not sorted: use membership, not ordered set algorithms.
    HashSet result;
    for (auto const& value : other)
      if (contains(value)) result.add(value);
    return result;
  }
  HashSet difference(HashSet const& other) const {
    HashSet result;
    for (auto const& value : *this)
      if (!other.contains(value)) result.add(value);
    return result;
  }
  HashSet combination(HashSet const& other) const {
    HashSet result(*this);
    result.addAll(other);
    return result;
  }
  bool operator==(HashSet const& other) const { return m_storage == other.m_storage; }
  void reserve(size_type capacity) { m_storage.reserve(capacity); }

private:
  iterator lastPosition() {
    auto result = begin();
    for (auto position = result; position != end(); ++position) result = position;
    return result;
  }
  const_iterator lastPosition() const {
    auto result = begin();
    for (auto position = result; position != end(); ++position) result = position;
    return result;
  }
  value_type takeAt(iterator position) {
    value_type value = std::move(*position);
    m_storage.erase(position);
    return value;
  }
  Storage m_storage;
};

template <typename Value, typename Hash = hash<Value>, typename Equals = std::equal_to<Value>, typename Allocator = std::allocator<Value>>
class StableHashSet {
public:
  using Storage = std::unordered_set<Value, Hash, Equals, Allocator>;
  using StarSetContainerTag = void;
  using key_type = Value;
  using value_type = Value;
  using iterator = typename Storage::iterator;
  using const_iterator = typename Storage::const_iterator;
  using size_type = typename Storage::size_type;
  using difference_type = typename Storage::difference_type;
  using allocator_type = Allocator;
  using hasher = Hash;
  using key_equal = Equals;

  StableHashSet() = default;
  template <typename... Args>
    requires (sizeof...(Args) > 0 && std::constructible_from<Storage, Args...>)
  explicit StableHashSet(Args&&... args) : m_storage(std::forward<Args>(args)...) {}
  template <typename InputIt, typename... Args>
    requires std::constructible_from<Storage, InputIt, InputIt, Args...>
  StableHashSet(InputIt first, InputIt last, Args&&... args)
    : m_storage(first, last, std::forward<Args>(args)...) {}
  StableHashSet(std::initializer_list<value_type> values) : m_storage(values) {}
  StableHashSet(std::initializer_list<value_type> values, size_type bucketCount, hasher const& hash = hasher(),
      key_equal const& equal = key_equal(), allocator_type const& allocator = allocator_type())
    : m_storage(values, bucketCount, hash, equal, allocator) {}
  StableHashSet(std::initializer_list<value_type> values, allocator_type const& allocator)
    : m_storage(values, allocator) {}
  StableHashSet(std::initializer_list<value_type> values, size_type bucketCount, allocator_type const& allocator)
    : m_storage(values, bucketCount, allocator) {}
  StableHashSet(std::initializer_list<value_type> values, size_type bucketCount, hasher const& hash, allocator_type const& allocator)
    : m_storage(values, bucketCount, hash, allocator) {}
  StableHashSet(StableHashSet const& other, allocator_type const& allocator) : m_storage(other.m_storage, allocator) {}
  StableHashSet(StableHashSet&& other, allocator_type const& allocator) : m_storage(std::move(other.m_storage), allocator) {}
  StableHashSet& operator=(std::initializer_list<value_type> values) { m_storage = values; return *this; }
  template <typename Range>
  static StableHashSet from(Range const& range) { return StableHashSet(std::ranges::begin(range), std::ranges::end(range)); }

  iterator begin() { return m_storage.begin(); }
  const_iterator begin() const { return m_storage.begin(); }
  const_iterator cbegin() const { return m_storage.cbegin(); }
  iterator end() { return m_storage.end(); }
  const_iterator end() const { return m_storage.end(); }
  const_iterator cend() const { return m_storage.cend(); }
  bool empty() const { return m_storage.empty(); }
  size_type size() const { return m_storage.size(); }
  void clear() { m_storage.clear(); }
  void swap(StableHashSet& other) noexcept(std::is_nothrow_swappable_v<Storage>) { std::swap(m_storage, other.m_storage); }

  // values() is an owning snapshot. valueView() borrows, with storage invalidation rules.
  List<value_type> values() const {
    List<value_type> result;
    result.reserve(size());
    std::ranges::copy(valueView(), std::back_inserter(result));
    return result;
  }
  auto valueView() const& { return std::ranges::subrange(begin(), end()); }
  auto valueView() const&& = delete;
  template <typename LookupKey = key_type>
    requires requires(Storage& storage, LookupKey const& key) { storage.find(key); }
  iterator find(LookupKey const& key) { return m_storage.find(key); }
  template <typename LookupKey = key_type>
    requires requires(Storage const& storage, LookupKey const& key) { storage.find(key); }
  const_iterator find(LookupKey const& key) const { return m_storage.find(key); }
  template <typename LookupKey = key_type>
    requires requires(Storage const& storage, LookupKey const& key) { storage.find(key); }
  bool contains(LookupKey const& key) const { return find(key) != end(); }
  template <typename LookupKey = key_type>
    requires requires(Storage const& storage, LookupKey const& key) { storage.find(key); }
  size_type count(LookupKey const& key) const { return contains(key) ? 1 : 0; }
  std::pair<iterator, bool> insert(value_type const& value) { return m_storage.insert(value); }
  std::pair<iterator, bool> insert(value_type&& value) { return m_storage.insert(std::move(value)); }
  iterator insert(const_iterator hint, value_type const& value) { return m_storage.insert(hint, value); }
  iterator insert(const_iterator hint, value_type&& value) { return m_storage.insert(hint, std::move(value)); }
  template <typename InputIt>
  void insert(InputIt first, InputIt last) { m_storage.insert(first, last); }
  void insert(std::initializer_list<value_type> values) { m_storage.insert(values); }
  template <typename... Args>
  auto emplace(Args&&... args) { return m_storage.emplace(std::forward<Args>(args)...); }
  template <typename... Args>
  iterator emplace_hint(const_iterator hint, Args&&... args) { return m_storage.emplace_hint(hint, std::forward<Args>(args)...); }
  iterator erase(const_iterator position) { return m_storage.erase(position); }
  iterator erase(const_iterator first, const_iterator last) { return m_storage.erase(first, last); }
  size_type erase(key_type const& key) { return m_storage.erase(key); }

  bool add(value_type const& value) { return insert(value).second; }
  bool add(value_type&& value) { return insert(std::move(value)).second; }
  // Replace even when equivalence does not imply identical stored state.
  bool replace(value_type value) {
    bool replaced = remove(value);
    insert(std::move(value));
    return replaced;
  }
  template <typename Range>
  void addAll(Range const& range) { insert(std::ranges::begin(range), std::ranges::end(range)); }
  bool remove(value_type const& value) { return erase(value) != 0; }
  template <typename Range>
  void removeAll(Range const& range) { for (auto const& value : range) remove(value); }

  [[nodiscard]] std::expected<value_type, StarException> first() const {
    if (empty()) return std::unexpected(SetException("first called on empty set"));
    return *begin();
  }
  std::optional<value_type> maybeFirst() const {
    if (empty()) return {};
    return std::optional<value_type>(std::in_place, *begin());
  }
  [[nodiscard]] std::expected<value_type, StarException> takeFirst() {
    if (empty()) return std::unexpected(SetException("takeFirst called on empty set"));
    return takeAt(begin());
  }
  std::optional<value_type> maybeTakeFirst() {
    if (empty()) return {};
    return std::optional<value_type>(std::in_place, takeAt(begin()));
  }
  [[nodiscard]] std::expected<value_type, StarException> last() const {
    if (empty()) return std::unexpected(SetException("last called on empty set"));
    return *lastPosition();
  }
  std::optional<value_type> maybeLast() const {
    if (empty()) return {};
    return std::optional<value_type>(std::in_place, *lastPosition());
  }
  [[nodiscard]] std::expected<value_type, StarException> takeLast() {
    if (empty()) return std::unexpected(SetException("takeLast called on empty set"));
    return takeAt(lastPosition());
  }
  std::optional<value_type> maybeTakeLast() {
    if (empty()) return {};
    return std::optional<value_type>(std::in_place, takeAt(lastPosition()));
  }
  bool hasIntersection(StableHashSet const& other) const {
    return std::ranges::any_of(other, [this](auto const& value) { return this->contains(value); });
  }
  StableHashSet intersection(StableHashSet const& other) const {
    // Hash iteration is not sorted: use membership, not ordered set algorithms.
    StableHashSet result;
    for (auto const& value : other)
      if (contains(value)) result.add(value);
    return result;
  }
  StableHashSet difference(StableHashSet const& other) const {
    StableHashSet result;
    for (auto const& value : *this)
      if (!other.contains(value)) result.add(value);
    return result;
  }
  StableHashSet combination(StableHashSet const& other) const {
    StableHashSet result(*this);
    result.addAll(other);
    return result;
  }
  bool operator==(StableHashSet const& other) const { return m_storage == other.m_storage; }
  void reserve(size_type capacity) { m_storage.reserve(capacity); }
  void rehash(size_type buckets) { m_storage.rehash(buckets); }
  auto hash_function() const { return m_storage.hash_function(); }
  auto key_eq() const { return m_storage.key_eq(); }
  auto get_allocator() const { return m_storage.get_allocator(); }
  auto max_size() const { return m_storage.max_size(); }
  auto bucket_count() const { return m_storage.bucket_count(); }
  auto load_factor() const { return m_storage.load_factor(); }
  auto max_load_factor() const { return m_storage.max_load_factor(); }
  void max_load_factor(float factor) { m_storage.max_load_factor(factor); }

private:
  iterator lastPosition() {
    auto result = begin();
    for (auto position = result; position != end(); ++position) result = position;
    return result;
  }
  const_iterator lastPosition() const {
    auto result = begin();
    for (auto position = result; position != end(); ++position) result = position;
    return result;
  }
  value_type takeAt(iterator position) {
    auto node = m_storage.extract(position);
    return std::move(node.value());
  }
  Storage m_storage;
};

template <typename T>
decltype(auto) setOutputValue(T const& value) {
  if constexpr (requires { outputOptional(value); })
    return outputOptional(value);
  else
    return (value);
}

template <typename Container>
using SetOutputResult = decltype(std::declval<std::ostream&>() << setOutputValue(std::declval<typename Container::value_type const&>()));

template <SetContainer Container>
std::ostream& operator<<(std::ostream& os, Container const& set)
    requires std::same_as<SetOutputResult<Container>, std::ostream&>;

template <SetContainer Container>
[[nodiscard]] std::expected<std::reference_wrapper<std::ostream>, StarException> operator<<(std::ostream& os, Container const& set)
    requires std::same_as<SetOutputResult<Container>, std::expected<std::reference_wrapper<std::ostream>, StarException>>;

template <SetContainer Container>
std::ostream& operator<<(std::ostream& os, Container const& set)
    requires std::same_as<SetOutputResult<Container>, std::ostream&> {
  os << "(";
  for (auto i = set.begin(); i != set.end(); ++i) {
    if (i != set.begin()) os << ", ";
    os << setOutputValue(*i);
  }
  return os << ")";
}

template <SetContainer Container>
[[nodiscard]] std::expected<std::reference_wrapper<std::ostream>, StarException> operator<<(std::ostream& os, Container const& set)
    requires std::same_as<SetOutputResult<Container>, std::expected<std::reference_wrapper<std::ostream>, StarException>> {
  os << "(";
  for (auto i = set.begin(); i != set.end(); ++i) {
    if (i != set.begin()) os << ", ";
    auto written = os << setOutputValue(*i);
    if (!written) return std::unexpected(std::move(written.error()));
  }
  os << ")";
  return std::ref(os);
}

}

export module star.set;

export template <typename Value, typename Compare, typename Allocator>
  requires std::same_as<decltype(std::declval<std::ostream&>() << std::declval<Star::Set<Value, Compare, Allocator> const&>()), std::ostream&>
struct std::formatter<Star::Set<Value, Compare, Allocator>> : Star::StreamFormatter {};

export template <typename Value, typename Hash, typename Equals, typename Allocator>
  requires std::same_as<decltype(std::declval<std::ostream&>() << std::declval<Star::HashSet<Value, Hash, Equals, Allocator> const&>()), std::ostream&>
struct std::formatter<Star::HashSet<Value, Hash, Equals, Allocator>> : Star::StreamFormatter {};

export template <typename Value, typename Hash, typename Equals, typename Allocator>
  requires std::same_as<decltype(std::declval<std::ostream&>() << std::declval<Star::StableHashSet<Value, Hash, Equals, Allocator> const&>()), std::ostream&>
struct std::formatter<Star::StableHashSet<Value, Hash, Equals, Allocator>> : Star::StreamFormatter {};

export namespace Star {
  using ::Star::SetExceptionTag;
  using ::Star::SetException;
  using ::Star::SetContainer;
  using ::Star::Set;
  using ::Star::HashSet;
  using ::Star::StableHashSet;
  using ::Star::operator<<;
}
