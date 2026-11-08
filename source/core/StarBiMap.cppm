module;

import std;
import star.string;
import star.string_view;
import star.map;
import star.list;
import star.hash;
import star.format;

import star.output_proxy;
namespace Star {

// Bi-directional map of unique sets of elements with quick map access from
// either the left or right element to the other side.  Every left side value
// must be unique from every other left side value and the same for the right
// side.
// Both index storage types must preserve key addresses across insertion and
// unrelated erasure. Cross-index links and public reference results borrow
// those nodes; overwriting a retained side preserves that side's address.
template <typename LeftT,
    typename RightT,
    typename LeftMapT = Map<LeftT, RightT const*>,
    typename RightMapT = Map<RightT, LeftT const*>>
class BiMap {
public:
  using Left = LeftT;
  using Right = RightT;
  using LeftMap = LeftMapT;
  using RightMap = RightMapT;

  using value_type = std::pair<Left, Right>;

  struct BiMapIterator {
    BiMapIterator& operator++();
    BiMapIterator operator++(int);

    bool operator==(BiMapIterator const& rhs) const;
    bool operator!=(BiMapIterator const& rhs) const;

    std::pair<Left const&, Right const&> operator*() const;

    typename LeftMap::const_iterator iterator;
  };

  using iterator = BiMapIterator;
  using const_iterator = iterator;

  template <typename Collection>
  static BiMap from(Collection const& c);

  BiMap();
  BiMap(BiMap const& map);

  template <typename InputIterator>
  BiMap(InputIterator beg, InputIterator end);

  [[nodiscard]] static std::expected<BiMap, StarException> create(std::initializer_list<value_type> list);

  List<Left> left_values() const;
  List<Right> right_values() const;
  List<value_type> pairs() const;

  bool contains_left(Left const& left) const;
  bool contains_right(Right const& right) const;

  [[nodiscard]] std::expected<std::reference_wrapper<Right const>, StarException> get_right(Left const& left) const;
  [[nodiscard]] std::expected<std::reference_wrapper<Left const>, StarException> get_left(Right const& right) const;

  Right value_right(Left const& left, Right const& def = Right()) const;
  Left value_left(Right const& right, Left const& def = Left()) const;

  std::optional<Right> maybe_right(Left const& left) const;

  std::optional<Left> maybe_left(Right const& right) const;

  [[nodiscard]] std::expected<Right, StarException> extract_right(Left const& left);
  [[nodiscard]] std::expected<Left, StarException> extract_left(Right const& right);

  std::optional<Right> try_extract_right(Left const& left);
  std::optional<Left> try_extract_left(Right const& right);

  std::optional<std::reference_wrapper<Right const>> find_right_ref(Left const& left) const;
  std::optional<std::reference_wrapper<Left const>> find_left_ref(Right const& right) const;

  BiMap& operator=(BiMap const& map);

  std::pair<iterator, bool> insert(value_type const& val);

  // Returns true if value was inserted, false if either the left or right side
  // already existed.
  bool insert(Left const& left, Right const& right);

  // Returns an error if the pair cannot be inserted.
  [[nodiscard]] std::expected<void, StarException> add(Left const& left, Right const& right);
  [[nodiscard]] std::expected<void, StarException> add(value_type const& value);

  // Overwrites the left / right mapping regardless of whether each side
  // already exists.
  void overwrite(Left const& left, Right const& right);
  void overwrite(value_type const& value);

  // Removes the pair with the given left side, returns true if this pair was
  // found, false otherwise.
  bool erase_left(Left const& left);

  // Removes the pair with the given right side, returns true if this pair was
  // found, false otherwise.
  bool erase_right(Right const& right);

  const_iterator begin() const;
  const_iterator end() const;

  std::size_t size() const;

  void clear();

  bool empty() const;

  bool operator==(BiMap const& m) const;

private:
  template <typename>
  friend class EnumMap;
  void rebind_indices();

  LeftMap m_leftMap;
  RightMap m_rightMap;
};

template <typename Left, typename Right, typename LeftHash = Star::hash<Left>, typename RightHash = Star::hash<Right>>
using BiHashMap = BiMap<Left, Right, StableHashMap<Left, Right const*, LeftHash>, StableHashMap<Right, Left const*, RightHash>>;

// Fixed ASCII enum spellings share the same Unicode-aware comparison and UTF-8
// error boundary as StringView. Storage and reverse indices are constant data.
constexpr int compareEnumNameBytes(std::string_view a, std::string_view b) {
  auto lower = [](unsigned char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; };
  for (std::size_t i = 0; i < std::min(a.size(), b.size()); ++i) {
    auto left = lower(static_cast<unsigned char>(a[i]));
    auto right = lower(static_cast<unsigned char>(b[i]));
    if (left != right)
      return left < right ? -1 : 1;
  }
  return a.size() < b.size() ? -1 : a.size() > b.size() ? 1 : 0;
}

template <typename Enum, std::size_t N>
consteval auto sortedEnumNames(std::array<std::pair<Enum, std::string_view>, N> names) {
  std::sort(names.begin(), names.end(), [](auto const& a, auto const& b) { return a.first < b.first; });
  return names;
}

template <auto const& Names>
consteval bool validEnumNames() {
  for (std::size_t i = 0; i < Names.size(); ++i) {
    for (unsigned char c : Names[i].second)
      if (c >= 0x80)
        return false;
    if (i && Names[i - 1].first >= Names[i].first)
      return false;
    for (std::size_t j = 0; j < i; ++j)
      if (compareEnumNameBytes(Names[i].second, Names[j].second) == 0)
        return false;
  }
  return true;
}

template <auto const& Names>
[[nodiscard]] std::expected<std::string_view, StarException> enumName(
    typename std::remove_cvref_t<decltype(Names)>::value_type::first_type value) {
  static_assert(validEnumNames<Names>(), "Fixed enum names must be unique ASCII spellings in enum order");
  auto position = std::lower_bound(Names.begin(), Names.end(), value,
      [](auto const& pair, auto key) { return pair.first < key; });
  if (position == Names.end() || position->first != value)
    return std::unexpected(map_diagnostic_error("Key '{}' not found in Map::get()",
                                                "enumName: key not found while rendering its diagnostic", value));
  return position->second;
}

template <auto const& Names>
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
      return compareEnumNameBytes(Names[a].second, Names[b].second) < 0;
    });
    return result;
  }();
  auto position = std::ranges::lower_bound(indices, bytes,
      [](std::string_view a, std::string_view b) { return compareEnumNameBytes(a, b) < 0; },
      [](std::size_t index) { return Names[index].second; });
  if (position == indices.end() || compareEnumNameBytes(Names[*position].second, bytes) != 0)
    return std::nullopt;
  return Names[*position].first;
}

template <auto const& Names>
[[nodiscard]] auto enumValue(StringView name)
    -> std::expected<typename std::remove_cvref_t<decltype(Names)>::value_type::first_type, StarException> {
  auto lookup = maybeEnumValue<Names>(name);
  if (!lookup)
    return std::unexpected(std::move(lookup.error()));
  auto const& matchedValue = *lookup;
  if (!matchedValue)
    return std::unexpected(map_diagnostic_error("Key '{}' not found in Map::get()",
                                                "enumValue: key not found while rendering its diagnostic", name));
  return *matchedValue;
}

// Case insensitive Enum <-> String map. Only validated, hash-stable keys enter
// the reverse index; public text lookup validates before touching the index.
template <typename EnumType>
class EnumMap {
  using Storage = BiMap<EnumType, CaseInsensitiveString,
      Map<EnumType, CaseInsensitiveString const*>,
      StableHashMap<CaseInsensitiveString, EnumType const*, CaseInsensitiveStringHash, CaseInsensitiveStringCompare>>;

public:
  using Left = EnumType;
  using Right = String;
  using value_type = std::pair<Left, Right>;

  struct iterator {
    typename Storage::const_iterator inner;
    iterator& operator++() { ++inner; return *this; }
    iterator operator++(int) { auto previous = *this; ++*this; return previous; }
    bool operator==(iterator const& rhs) const { return inner == rhs.inner; }
    bool operator!=(iterator const& rhs) const { return !(*this == rhs); }
    std::pair<Left const&, Right const&> operator*() const {
      auto pair = *inner;
      return {pair.first, pair.second.string()};
    }
  };
  using const_iterator = iterator;

  EnumMap() = default;
  EnumMap(EnumMap const&) = default;
  EnumMap(EnumMap&&) = default;
  EnumMap& operator=(EnumMap const&) = default;
  EnumMap& operator=(EnumMap&&) = default;

  [[nodiscard]] static std::expected<EnumMap, StarException> create(std::initializer_list<value_type> values) {
    EnumMap result;
    for (auto const& value : values) {
      auto added = result.add(value);
      if (!added)
        return std::unexpected(std::move(added.error()));
    }
    return result;
  }

  template <typename Collection>
  [[nodiscard]] static std::expected<EnumMap, StarException> from(Collection const& values) {
    EnumMap result;
    for (auto const& value : values) {
      auto inserted = result.insert(value.first, value.second);
      if (!inserted)
        return std::unexpected(std::move(inserted.error()));
    }
    return result;
  }

  List<Left> left_values() const { return m_storage.left_values(); }
  List<Right> right_values() const {
    List<Right> result;
    for (auto const& pair : *this)
      result.push_back(pair.second);
    return result;
  }
  List<value_type> pairs() const {
    List<value_type> result;
    for (auto const& pair : *this)
      result.push_back(value_type(pair.first, pair.second));
    return result;
  }

  bool contains_left(Left const& left) const { return m_storage.contains_left(left); }
  [[nodiscard]] std::expected<bool, StarException> contains_right(Right const& right) const {
    return find_left_ref(right).transform([](auto const& matchedLeft) { return matchedLeft.has_value(); });
  }
  [[nodiscard]] std::expected<std::reference_wrapper<Right const>, StarException> get_right(Left const& left) const {
    return m_storage.get_right(left).transform(
      [](auto const& storedRight) { return std::cref(storedRight.get().string()); });
  }
  [[nodiscard]] std::expected<std::reference_wrapper<Left const>, StarException> get_left(Right const& right) const {
    auto lookup = find_left_ref(right);
    if (!lookup)
      return std::unexpected(std::move(lookup.error()));
    auto const& matchedLeft = *lookup;
    if (!matchedLeft)
      return std::unexpected(map_diagnostic_error(
        "Key '{}' not found in Map::get()", "EnumMap::get_left: key not found while rendering its diagnostic", right));
    return *matchedLeft;
  }
  Right value_right(Left const& left, Right const& fallback = Right()) const {
    if (auto value = find_right_ref(left))
      return value->get();
    return fallback;
  }
  [[nodiscard]] std::expected<Left, StarException> value_left(Right const& right, Left const& fallback = Left()) const {
    auto lookup = find_left_ref(right);
    if (!lookup)
      return std::unexpected(std::move(lookup.error()));
    auto const& matchedLeft = *lookup;
    return matchedLeft ? matchedLeft->get() : fallback;
  }
  std::optional<Right> maybe_right(Left const& left) const {
    if (auto value = find_right_ref(left))
      return value->get();
    return {};
  }
  [[nodiscard]] std::expected<std::optional<Left>, StarException> maybe_left(Right const& right) const {
    auto lookup = find_left_ref(right);
    if (!lookup)
      return std::unexpected(std::move(lookup.error()));
    auto const& matchedLeft = *lookup;
    if (!matchedLeft)
      return std::optional<Left>();
    return std::optional<Left>(matchedLeft->get());
  }
  [[nodiscard]] std::expected<Right, StarException> extract_right(Left const& left) {
    auto value = m_storage.extract_right(left);
    if (!value) return std::unexpected(std::move(value.error()));
    return value->string();
  }
  [[nodiscard]] std::expected<Left, StarException> extract_left(Right const& right) {
    auto lookup = find_left_ref(right);
    if (!lookup)
      return std::unexpected(std::move(lookup.error()));
    auto const& matchedLeft = *lookup;
    if (!matchedLeft)
      return std::unexpected(map_diagnostic_error("No such key '{}' in BiMap::extract_left",
                                                  "EnumMap::extract_left: key not found while rendering its diagnostic",
                                                  right));
    Left result = matchedLeft->get();
    m_storage.erase_left(result);
    return result;
  }
  std::optional<Right> try_extract_right(Left const& left) {
    if (auto value = m_storage.try_extract_right(left))
      return value->string();
    return {};
  }
  [[nodiscard]] std::expected<std::optional<Left>, StarException> try_extract_left(Right const& right) {
    auto lookup = find_left_ref(right);
    if (!lookup)
      return std::unexpected(std::move(lookup.error()));
    auto const& matchedLeft = *lookup;
    if (!matchedLeft)
      return std::optional<Left>();
    Left result = matchedLeft->get();
    m_storage.erase_left(result);
    return std::optional<Left>(result);
  }
  std::optional<std::reference_wrapper<Right const>> find_right_ref(Left const& left) const {
    auto value = m_storage.find_right_ref(left);
    if (value) return std::cref(value->get().string());
    return std::nullopt;
  }
  [[nodiscard]] std::expected<std::optional<std::reference_wrapper<Left const>>, StarException>
  find_left_ref(Right const& right) const {
    auto key = CaseInsensitiveStringView::create(right);
    if (!key) return std::unexpected(std::move(key.error()));
    auto position = m_storage.m_rightMap.find(*key);
    if (position == m_storage.m_rightMap.end()) return std::nullopt;
    return std::optional{std::reference_wrapper<Left const>(*position->second)};
  }
  [[nodiscard]] std::expected<std::pair<iterator, bool>, StarException> insert(value_type const& value) {
    auto key = CaseInsensitiveString::create(value.second);
    if (!key) return std::unexpected(std::move(key.error()));
    auto result = m_storage.insert(typename Storage::value_type(value.first, std::move(*key)));
    return std::pair<iterator, bool>{iterator{result.first}, result.second};
  }
  [[nodiscard]] std::expected<bool, StarException> insert(Left const& left, Right const& right) {
    auto key = CaseInsensitiveString::create(right);
    if (!key) return std::unexpected(std::move(key.error()));
    return m_storage.insert(left, *key);
  }
  [[nodiscard]] std::expected<void, StarException> add(Left const& left, Right const& right) {
    auto key = CaseInsensitiveString::create(right);
    if (!key) return std::unexpected(std::move(key.error()));
    return m_storage.add(left, *key);
  }
  [[nodiscard]] std::expected<void, StarException> add(value_type const& value) {
    return add(value.first, value.second);
  }
  [[nodiscard]] std::expected<void, StarException> overwrite(Left const& left, Right const& right) {
    auto key = CaseInsensitiveString::create(right);
    if (!key) return std::unexpected(std::move(key.error()));
    m_storage.overwrite(left, *key);
    return {};
  }
  [[nodiscard]] std::expected<void, StarException> overwrite(value_type const& value) {
    return overwrite(value.first, value.second);
  }
  bool erase_left(Left const& left) { return m_storage.erase_left(left); }
  [[nodiscard]] std::expected<bool, StarException> erase_right(Right const& right) {
    auto lookup = find_left_ref(right);
    if (!lookup)
      return std::unexpected(std::move(lookup.error()));
    auto const& matchedLeft = *lookup;
    return matchedLeft ? m_storage.erase_left(matchedLeft->get()) : false;
  }
  iterator begin() const { return {m_storage.begin()}; }
  iterator end() const { return {m_storage.end()}; }
  std::size_t size() const { return m_storage.size(); }
  bool empty() const { return m_storage.empty(); }
  void clear() { m_storage.clear(); }
  bool operator==(EnumMap const& rhs) const { return m_storage == rhs.m_storage; }

private:
  Storage m_storage;
};

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
auto BiMap<LeftT, RightT, LeftMapT, RightMapT>::BiMapIterator::operator++() -> BiMapIterator & {
  ++iterator;
  return *this;
}

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
auto BiMap<LeftT, RightT, LeftMapT, RightMapT>::BiMapIterator::operator++(int) -> BiMapIterator {
  BiMapIterator last{iterator};
  ++iterator;
  return last;
}

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
bool BiMap<LeftT, RightT, LeftMapT, RightMapT>::BiMapIterator::operator==(BiMapIterator const& rhs) const {
  return iterator == rhs.iterator;
}

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
bool BiMap<LeftT, RightT, LeftMapT, RightMapT>::BiMapIterator::operator!=(BiMapIterator const& rhs) const {
  return iterator != rhs.iterator;
}

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
std::pair<LeftT const&, RightT const&> BiMap<LeftT, RightT, LeftMapT, RightMapT>::BiMapIterator::operator*() const {
  return {iterator->first, *iterator->second};
}

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
template <typename Collection>
BiMap<LeftT, RightT, LeftMapT, RightMapT> BiMap<LeftT, RightT, LeftMapT, RightMapT>::from(Collection const& c) {
  return BiMap(c.begin(), c.end());
}

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
BiMap<LeftT, RightT, LeftMapT, RightMapT>::BiMap() = default;

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
BiMap<LeftT, RightT, LeftMapT, RightMapT>::BiMap(BiMap const& map)
    : m_leftMap(map.m_leftMap), m_rightMap(map.m_rightMap) {
  rebind_indices();
}

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
template <typename InputIterator>
BiMap<LeftT, RightT, LeftMapT, RightMapT>::BiMap(InputIterator beg, InputIterator end) {
  while (beg != end) {
    insert(*beg);
    ++beg;
  }
}

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
auto BiMap<LeftT, RightT, LeftMapT, RightMapT>::create(std::initializer_list<value_type> list) -> std::expected<BiMap, StarException> {
  BiMap result;
  for (value_type const& v : list) {
    auto inserted = result.add(v.first, v.second);
    if (!inserted)
      return std::unexpected(std::move(inserted.error()));
  }
  return result;
}
template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
void BiMap<LeftT, RightT, LeftMapT, RightMapT>::rebind_indices() {
  for (auto& left : m_leftMap) {
    auto right = m_rightMap.find(*left.second);
    left.second = &right->first;
    right->second = &left.first;
  }
}

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
List<LeftT> BiMap<LeftT, RightT, LeftMapT, RightMapT>::left_values() const {
  return m_leftMap.keys();
}

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
List<RightT> BiMap<LeftT, RightT, LeftMapT, RightMapT>::right_values() const {
  return m_rightMap.keys();
}

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
auto BiMap<LeftT, RightT, LeftMapT, RightMapT>::pairs() const -> List<value_type> {
  List<value_type> values;
  for (auto const& p : *this)
    values.push_back(p);
  return values;
}

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
bool BiMap<LeftT, RightT, LeftMapT, RightMapT>::contains_left(Left const& left) const {
  return m_leftMap.contains(left);
}

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
bool BiMap<LeftT, RightT, LeftMapT, RightMapT>::contains_right(Right const& right) const {
  return m_rightMap.contains(right);
}

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
std::expected<std::reference_wrapper<RightT const>, StarException>
BiMap<LeftT, RightT, LeftMapT, RightMapT>::get_right(Left const& left) const {
  return m_leftMap.get(left).transform(
    [](auto const& rightSlot) { return std::reference_wrapper<RightT const>(*rightSlot.get()); });
}

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
std::expected<std::reference_wrapper<LeftT const>, StarException>
BiMap<LeftT, RightT, LeftMapT, RightMapT>::get_left(Right const& right) const {
  return m_rightMap.get(right).transform(
    [](auto const& leftSlot) { return std::reference_wrapper<LeftT const>(*leftSlot.get()); });
}

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
RightT BiMap<LeftT, RightT, LeftMapT, RightMapT>::value_right(Left const& left, Right const& def) const {
  return maybe_right(left).value_or(def);
}

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
LeftT BiMap<LeftT, RightT, LeftMapT, RightMapT>::value_left(Right const& right, Left const& def) const {
  return maybe_left(right).value_or(def);
}

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
std::optional<RightT> BiMap<LeftT, RightT, LeftMapT, RightMapT>::maybe_right(Left const& left) const {
  auto i = m_leftMap.find(left);
  if (i != m_leftMap.end())
    return std::optional<RightT>(std::in_place, *i->second);
  return {};
}

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
std::optional<LeftT> BiMap<LeftT, RightT, LeftMapT, RightMapT>::maybe_left(Right const& right) const {
  auto i = m_rightMap.find(right);
  if (i != m_rightMap.end())
    return std::optional<LeftT>(std::in_place, *i->second);
  return {};
}

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
std::expected<RightT, StarException> BiMap<LeftT, RightT, LeftMapT, RightMapT>::extract_right(Left const& left) {
  if (auto right = try_extract_right(left))
    return std::move(*right);
  return std::unexpected(map_diagnostic_error("No such key '{}' in BiMap::extract_right",
                                              "BiMap::extract_right: key not found while rendering its diagnostic",
                                              left));
}

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
std::expected<LeftT, StarException> BiMap<LeftT, RightT, LeftMapT, RightMapT>::extract_left(Right const& right) {
  if (auto left = try_extract_left(right))
    return std::move(*left);
  return std::unexpected(map_diagnostic_error("No such key '{}' in BiMap::extract_left",
                                              "BiMap::extract_left: key not found while rendering its diagnostic",
                                              right));
}

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
std::optional<RightT> BiMap<LeftT, RightT, LeftMapT, RightMapT>::try_extract_right(Left const& left) {
  if (auto rightPtr = m_leftMap.try_extract_value(left).value_or(nullptr)) {
    Right right = *rightPtr;
    m_rightMap.remove(*rightPtr);
    return std::optional<RightT>(std::in_place, std::move(right));
  } else {
    return {};
  }
}

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
std::optional<LeftT> BiMap<LeftT, RightT, LeftMapT, RightMapT>::try_extract_left(Right const& right) {
  if (auto leftPtr = m_rightMap.try_extract_value(right).value_or(nullptr)) {
    Left left = *leftPtr;
    m_leftMap.remove(*leftPtr);
    return std::optional<LeftT>(std::in_place, std::move(left));
  } else {
    return {};
  }
}

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
std::optional<std::reference_wrapper<RightT const>>
BiMap<LeftT, RightT, LeftMapT, RightMapT>::find_right_ref(Left const& left) const {
  auto position = m_leftMap.find(left);
  if (position == m_leftMap.end()) return std::nullopt;
  return std::reference_wrapper<RightT const>(*position->second);
}

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
std::optional<std::reference_wrapper<LeftT const>>
BiMap<LeftT, RightT, LeftMapT, RightMapT>::find_left_ref(Right const& right) const {
  auto position = m_rightMap.find(right);
  if (position == m_rightMap.end()) return std::nullopt;
  return std::reference_wrapper<LeftT const>(*position->second);
}

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
BiMap<LeftT, RightT, LeftMapT, RightMapT>& BiMap<LeftT, RightT, LeftMapT, RightMapT>::operator=(BiMap const& map) {
  if (this != &map) {
    m_leftMap = map.m_leftMap;
    m_rightMap = map.m_rightMap;
    rebind_indices();
  }
  return *this;
}

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
auto BiMap<LeftT, RightT, LeftMapT, RightMapT>::insert(value_type const& val) -> std::pair<iterator, bool> {
  auto leftRes = m_leftMap.insert(std::make_pair(val.first, nullptr));
  if (!leftRes.second)
    return {BiMapIterator{leftRes.first}, false};

  auto rightRes = m_rightMap.insert(std::make_pair(val.second, nullptr));
  if (!rightRes.second) {
    m_leftMap.erase(leftRes.first);
    return {BiMapIterator{m_leftMap.find(*rightRes.first->second)}, false};
  }
  leftRes.first->second = &rightRes.first->first;
  rightRes.first->second = &leftRes.first->first;
  return {BiMapIterator{leftRes.first}, true};
};

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
bool BiMap<LeftT, RightT, LeftMapT, RightMapT>::insert(Left const& left, Right const& right) {
  return insert(std::make_pair(left, right)).second;
}

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
std::expected<void, StarException> BiMap<LeftT, RightT, LeftMapT, RightMapT>::add(Left const& left, Right const& right) {
  if (m_leftMap.contains(left))
    return std::unexpected(
      map_diagnostic_error("BiMap already contains left side value '{}'",
                           "BiMap::add: left side value already present while rendering its diagnostic", left));
  if (m_rightMap.contains(right))
    return std::unexpected(
      map_diagnostic_error("BiMap already contains right side value '{}'",
                           "BiMap::add: right side value already present while rendering its diagnostic", right));
  insert(left, right);
  return {};
}

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
std::expected<void, StarException> BiMap<LeftT, RightT, LeftMapT, RightMapT>::add(value_type const& value) {
  return add(value.first, value.second);
}

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
void BiMap<LeftT, RightT, LeftMapT, RightMapT>::overwrite(Left const& left, Right const& right) {
  auto left_position = m_leftMap.find(left);
  auto right_position = m_rightMap.find(right);
  if (left_position != m_leftMap.end()) {
    auto old_right = left_position->second;
    if (right_position != m_rightMap.end()) {
      if (old_right == &right_position->first)
        return;
      auto old_left = m_leftMap.find(*right_position->second);
      left_position->second = &right_position->first;
      right_position->second = &left_position->first;
      m_leftMap.erase(old_left);
    } else {
      // Insert before removing: either argument can borrow an indexed key.
      // Node addresses survive a hash rehash, but its iterators do not.
      right_position = m_rightMap.insert(std::make_pair(right, &left_position->first)).first;
      left_position->second = &right_position->first;
    }
    m_rightMap.erase(m_rightMap.find(*old_right));
  } else if (right_position != m_rightMap.end()) {
    auto old_left = right_position->second;
    left_position = m_leftMap.insert(std::make_pair(left, &right_position->first)).first;
    right_position->second = &left_position->first;
    m_leftMap.erase(m_leftMap.find(*old_left));
  } else {
    insert(left, right);
  }
}

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
void BiMap<LeftT, RightT, LeftMapT, RightMapT>::overwrite(value_type const& value) {
  return overwrite(value.first, value.second);
}

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
bool BiMap<LeftT, RightT, LeftMapT, RightMapT>::erase_left(Left const& left) {
  auto left_position = m_leftMap.find(left);
  if (left_position != m_leftMap.end()) {
    auto right_position = m_rightMap.find(*left_position->second);
    m_rightMap.erase(right_position);
    m_leftMap.erase(left_position);
    return true;
  }

  return false;
}

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
bool BiMap<LeftT, RightT, LeftMapT, RightMapT>::erase_right(Right const& right) {
  auto right_position = m_rightMap.find(right);
  if (right_position != m_rightMap.end()) {
    auto left_position = m_leftMap.find(*right_position->second);
    m_leftMap.erase(left_position);
    m_rightMap.erase(right_position);
    return true;
  }

  return false;
}

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
auto BiMap<LeftT, RightT, LeftMapT, RightMapT>::begin() const -> const_iterator {
  return BiMapIterator{m_leftMap.begin()};
}

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
auto BiMap<LeftT, RightT, LeftMapT, RightMapT>::end() const -> const_iterator {
  return BiMapIterator{m_leftMap.end()};
}

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
std::size_t BiMap<LeftT, RightT, LeftMapT, RightMapT>::size() const {
  return m_leftMap.size();
}

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
void BiMap<LeftT, RightT, LeftMapT, RightMapT>::clear() {
  m_leftMap.clear();
  m_rightMap.clear();
}

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
bool BiMap<LeftT, RightT, LeftMapT, RightMapT>::empty() const {
  return m_leftMap.empty();
}

template <typename LeftT, typename RightT, typename LeftMapT, typename RightMapT>
bool BiMap<LeftT, RightT, LeftMapT, RightMapT>::operator==(BiMap const& m) const {
  if (&m == this)
    return true;

  if (size() != m.size())
    return false;

  for (auto const& pair : *this) {
    auto value = m.find_right_ref(pair.first);
    if (!value || value->get() != pair.second)
      return false;
  }

  return true;
}

}

export module star.bi_map;

export namespace Star {
  using ::Star::BiMap;
  using ::Star::BiHashMap;
  using ::Star::EnumMap;
  using ::Star::sortedEnumNames;
  using ::Star::validEnumNames;
  using ::Star::enumName;
  using ::Star::maybeEnumValue;
  using ::Star::enumValue;
}
