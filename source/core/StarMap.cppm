module;
import star.text_formatter;

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
concept map_container = requires { typename std::remove_cvref_t<T>::StarMapContainerTag; }
    && std::same_as<typename std::remove_cvref_t<T>::StarMapContainerTag, void>;

template <typename Storage, typename Key>
concept map_lookup = requires(Storage& storage, Key const& key) { storage.find(key); };

template <typename T> [[nodiscard]] std::expected<std::string, StarException> map_diagnostic_value(T const& value) {
  if constexpr (std::is_enum_v<T>) {
    return Star::format("{}", std::to_underlying(value));
  } else {
    TextBuffer output;
    using PrintResult = decltype(append_text(output, output_any(value)));
    if constexpr (std::same_as<PrintResult, std::expected<void, StarException>>) {
      auto printed = append_text(output, output_any(value));
      if (!printed)
        return std::unexpected(std::move(printed.error()));
    } else {
      append_text(output, output_any(value));
    }
    return std::move(output).str();
  }
}

template <typename Error = MapException, typename T>
[[nodiscard]] StarException map_diagnostic_error(std::format_string<std::string> message, std::string_view context,
                                                 T const& value) {
  auto rendered = map_diagnostic_value(value);
  if (!rendered)
    return std::move(rendered.error()).with_context(std::string(context));
  return Error(Star::format(message, std::move(*rendered)));
}

// Optional storage vocabulary is void when it does not apply to that storage kind.
// Operations themselves are constrained below rather than supplied by adapter bases.
template <typename Storage> struct MapStorageTypes {
  using key_compare = void;
  using value_compare = void;
  using hasher = typename Storage::hasher;
  using key_equal = typename Storage::key_equal;
};

template <typename Storage>
  requires requires {
    typename Storage::key_compare;
    typename Storage::value_compare;
  }
struct MapStorageTypes<Storage> {
  using key_compare = typename Storage::key_compare;
  using value_compare = typename Storage::value_compare;
  using hasher = void;
  using key_equal = void;
};

template <typename StorageType> class BasicMap {
public:
  using Storage = StorageType;
  using StarMapContainerTag = void;
  using key_type = typename Storage::key_type;
  using mapped_type = typename Storage::mapped_type;
  using value_type = typename Storage::value_type;
  using iterator = typename Storage::iterator;
  using const_iterator = typename Storage::const_iterator;
  using size_type = typename Storage::size_type;
  using difference_type = typename Storage::difference_type;
  using allocator_type = typename Storage::allocator_type;
  using key_compare = typename MapStorageTypes<Storage>::key_compare;
  using value_compare = typename MapStorageTypes<Storage>::value_compare;
  using hasher = typename MapStorageTypes<Storage>::hasher;
  using key_equal = typename MapStorageTypes<Storage>::key_equal;

  BasicMap() = default;
  template <typename... Args>
    requires(sizeof...(Args) > 0 && std::constructible_from<Storage, Args...>)
  explicit BasicMap(Args&&... args) : m_storage(std::forward<Args>(args)...) {}
  template <typename InputIt, typename... Args>
    requires std::constructible_from<Storage, InputIt, InputIt, Args...>
  BasicMap(InputIt first, InputIt last, Args&&... args) : m_storage(first, last, std::forward<Args>(args)...) {}
  BasicMap(std::initializer_list<value_type> entries) : m_storage(entries) {}
  template <typename... Args>
    requires(sizeof...(Args) > 0 && std::constructible_from<Storage, std::initializer_list<value_type>, Args...>)
  BasicMap(std::initializer_list<value_type> entries, Args&&... args)
      : m_storage(entries, std::forward<Args>(args)...) {}
  BasicMap(std::initializer_list<value_type> entries, allocator_type const& allocator)
    requires(!std::constructible_from<Storage, std::initializer_list<value_type>, allocator_type const&>
             && std::constructible_from<Storage, std::initializer_list<value_type>, size_type, allocator_type const&>)
      : m_storage(entries, size_type(0), allocator) {}
  BasicMap(BasicMap const& other, allocator_type const& allocator) : m_storage(other.m_storage, allocator) {}
  BasicMap(BasicMap&& other, allocator_type const& allocator) : m_storage(std::move(other.m_storage), allocator) {}
  BasicMap& operator=(std::initializer_list<value_type> entries) {
    m_storage = entries;
    return *this;
  }

  template <typename Range> static BasicMap from(Range const& range) {
    return BasicMap(std::ranges::begin(range), std::ranges::end(range));
  }

  iterator begin() { return m_storage.begin(); }
  const_iterator begin() const { return m_storage.begin(); }
  const_iterator cbegin() const { return m_storage.cbegin(); }
  iterator end() { return m_storage.end(); }
  const_iterator end() const { return m_storage.end(); }
  const_iterator cend() const { return m_storage.cend(); }
  bool empty() const { return m_storage.empty(); }
  size_type size() const { return m_storage.size(); }
  void clear() { m_storage.clear(); }
  void swap(BasicMap& other) noexcept(std::is_nothrow_swappable_v<Storage>) { std::swap(m_storage, other.m_storage); }

  // Snapshots own their elements; views borrow this owner and follow storage invalidation.
  List<key_type> keys() const {
    List<key_type> result;
    result.reserve(size());
    std::ranges::copy(key_view(), std::back_inserter(result));
    return result;
  }
  List<mapped_type> values() const {
    List<mapped_type> result;
    result.reserve(size());
    std::ranges::copy(value_view(), std::back_inserter(result));
    return result;
  }
  List<std::pair<key_type, mapped_type>> pairs() const {
    List<std::pair<key_type, mapped_type>> result;
    result.reserve(size());
    std::ranges::copy(*this, std::back_inserter(result));
    return result;
  }
  auto key_view() const& { return std::views::keys(m_storage); }
  auto key_view() const&& = delete;
  auto value_view() & { return std::views::values(m_storage); }
  auto value_view() const& { return std::views::values(m_storage); }
  auto value_view() && = delete;
  auto value_view() const&& = delete;

  template <typename LookupKey = key_type>
    requires map_lookup<Storage, LookupKey>
  iterator find(LookupKey const& key) & {
    return m_storage.find(key);
  }
  template <typename LookupKey = key_type>
    requires map_lookup<Storage const, LookupKey>
  const_iterator find(LookupKey const& key) const& {
    return m_storage.find(key);
  }
  template <typename LookupKey = key_type> iterator find(LookupKey const&) && = delete;
  template <typename LookupKey = key_type> const_iterator find(LookupKey const&) const&& = delete;
  template <typename LookupKey = key_type>
    requires map_lookup<Storage const, LookupKey>
  bool contains(LookupKey const& key) const {
    return find(key) != end();
  }
  template <typename LookupKey = key_type>
    requires map_lookup<Storage const, LookupKey>
  size_type count(LookupKey const& key) const {
    return contains(key) ? 1 : 0;
  }

  template <typename LookupKey = key_type>
    requires map_lookup<Storage, LookupKey>
  [[nodiscard]] auto get(LookupKey const& key) & {
    return get_impl(*this, key);
  }
  template <typename LookupKey = key_type>
    requires map_lookup<Storage const, LookupKey>
  [[nodiscard]] auto get(LookupKey const& key) const& {
    return get_impl(*this, key);
  }
  template <typename LookupKey = key_type> void get(LookupKey const&) && = delete;
  template <typename LookupKey = key_type> void get(LookupKey const&) const&& = delete;
  [[nodiscard]] auto at(key_type const& key) & { return get(key); }
  [[nodiscard]] auto at(key_type const& key) const& { return get(key); }
  void at(key_type const&) && = delete;
  void at(key_type const&) const&& = delete;
  template <typename LookupKey = key_type>
    requires map_lookup<Storage const, LookupKey>
  mapped_type value(LookupKey const& key, mapped_type fallback = mapped_type()) const {
    auto position = find(key);
    return position == end() ? std::move(fallback) : position->second;
  }
  template <typename LookupKey = key_type>
    requires map_lookup<Storage const, LookupKey>
  std::optional<mapped_type> maybe(LookupKey const& key) const {
    auto position = find(key);
    if (position == end()) return {};
    return std::optional<mapped_type>(std::in_place, position->second);
  }
  // Borrow the mapped slot itself, including when mapped_type is a reference_wrapper.
  template <typename LookupKey = key_type>
    requires map_lookup<Storage, LookupKey>
  auto find_ref(LookupKey const& key) & {
    return find_ref_impl(*this, key);
  }
  template <typename LookupKey = key_type>
    requires map_lookup<Storage const, LookupKey>
  auto find_ref(LookupKey const& key) const& {
    return find_ref_impl(*this, key);
  }
  template <typename LookupKey = key_type> void find_ref(LookupKey const&) && = delete;
  template <typename LookupKey = key_type> void find_ref(LookupKey const&) const&& = delete;
  bool remove(key_type const& key) { return m_storage.erase(key) != 0; }
  bool erase_values(mapped_type const& value) {
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
  [[nodiscard]] std::expected<mapped_type, StarException> extract_value(key_type const& key) {
    auto position = find(key);
    if (position == end())
      return std::unexpected(map_diagnostic_error("Key '{}' not found in Map::extract_value()",
                                                  "Map::extract_value: key not found while rendering its diagnostic",
                                                  key));
    std::expected<mapped_type, StarException> value(std::in_place, std::move(position->second));
    m_storage.erase(position);
    return value;
  }
  std::optional<mapped_type> try_extract_value(key_type const& key) {
    auto position = find(key);
    if (position == end()) return {};
    std::optional<mapped_type> value(std::in_place, std::move(position->second));
    m_storage.erase(position);
    return value;
  }
  [[nodiscard]] std::expected<key_type, StarException> key_of(mapped_type const& value) const {
    auto position = std::find_if(begin(), end(), [&](auto const& entry) { return entry.second == value; });
    if (position != end()) return position->first;
    return std::unexpected(map_diagnostic_error("Value '{}' not found in Map::key_of()",
                                                "Map::key_of: value not found while rendering its diagnostic", value));
  }
  bool contains_value(mapped_type const& value) const {
    return std::find_if(begin(), end(), [&](auto const& entry) { return entry.second == value; }) != end();
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
      return std::unexpected(map_diagnostic_error("Entry with key '{}' already present.",
                                                  "Map::add: key already present while rendering its diagnostic",
                                                  result.first->first));
    return std::reference_wrapper<mapped_type>(result.first->second);
  }
  mapped_type& set(key_type key, mapped_type value) { return m_storage.insert_or_assign(std::move(key), std::move(value)).first->second; }
  template <typename OtherMap> bool merge(OtherMap const& other, bool overwrite = false) {
    return map_merge(*this, other, overwrite);
  }
  bool operator==(BasicMap const& other) const { return this == &other || maps_equal(*this, other); }

  auto lower_bound(key_type const& key)
    requires requires(Storage& storage) { storage.lower_bound(key); }
  {
    return m_storage.lower_bound(key);
  }
  auto lower_bound(key_type const& key) const
    requires requires(Storage const& storage) { storage.lower_bound(key); }
  {
    return m_storage.lower_bound(key);
  }
  auto upper_bound(key_type const& key)
    requires requires(Storage& storage) { storage.upper_bound(key); }
  {
    return m_storage.upper_bound(key);
  }
  auto upper_bound(key_type const& key) const
    requires requires(Storage const& storage) { storage.upper_bound(key); }
  {
    return m_storage.upper_bound(key);
  }
  auto key_comp() const
    requires requires(Storage const& storage) { storage.key_comp(); }
  {
    return m_storage.key_comp();
  }
  auto value_comp() const
    requires requires(Storage const& storage) { storage.value_comp(); }
  {
    return m_storage.value_comp();
  }
  auto rbegin()
    requires requires(Storage& storage) { storage.rbegin(); }
  {
    return m_storage.rbegin();
  }
  auto rbegin() const
    requires requires(Storage const& storage) { storage.rbegin(); }
  {
    return m_storage.rbegin();
  }
  auto rend()
    requires requires(Storage& storage) { storage.rend(); }
  {
    return m_storage.rend();
  }
  auto rend() const
    requires requires(Storage const& storage) { storage.rend(); }
  {
    return m_storage.rend();
  }
  void reserve(size_type capacity)
    requires requires(Storage& storage) { storage.reserve(capacity); }
  {
    m_storage.reserve(capacity);
  }
  void rehash(size_type buckets)
    requires requires(Storage& storage) { storage.rehash(buckets); }
  {
    m_storage.rehash(buckets);
  }
  auto hash_function() const
    requires requires(Storage const& storage) { storage.hash_function(); }
  {
    return m_storage.hash_function();
  }
  auto key_eq() const
    requires requires(Storage const& storage) { storage.key_eq(); }
  {
    return m_storage.key_eq();
  }
  auto get_allocator() const
    requires requires(Storage const& storage) { storage.get_allocator(); }
  {
    return m_storage.get_allocator();
  }
  auto max_size() const
    requires requires(Storage const& storage) { storage.max_size(); }
  {
    return m_storage.max_size();
  }
  auto bucket_count() const
    requires requires(Storage const& storage) { storage.bucket_count(); }
  {
    return m_storage.bucket_count();
  }
  auto load_factor() const
    requires requires(Storage const& storage) { storage.load_factor(); }
  {
    return m_storage.load_factor();
  }
  auto max_load_factor() const
    requires requires(Storage const& storage) { storage.max_load_factor(); }
  {
    return m_storage.max_load_factor();
  }
  void max_load_factor(float factor)
    requires requires(Storage& storage) { storage.max_load_factor(factor); }
  {
    m_storage.max_load_factor(factor);
  }

private:
  template <typename Self> using slot_type = std::conditional_t<std::is_const_v<Self>, mapped_type const, mapped_type>;

  template <typename Self, typename LookupKey>
  static auto get_impl(Self& self, LookupKey const& key)
    -> std::expected<std::reference_wrapper<slot_type<Self>>, StarException> {
    auto position = self.m_storage.find(key);
    if (position == self.m_storage.end())
      return std::unexpected(map_diagnostic_error("Key '{}' not found in Map::get()",
                                                  "Map::get: key not found while rendering its diagnostic", key));
    return std::reference_wrapper<slot_type<Self>>(position->second);
  }

  template <typename Self, typename LookupKey>
  static auto find_ref_impl(Self& self, LookupKey const& key)
    -> std::optional<std::reference_wrapper<slot_type<Self>>> {
    auto position = self.m_storage.find(key);
    if (position == self.m_storage.end())
      return std::nullopt;
    return std::reference_wrapper<slot_type<Self>>(position->second);
  }

  Storage m_storage;
};

template <typename Key, typename Value, typename Compare = std::less<Key>,
          typename Allocator = std::allocator<std::pair<Key const, Value>>>
using Map = BasicMap<std::map<Key, Value, Compare, Allocator>>;

template <typename Key, typename Value, typename Hash = hash<Key>, typename Equals = std::equal_to<>,
          typename Allocator = std::allocator<std::pair<Key const, Value>>>
using HashMap = BasicMap<FlatHashMap<Key, Value, Hash, Equals, Allocator>>;

template <typename Key, typename Value, typename Hash = hash<Key>, typename Equals = std::equal_to<>,
          typename Allocator = std::allocator<std::pair<Key const, Value>>>
using StableHashMap = BasicMap<std::unordered_map<Key, Value, Hash, Equals, Allocator>>;

template <typename T>
using map_element_output_result = optional_diagnostic_result<T>;

template <typename T>
concept map_element_text_writable =
    std::same_as<map_element_output_result<T>, void>
    || std::same_as<map_element_output_result<T>, std::expected<void, StarException>>;

template <typename MapType>
using map_output_result = std::conditional_t<
    std::same_as<map_element_output_result<typename MapType::key_type>, void>
        && std::same_as<map_element_output_result<typename MapType::mapped_type>, void>,
    void, std::expected<void, StarException>>;

template <typename MapType>
  requires map_element_text_writable<typename MapType::key_type>
      && map_element_text_writable<typename MapType::mapped_type>
map_output_result<MapType> print_map(TextBuffer& output, MapType const& map) {
  output.append("{ ");
  for (auto i = map.begin(); i != map.end(); ++i) {
    output.append(i == map.begin() ? "\"" : ", \"");
    if constexpr (std::same_as<map_element_output_result<typename MapType::key_type>, void>) {
      append_text(output, optional_diagnostic_value(i->first));
    } else {
      auto written = append_text(output, optional_diagnostic_value(i->first));
      if (!written)
        return std::unexpected(std::move(written.error()));
    }
    output.append("\" : \"");
    if constexpr (std::same_as<map_element_output_result<typename MapType::mapped_type>, void>) {
      append_text(output, optional_diagnostic_value(i->second));
    } else {
      auto written = append_text(output, optional_diagnostic_value(i->second));
      if (!written)
        return std::unexpected(std::move(written.error()));
    }
    output.push_back('"');
  }
  output.append(" }");
  if constexpr (!std::same_as<map_output_result<MapType>, void>)
    return {};
}

template <map_container Container>
  requires map_element_text_writable<typename Container::key_type>
      && map_element_text_writable<typename Container::mapped_type>
map_output_result<Container> write_text(TextBuffer& output, Container const& map) {
  return print_map(output, map);
}

}

export module star.map;

// Specialize on the ordinary wrapper, not aliases with non-deduced parameters.
export template <typename Storage>
  requires Star::nonfallible_text_writer<Star::BasicMap<Storage>>
struct std::formatter<Star::BasicMap<Storage>> : Star::TextFormatter {};

export namespace Star {
using ::Star::BasicMap;
using ::Star::HashMap;
using ::Star::Map;
using ::Star::map_diagnostic_error;
using ::Star::map_diagnostic_value;
using ::Star::map_container;
using ::Star::MapException;
using ::Star::MapExceptionTag;
using ::Star::print_map;
using ::Star::StableHashMap;
using ::Star::write_text;
}
