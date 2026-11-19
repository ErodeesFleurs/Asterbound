module;

import std;
import star.string;
import star.string_view;
import star.algorithm;
import star.map;
import star.list;
import star.hash;
import star.format;

import star.output_proxy;
namespace Star {

// Links do not own their target. A slot is unbound only while a new pair is
// being inserted; every observable entry links to the other index's key.
template <typename T>
using BiMapLink = std::optional<std::reference_wrapper<T const>>;

template <typename Index, typename Key, typename Other>
concept bimap_index = std::same_as<typename Index::key_type, Key>
    && std::same_as<typename Index::mapped_type, BiMapLink<Other>>
    && std::is_nothrow_move_constructible_v<Index>
    && requires(Index& index, typename Index::const_iterator position) {
      { index.extract(position).key() } -> std::same_as<Key&>;
    };

template <typename Query, typename Index>
concept bimap_query = requires(Index const& index, Query const& key) {
  { index.find(key) } -> std::same_as<typename Index::const_iterator>;
};

template <typename It, typename Left, typename Right>
concept bimap_pair_iterator = std::input_iterator<It> && requires(It it) {
  requires std::constructible_from<Left, decltype(((*it).first))>;
  requires std::constructible_from<Right, decltype(((*it).second))>;
};

template <typename Collection, typename Left, typename Right>
concept bimap_pair_range = std::ranges::input_range<Collection const>
    && bimap_pair_iterator<std::ranges::iterator_t<Collection const>, Left, Right>;

// Both indices must own stable key nodes. Insertion, unrelated erasure and
// ordinary move construction must preserve their addresses (Map and
// StableHashMap do). A retained key keeps its address through overwrite.
template <typename LeftT, typename RightT,
    bimap_index<LeftT, RightT> LeftMapT = Map<LeftT, BiMapLink<RightT>>,
    bimap_index<RightT, LeftT> RightMapT = Map<RightT, BiMapLink<LeftT>>>
class BiMap {
public:
  using Left = LeftT;
  using Right = RightT;
  using LeftMap = LeftMapT;
  using RightMap = RightMapT;
  using value_type = std::pair<Left, Right>;

  struct BiMapIterator {
    using value_type = std::pair<Left, Right>;
    using difference_type = std::ptrdiff_t;
    using iterator_concept = std::forward_iterator_tag;

    typename LeftMap::const_iterator position;
    BiMapIterator& operator++() { ++position; return *this; }
    BiMapIterator operator++(int) { auto previous = *this; ++*this; return previous; }
    bool operator==(BiMapIterator const&) const = default;
    std::pair<Left const&, Right const&> operator*() const {
      return {position->first, position->second->get()};
    }
  };

  using iterator = BiMapIterator;
  using const_iterator = iterator;

  BiMap() = default;
  BiMap(BiMap const& other) : m_indices(other.m_indices) {
    for (auto& [left, right_link] : m_indices.left) {
      auto right = m_indices.right.find(right_link->get());
      right_link = std::cref(right->first);
      right->second = std::cref(left);
    }
  }

  BiMap(BiMap&&) noexcept = default;
  BiMap& operator=(BiMap&& other) noexcept {
    if (this != std::addressof(other)) {
      // Move *construct* both indices, never move-assign their containers:
      // unequal non-propagating allocators could otherwise relocate keys.
      std::destroy_at(std::addressof(m_indices));
      std::construct_at(std::addressof(m_indices), std::move(other.m_indices));
    }
    return *this;
  }
  BiMap& operator=(BiMap const& other) {
    if (this != std::addressof(other))
      *this = BiMap(other);
    return *this;
  }

  template <bimap_pair_iterator<Left, Right> It, std::sentinel_for<It> End>
  BiMap(It first, End last) {
    if constexpr (std::sized_sentinel_for<End, It>)
      reserve(static_cast<std::size_t>(last - first));
    for (; first != last; ++first) {
      auto&& pair = *first;
      insert(std::forward<decltype(pair)>(pair).first, std::forward<decltype(pair)>(pair).second);
    }
  }

  template <bimap_pair_range<Left, Right> Collection>
  static BiMap from(Collection const& values) {
    BiMap result;
    if constexpr (std::ranges::sized_range<Collection const>)
      result.reserve(std::ranges::size(values));
    for (auto&& pair : values)
      result.insert(std::forward<decltype(pair)>(pair).first, std::forward<decltype(pair)>(pair).second);
    return result;
  }

  [[nodiscard]] static std::expected<BiMap, StarException> create(std::initializer_list<value_type> values) {
    BiMap result;
    result.reserve(values.size());
    for (auto const& pair : values) {
      auto added = result.add(pair);
      if (!added)
        return std::unexpected(std::move(added.error()));
    }
    return result;
  }

  List<Left> left_values() const { return m_indices.left.keys(); }
  List<Right> right_values() const { return m_indices.right.keys(); }
  List<value_type> pairs() const {
    List<value_type> result;
    result.reserve(size());
    for (auto [left, right] : *this)
      result.emplace_back(left, right);
    return result;
  }

  template <bimap_query<LeftMap> Key = Left>
  bool contains_left(Key const& key) const { return m_indices.left.contains(key); }
  template <bimap_query<RightMap> Key = Right>
  bool contains_right(Key const& key) const { return m_indices.right.contains(key); }

  template <bimap_query<LeftMap> Key = Left>
  [[nodiscard]] auto get_right(Key const& key) const& {
    return m_indices.left.get(key).transform([](auto slot) { return *slot.get(); });
  }
  template <bimap_query<RightMap> Key = Right>
  [[nodiscard]] auto get_left(Key const& key) const& {
    return m_indices.right.get(key).transform([](auto slot) { return *slot.get(); });
  }
  template <typename Key = Left> void get_right(Key const&) const&& = delete;
  template <typename Key = Right> void get_left(Key const&) const&& = delete;

  template <bimap_query<LeftMap> Key = Left>
  BiMapLink<Right> find_right_ref(Key const& key) const& { return lookup(m_indices.left, key); }
  template <bimap_query<RightMap> Key = Right>
  BiMapLink<Left> find_left_ref(Key const& key) const& { return lookup(m_indices.right, key); }
  template <typename Key = Left> void find_right_ref(Key const&) const&& = delete;
  template <typename Key = Right> void find_left_ref(Key const&) const&& = delete;

  template <bimap_query<LeftMap> Key = Left>
  Right value_right(Key const& key, Right const& fallback = Right()) const {
    auto match = find_right_ref(key);
    return match ? match->get() : fallback;
  }
  template <bimap_query<RightMap> Key = Right>
  Left value_left(Key const& key, Left const& fallback = Left()) const {
    auto match = find_left_ref(key);
    return match ? match->get() : fallback;
  }
  template <bimap_query<LeftMap> Key = Left>
  std::optional<Right> maybe_right(Key const& key) const {
    return find_right_ref(key).transform([](auto right) { return right.get(); });
  }
  template <bimap_query<RightMap> Key = Right>
  std::optional<Left> maybe_left(Key const& key) const {
    return find_left_ref(key).transform([](auto left) { return left.get(); });
  }

  template <bimap_query<LeftMap> Key = Left>
  std::optional<Right> try_extract_right(Key const& key) { return extract_pair(m_indices.left, m_indices.right, key); }
  template <bimap_query<RightMap> Key = Right>
  std::optional<Left> try_extract_left(Key const& key) { return extract_pair(m_indices.right, m_indices.left, key); }

  template <bimap_query<LeftMap> Key = Left>
  [[nodiscard]] std::expected<Right, StarException> extract_right(Key const& key) {
    if (auto right = try_extract_right(key))
      return std::move(*right);
    return std::unexpected(map_diagnostic_error("No such key '{}' in BiMap::extract_right",
        "BiMap::extract_right: key not found while rendering its diagnostic", key));
  }
  template <bimap_query<RightMap> Key = Right>
  [[nodiscard]] std::expected<Left, StarException> extract_left(Key const& key) {
    if (auto left = try_extract_left(key))
      return std::move(*left);
    return std::unexpected(map_diagnostic_error("No such key '{}' in BiMap::extract_left",
        "BiMap::extract_left: key not found while rendering its diagnostic", key));
  }

  std::pair<iterator, bool> insert(value_type const& pair) & {
    return insert_position(pair.first, pair.second);
  }
  std::pair<iterator, bool> insert(value_type&& pair) & {
    return insert_position(std::move(pair.first), std::move(pair.second));
  }
  template <typename L = Left, typename R = Right>
    requires std::constructible_from<Left, L&&> && std::constructible_from<Right, R&&>
  bool insert(L&& left, R&& right) {
    return insert_impl<InsertUse::Flag>(as_key<Left>(std::forward<L>(left)),
        as_key<Right>(std::forward<R>(right))).status == InsertStatus::Inserted;
  }

  template <typename L = Left, typename R = Right>
    requires std::constructible_from<Left, L&&> && std::constructible_from<Right, R&&>
  [[nodiscard]] std::expected<void, StarException> add(L&& left, R&& right) {
    auto&& left_key = as_key<Left>(std::forward<L>(left));
    auto&& right_key = as_key<Right>(std::forward<R>(right));
    auto result = insert_impl<InsertUse::Checked>(std::forward<decltype(left_key)>(left_key),
        std::forward<decltype(right_key)>(right_key));
    if (result.status == InsertStatus::LeftConflict)
      return std::unexpected(map_diagnostic_error("BiMap already contains left side value '{}'",
          "BiMap::add: left side value already present while rendering its diagnostic", left_key));
    if (result.status == InsertStatus::RightConflict)
      return std::unexpected(map_diagnostic_error("BiMap already contains right side value '{}'",
          "BiMap::add: right side value already present while rendering its diagnostic", right_key));
    return {};
  }
  [[nodiscard]] auto add(value_type const& pair) { return add(pair.first, pair.second); }
  [[nodiscard]] auto add(value_type&& pair) { return add(std::move(pair.first), std::move(pair.second)); }

  template <typename L = Left, typename R = Right>
    requires std::constructible_from<Left, L&&> && std::constructible_from<Right, R&&>
  void overwrite(L&& left, R&& right) {
    // Insert before erasing: the inputs can themselves borrow indexed keys.
    // Keep references, not iterators, to keys across a possible hash rehash.
    auto [r, new_right] = m_indices.right.try_emplace(as_key<Right>(std::forward<R>(right)));
    auto rollback = make_scope_exit([&] { if (new_right) m_indices.right.erase(r); });
    auto [l, new_left] = m_indices.left.try_emplace(as_key<Left>(std::forward<L>(left)));
    rollback.release();
    if (!new_left && !new_right && std::addressof(l->second->get()) == std::addressof(r->first))
      return;
    if (!new_left)
      m_indices.right.erase(l->second->get());
    if (!new_right)
      m_indices.left.erase(r->second->get());
    l->second = std::cref(r->first);
    r->second = std::cref(l->first);
  }
  void overwrite(value_type const& pair) { overwrite(pair.first, pair.second); }
  void overwrite(value_type&& pair) { overwrite(std::move(pair.first), std::move(pair.second)); }

  template <bimap_query<LeftMap> Key = Left>
  bool erase_left(Key const& key) { return erase_pair(m_indices.left, m_indices.right, key); }
  template <bimap_query<RightMap> Key = Right>
  bool erase_right(Key const& key) { return erase_pair(m_indices.right, m_indices.left, key); }

  const_iterator begin() const& { return {m_indices.left.begin()}; }
  const_iterator end() const& { return {m_indices.left.end()}; }
  const_iterator begin() const&& = delete;
  const_iterator end() const&& = delete;
  std::size_t size() const { return m_indices.left.size(); }
  bool empty() const { return m_indices.left.empty(); }
  void clear() { m_indices.left.clear(); m_indices.right.clear(); }
  void reserve(std::size_t capacity) {
    if constexpr (requires { m_indices.left.reserve(capacity); })
      m_indices.left.reserve(capacity);
    if constexpr (requires { m_indices.right.reserve(capacity); })
      m_indices.right.reserve(capacity);
  }

  bool operator==(BiMap const& other) const {
    if (this == std::addressof(other)) return true;
    if (size() != other.size()) return false;
    for (auto [left, right] : *this) {
      auto match = other.find_right_ref(left);
      if (!match || match->get() != right)
        return false;
    }
    return true;
  }

private:
  template <typename Key, typename T>
    requires std::constructible_from<Key, T&&>
  static decltype(auto) as_key(T&& value) {
    if constexpr (std::same_as<Key, std::remove_cvref_t<T>>)
      return std::forward<T>(value);
    else
      return Key(std::forward<T>(value));
  }

  enum class InsertStatus { Inserted, LeftConflict, RightConflict };
  enum class InsertUse { Flag, Position, Checked };
  struct InsertResult {
    typename LeftMap::iterator position;
    InsertStatus status;
  };
  template <InsertUse Use, typename L, typename R>
    requires std::same_as<std::remove_cvref_t<L>, Left> && std::same_as<std::remove_cvref_t<R>, Right>
  InsertResult insert_impl(L&& left, R&& right) {
    // The iterator overload must return the existing left node on a conflict.
    // Probe it before allocating a tentative right node that would be discarded.
    if constexpr (Use == InsertUse::Position) {
      auto existing = m_indices.left.find(left);
      if (existing != m_indices.left.end())
        return {existing, InsertStatus::LeftConflict};
    }
    auto [r, new_right] = m_indices.right.try_emplace(std::forward<R>(right));
    if (!new_right) {
      if constexpr (Use == InsertUse::Checked) {
        auto l = m_indices.left.find(left);
        if (l != m_indices.left.end())
          return {l, InsertStatus::LeftConflict};
      }
      if constexpr (Use == InsertUse::Position)
        return {m_indices.left.find(r->second->get()), InsertStatus::RightConflict};
      return {m_indices.left.end(), InsertStatus::RightConflict};
    }
    auto rollback = make_scope_exit([&] { m_indices.right.erase(r); });
    auto [l, new_left] = m_indices.left.try_emplace(std::forward<L>(left), std::cref(r->first));
    if (!new_left)
      return {l, InsertStatus::LeftConflict};
    r->second = std::cref(l->first);
    rollback.release();
    return {l, InsertStatus::Inserted};
  }
  template <typename L, typename R>
    requires std::same_as<std::remove_cvref_t<L>, Left> && std::same_as<std::remove_cvref_t<R>, Right>
  std::pair<iterator, bool> insert_position(L&& left, R&& right) {
    auto result = insert_impl<InsertUse::Position>(std::forward<L>(left), std::forward<R>(right));
    return {iterator{result.position}, result.status == InsertStatus::Inserted};
  }

  template <typename Index, bimap_query<Index> Key>
  static auto lookup(Index const& index, Key const& key) -> typename Index::mapped_type {
    auto position = index.find(key);
    if (position == index.end()) return std::nullopt;
    return position->second;
  }
  template <typename Index, typename Other, bimap_query<Index> Key>
  static bool erase_pair(Index& index, Other& other, Key const& key) {
    auto position = index.find(key);
    if (position == index.end()) return false;
    other.erase(position->second->get());
    index.erase(position);
    return true;
  }
  template <typename Index, typename Other, bimap_query<Index> Key>
  static auto extract_pair(Index& index, Other& other, Key const& key) -> std::optional<typename Other::key_type> {
    auto position = index.find(key);
    if (position == index.end()) return std::nullopt;
    auto node = other.extract(other.find(position->second->get()));
    index.erase(position);
    return std::optional<typename Other::key_type>(std::in_place, std::move(node.key()));
  }

  struct Indices {
    LeftMap left;
    RightMap right;
  };
  Indices m_indices;
};

template <typename Left, typename Right, typename LeftHash = Star::hash<Left>, typename RightHash = Star::hash<Right>>
using BiHashMap = BiMap<Left, Right, StableHashMap<Left, BiMapLink<Right>, LeftHash>,
    StableHashMap<Right, BiMapLink<Left>, RightHash>>;

// Fixed ASCII enum spellings share the same Unicode-aware comparison and UTF-8
// error boundary as StringView. Storage and reverse indices are constant data.
constexpr int compare_enum_name_bytes(std::string_view a, std::string_view b) {
  auto lower = [](unsigned char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; };
  for (std::size_t i = 0; i < std::min(a.size(), b.size()); ++i) {
    auto left = lower(static_cast<unsigned char>(a[i]));
    auto right = lower(static_cast<unsigned char>(b[i]));
    if (left != right)
      return left < right ? -1 : 1;
  }
  return a.size() < b.size() ? -1 : a.size() > b.size() ? 1 : 0;
}

template <typename Key>
concept ordered_name_key = requires(Key const& left, Key const& right) {
  { left < right } -> std::convertible_to<bool>;
};

template <typename Table>
concept fixed_name_table = std::ranges::random_access_range<Table const> && requires(Table const& names) {
  requires ordered_name_key<typename std::remove_cvref_t<Table>::value_type::first_type>;
  { names.size() } -> std::convertible_to<std::size_t>;
  { names[0].second } -> std::convertible_to<std::string_view>;
};

template <ordered_name_key Enum, std::size_t N>
consteval auto sortedEnumNames(std::array<std::pair<Enum, std::string_view>, N> names) {
  std::sort(names.begin(), names.end(), [](auto const& a, auto const& b) { return a.first < b.first; });
  return names;
}

template <auto const& Names> requires fixed_name_table<std::remove_cvref_t<decltype(Names)>>
consteval bool validEnumNames() {
  for (std::size_t i = 0; i < Names.size(); ++i) {
    for (unsigned char c : Names[i].second)
      if (c >= 0x80)
        return false;
    if (i && !(Names[i - 1].first < Names[i].first))
      return false;
    for (std::size_t j = 0; j < i; ++j)
      if (compare_enum_name_bytes(Names[i].second, Names[j].second) == 0)
        return false;
  }
  return true;
}

template <auto const& Names> requires fixed_name_table<std::remove_cvref_t<decltype(Names)>>
[[nodiscard]] std::expected<std::string_view, StarException> enumName(
    typename std::remove_cvref_t<decltype(Names)>::value_type::first_type value) {
  static_assert(validEnumNames<Names>(), "Fixed enum names must be unique ASCII spellings in enum order");
  auto position = std::lower_bound(Names.begin(), Names.end(), value,
      [](auto const& pair, auto key) { return pair.first < key; });
  if (position == Names.end() || value < position->first)
    return std::unexpected(map_diagnostic_error("Key '{}' not found in Map::get()",
                                                "enumName: key not found while rendering its diagnostic", value));
  return position->second;
}

template <auto const& Names> requires fixed_name_table<std::remove_cvref_t<decltype(Names)>>
[[nodiscard]] auto maybeEnumValue(StringView name)
    -> std::expected<std::optional<typename std::remove_cvref_t<decltype(Names)>::value_type::first_type>, StarException> {
  static_assert(validEnumNames<Names>(), "Fixed enum names must be unique ASCII spellings in enum order");
  auto bytes = name.utf8();
  if (!std::ranges::all_of(bytes, [](unsigned char c) { return c < 0x80; })) {
    // String::to_lower only folds ASCII, so non-ASCII cannot match this table.
    // Still validate the whole input: a malformed suffix must remain an error.
    auto length = name.size();
    if (!length)
      return std::unexpected(std::move(length.error()));
    return std::nullopt;
  }
  static constexpr auto indices = [] {
    std::array<std::size_t, Names.size()> result{};
    for (std::size_t i = 0; i < result.size(); ++i)
      result[i] = i;
    std::sort(result.begin(), result.end(), [](auto a, auto b) {
      return compare_enum_name_bytes(Names[a].second, Names[b].second) < 0;
    });
    return result;
  }();
  auto position = std::ranges::lower_bound(indices, bytes,
      [](std::string_view a, std::string_view b) { return compare_enum_name_bytes(a, b) < 0; },
      [](std::size_t index) { return Names[index].second; });
  if (position == indices.end() || compare_enum_name_bytes(Names[*position].second, bytes) != 0)
    return std::nullopt;
  return Names[*position].first;
}

template <auto const& Names> requires fixed_name_table<std::remove_cvref_t<decltype(Names)>>
[[nodiscard]] auto enumValue(StringView name)
    -> std::expected<typename std::remove_cvref_t<decltype(Names)>::value_type::first_type, StarException> {
  auto lookup = maybeEnumValue<Names>(name);
  if (!lookup)
    return std::unexpected(std::move(lookup.error()));
  if (!*lookup)
    return std::unexpected(map_diagnostic_error("Key '{}' not found in Map::get()",
                                                "enumValue: key not found while rendering its diagnostic", name));
  return **lookup;
}

template <typename Query>
concept enum_name_query = std::same_as<Query, CaseInsensitiveStringView> || std::same_as<Query, CaseInsensitiveString>
    || std::same_as<Query, String> || std::same_as<Query, StringView>
    || std::convertible_to<Query const&, std::string_view>;

// Case insensitive Enum <-> String map. Only validated, hash-stable keys enter
// the reverse index; public text lookup validates before touching the index.
template <ordered_name_key EnumType>
class EnumMap {
  using Storage = BiMap<EnumType, CaseInsensitiveString,
      Map<EnumType, BiMapLink<CaseInsensitiveString>>,
      StableHashMap<CaseInsensitiveString, BiMapLink<EnumType>, CaseInsensitiveStringHash, CaseInsensitiveStringCompare>>;

public:
  using Left = EnumType;
  using Right = String;
  using value_type = std::pair<Left, Right>;

  struct Iterator {
    using difference_type = std::ptrdiff_t;
    using value_type = std::pair<Left, Right>;
    using iterator_concept = std::forward_iterator_tag;

    typename Storage::const_iterator inner;
    Iterator& operator++() { ++inner; return *this; }
    Iterator operator++(int) { auto previous = *this; ++*this; return previous; }
    bool operator==(Iterator const&) const = default;
    std::pair<Left const&, Right const&> operator*() const {
      auto pair = *inner;
      return {pair.first, pair.second.string()};
    }
  };
  using iterator = Iterator;
  using const_iterator = Iterator;

  [[nodiscard]] static std::expected<EnumMap, StarException> create(std::initializer_list<value_type> values) {
    EnumMap result;
    result.m_storage.reserve(values.size());
    for (auto const& value : values) {
      auto added = result.add(value);
      if (!added)
        return std::unexpected(std::move(added.error()));
    }
    return result;
  }

  template <typename Collection>
    requires bimap_pair_range<Collection, Left, Right>
  [[nodiscard]] static std::expected<EnumMap, StarException> from(Collection const& values) {
    EnumMap result;
    if constexpr (std::ranges::sized_range<Collection const>)
      result.m_storage.reserve(std::ranges::size(values));
    for (auto&& value : values) {
      auto inserted = result.insert(std::forward<decltype(value)>(value).first,
                                    Right(std::forward<decltype(value)>(value).second));
      if (!inserted)
        return std::unexpected(std::move(inserted.error()));
    }
    return result;
  }

  List<Left> left_values() const { return m_storage.left_values(); }
  List<Right> right_values() const {
    List<Right> result;
    result.reserve(size());
    for (auto const& pair : *this)
      result.push_back(pair.second);
    return result;
  }
  List<value_type> pairs() const {
    List<value_type> result;
    result.reserve(size());
    for (auto const& pair : *this)
      result.emplace_back(pair.first, pair.second);
    return result;
  }

  bool contains_left(Left const& left) const { return m_storage.contains_left(left); }
  template <enum_name_query Query = Right>
  [[nodiscard]] std::expected<bool, StarException> contains_right(Query const& right) const {
    return validated_name(right).transform([this](auto const& key) { return m_storage.contains_right(key); });
  }
  [[nodiscard]] std::expected<std::reference_wrapper<Right const>, StarException> get_right(Left const& left) const& {
    return m_storage.get_right(left).transform(
      [](auto const& stored) { return std::cref(stored.get().string()); });
  }
  void get_right(Left const&) const&& = delete;

  template <enum_name_query Query = Right>
  [[nodiscard]] std::expected<std::reference_wrapper<Left const>, StarException> get_left(Query const& right) const& {
    return validated_name(right).and_then([this](auto const& key) { return m_storage.get_left(key); });
  }
  template <enum_name_query Query = Right> void get_left(Query const&) const&& = delete;
  Right value_right(Left const& left, Right const& fallback = Right()) const {
    if (auto value = find_right_ref(left))
      return value->get();
    return fallback;
  }
  template <enum_name_query Query = Right>
  [[nodiscard]] std::expected<Left, StarException> value_left(Query const& right, Left const& fallback = Left()) const {
    return validated_name(right).transform([&](auto const& key) { return m_storage.value_left(key, fallback); });
  }
  std::optional<Right> maybe_right(Left const& left) const {
    if (auto value = find_right_ref(left))
      return value->get();
    return {};
  }
  template <enum_name_query Query = Right>
  [[nodiscard]] std::expected<std::optional<Left>, StarException> maybe_left(Query const& right) const {
    return validated_name(right).transform([this](auto const& key) { return m_storage.maybe_left(key); });
  }
  [[nodiscard]] std::expected<Right, StarException> extract_right(Left const& left) {
    auto value = m_storage.extract_right(left);
    if (!value) return std::unexpected(std::move(value.error()));
    return std::move(*value).take_string();
  }
  template <enum_name_query Query = Right>
  [[nodiscard]] std::expected<Left, StarException> extract_left(Query const& right) {
    return validated_name(right).and_then([this](auto const& key) { return m_storage.extract_left(key); });
  }
  std::optional<Right> try_extract_right(Left const& left) {
    if (auto value = m_storage.try_extract_right(left))
      return std::move(*value).take_string();
    return {};
  }
  template <enum_name_query Query = Right>
  [[nodiscard]] std::expected<std::optional<Left>, StarException> try_extract_left(Query const& right) {
    return validated_name(right).transform([this](auto const& key) { return m_storage.try_extract_left(key); });
  }
  std::optional<std::reference_wrapper<Right const>> find_right_ref(Left const& left) const& {
    auto value = m_storage.find_right_ref(left);
    if (value) return std::cref(value->get().string());
    return std::nullopt;
  }
  void find_right_ref(Left const&) const&& = delete;

  template <enum_name_query Query = Right>
  [[nodiscard]] std::expected<std::optional<std::reference_wrapper<Left const>>, StarException>
  find_left_ref(Query const& right) const& {
    return validated_name(right).transform([this](auto const& key) { return m_storage.find_left_ref(key); });
  }
  template <enum_name_query Query = Right> void find_left_ref(Query const&) const&& = delete;

  [[nodiscard]] std::expected<std::pair<iterator, bool>, StarException> insert(value_type value) & {
    auto key = CaseInsensitiveString::create(std::move(value.second));
    if (!key) return std::unexpected(std::move(key.error()));
    auto result = m_storage.insert(typename Storage::value_type(std::move(value.first), std::move(*key)));
    return std::pair<iterator, bool>{iterator{result.first}, result.second};
  }
  template <typename Key = Left>
    requires std::constructible_from<Left, Key&&>
  [[nodiscard]] std::expected<bool, StarException> insert(Key&& left, Right right) {
    auto key = CaseInsensitiveString::create(std::move(right));
    if (!key) return std::unexpected(std::move(key.error()));
    return m_storage.insert(std::forward<Key>(left), std::move(*key));
  }
  template <typename Key = Left>
    requires std::constructible_from<Left, Key&&>
  [[nodiscard]] std::expected<void, StarException> add(Key&& left, Right right) {
    auto key = CaseInsensitiveString::create(std::move(right));
    if (!key) return std::unexpected(std::move(key.error()));
    return m_storage.add(std::forward<Key>(left), std::move(*key));
  }
  [[nodiscard]] std::expected<void, StarException> add(value_type const& value) {
    return add(value.first, value.second);
  }
  [[nodiscard]] std::expected<void, StarException> add(value_type&& value) {
    return add(std::move(value.first), std::move(value.second));
  }
  template <typename Key = Left>
    requires std::constructible_from<Left, Key&&>
  [[nodiscard]] std::expected<void, StarException> overwrite(Key&& left, Right right) {
    auto key = CaseInsensitiveString::create(std::move(right));
    if (!key) return std::unexpected(std::move(key.error()));
    m_storage.overwrite(std::forward<Key>(left), std::move(*key));
    return {};
  }
  [[nodiscard]] std::expected<void, StarException> overwrite(value_type const& value) {
    return overwrite(value.first, value.second);
  }
  [[nodiscard]] std::expected<void, StarException> overwrite(value_type&& value) {
    return overwrite(std::move(value.first), std::move(value.second));
  }
  bool erase_left(Left const& left) { return m_storage.erase_left(left); }
  template <enum_name_query Query = Right>
  [[nodiscard]] std::expected<bool, StarException> erase_right(Query const& right) {
    return validated_name(right).transform([this](auto const& key) { return m_storage.erase_right(key); });
  }
  iterator begin() const& { return {m_storage.begin()}; }
  iterator begin() const&& = delete;
  iterator end() const& { return {m_storage.end()}; }
  iterator end() const&& = delete;
  std::size_t size() const { return m_storage.size(); }
  bool empty() const { return m_storage.empty(); }
  void clear() { m_storage.clear(); }
  bool operator==(EnumMap const& rhs) const { return m_storage == rhs.m_storage; }

private:
  template <enum_name_query Query>
  static std::expected<CaseInsensitiveStringView, StarException> validated_name(Query const& name) {
    if constexpr (std::same_as<Query, CaseInsensitiveStringView>)
      return name;
    else if constexpr (std::same_as<Query, CaseInsensitiveString>)
      return name.view();
    else if constexpr (std::same_as<Query, String> || std::same_as<Query, StringView>)
      return CaseInsensitiveStringView::create(std::string_view(name.utf8()));
    else
      return CaseInsensitiveStringView::create(std::string_view(name));
  }

  Storage m_storage;
};


}

export module star.bi_map;

export namespace Star {
  using ::Star::BiMap;
  using ::Star::BiMapLink;
  using ::Star::bimap_index;
  using ::Star::bimap_pair_iterator;
  using ::Star::bimap_pair_range;
  using ::Star::BiHashMap;
  using ::Star::EnumMap;
  using ::Star::sortedEnumNames;
  using ::Star::validEnumNames;
  using ::Star::enumName;
  using ::Star::maybeEnumValue;
  using ::Star::enumValue;
}
