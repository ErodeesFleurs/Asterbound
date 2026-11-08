module;
import std;
import star.text_formatter;

import star.map;
import star.flat_hash_map;
import star.list;
import star.hash;
import star.format;

import star.output_proxy;
import star.algorithm;
namespace Star {

// Wraps a normal map type and provides an element order independent of the
// underlying map order.
template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
class OrderedMapWrapper {
public:
  using key_type = Key;
  using mapped_type = Value;
  using value_type = std::pair<key_type const, mapped_type>;

  using OrderType = LinkedList<value_type, Allocator>;
  using MapType = Map<std::reference_wrapper<key_type const>, typename OrderType::iterator, MapArgs...,
                      typename std::allocator_traits<Allocator>::template rebind_alloc<
                        std::pair<std::reference_wrapper<key_type const> const, typename OrderType::iterator>>>;

  using iterator = typename OrderType::iterator;
  using const_iterator = typename OrderType::const_iterator;

  using reverse_iterator = typename OrderType::reverse_iterator;
  using const_reverse_iterator = typename OrderType::const_reverse_iterator;

  template <typename Collection>
  static OrderedMapWrapper from(Collection const& c);

  OrderedMapWrapper();

  OrderedMapWrapper(OrderedMapWrapper const& map);

  template <typename InputIterator>
  OrderedMapWrapper(InputIterator beg, InputIterator end);

  OrderedMapWrapper(std::initializer_list<value_type> list);

  List<key_type> keys() const;
  List<mapped_type> values() const;
  List<std::pair<key_type, mapped_type>> pairs() const;

  bool contains(key_type const& k) const;

  // Returns MapException if the key is not found.
  [[nodiscard]] std::expected<std::reference_wrapper<mapped_type>, StarException> get(key_type const& k);
  [[nodiscard]] std::expected<std::reference_wrapper<mapped_type const>, StarException> get(key_type const& k) const;

  // Return def if key not found
  mapped_type value(key_type const& k, mapped_type d = mapped_type()) const;

  std::optional<mapped_type> maybe(key_type const& k) const;

  std::optional<std::reference_wrapper<mapped_type const>> find_ref(key_type const& k) const {
    auto position = find(k);
    if (position == end()) return std::nullopt;
    return std::reference_wrapper<mapped_type const>(position->second);
  }
  std::optional<std::reference_wrapper<mapped_type>> find_ref(key_type const& k) {
    auto position = find(k);
    if (position == end()) return std::nullopt;
    return std::reference_wrapper<mapped_type>(position->second);
  }

  mapped_type& operator[](key_type const& k);

  OrderedMapWrapper& operator=(OrderedMapWrapper const& map);

  bool operator==(OrderedMapWrapper const& m) const;

  // Finds the first matching value's key, or returns MapException.
  [[nodiscard]] std::expected<key_type, StarException> key_of(mapped_type const& v) const;

  std::pair<iterator, bool> insert(value_type const& v);
  std::pair<iterator, bool> insert(value_type&& v);
  std::pair<iterator, bool> insert(key_type k, mapped_type v);

  std::pair<iterator, bool> insert_front(value_type const& v);
  std::pair<iterator, bool> insert_front(value_type&& v);
  std::pair<iterator, bool> insert_front(key_type k, mapped_type v);

  // Constructs the mapped value only when the key is absent.
  template <typename... Args> std::pair<iterator, bool> try_emplace(key_type const& k, Args&&... args);
  template <typename... Args> std::pair<iterator, bool> try_emplace(key_type&& k, Args&&... args);

  // Add a key / value pair, returning an error for an existing key.
  [[nodiscard]] std::expected<std::reference_wrapper<mapped_type>, StarException> add(key_type k, mapped_type v);

  // Set a key to a value, always override if it already exists
  mapped_type& set(key_type k, mapped_type v);

  // Appends all values of given map into this map.  If overwite is false, then
  // skips values that already exist in this map.  Returns false if any keys
  // previously existed.
  bool merge(OrderedMapWrapper const& m, bool overwrite = false);

  // Removes the item with key k and returns true if found, false otherwise.
  bool remove(key_type const& k);

  // Remove and return the value, or MapException if the key is not found.
  [[nodiscard]] std::expected<mapped_type, StarException> extract_value(key_type const& k);

  std::optional<value_type> try_extract_value(key_type const& k);

  const_iterator begin() const;
  const_iterator end() const;

  iterator begin();
  iterator end();

  const_reverse_iterator rbegin() const;
  const_reverse_iterator rend() const;

  reverse_iterator rbegin();
  reverse_iterator rend();

  std::size_t size() const;

  iterator erase(iterator i);
  std::size_t erase(key_type const& k);

  iterator find(key_type const& k);
  const_iterator find(key_type const& k) const;

  std::optional<std::size_t> index_of(key_type const& k) const;

  [[nodiscard]] std::expected<std::reference_wrapper<key_type const>, StarException> key_at(std::size_t i) const;
  [[nodiscard]] std::expected<std::reference_wrapper<mapped_type const>, StarException> value_at(std::size_t i) const;
  [[nodiscard]] std::expected<std::reference_wrapper<mapped_type>, StarException> value_at(std::size_t i);

  [[nodiscard]] std::expected<value_type, StarException> extract_front();
  [[nodiscard]] std::expected<void, StarException> erase_front();

  [[nodiscard]] std::expected<std::reference_wrapper<value_type const>, StarException> first() const;

  [[nodiscard]] std::expected<std::reference_wrapper<key_type const>, StarException> first_key() const;
  [[nodiscard]] std::expected<std::reference_wrapper<mapped_type>, StarException> first_value();
  [[nodiscard]] std::expected<std::reference_wrapper<mapped_type const>, StarException> first_value() const;

  iterator insert(iterator pos, value_type v);

  void clear();

  bool empty() const;

  iterator move_to_back(iterator i);
  [[nodiscard]] std::expected<void, StarException> move_to_back(key_type const& k);

  iterator move_to_front(iterator i);
  [[nodiscard]] std::expected<void, StarException> move_to_front(key_type const& k);

  template <typename Compare>
  void sort(Compare comp);

  void sort_by_key();
  void sort_by_value();

private:
  template <typename KeyArg, typename... Args> iterator emplace_unchecked(iterator pos, KeyArg&& key, Args&&... args) {
    auto node = m_order.emplace(pos, std::piecewise_construct, std::forward_as_tuple(std::forward<KeyArg>(key)),
                                std::forward_as_tuple(std::forward<Args>(args)...));
    m_map.emplace(std::cref(node->first), node);
    return node;
  }

  template <typename KeyArg, typename... Args>
  std::pair<iterator, bool> try_emplace_at(iterator pos, KeyArg&& key, Args&&... args) {
    auto position = m_map.find(key);
    if (position != m_map.end())
      return {position->second, false};
    return {emplace_unchecked(pos, std::forward<KeyArg>(key), std::forward<Args>(args)...), true};
  }

  template <typename Self>
  static auto get_impl(Self& self, key_type const& key)
    -> std::expected<std::reference_wrapper<std::remove_reference_t<decltype((self.m_order.begin()->second))>>,
                     StarException> {
    auto position = self.m_map.find(key);
    if (position == self.m_map.end())
      return std::unexpected(map_diagnostic_error("Key '{}' not found in OrderedMap::get()",
                                                  "OrderedMap::get: key not found while rendering its diagnostic",
                                                  key));
    return std::reference_wrapper<std::remove_reference_t<decltype((self.m_order.begin()->second))>>(position->second->second);
  }

  template <typename Self>
  static auto value_at_impl(Self& self, std::size_t index)
    -> std::expected<std::reference_wrapper<std::remove_reference_t<decltype((self.m_order.begin()->second))>>,
                     StarException> {
    if (index >= self.size())
      return std::unexpected(MapException(Star::format("index {} out of range in OrderedMap::value_at()", index)));
    auto position = self.m_order.begin();
    std::advance(position, index);
    return std::reference_wrapper<std::remove_reference_t<decltype((self.m_order.begin()->second))>>(position->second);
  }

  MapType m_map;
  OrderType m_order;
};

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto write_text(TextBuffer& output, OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...> const& map)
  -> decltype(print_map(output, map));

template <typename Key, typename Value, typename Compare = std::less<Key>, typename Allocator = std::allocator<std::pair<Key const, Value>>>
using OrderedMap = OrderedMapWrapper<std::map, Key, Value, Allocator, Compare>;

template <typename Key, typename Value, typename Hash = Star::hash<Key>, typename Equals = std::equal_to<Key>, typename Allocator = std::allocator<std::pair<Key const, Value>>>
using OrderedHashMap = OrderedMapWrapper<FlatHashMap, Key, Value, Allocator, Hash, Equals>;

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
template <typename Collection>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::from(Collection const& c) -> OrderedMapWrapper {
  return OrderedMapWrapper(c.begin(), c.end());
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::OrderedMapWrapper() = default;

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::OrderedMapWrapper(OrderedMapWrapper const& map) {
  for (auto const& p : map)
    insert(p);
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
template <typename InputIterator>
OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::OrderedMapWrapper(InputIterator beg, InputIterator end) {
  while (beg != end) {
    insert(*beg);
    ++beg;
  }
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::OrderedMapWrapper(std::initializer_list<value_type> list) {
  for (auto const& v : list)
    insert(v);
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::keys() const -> List<key_type> {
  List<key_type> keys;
  keys.reserve(size());
  std::ranges::copy(std::views::keys(*this), std::back_inserter(keys));
  return keys;
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::values() const -> List<mapped_type> {
  List<mapped_type> values;
  values.reserve(size());
  std::ranges::copy(std::views::values(*this), std::back_inserter(values));
  return values;
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::pairs() const -> List<std::pair<key_type, mapped_type>> {
  List<std::pair<key_type, mapped_type>> plist;
  plist.reserve(size());
  std::ranges::copy(*this, std::back_inserter(plist));
  return plist;
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
bool OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::contains(key_type const& k) const {
  return m_map.find(k) != m_map.end();
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::get(key_type const& k) -> std::expected<std::reference_wrapper<mapped_type>, StarException> {
  return get_impl(*this, k);
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::get(key_type const& k) const -> std::expected<std::reference_wrapper<mapped_type const>, StarException> {
  return get_impl(*this, k);
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::value(key_type const& k, mapped_type d) const -> mapped_type {
  auto i = m_map.find(k);
  if (i == m_map.end())
    return d;
  else
    return i->second->second;
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::maybe(key_type const& k) const -> std::optional<mapped_type> {
  auto i = find(k);
  if (i == end())
    return {};
  else
    return std::optional<mapped_type>(std::in_place, i->second);
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::operator[](key_type const& k) -> mapped_type& {
  return try_emplace(k).first->second;
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::operator=(OrderedMapWrapper const& map) -> OrderedMapWrapper& {
  if (this != &map) {
    clear();
    for (auto const& p : map)
      insert(p);
  }

  return *this;
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
bool OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::operator==(OrderedMapWrapper const& m) const {
  return this == &m || maps_equal(*this, m);
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::key_of(mapped_type const& v) const
  -> std::expected<key_type, StarException> {
  auto found = std::find_if(begin(), end(), [&](auto const& pair) {
    if (pair.second == v)
      return true;
    return false;
  });
  if (found != end())
    return found->first;
  return std::unexpected(map_diagnostic_error("Value '{}' not found in OrderedMap::key_of()",
                                              "OrderedMap::key_of: value not found while rendering its diagnostic", v));
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::insert(value_type const& v) -> std::pair<iterator, bool> {
  return try_emplace(v.first, v.second);
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::insert(value_type&& v) -> std::pair<iterator, bool> {
  return try_emplace(v.first, std::move(v.second));
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::insert(key_type k, mapped_type v) -> std::pair<iterator, bool> {
  return try_emplace(std::move(k), std::move(v));
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::insert_front(value_type const& v)
  -> std::pair<iterator, bool> {
  return try_emplace_at(m_order.begin(), v.first, v.second);
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::insert_front(value_type&& v)
  -> std::pair<iterator, bool> {
  return try_emplace_at(m_order.begin(), v.first, std::move(v.second));
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::insert_front(key_type k, mapped_type v)
  -> std::pair<iterator, bool> {
  return try_emplace_at(m_order.begin(), std::move(k), std::move(v));
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
template <typename... Args>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::try_emplace(key_type const& k, Args&&... args)
  -> std::pair<iterator, bool> {
  return try_emplace_at(m_order.end(), k, std::forward<Args>(args)...);
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
template <typename... Args>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::try_emplace(key_type&& k, Args&&... args)
  -> std::pair<iterator, bool> {
  return try_emplace_at(m_order.end(), std::move(k), std::forward<Args>(args)...);
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::add(key_type k, mapped_type v) -> std::expected<std::reference_wrapper<mapped_type>, StarException> {
  auto pair = try_emplace(std::move(k), std::move(v));
  if (!pair.second)
    return std::unexpected(map_diagnostic_error("Entry with key '{}' already present.",
                                                "OrderedMap::add: key already present while rendering its diagnostic",
                                                pair.first->first));
  return std::reference_wrapper<mapped_type>(pair.first->second);
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::set(key_type k, mapped_type v) -> mapped_type& {
  auto [position, inserted] = try_emplace(std::move(k), std::move(v));
  if (!inserted)
    position->second = std::move(v);
  return position->second;
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
bool OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::merge(OrderedMapWrapper const& m, bool overwrite) {
  return map_merge(*this, m, overwrite);
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
bool OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::remove(key_type const& k) {
  auto i = m_map.find(k);
  if (i != m_map.end()) {
    auto orderIt = i->second;
    m_map.erase(i);

    m_order.erase(orderIt);
    return true;
  } else {
    return false;
  }
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::extract_value(key_type const& k)
  -> std::expected<mapped_type, StarException> {
  auto i = m_map.find(k);
  if (i != m_map.end()) {
    auto orderIt = i->second;
    std::expected<mapped_type, StarException> result(std::in_place, std::move(orderIt->second));
    m_map.erase(i);
    m_order.erase(orderIt);
    return result;
  } else {
    return std::unexpected(
      map_diagnostic_error("Key '{}' not found in OrderedMap::extract_value()",
                           "OrderedMap::extract_value: key not found while rendering its diagnostic", k));
  }
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::try_extract_value(key_type const& k)
  -> std::optional<value_type> {
  auto position = m_map.find(k);
  if (position != m_map.end()) {
    auto node = position->second;
    std::optional<value_type> result(std::in_place, std::move(*node));
    m_map.erase(position);
    m_order.erase(node);
    return result;
  }

  return {};
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::begin() const -> const_iterator {
  return m_order.begin();
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::end() const -> const_iterator {
  return m_order.end();
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::begin() -> iterator {
  return m_order.begin();
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::end() -> iterator {
  return m_order.end();
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::rbegin() const -> const_reverse_iterator {
  return m_order.rbegin();
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::rend() const -> const_reverse_iterator {
  return m_order.rend();
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::rbegin() -> reverse_iterator {
  return m_order.rbegin();
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::rend() -> reverse_iterator {
  return m_order.rend();
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::size() const -> std::size_t {
  return m_map.size();
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::erase(iterator i) -> iterator {
  m_map.erase(i->first);
  return m_order.erase(i);
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::erase(key_type const& k) -> std::size_t {
  if (remove(k))
    return 1;
  return 0;
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::find(key_type const& k) -> iterator {
  auto i = m_map.find(k);
  if (i == m_map.end())
    return m_order.end();
  else
    return i->second;
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::find(key_type const& k) const -> const_iterator {
  auto i = m_map.find(k);
  if (i == m_map.end())
    return m_order.end();
  else
    return i->second;
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::index_of(key_type const& k) const
  -> std::optional<std::size_t> {
  typename MapType::const_iterator i = m_map.find(k);
  if (i == m_map.end())
    return {};

  return std::distance(begin(), const_iterator(i->second));
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::key_at(std::size_t i) const
  -> std::expected<std::reference_wrapper<key_type const>, StarException> {
  if (i >= size())
    return std::unexpected(MapException(Star::format("index {} out of range in OrderedMap::at()", i)));

  auto it = begin();
  std::advance(it, i);
  return std::reference_wrapper<key_type const>(it->first);
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::value_at(std::size_t i) const
  -> std::expected<std::reference_wrapper<mapped_type const>, StarException> {
  return value_at_impl(*this, i);
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::value_at(std::size_t i)
  -> std::expected<std::reference_wrapper<mapped_type>, StarException> {
  return value_at_impl(*this, i);
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::extract_front()
  -> std::expected<value_type, StarException> {
  if (empty())
    return std::unexpected(MapException("OrderedMap::extract_front() called on empty OrderedMap"));

  iterator i = m_order.begin();
  std::expected<value_type, StarException> result(std::in_place, std::move(*i));
  erase(i);
  return result;
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
std::expected<void, StarException> OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::erase_front() {
  if (empty())
    return std::unexpected(MapException("OrderedMap::erase_front() called on empty OrderedMap"));
  erase(begin());
  return {};
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::first() const -> std::expected<std::reference_wrapper<value_type const>, StarException> {
  if (empty())
    return std::unexpected(MapException("OrderedMap::first() called on empty OrderedMap"));
  return std::reference_wrapper<value_type const>(*m_order.begin());
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::first_value()
  -> std::expected<std::reference_wrapper<mapped_type>, StarException> {
  return value_at(0);
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::first_value() const
  -> std::expected<std::reference_wrapper<mapped_type const>, StarException> {
  return value_at(0);
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::first_key() const
  -> std::expected<std::reference_wrapper<key_type const>, StarException> {
  return key_at(0);
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::insert(iterator pos, value_type v) -> iterator {
  auto i = m_map.find(v.first);
  if (i == m_map.end()) {
    return emplace_unchecked(pos, v.first, std::move(v.second));
  } else {
    i->second->second = std::move(v.second);
    m_order.splice(pos, m_order, i->second);
    return i->second;
  }
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
void OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::clear() {
  m_map.clear();
  m_order.clear();
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
bool OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::empty() const {
  return size() == 0;
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::move_to_back(iterator i) -> iterator {
  m_order.splice(m_order.end(), m_order, i);
  return std::prev(m_order.end());
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::move_to_front(iterator i) -> iterator {
  m_order.splice(m_order.begin(), m_order, i);
  return m_order.begin();
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
std::expected<void, StarException>
OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::move_to_back(key_type const& k) {
  auto i = m_map.find(k);
  if (i == m_map.end())
    return std::unexpected(
      map_diagnostic_error("Key not found in OrderedMap::move_to_back('{}')",
                           "OrderedMap::move_to_back: key not found while rendering its diagnostic", k));
  move_to_back(i->second);
  return {};
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
std::expected<void, StarException>
OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::move_to_front(key_type const& k) {
  auto i = m_map.find(k);
  if (i == m_map.end())
    return std::unexpected(
      map_diagnostic_error("Key not found in OrderedMap::move_to_front('{}')",
                           "OrderedMap::move_to_front: key not found while rendering its diagnostic", k));
  move_to_front(i->second);
  return {};
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
template <typename Compare>
void OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::sort(Compare comp) {
  m_order.sort(comp);
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
void OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::sort_by_key() {
  sort([](value_type const& a, value_type const& b) {
      return a.first < b.first;
    });
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
void OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>::sort_by_value() {
  sort([](value_type const& a, value_type const& b) {
      return a.second < b.second;
    });
}

template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
auto write_text(TextBuffer& output, OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...> const& map)
  -> decltype(print_map(output, map)) {
  return print_map(output, map);
}
}

export module star.ordered_map;

export template <template <typename...> class Map, typename Key, typename Value, typename Allocator, typename... MapArgs>
  requires Star::nonfallible_text_writer<Star::OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>>
struct std::formatter<Star::OrderedMapWrapper<Map, Key, Value, Allocator, MapArgs...>> : Star::TextFormatter {};

export namespace Star {
  using ::Star::OrderedMapWrapper;
  using ::Star::OrderedMap;
  using ::Star::OrderedHashMap;
  using ::Star::write_text;
}
