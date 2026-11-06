module;
import star.stream_formatter;

import std;
import star.flat_hash_map;
import star.list;
import star.optional;
import star.hash;
import star.format;
import star.output_proxy;
import star.algorithm;

namespace Star {

struct MapExceptionTag {
  static constexpr std::string_view name() { return "MapException"; }
};
using MapException = StarError<MapExceptionTag, StarException>;

template <typename T>
concept MapContainer = requires { typename std::remove_cvref_t<T>::StarMapContainerTag; }
    && std::same_as<typename std::remove_cvref_t<T>::StarMapContainerTag, void>;

template <typename T>
[[nodiscard]] std::expected<std::string, StarException> mapDiagnosticValue(T const& value) {
  if constexpr (std::is_enum_v<T>) {
    return strf("{}", std::to_underlying(value));
  } else {
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    using PrintResult = decltype(stream << outputAny(value));
    if constexpr (std::same_as<PrintResult, std::expected<std::reference_wrapper<std::ostream>, StarException>>) {
      auto printed = stream << outputAny(value);
      if (!printed)
        return std::unexpected(std::move(printed.error()));
    } else {
      stream << outputAny(value);
    }
    return std::move(stream).str();
  }
}

template <typename Error = MapException, typename T>
[[nodiscard]] StarException mapDiagnosticError(std::format_string<std::string> message,
    std::string_view context, T const& value) {
  auto rendered = mapDiagnosticValue(value);
  if (!rendered)
    return std::move(rendered.error()).withContext(std::string(context));
  return Error(strf(message, std::move(*rendered)));
}

template <typename Key, typename Value, typename Compare = std::less<Key>, typename Allocator = std::allocator<std::pair<Key const, Value>>>
class Map {
public:
  using Storage = std::map<Key, Value, Compare, Allocator>;
  using StarMapContainerTag = void;
  using key_type = Key;
  using mapped_type = Value;
  using value_type = typename Storage::value_type;
  using iterator = typename Storage::iterator;
  using const_iterator = typename Storage::const_iterator;
  using size_type = typename Storage::size_type;
  using difference_type = typename Storage::difference_type;
  using allocator_type = Allocator;
  using key_compare = Compare;
  using value_compare = typename Storage::value_compare;

  Map() = default;
  template <typename... Args>
    requires (sizeof...(Args) > 0 && std::constructible_from<Storage, Args...>)
  explicit Map(Args&&... args) : m_storage(std::forward<Args>(args)...) {}
  template <typename InputIt, typename... Args>
    requires std::constructible_from<Storage, InputIt, InputIt, Args...>
  Map(InputIt first, InputIt last, Args&&... args)
    : m_storage(first, last, std::forward<Args>(args)...) {}
  Map(std::initializer_list<value_type> entries) : m_storage(entries) {}
  Map(std::initializer_list<value_type> entries, Compare const& compare, allocator_type const& allocator = allocator_type())
    : m_storage(entries, compare, allocator) {}
  Map(std::initializer_list<value_type> entries, allocator_type const& allocator)
    : m_storage(entries, allocator) {}
  Map(Map const& other, allocator_type const& allocator) : m_storage(other.m_storage, allocator) {}
  Map(Map&& other, allocator_type const& allocator) : m_storage(std::move(other.m_storage), allocator) {}
  Map& operator=(std::initializer_list<value_type> entries) { m_storage = entries; return *this; }

  template <typename Range>
  static Map from(Range const& range) { return Map(std::ranges::begin(range), std::ranges::end(range)); }

  iterator begin() { return m_storage.begin(); }
  const_iterator begin() const { return m_storage.begin(); }
  const_iterator cbegin() const { return m_storage.cbegin(); }
  iterator end() { return m_storage.end(); }
  const_iterator end() const { return m_storage.end(); }
  const_iterator cend() const { return m_storage.cend(); }
  bool empty() const { return m_storage.empty(); }
  size_type size() const { return m_storage.size(); }
  void clear() { m_storage.clear(); }
  void swap(Map& other) noexcept(std::is_nothrow_swappable_v<Storage>) { std::swap(m_storage, other.m_storage); }

  // Snapshots own their elements; views borrow this owner and follow storage invalidation.
  List<key_type> keys() const {
    List<key_type> result;
    result.reserve(size());
    std::ranges::copy(keyView(), std::back_inserter(result));
    return result;
  }
  List<mapped_type> values() const {
    List<mapped_type> result;
    result.reserve(size());
    std::ranges::copy(valueView(), std::back_inserter(result));
    return result;
  }
  List<std::pair<key_type, mapped_type>> pairs() const {
    List<std::pair<key_type, mapped_type>> result;
    result.reserve(size());
    std::ranges::copy(*this, std::back_inserter(result));
    return result;
  }
  auto keyView() const& { return std::views::keys(m_storage); }
  auto keyView() const&& = delete;
  auto valueView() & { return std::views::values(m_storage); }
  auto valueView() const& { return std::views::values(m_storage); }
  auto valueView() && = delete;
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

  template <typename LookupKey = key_type>
    requires requires(Storage& storage, LookupKey const& key) { storage.find(key); }
  [[nodiscard]] std::expected<std::reference_wrapper<mapped_type>, StarException> get(LookupKey const& key) {
    auto position = find(key);
    if (position == end())
      return std::unexpected(mapDiagnosticError("Key '{}' not found in Map::get()", "Map::get: key not found while rendering its diagnostic", key));
    return std::reference_wrapper<mapped_type>(position->second);
  }
  template <typename LookupKey = key_type>
    requires requires(Storage const& storage, LookupKey const& key) { storage.find(key); }
  [[nodiscard]] std::expected<std::reference_wrapper<mapped_type const>, StarException> get(LookupKey const& key) const {
    auto position = find(key);
    if (position == end())
      return std::unexpected(mapDiagnosticError("Key '{}' not found in Map::get()", "Map::get: key not found while rendering its diagnostic", key));
    return std::reference_wrapper<mapped_type const>(position->second);
  }
  [[nodiscard]] std::expected<std::reference_wrapper<mapped_type>, StarException> at(key_type const& key) { return get(key); }
  [[nodiscard]] std::expected<std::reference_wrapper<mapped_type const>, StarException> at(key_type const& key) const { return get(key); }
  template <typename LookupKey = key_type>
    requires requires(Storage const& storage, LookupKey const& key) { storage.find(key); }
  mapped_type value(LookupKey const& key, mapped_type fallback = mapped_type()) const {
    auto position = find(key);
    return position == end() ? std::move(fallback) : position->second;
  }
  template <typename LookupKey = key_type>
    requires requires(Storage const& storage, LookupKey const& key) { storage.find(key); }
  std::optional<mapped_type> maybe(LookupKey const& key) const {
    auto position = find(key);
    if (position == end()) return {};
    return std::optional<mapped_type>(std::in_place, position->second);
  }
  template <typename LookupKey = key_type>
    requires requires(Storage& storage, LookupKey const& key) { storage.find(key); }
  // Borrow the mapped slot itself, including when mapped_type is a reference_wrapper.
  std::optional<std::reference_wrapper<mapped_type>> maybeRef(LookupKey const& key) {
    auto position = find(key);
    if (position == end()) return std::nullopt;
    return std::reference_wrapper<mapped_type>(position->second);
  }
  template <typename LookupKey = key_type>
    requires requires(Storage const& storage, LookupKey const& key) { storage.find(key); }
  std::optional<std::reference_wrapper<mapped_type const>> maybeRef(LookupKey const& key) const {
    auto position = find(key);
    if (position == end()) return std::nullopt;
    return std::reference_wrapper<mapped_type const>(position->second);
  }
  bool remove(key_type const& key) { return m_storage.erase(key) != 0; }
  bool removeValues(mapped_type const& value) {
    bool removed = false;
    for (auto position = begin(); position != end();) {
      if (position->second == value) {
        position = m_storage.erase(position);
        removed = true;
      } else {
        ++position;
      }
    }
    return removed;
  }
  [[nodiscard]] std::expected<mapped_type, StarException> take(key_type const& key) {
    if (auto value = maybeTake(key)) return std::move(*value);
    return std::unexpected(mapDiagnosticError("Key '{}' not found in Map::take()", "Map::take: key not found while rendering its diagnostic", key));
  }
  std::optional<mapped_type> maybeTake(key_type const& key) {
    auto position = find(key);
    if (position == end()) return {};
    mapped_type value = std::move(position->second);
    m_storage.erase(position);
    return std::optional<mapped_type>(std::in_place, std::move(value));
  }
  [[nodiscard]] std::expected<key_type, StarException> keyOf(mapped_type const& value) const {
    auto position = std::ranges::find(*this, value, &value_type::second);
    if (position != end()) return position->first;
    return std::unexpected(mapDiagnosticError("Value '{}' not found in Map::keyOf()", "Map::keyOf: value not found while rendering its diagnostic", value));
  }
  bool hasValue(mapped_type const& value) const {
    return std::ranges::find(*this, value, &value_type::second) != end();
  }

  mapped_type& operator[](key_type const& key) { return m_storage[key]; }
  mapped_type& operator[](key_type&& key) { return m_storage[std::move(key)]; }
  std::pair<iterator, bool> insert(value_type const& value) { return m_storage.insert(value); }
  std::pair<iterator, bool> insert(value_type&& value) { return m_storage.insert(std::move(value)); }
  template <typename Pair>
    requires std::constructible_from<value_type, Pair&&>
  std::pair<iterator, bool> insert(Pair&& pair) { return m_storage.insert(std::forward<Pair>(pair)); }
  iterator insert(const_iterator hint, value_type const& value) { return m_storage.insert(hint, value); }
  iterator insert(const_iterator hint, value_type&& value) { return m_storage.insert(hint, std::move(value)); }
  template <typename InputIt>
  void insert(InputIt first, InputIt last) { m_storage.insert(first, last); }
  void insert(std::initializer_list<value_type> values) { m_storage.insert(values); }
  std::pair<iterator, bool> insert(key_type key, mapped_type value) { return m_storage.try_emplace(std::move(key), std::move(value)); }
  template <typename... Args>
  auto emplace(Args&&... args) { return m_storage.emplace(std::forward<Args>(args)...); }
  template <typename... Args>
  iterator emplace_hint(const_iterator hint, Args&&... args) { return m_storage.emplace_hint(hint, std::forward<Args>(args)...); }
  template <typename... Args>
  auto try_emplace(key_type const& key, Args&&... args) { return m_storage.try_emplace(key, std::forward<Args>(args)...); }
  template <typename... Args>
  auto try_emplace(key_type&& key, Args&&... args) { return m_storage.try_emplace(std::move(key), std::forward<Args>(args)...); }
  template <typename Mapped>
  auto insert_or_assign(key_type const& key, Mapped&& value) { return m_storage.insert_or_assign(key, std::forward<Mapped>(value)); }
  template <typename Mapped>
  auto insert_or_assign(key_type&& key, Mapped&& value) { return m_storage.insert_or_assign(std::move(key), std::forward<Mapped>(value)); }
  iterator erase(const_iterator position) { return m_storage.erase(position); }
  iterator erase(const_iterator first, const_iterator last) { return m_storage.erase(first, last); }
  size_type erase(key_type const& key) { return m_storage.erase(key); }
  auto equal_range(key_type const& key) { return m_storage.equal_range(key); }
  auto equal_range(key_type const& key) const { return m_storage.equal_range(key); }
  [[nodiscard]] std::expected<std::reference_wrapper<mapped_type>, StarException> add(key_type key, mapped_type value) {
    auto result = m_storage.try_emplace(std::move(key), std::move(value));
    if (!result.second)
      return std::unexpected(mapDiagnosticError("Entry with key '{}' already present.", "Map::add: key already present while rendering its diagnostic", result.first->first));
    return std::reference_wrapper<mapped_type>(result.first->second);
  }
  mapped_type& set(key_type key, mapped_type value) { return m_storage.insert_or_assign(std::move(key), std::move(value)).first->second; }
  template <typename OtherMap>
  bool merge(OtherMap const& other, bool overwrite = false) { return mapMerge(*this, other, overwrite); }
  bool operator==(Map const& other) const { return this == &other || mapsEqual(*this, other); }
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
  Storage m_storage;
};

template <typename Key, typename Value, typename Hash = hash<Key>, typename Equals = std::equal_to<>, typename Allocator = std::allocator<std::pair<Key const, Value>>>
class HashMap {
public:
  using Storage = FlatHashMap<Key, Value, Hash, Equals, Allocator>;
  using StarMapContainerTag = void;
  using key_type = Key;
  using mapped_type = Value;
  using value_type = typename Storage::value_type;
  using iterator = typename Storage::iterator;
  using const_iterator = typename Storage::const_iterator;
  using size_type = typename Storage::size_type;
  using difference_type = typename Storage::difference_type;
  using allocator_type = Allocator;
  using hasher = Hash;
  using key_equal = Equals;

  HashMap() = default;
  template <typename... Args>
    requires (sizeof...(Args) > 0 && std::constructible_from<Storage, Args...>)
  explicit HashMap(Args&&... args) : m_storage(std::forward<Args>(args)...) {}
  template <typename InputIt, typename... Args>
    requires std::constructible_from<Storage, InputIt, InputIt, Args...>
  HashMap(InputIt first, InputIt last, Args&&... args)
    : m_storage(first, last, std::forward<Args>(args)...) {}
  HashMap(std::initializer_list<value_type> entries) : m_storage(entries) {}
  HashMap(std::initializer_list<value_type> entries, size_type bucketCount, hasher const& hash = hasher(),
      key_equal const& equal = key_equal(), allocator_type const& allocator = allocator_type())
    : m_storage(entries, bucketCount, hash, equal, allocator) {}
  HashMap(std::initializer_list<value_type> entries, allocator_type const& allocator)
    : m_storage(entries, 0, hasher(), key_equal(), allocator) {}
  HashMap(std::initializer_list<value_type> entries, size_type bucketCount, allocator_type const& allocator)
    : m_storage(entries, bucketCount, allocator) {}
  HashMap(std::initializer_list<value_type> entries, size_type bucketCount, hasher const& hash, allocator_type const& allocator)
    : m_storage(entries, bucketCount, hash, allocator) {}
  HashMap(HashMap const& other, allocator_type const& allocator) : m_storage(other.m_storage, allocator) {}
  HashMap(HashMap&& other, allocator_type const& allocator) : m_storage(std::move(other.m_storage), allocator) {}
  HashMap& operator=(std::initializer_list<value_type> entries) { m_storage = entries; return *this; }

  template <typename Range>
  static HashMap from(Range const& range) { return HashMap(std::ranges::begin(range), std::ranges::end(range)); }

  iterator begin() { return m_storage.begin(); }
  const_iterator begin() const { return m_storage.begin(); }
  const_iterator cbegin() const { return m_storage.cbegin(); }
  iterator end() { return m_storage.end(); }
  const_iterator end() const { return m_storage.end(); }
  const_iterator cend() const { return m_storage.cend(); }
  bool empty() const { return m_storage.empty(); }
  size_type size() const { return m_storage.size(); }
  void clear() { m_storage.clear(); }
  void swap(HashMap& other) noexcept(std::is_nothrow_swappable_v<Storage>) { std::swap(m_storage, other.m_storage); }

  // Snapshots own their elements; views borrow this owner and follow storage invalidation.
  List<key_type> keys() const {
    List<key_type> result;
    result.reserve(size());
    std::ranges::copy(keyView(), std::back_inserter(result));
    return result;
  }
  List<mapped_type> values() const {
    List<mapped_type> result;
    result.reserve(size());
    std::ranges::copy(valueView(), std::back_inserter(result));
    return result;
  }
  List<std::pair<key_type, mapped_type>> pairs() const {
    List<std::pair<key_type, mapped_type>> result;
    result.reserve(size());
    std::ranges::copy(*this, std::back_inserter(result));
    return result;
  }
  auto keyView() const& { return std::views::keys(m_storage); }
  auto keyView() const&& = delete;
  auto valueView() & { return std::views::values(m_storage); }
  auto valueView() const& { return std::views::values(m_storage); }
  auto valueView() && = delete;
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

  template <typename LookupKey = key_type>
    requires requires(Storage& storage, LookupKey const& key) { storage.find(key); }
  [[nodiscard]] std::expected<std::reference_wrapper<mapped_type>, StarException> get(LookupKey const& key) {
    auto position = find(key);
    if (position == end())
      return std::unexpected(mapDiagnosticError("Key '{}' not found in Map::get()", "Map::get: key not found while rendering its diagnostic", key));
    return std::reference_wrapper<mapped_type>(position->second);
  }
  template <typename LookupKey = key_type>
    requires requires(Storage const& storage, LookupKey const& key) { storage.find(key); }
  [[nodiscard]] std::expected<std::reference_wrapper<mapped_type const>, StarException> get(LookupKey const& key) const {
    auto position = find(key);
    if (position == end())
      return std::unexpected(mapDiagnosticError("Key '{}' not found in Map::get()", "Map::get: key not found while rendering its diagnostic", key));
    return std::reference_wrapper<mapped_type const>(position->second);
  }
  [[nodiscard]] std::expected<std::reference_wrapper<mapped_type>, StarException> at(key_type const& key) { return get(key); }
  [[nodiscard]] std::expected<std::reference_wrapper<mapped_type const>, StarException> at(key_type const& key) const { return get(key); }
  template <typename LookupKey = key_type>
    requires requires(Storage const& storage, LookupKey const& key) { storage.find(key); }
  mapped_type value(LookupKey const& key, mapped_type fallback = mapped_type()) const {
    auto position = find(key);
    return position == end() ? std::move(fallback) : position->second;
  }
  template <typename LookupKey = key_type>
    requires requires(Storage const& storage, LookupKey const& key) { storage.find(key); }
  std::optional<mapped_type> maybe(LookupKey const& key) const {
    auto position = find(key);
    if (position == end()) return {};
    return std::optional<mapped_type>(std::in_place, position->second);
  }
  template <typename LookupKey = key_type>
    requires requires(Storage& storage, LookupKey const& key) { storage.find(key); }
  std::optional<std::reference_wrapper<mapped_type>> maybeRef(LookupKey const& key) {
    auto position = find(key);
    if (position == end()) return std::nullopt;
    return std::reference_wrapper<mapped_type>(position->second);
  }
  template <typename LookupKey = key_type>
    requires requires(Storage const& storage, LookupKey const& key) { storage.find(key); }
  std::optional<std::reference_wrapper<mapped_type const>> maybeRef(LookupKey const& key) const {
    auto position = find(key);
    if (position == end()) return std::nullopt;
    return std::reference_wrapper<mapped_type const>(position->second);
  }
  bool remove(key_type const& key) { return m_storage.erase(key) != 0; }
  bool removeValues(mapped_type const& value) {
    bool removed = false;
    for (auto position = begin(); position != end();) {
      if (position->second == value) {
        position = m_storage.erase(position);
        removed = true;
      } else {
        ++position;
      }
    }
    return removed;
  }
  [[nodiscard]] std::expected<mapped_type, StarException> take(key_type const& key) {
    if (auto value = maybeTake(key)) return std::move(*value);
    return std::unexpected(mapDiagnosticError("Key '{}' not found in Map::take()", "Map::take: key not found while rendering its diagnostic", key));
  }
  std::optional<mapped_type> maybeTake(key_type const& key) {
    auto position = find(key);
    if (position == end()) return {};
    mapped_type value = std::move(position->second);
    m_storage.erase(position);
    return std::optional<mapped_type>(std::in_place, std::move(value));
  }
  [[nodiscard]] std::expected<key_type, StarException> keyOf(mapped_type const& value) const {
    auto position = std::ranges::find(*this, value, &value_type::second);
    if (position != end()) return position->first;
    return std::unexpected(mapDiagnosticError("Value '{}' not found in Map::keyOf()", "Map::keyOf: value not found while rendering its diagnostic", value));
  }
  bool hasValue(mapped_type const& value) const {
    return std::ranges::find(*this, value, &value_type::second) != end();
  }

  mapped_type& operator[](key_type const& key) { return m_storage[key]; }
  mapped_type& operator[](key_type&& key) { return m_storage[std::move(key)]; }
  std::pair<iterator, bool> insert(value_type const& value) { return m_storage.insert(value); }
  std::pair<iterator, bool> insert(value_type&& value) { return m_storage.insert(std::move(value)); }
  template <typename Pair>
    requires std::constructible_from<value_type, Pair&&>
  std::pair<iterator, bool> insert(Pair&& pair) { return m_storage.insert(std::forward<Pair>(pair)); }
  iterator insert(const_iterator hint, value_type const& value) { return m_storage.insert(hint, value); }
  iterator insert(const_iterator hint, value_type&& value) { return m_storage.insert(hint, std::move(value)); }
  template <typename InputIt>
  void insert(InputIt first, InputIt last) { m_storage.insert(first, last); }
  void insert(std::initializer_list<value_type> values) { m_storage.insert(values); }
  std::pair<iterator, bool> insert(key_type key, mapped_type value) { return m_storage.try_emplace(std::move(key), std::move(value)); }
  template <typename... Args>
  auto emplace(Args&&... args) { return m_storage.emplace(std::forward<Args>(args)...); }
  template <typename... Args>
  iterator emplace_hint(const_iterator hint, Args&&... args) { return m_storage.emplace_hint(hint, std::forward<Args>(args)...); }
  template <typename... Args>
  auto try_emplace(key_type const& key, Args&&... args) { return m_storage.try_emplace(key, std::forward<Args>(args)...); }
  template <typename... Args>
  auto try_emplace(key_type&& key, Args&&... args) { return m_storage.try_emplace(std::move(key), std::forward<Args>(args)...); }
  template <typename Mapped>
  auto insert_or_assign(key_type const& key, Mapped&& value) { return m_storage.insert_or_assign(key, std::forward<Mapped>(value)); }
  template <typename Mapped>
  auto insert_or_assign(key_type&& key, Mapped&& value) { return m_storage.insert_or_assign(std::move(key), std::forward<Mapped>(value)); }
  iterator erase(const_iterator position) { return m_storage.erase(position); }
  iterator erase(const_iterator first, const_iterator last) { return m_storage.erase(first, last); }
  size_type erase(key_type const& key) { return m_storage.erase(key); }
  auto equal_range(key_type const& key) { return m_storage.equal_range(key); }
  auto equal_range(key_type const& key) const { return m_storage.equal_range(key); }
  [[nodiscard]] std::expected<std::reference_wrapper<mapped_type>, StarException> add(key_type key, mapped_type value) {
    auto result = m_storage.try_emplace(std::move(key), std::move(value));
    if (!result.second)
      return std::unexpected(mapDiagnosticError("Entry with key '{}' already present.", "Map::add: key already present while rendering its diagnostic", result.first->first));
    return std::reference_wrapper<mapped_type>(result.first->second);
  }
  mapped_type& set(key_type key, mapped_type value) { return m_storage.insert_or_assign(std::move(key), std::move(value)).first->second; }
  template <typename OtherMap>
  bool merge(OtherMap const& other, bool overwrite = false) { return mapMerge(*this, other, overwrite); }
  bool operator==(HashMap const& other) const { return this == &other || mapsEqual(*this, other); }
  void reserve(size_type capacity) { m_storage.reserve(capacity); }

private:
  Storage m_storage;
};

template <typename Key, typename Value, typename Hash = hash<Key>, typename Equals = std::equal_to<>, typename Allocator = std::allocator<std::pair<Key const, Value>>>
class StableHashMap {
public:
  using Storage = std::unordered_map<Key, Value, Hash, Equals, Allocator>;
  using StarMapContainerTag = void;
  using key_type = Key;
  using mapped_type = Value;
  using value_type = typename Storage::value_type;
  using iterator = typename Storage::iterator;
  using const_iterator = typename Storage::const_iterator;
  using size_type = typename Storage::size_type;
  using difference_type = typename Storage::difference_type;
  using allocator_type = Allocator;
  using hasher = Hash;
  using key_equal = Equals;

  StableHashMap() = default;
  template <typename... Args>
    requires (sizeof...(Args) > 0 && std::constructible_from<Storage, Args...>)
  explicit StableHashMap(Args&&... args) : m_storage(std::forward<Args>(args)...) {}
  template <typename InputIt, typename... Args>
    requires std::constructible_from<Storage, InputIt, InputIt, Args...>
  StableHashMap(InputIt first, InputIt last, Args&&... args)
    : m_storage(first, last, std::forward<Args>(args)...) {}
  StableHashMap(std::initializer_list<value_type> entries) : m_storage(entries) {}
  StableHashMap(std::initializer_list<value_type> entries, size_type bucketCount, hasher const& hash = hasher(),
      key_equal const& equal = key_equal(), allocator_type const& allocator = allocator_type())
    : m_storage(entries, bucketCount, hash, equal, allocator) {}
  StableHashMap(std::initializer_list<value_type> entries, allocator_type const& allocator)
    : m_storage(entries, allocator) {}
  StableHashMap(std::initializer_list<value_type> entries, size_type bucketCount, allocator_type const& allocator)
    : m_storage(entries, bucketCount, allocator) {}
  StableHashMap(std::initializer_list<value_type> entries, size_type bucketCount, hasher const& hash, allocator_type const& allocator)
    : m_storage(entries, bucketCount, hash, allocator) {}
  StableHashMap(StableHashMap const& other, allocator_type const& allocator) : m_storage(other.m_storage, allocator) {}
  StableHashMap(StableHashMap&& other, allocator_type const& allocator) : m_storage(std::move(other.m_storage), allocator) {}
  StableHashMap& operator=(std::initializer_list<value_type> entries) { m_storage = entries; return *this; }

  template <typename Range>
  static StableHashMap from(Range const& range) { return StableHashMap(std::ranges::begin(range), std::ranges::end(range)); }

  iterator begin() { return m_storage.begin(); }
  const_iterator begin() const { return m_storage.begin(); }
  const_iterator cbegin() const { return m_storage.cbegin(); }
  iterator end() { return m_storage.end(); }
  const_iterator end() const { return m_storage.end(); }
  const_iterator cend() const { return m_storage.cend(); }
  bool empty() const { return m_storage.empty(); }
  size_type size() const { return m_storage.size(); }
  void clear() { m_storage.clear(); }
  void swap(StableHashMap& other) noexcept(std::is_nothrow_swappable_v<Storage>) { std::swap(m_storage, other.m_storage); }

  // Snapshots own their elements; views borrow this owner and follow storage invalidation.
  List<key_type> keys() const {
    List<key_type> result;
    result.reserve(size());
    std::ranges::copy(keyView(), std::back_inserter(result));
    return result;
  }
  List<mapped_type> values() const {
    List<mapped_type> result;
    result.reserve(size());
    std::ranges::copy(valueView(), std::back_inserter(result));
    return result;
  }
  List<std::pair<key_type, mapped_type>> pairs() const {
    List<std::pair<key_type, mapped_type>> result;
    result.reserve(size());
    std::ranges::copy(*this, std::back_inserter(result));
    return result;
  }
  auto keyView() const& { return std::views::keys(m_storage); }
  auto keyView() const&& = delete;
  auto valueView() & { return std::views::values(m_storage); }
  auto valueView() const& { return std::views::values(m_storage); }
  auto valueView() && = delete;
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

  template <typename LookupKey = key_type>
    requires requires(Storage& storage, LookupKey const& key) { storage.find(key); }
  [[nodiscard]] std::expected<std::reference_wrapper<mapped_type>, StarException> get(LookupKey const& key) {
    auto position = find(key);
    if (position == end())
      return std::unexpected(mapDiagnosticError("Key '{}' not found in Map::get()", "Map::get: key not found while rendering its diagnostic", key));
    return std::reference_wrapper<mapped_type>(position->second);
  }
  template <typename LookupKey = key_type>
    requires requires(Storage const& storage, LookupKey const& key) { storage.find(key); }
  [[nodiscard]] std::expected<std::reference_wrapper<mapped_type const>, StarException> get(LookupKey const& key) const {
    auto position = find(key);
    if (position == end())
      return std::unexpected(mapDiagnosticError("Key '{}' not found in Map::get()", "Map::get: key not found while rendering its diagnostic", key));
    return std::reference_wrapper<mapped_type const>(position->second);
  }
  [[nodiscard]] std::expected<std::reference_wrapper<mapped_type>, StarException> at(key_type const& key) { return get(key); }
  [[nodiscard]] std::expected<std::reference_wrapper<mapped_type const>, StarException> at(key_type const& key) const { return get(key); }
  template <typename LookupKey = key_type>
    requires requires(Storage const& storage, LookupKey const& key) { storage.find(key); }
  mapped_type value(LookupKey const& key, mapped_type fallback = mapped_type()) const {
    auto position = find(key);
    return position == end() ? std::move(fallback) : position->second;
  }
  template <typename LookupKey = key_type>
    requires requires(Storage const& storage, LookupKey const& key) { storage.find(key); }
  std::optional<mapped_type> maybe(LookupKey const& key) const {
    auto position = find(key);
    if (position == end()) return {};
    return std::optional<mapped_type>(std::in_place, position->second);
  }
  template <typename LookupKey = key_type>
    requires requires(Storage& storage, LookupKey const& key) { storage.find(key); }
  std::optional<std::reference_wrapper<mapped_type>> maybeRef(LookupKey const& key) {
    auto position = find(key);
    if (position == end()) return std::nullopt;
    return std::reference_wrapper<mapped_type>(position->second);
  }
  template <typename LookupKey = key_type>
    requires requires(Storage const& storage, LookupKey const& key) { storage.find(key); }
  std::optional<std::reference_wrapper<mapped_type const>> maybeRef(LookupKey const& key) const {
    auto position = find(key);
    if (position == end()) return std::nullopt;
    return std::reference_wrapper<mapped_type const>(position->second);
  }
  bool remove(key_type const& key) { return m_storage.erase(key) != 0; }
  bool removeValues(mapped_type const& value) {
    bool removed = false;
    for (auto position = begin(); position != end();) {
      if (position->second == value) {
        position = m_storage.erase(position);
        removed = true;
      } else {
        ++position;
      }
    }
    return removed;
  }
  [[nodiscard]] std::expected<mapped_type, StarException> take(key_type const& key) {
    if (auto value = maybeTake(key)) return std::move(*value);
    return std::unexpected(mapDiagnosticError("Key '{}' not found in Map::take()", "Map::take: key not found while rendering its diagnostic", key));
  }
  std::optional<mapped_type> maybeTake(key_type const& key) {
    auto position = find(key);
    if (position == end()) return {};
    mapped_type value = std::move(position->second);
    m_storage.erase(position);
    return std::optional<mapped_type>(std::in_place, std::move(value));
  }
  [[nodiscard]] std::expected<key_type, StarException> keyOf(mapped_type const& value) const {
    auto position = std::ranges::find(*this, value, &value_type::second);
    if (position != end()) return position->first;
    return std::unexpected(mapDiagnosticError("Value '{}' not found in Map::keyOf()", "Map::keyOf: value not found while rendering its diagnostic", value));
  }
  bool hasValue(mapped_type const& value) const {
    return std::ranges::find(*this, value, &value_type::second) != end();
  }

  mapped_type& operator[](key_type const& key) { return m_storage[key]; }
  mapped_type& operator[](key_type&& key) { return m_storage[std::move(key)]; }
  std::pair<iterator, bool> insert(value_type const& value) { return m_storage.insert(value); }
  std::pair<iterator, bool> insert(value_type&& value) { return m_storage.insert(std::move(value)); }
  template <typename Pair>
    requires std::constructible_from<value_type, Pair&&>
  std::pair<iterator, bool> insert(Pair&& pair) { return m_storage.insert(std::forward<Pair>(pair)); }
  iterator insert(const_iterator hint, value_type const& value) { return m_storage.insert(hint, value); }
  iterator insert(const_iterator hint, value_type&& value) { return m_storage.insert(hint, std::move(value)); }
  template <typename InputIt>
  void insert(InputIt first, InputIt last) { m_storage.insert(first, last); }
  void insert(std::initializer_list<value_type> values) { m_storage.insert(values); }
  std::pair<iterator, bool> insert(key_type key, mapped_type value) { return m_storage.try_emplace(std::move(key), std::move(value)); }
  template <typename... Args>
  auto emplace(Args&&... args) { return m_storage.emplace(std::forward<Args>(args)...); }
  template <typename... Args>
  iterator emplace_hint(const_iterator hint, Args&&... args) { return m_storage.emplace_hint(hint, std::forward<Args>(args)...); }
  template <typename... Args>
  auto try_emplace(key_type const& key, Args&&... args) { return m_storage.try_emplace(key, std::forward<Args>(args)...); }
  template <typename... Args>
  auto try_emplace(key_type&& key, Args&&... args) { return m_storage.try_emplace(std::move(key), std::forward<Args>(args)...); }
  template <typename Mapped>
  auto insert_or_assign(key_type const& key, Mapped&& value) { return m_storage.insert_or_assign(key, std::forward<Mapped>(value)); }
  template <typename Mapped>
  auto insert_or_assign(key_type&& key, Mapped&& value) { return m_storage.insert_or_assign(std::move(key), std::forward<Mapped>(value)); }
  iterator erase(const_iterator position) { return m_storage.erase(position); }
  iterator erase(const_iterator first, const_iterator last) { return m_storage.erase(first, last); }
  size_type erase(key_type const& key) { return m_storage.erase(key); }
  auto equal_range(key_type const& key) { return m_storage.equal_range(key); }
  auto equal_range(key_type const& key) const { return m_storage.equal_range(key); }
  [[nodiscard]] std::expected<std::reference_wrapper<mapped_type>, StarException> add(key_type key, mapped_type value) {
    auto result = m_storage.try_emplace(std::move(key), std::move(value));
    if (!result.second)
      return std::unexpected(mapDiagnosticError("Entry with key '{}' already present.", "Map::add: key already present while rendering its diagnostic", result.first->first));
    return std::reference_wrapper<mapped_type>(result.first->second);
  }
  mapped_type& set(key_type key, mapped_type value) { return m_storage.insert_or_assign(std::move(key), std::move(value)).first->second; }
  template <typename OtherMap>
  bool merge(OtherMap const& other, bool overwrite = false) { return mapMerge(*this, other, overwrite); }
  bool operator==(StableHashMap const& other) const { return this == &other || mapsEqual(*this, other); }
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
  Storage m_storage;
};

template <typename T>
decltype(auto) mapOutputValue(T const& value) {
  if constexpr (requires { outputOptional(value); })
    return outputOptional(value);
  else
    return (value);
}

template <typename T>
using MapElementOutputResult = decltype(std::declval<std::ostream&>() << mapOutputValue(std::declval<T const&>()));

template <typename MapType>
using MapOutputResult = std::conditional_t<
    std::same_as<MapElementOutputResult<typename MapType::key_type>, std::ostream&>
        && std::same_as<MapElementOutputResult<typename MapType::mapped_type>, std::ostream&>,
    std::ostream&, std::expected<std::reference_wrapper<std::ostream>, StarException>>;

template <MapContainer Container>
std::ostream& operator<<(std::ostream& os, Container const& map)
    requires std::same_as<MapOutputResult<Container>, std::ostream&>;

template <MapContainer Container>
[[nodiscard]] std::expected<std::reference_wrapper<std::ostream>, StarException> operator<<(std::ostream& os, Container const& map)
    requires std::same_as<MapOutputResult<Container>, std::expected<std::reference_wrapper<std::ostream>, StarException>>;

template <typename MapType>
MapOutputResult<MapType> printMap(std::ostream& os, MapType const& map) {
  os << "{ ";
  for (auto i = map.begin(); i != map.end(); ++i) {
    os << (i == map.begin() ? "\"" : ", \"");
    if constexpr (std::same_as<MapElementOutputResult<typename MapType::key_type>, std::ostream&>) {
      os << mapOutputValue(i->first);
    } else {
      auto written = os << mapOutputValue(i->first);
      if (!written) return std::unexpected(std::move(written.error()));
    }
    os << "\" : \"";
    if constexpr (std::same_as<MapElementOutputResult<typename MapType::mapped_type>, std::ostream&>) {
      os << mapOutputValue(i->second);
    } else {
      auto written = os << mapOutputValue(i->second);
      if (!written) return std::unexpected(std::move(written.error()));
    }
    os << "\"";
  }
  os << " }";
  if constexpr (std::same_as<MapOutputResult<MapType>, std::ostream&>)
    return os;
  else
    return std::ref(os);
}

template <MapContainer Container>
std::ostream& operator<<(std::ostream& os, Container const& map)
    requires std::same_as<MapOutputResult<Container>, std::ostream&> {
  return printMap(os, map);
}

template <MapContainer Container>
[[nodiscard]] std::expected<std::reference_wrapper<std::ostream>, StarException> operator<<(std::ostream& os, Container const& map)
    requires std::same_as<MapOutputResult<Container>, std::expected<std::reference_wrapper<std::ostream>, StarException>> {
  return printMap(os, map);
}

}

export module star.map;

export template <typename Key, typename Value, typename Compare, typename Allocator>
  requires std::same_as<decltype(std::declval<std::ostream&>() << std::declval<Star::Map<Key, Value, Compare, Allocator> const&>()), std::ostream&>
struct std::formatter<Star::Map<Key, Value, Compare, Allocator>> : Star::StreamFormatter {};

export template <typename Key, typename Value, typename Hash, typename Equals, typename Allocator>
  requires std::same_as<decltype(std::declval<std::ostream&>() << std::declval<Star::HashMap<Key, Value, Hash, Equals, Allocator> const&>()), std::ostream&>
struct std::formatter<Star::HashMap<Key, Value, Hash, Equals, Allocator>> : Star::StreamFormatter {};

export template <typename Key, typename Value, typename Hash, typename Equals, typename Allocator>
  requires std::same_as<decltype(std::declval<std::ostream&>() << std::declval<Star::StableHashMap<Key, Value, Hash, Equals, Allocator> const&>()), std::ostream&>
struct std::formatter<Star::StableHashMap<Key, Value, Hash, Equals, Allocator>> : Star::StreamFormatter {};

export namespace Star {
  using ::Star::MapExceptionTag;
  using ::Star::MapException;
  using ::Star::MapContainer;
  using ::Star::mapDiagnosticValue;
  using ::Star::mapDiagnosticError;
  using ::Star::Map;
  using ::Star::HashMap;
  using ::Star::StableHashMap;
  using ::Star::printMap;
  using ::Star::operator<<;
}
