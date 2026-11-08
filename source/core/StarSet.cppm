module;
import star.text_formatter;

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
concept set_container = requires { typename std::remove_cvref_t<T>::StarSetContainerTag; }
    && std::same_as<typename std::remove_cvref_t<T>::StarSetContainerTag, void>;

template <typename Storage, typename Key>
concept set_lookup = requires(Storage& storage, Key const& key) { storage.find(key); };

template <typename Range, typename Value>
concept set_source = std::ranges::input_range<Range>
  && std::is_constructible_v<Value, std::ranges::range_reference_t<Range>>;

template <typename Range, typename Value>
concept set_erase_source = std::ranges::input_range<Range>
  && std::convertible_to<std::ranges::range_reference_t<Range>, Value const&>;

template <typename Value>
concept set_copyable_value = std::is_constructible_v<Value, Value const&>;

template <typename Storage> struct set_storage_types {
  using key_compare = void;
  using value_compare = void;
  using hasher = typename Storage::hasher;
  using key_equal = typename Storage::key_equal;
};

template <typename Storage>
  requires requires { typename Storage::key_compare; typename Storage::value_compare; }
struct set_storage_types<Storage> {
  using key_compare = typename Storage::key_compare;
  using value_compare = typename Storage::value_compare;
  using hasher = void;
  using key_equal = void;
};

template <typename StorageType> class BasicSet {
public:
  using Storage = StorageType;
  using StarSetContainerTag = void;
  using key_type = typename Storage::key_type;
  using value_type = typename Storage::value_type;
  using iterator = typename Storage::iterator;
  using const_iterator = typename Storage::const_iterator;
  using size_type = typename Storage::size_type;
  using difference_type = typename Storage::difference_type;
  using allocator_type = typename Storage::allocator_type;
  using key_compare = typename set_storage_types<Storage>::key_compare;
  using value_compare = typename set_storage_types<Storage>::value_compare;
  using hasher = typename set_storage_types<Storage>::hasher;
  using key_equal = typename set_storage_types<Storage>::key_equal;

  BasicSet() = default;
  BasicSet(BasicSet const&)
    requires(set_copyable_value<value_type> && std::is_copy_constructible_v<Storage>) = default;
  BasicSet(BasicSet&&) = default;
  BasicSet& operator=(BasicSet const&)
    requires(set_copyable_value<value_type> && std::is_copy_assignable_v<Storage>) = default;
  BasicSet& operator=(BasicSet&&) = default;
  template <typename... Args>
    requires(sizeof...(Args) > 0 && std::constructible_from<Storage, Args...>)
  explicit BasicSet(Args&&... args) : m_storage(std::forward<Args>(args)...) {}
  template <std::input_iterator Iterator, std::sentinel_for<Iterator> Sentinel = Iterator, typename... Args>
    requires(std::is_constructible_v<value_type, std::iter_reference_t<Iterator>>
             && std::constructible_from<Storage, Iterator, Sentinel, Args...>)
  BasicSet(Iterator first, Sentinel last, Args&&... args)
    : m_storage(std::move(first), std::move(last), std::forward<Args>(args)...) {}
  template <std::input_iterator Iterator, std::sentinel_for<Iterator> Sentinel = Iterator, typename... Args>
    requires(std::is_constructible_v<value_type, std::iter_reference_t<Iterator>>
             && !std::constructible_from<Storage, Iterator, Sentinel, Args...>
             && std::constructible_from<Storage, Args...>)
  BasicSet(Iterator first, Sentinel last, Args&&... args) : m_storage(std::forward<Args>(args)...) {
    insert_range(std::ranges::subrange(std::move(first), std::move(last)));
  }
  BasicSet(std::initializer_list<value_type> values)
    requires set_copyable_value<value_type>
    : m_storage(values) {}
  template <typename... Args>
    requires(sizeof...(Args) > 0 && set_copyable_value<value_type>
             && std::constructible_from<Storage, std::initializer_list<value_type>, Args...>)
  BasicSet(std::initializer_list<value_type> values, Args&&... args)
    : m_storage(values, std::forward<Args>(args)...) {}
  BasicSet(std::initializer_list<value_type> values, allocator_type const& allocator)
    requires(set_copyable_value<value_type>
             && !std::constructible_from<Storage, std::initializer_list<value_type>, allocator_type const&>
             && std::constructible_from<Storage, std::initializer_list<value_type>, size_type, allocator_type const&>)
    : m_storage(values, size_type(0), allocator) {}
  BasicSet(BasicSet const& other, allocator_type const& allocator)
    requires(set_copyable_value<value_type> && std::constructible_from<Storage, Storage const&, allocator_type const&>)
    : m_storage(other.m_storage, allocator) {}
  BasicSet(BasicSet&& other, allocator_type const& allocator)
    requires std::constructible_from<Storage, Storage&&, allocator_type const&>
    : m_storage(std::move(other.m_storage), allocator) {}
  BasicSet& operator=(std::initializer_list<value_type> values)
    requires set_copyable_value<value_type>
  {
    m_storage = values;
    return *this;
  }
  template <typename Range>
    requires set_source<Range, value_type>
  static BasicSet from(Range&& range) {
    return BasicSet(std::ranges::begin(range), std::ranges::end(range));
  }

  iterator begin() noexcept(noexcept(m_storage.begin())) { return m_storage.begin(); }
  const_iterator begin() const noexcept(noexcept(m_storage.begin())) { return m_storage.begin(); }
  const_iterator cbegin() const noexcept(noexcept(m_storage.cbegin())) { return m_storage.cbegin(); }
  iterator end() noexcept(noexcept(m_storage.end())) { return m_storage.end(); }
  const_iterator end() const noexcept(noexcept(m_storage.end())) { return m_storage.end(); }
  const_iterator cend() const noexcept(noexcept(m_storage.cend())) { return m_storage.cend(); }
  bool empty() const noexcept(noexcept(m_storage.empty())) { return m_storage.empty(); }
  size_type size() const noexcept(noexcept(m_storage.size())) { return m_storage.size(); }
  void clear() noexcept(noexcept(m_storage.clear())) { m_storage.clear(); }
  void swap(BasicSet& other) noexcept(std::is_nothrow_swappable_v<Storage>) { std::swap(m_storage, other.m_storage); }

  // Snapshots own their elements. Views borrow an lvalue owner and follow the
  // underlying storage's invalidation rules.
  List<value_type> values() const
    requires set_copyable_value<value_type>
  {
    List<value_type> result;
    result.reserve(size());
    for (auto const& value : *this)
      result.push_back(value);
    return result;
  }
  auto value_view() const& { return std::ranges::subrange(begin(), end()); }
  auto value_view() const&& = delete;
  template <typename Key = key_type>
    requires set_lookup<Storage, Key>
  iterator find(Key const& key) & { return m_storage.find(key); }
  template <typename Key = key_type>
    requires set_lookup<Storage const, Key>
  const_iterator find(Key const& key) const& { return m_storage.find(key); }
  template <typename Key = key_type> iterator find(Key const&) && = delete;
  template <typename Key = key_type> const_iterator find(Key const&) const&& = delete;
  template <typename Key = key_type>
    requires set_lookup<Storage const, Key>
  bool contains(Key const& key) const { return find(key) != end(); }
  template <typename Key = key_type>
    requires set_lookup<Storage const, Key>
  size_type count(Key const& key) const { return contains(key) ? 1 : 0; }

  std::pair<iterator, bool> insert(value_type const& value)
    requires set_copyable_value<value_type>
  { return m_storage.insert(value); }
  std::pair<iterator, bool> insert(value_type&& value)
    requires std::is_constructible_v<value_type, value_type&&>
  { return m_storage.insert(std::move(value)); }
  iterator insert(const_iterator hint, value_type const& value)
    requires set_copyable_value<value_type>
  { return m_storage.insert(hint, value); }
  iterator insert(const_iterator hint, value_type&& value)
    requires std::is_constructible_v<value_type, value_type&&>
  { return m_storage.insert(hint, std::move(value)); }
  template <std::input_iterator Iterator, std::sentinel_for<Iterator> Sentinel = Iterator>
    requires std::is_constructible_v<value_type, std::iter_reference_t<Iterator>>
  void insert(Iterator first, Sentinel last) {
    insert_range(std::ranges::subrange(std::move(first), std::move(last)));
  }
  void insert(std::initializer_list<value_type> values)
    requires set_copyable_value<value_type>
  { insert_range(values); }
  template <typename... Args>
    requires std::constructible_from<value_type, Args...>
  auto emplace(Args&&... args) { return m_storage.emplace(std::forward<Args>(args)...); }
  template <typename... Args>
    requires std::constructible_from<value_type, Args...>
  iterator emplace_hint(const_iterator hint, Args&&... args) {
    return m_storage.emplace_hint(hint, std::forward<Args>(args)...);
  }
  iterator erase(const_iterator position) { return m_storage.erase(position); }
  iterator erase(const_iterator first, const_iterator last) { return m_storage.erase(first, last); }
  size_type erase(key_type const& key) { return m_storage.erase(key); }
  auto equal_range(key_type const& key) { return m_storage.equal_range(key); }
  auto equal_range(key_type const& key) const { return m_storage.equal_range(key); }
  bool add(value_type const& value)
    requires set_copyable_value<value_type>
  { return insert(value).second; }
  bool add(value_type&& value)
    requires std::is_constructible_v<value_type, value_type&&>
  { return insert(std::move(value)).second; }
  bool remove(value_type const& value) { return erase(value) != 0; }
  // Own the argument before erasing an equivalent stored object, including an
  // argument that aliases that object and a replacement with different payload.
  bool replace(value_type value)
    requires std::is_constructible_v<value_type, value_type&&>
  {
    bool replaced = remove(value);
    insert(std::move(value));
    return replaced;
  }
  template <typename Range>
    requires set_source<Range, value_type>
  void insert_range(Range&& range) {
    if (aliases(range))
      return;
    if constexpr (requires { m_storage.insert_range(std::forward<Range>(range)); }) {
      m_storage.insert_range(std::forward<Range>(range));
    } else if constexpr (std::ranges::common_range<Range>) {
      m_storage.insert(std::ranges::begin(range), std::ranges::end(range));
    } else {
      for (auto&& value : range)
        m_storage.emplace(std::forward<decltype(value)>(value));
    }
  }
  template <typename Range>
    requires set_erase_source<Range, value_type>
  void erase_all(Range&& range) {
    if constexpr (std::same_as<std::remove_cvref_t<Range>, BasicSet>) {
      if (std::addressof(range) == this) {
        clear();
        return;
      }
    }
    if constexpr (std::ranges::common_range<Range>
                  && (std::same_as<std::ranges::iterator_t<Range>, iterator>
                      || std::same_as<std::ranges::iterator_t<Range>, const_iterator>)) {
      if (aliases(range)) {
        m_storage.erase(std::ranges::begin(range), std::ranges::end(range));
        return;
      }
    }
    for (auto const& value : range)
      remove(value);
  }

  [[nodiscard]] std::expected<value_type, StarException> first() const
    requires set_copyable_value<value_type>
  {
    if (empty()) return std::unexpected(SetException("first called on empty set"));
    return std::expected<value_type, StarException>(std::in_place, *begin());
  }
  std::optional<value_type> maybe_first() const
    requires set_copyable_value<value_type>
  {
    if (empty()) return {};
    return std::optional<value_type>(std::in_place, *begin());
  }
  [[nodiscard]] std::expected<value_type, StarException> last() const
    requires set_copyable_value<value_type>
  {
    if (empty()) return std::unexpected(SetException("last called on empty set"));
    return std::expected<value_type, StarException>(std::in_place, *last_position());
  }
  std::optional<value_type> maybe_last() const
    requires set_copyable_value<value_type>
  {
    if (empty()) return {};
    return std::optional<value_type>(std::in_place, *last_position());
  }
  [[nodiscard]] std::expected<value_type, StarException> take_first()
    requires std::is_constructible_v<value_type, value_type&&>
  {
    if (empty()) return std::unexpected(SetException("take_first called on empty set"));
    return take_at(begin());
  }
  std::optional<value_type> maybe_take_first()
    requires std::is_constructible_v<value_type, value_type&&>
  {
    if (empty()) return {};
    return std::optional<value_type>(std::in_place, extract_at(begin()));
  }
  [[nodiscard]] std::expected<value_type, StarException> take_last()
    requires std::is_constructible_v<value_type, value_type&&>
  {
    if (empty()) return std::unexpected(SetException("take_last called on empty set"));
    return take_at(last_position());
  }
  std::optional<value_type> maybe_take_last()
    requires std::is_constructible_v<value_type, value_type&&>
  {
    if (empty()) return {};
    return std::optional<value_type>(std::in_place, extract_at(last_position()));
  }
  bool has_intersection(BasicSet const& other) const {
    return std::ranges::any_of(other, [this](auto const& value) { return contains(value); });
  }
  BasicSet intersection(BasicSet const& other) const
    requires set_copyable_value<value_type>
  {
    if constexpr (!std::is_void_v<key_compare>) {
      if (compatible_order(other))
        return intersection(other, m_storage.key_comp());
      // Normalize only an incompatible ordering; the usual standard comparator
      // path keeps its linear merge and does not allocate a second input tree.
      BasicSet normalized(other.begin(), other.end(), m_storage.key_comp(), m_storage.get_allocator());
      return intersection(normalized, m_storage.key_comp());
    } else {
      auto result = empty_like();
      for (auto const& value : other)
        if (contains(value)) result.add(value);
      return result;
    }
  }
  BasicSet difference(BasicSet const& other) const
    requires set_copyable_value<value_type>
  {
    if constexpr (!std::is_void_v<key_compare>) {
      if (compatible_order(other))
        return difference(other, m_storage.key_comp());
      BasicSet normalized(other.begin(), other.end(), m_storage.key_comp(), m_storage.get_allocator());
      return difference(normalized, m_storage.key_comp());
    } else {
      auto result = empty_like();
      for (auto const& value : *this)
        if (!other.contains(value)) result.add(value);
      return result;
    }
  }
  // As with the standard set algorithms, an explicitly supplied comparator
  // requires both input ranges to be sorted under that comparator.
  template <typename Compare>
    requires(!std::is_void_v<key_compare> && set_copyable_value<value_type>
             && std::predicate<Compare&, value_type const&, value_type const&>)
  BasicSet intersection(BasicSet const& other, Compare compare) const {
    auto result = empty_like();
    std::ranges::set_intersection(*this, other, std::inserter(result, result.end()), std::move(compare));
    return result;
  }
  template <typename Compare>
    requires(!std::is_void_v<key_compare> && set_copyable_value<value_type>
             && std::predicate<Compare&, value_type const&, value_type const&>)
  BasicSet difference(BasicSet const& other, Compare compare) const {
    auto result = empty_like();
    std::ranges::set_difference(*this, other, std::inserter(result, result.end()), std::move(compare));
    return result;
  }
  BasicSet combination(BasicSet const& other) const
    requires set_copyable_value<value_type>
  {
    BasicSet result(*this, m_storage.get_allocator());
    result.insert_range(other);
    return result;
  }
  bool operator==(BasicSet const& other) const { return m_storage == other.m_storage; }

  auto lower_bound(key_type const& key)
    requires requires(Storage& storage) { storage.lower_bound(key); }
  { return m_storage.lower_bound(key); }
  auto lower_bound(key_type const& key) const
    requires requires(Storage const& storage) { storage.lower_bound(key); }
  { return m_storage.lower_bound(key); }
  auto upper_bound(key_type const& key)
    requires requires(Storage& storage) { storage.upper_bound(key); }
  { return m_storage.upper_bound(key); }
  auto upper_bound(key_type const& key) const
    requires requires(Storage const& storage) { storage.upper_bound(key); }
  { return m_storage.upper_bound(key); }
  auto key_comp() const requires requires(Storage const& storage) { storage.key_comp(); }
  { return m_storage.key_comp(); }
  auto value_comp() const requires requires(Storage const& storage) { storage.value_comp(); }
  { return m_storage.value_comp(); }
  auto rbegin() requires requires(Storage& storage) { storage.rbegin(); }
  { return m_storage.rbegin(); }
  auto rbegin() const requires requires(Storage const& storage) { storage.rbegin(); }
  { return m_storage.rbegin(); }
  auto rend() requires requires(Storage& storage) { storage.rend(); }
  { return m_storage.rend(); }
  auto rend() const requires requires(Storage const& storage) { storage.rend(); }
  { return m_storage.rend(); }
  void reserve(size_type capacity) requires requires(Storage& storage) { storage.reserve(capacity); }
  { m_storage.reserve(capacity); }
  void rehash(size_type buckets) requires requires(Storage& storage) { storage.rehash(buckets); }
  { m_storage.rehash(buckets); }
  auto hash_function() const requires requires(Storage const& storage) { storage.hash_function(); }
  { return m_storage.hash_function(); }
  auto key_eq() const requires requires(Storage const& storage) { storage.key_eq(); }
  { return m_storage.key_eq(); }
  auto get_allocator() const { return m_storage.get_allocator(); }
  auto max_size() const requires requires(Storage const& storage) { storage.max_size(); }
  { return m_storage.max_size(); }
  auto bucket_count() const requires requires(Storage const& storage) { storage.bucket_count(); }
  { return m_storage.bucket_count(); }
  auto load_factor() const requires requires(Storage const& storage) { storage.load_factor(); }
  { return m_storage.load_factor(); }
  auto max_load_factor() const requires requires(Storage const& storage) { storage.max_load_factor(); }
  { return m_storage.max_load_factor(); }
  void max_load_factor(float factor) requires requires(Storage& storage) { storage.max_load_factor(factor); }
  { m_storage.max_load_factor(factor); }

private:
  template <std::ranges::input_range Range>
  bool aliases(Range& range) const {
    if constexpr (std::same_as<std::remove_cvref_t<Range>, BasicSet>)
      if (std::addressof(range) == this) return true;
    // Only inspect real storage iterators, never dereference a projection just
    // to detect borrowing. Opaque projected sources must not overlap this set.
    if constexpr (std::same_as<std::ranges::iterator_t<Range>, iterator>
                  || std::same_as<std::ranges::iterator_t<Range>, const_iterator>) {
      auto first = std::ranges::begin(range);
      if (first == std::ranges::end(range)) return false;
      auto found = find(*first);
      return found != end() && std::addressof(*found) == std::addressof(*first);
    }
    return false;
  }
  auto last_position() {
    if constexpr (std::bidirectional_iterator<iterator>) return std::prev(end());
    else {
      auto last = begin();
      for (auto position = last; position != end(); ++position) last = position;
      return last;
    }
  }
  auto last_position() const {
    if constexpr (std::bidirectional_iterator<const_iterator>) return std::prev(end());
    else {
      auto last = begin();
      for (auto position = last; position != end(); ++position) last = position;
      return last;
    }
  }
  value_type extract_at(iterator position) {
    if constexpr (requires { m_storage.extract_value(position); }) {
      return m_storage.extract_value(position);
    } else {
      auto node = m_storage.extract(position);
      return std::move(node.value());
    }
  }
  std::expected<value_type, StarException> take_at(iterator position) {
    if constexpr (requires { m_storage.extract(position); }) {
      auto node = m_storage.extract(position);
      return std::expected<value_type, StarException>(std::in_place, std::move(node.value()));
    } else {
      return extract_at(position);
    }
  }
  BasicSet empty_like() const {
    if constexpr (!std::is_void_v<key_compare>)
      return BasicSet(m_storage.key_comp(), m_storage.get_allocator());
    else {
      BasicSet result(size_type(0), m_storage.hash_function(), m_storage.key_eq(), m_storage.get_allocator());
      if constexpr (requires { result.m_storage.max_load_factor(m_storage.max_load_factor()); })
        result.m_storage.max_load_factor(m_storage.max_load_factor());
      return result;
    }
  }
  bool compatible_order(BasicSet const& other) const
    requires(!std::is_void_v<key_compare>)
  {
    if constexpr (std::same_as<key_compare, std::less<key_type>> || std::same_as<key_compare, std::less<>>
                  || std::same_as<key_compare, std::greater<key_type>> || std::same_as<key_compare, std::greater<>>)
      return true;
    else {
      if constexpr (std::equality_comparable<key_compare>)
        if (m_storage.key_comp() == other.m_storage.key_comp()) return true;
      return std::ranges::is_sorted(other, m_storage.key_comp());
    }
  }
  Storage m_storage;
};

template <typename Value, typename Compare = std::less<Value>, typename Allocator = std::allocator<Value>>
using Set = BasicSet<std::set<Value, Compare, Allocator>>;
template <typename Value, typename Hash = hash<Value>, typename Equals = std::equal_to<Value>, typename Allocator = std::allocator<Value>>
using HashSet = BasicSet<FlatHashSet<Value, Hash, Equals, Allocator>>;
template <typename Value, typename Hash = hash<Value>, typename Equals = std::equal_to<Value>, typename Allocator = std::allocator<Value>>
using StableHashSet = BasicSet<std::unordered_set<Value, Hash, Equals, Allocator>>;

template <typename Container>
using set_output_result = optional_diagnostic_result<typename Container::value_type>;

template <set_container Container>
void write_text(TextBuffer& output, Container const& set)
    requires std::same_as<set_output_result<Container>, void>;

template <set_container Container>
[[nodiscard]] std::expected<void, StarException> write_text(TextBuffer& output, Container const& set)
    requires std::same_as<set_output_result<Container>, std::expected<void, StarException>>;

template <set_container Container>
void write_text(TextBuffer& output, Container const& set)
    requires std::same_as<set_output_result<Container>, void> {
  output.push_back('(');
  for (auto i = set.begin(); i != set.end(); ++i) {
    if (i != set.begin())
      output.append(", ");
    append_text(output, optional_diagnostic_value(*i));
  }
  output.push_back(')');
}

template <set_container Container>
[[nodiscard]] std::expected<void, StarException> write_text(TextBuffer& output, Container const& set)
    requires std::same_as<set_output_result<Container>, std::expected<void, StarException>> {
  output.push_back('(');
  for (auto i = set.begin(); i != set.end(); ++i) {
    if (i != set.begin())
      output.append(", ");
    auto written = append_text(output, optional_diagnostic_value(*i));
    if (!written)
      return std::unexpected(std::move(written.error()));
  }
  output.push_back(')');
  return {};
}

}

export module star.set;

export template <typename Storage>
  requires Star::nonfallible_text_writer<Star::BasicSet<Storage>>
struct std::formatter<Star::BasicSet<Storage>> : Star::TextFormatter {};

export namespace Star {
  using ::Star::SetExceptionTag;
  using ::Star::SetException;
  using ::Star::set_container;
  using ::Star::set_lookup;
  using ::Star::set_source;
  using ::Star::set_erase_source;
  using ::Star::set_copyable_value;
  using ::Star::BasicSet;
  using ::Star::Set;
  using ::Star::HashSet;
  using ::Star::StableHashSet;
  using ::Star::write_text;
}
