module;
import star.stream_formatter;

import std;

import star.flat_hash_map;
import star.set;
import star.list;
import star.optional;
import star.hash;
import star.format;

import star.either;
namespace Star {

template <typename Range, typename Value>
concept ordered_set_source = std::ranges::input_range<Range>
  && std::is_constructible_v<Value,
      std::conditional_t<std::is_rvalue_reference_v<Range&&>, std::ranges::range_rvalue_reference_t<Range>,
          std::ranges::range_reference_t<Range>>>;

template <template <typename...> class Map, typename Value, typename Allocator, typename... Args>
class OrderedSetWrapper {
public:
  using value_type = Value;
  using allocator_type = Allocator;
  using OrderType = LinkedList<value_type, typename std::allocator_traits<Allocator>::template rebind_alloc<value_type>>;
  using MapType = Map<std::reference_wrapper<value_type const>, typename OrderType::const_iterator, Args...,
      typename std::allocator_traits<Allocator>::template rebind_alloc<
          std::pair<std::reference_wrapper<value_type const> const, typename OrderType::const_iterator>>>;
  using const_iterator = typename OrderType::const_iterator;
  using iterator = const_iterator;
  using const_reverse_iterator = typename OrderType::const_reverse_iterator;
  using reverse_iterator = const_reverse_iterator;
  using const_reference = std::reference_wrapper<value_type const>;
  // std::cref unwraps wrapper-valued keys; construct the outer slot borrow.

  OrderedSetWrapper() = default;

  explicit OrderedSetWrapper(allocator_type const& alloc)
    : m_order(make_order(alloc)), m_map(typename MapType::allocator_type(alloc)) {}

  explicit OrderedSetWrapper(Args const&... policies, allocator_type const& alloc = {})
    : m_order(make_order(alloc)), m_map(make_map(alloc, policies...)) {}

  OrderedSetWrapper(OrderedSetWrapper const& set)
    requires std::is_constructible_v<value_type, value_type const&>
    : OrderedSetWrapper(set, std::allocator_traits<allocator_type>::select_on_container_copy_construction(set.get_allocator())) {}

  OrderedSetWrapper(OrderedSetWrapper const& set, allocator_type const& alloc)
    requires std::is_constructible_v<value_type, value_type const&>
    : m_order(make_order(alloc)), m_map(empty_map(set.m_map, alloc)) {
    reserve_index(set.size());
    for (auto const& value : set)
      emplace(value);
  }

  OrderedSetWrapper(OrderedSetWrapper&&) = default;

  OrderedSetWrapper(OrderedSetWrapper&& set, allocator_type const& alloc)
    : m_order(make_order(alloc)), m_map(move_map(set, alloc)) {
    if (alloc == set.get_allocator()) {
      m_order.splice(m_order.end(), set.m_order);
    } else {
      // The old index must stop observing keys before any key is moved.
      set.m_map.clear();
      for (auto& value : set.m_order)
        add(transfer_reference(value));
      set.m_order.clear();
    }
  }

  template <std::input_iterator Iterator, std::sentinel_for<Iterator> Sentinel = Iterator>
    requires std::is_constructible_v<value_type, std::iter_reference_t<Iterator>>
  OrderedSetWrapper(Iterator first, Sentinel last, allocator_type const& alloc = {})
    : OrderedSetWrapper(alloc) {
    reserve_index(first, last);
    for (; first != last; ++first)
      insert_reference(*first);
  }

  template <std::input_iterator Iterator, std::sentinel_for<Iterator> Sentinel = Iterator>
    requires std::is_constructible_v<value_type, std::iter_reference_t<Iterator>>
  OrderedSetWrapper(Iterator first, Sentinel last, Args const&... policies, allocator_type const& alloc = {})
    : OrderedSetWrapper(policies..., alloc) {
    reserve_index(first, last);
    for (; first != last; ++first)
      insert_reference(*first);
  }

  template <typename Range>
    requires ordered_set_source<Range, value_type>
  OrderedSetWrapper(std::from_range_t, Range&& range, allocator_type const& alloc = {})
    : OrderedSetWrapper(alloc) {
    reserve_index(range);
    insert_range(std::forward<Range>(range));
  }

  template <typename Range>
    requires ordered_set_source<Range, value_type>
  OrderedSetWrapper(std::from_range_t, Range&& range, Args const&... policies, allocator_type const& alloc = {})
    : OrderedSetWrapper(policies..., alloc) {
    reserve_index(range);
    insert_range(std::forward<Range>(range));
  }

  OrderedSetWrapper(std::initializer_list<value_type> values, allocator_type const& alloc = {})
    requires std::is_constructible_v<value_type, value_type const&>
    : OrderedSetWrapper(values.begin(), values.end(), alloc) {}

  OrderedSetWrapper(std::initializer_list<value_type> values, Args const&... policies, allocator_type const& alloc = {})
    requires std::is_constructible_v<value_type, value_type const&>
    : OrderedSetWrapper(values.begin(), values.end(), policies..., alloc) {}

  template <typename Range>
    requires ordered_set_source<Range, value_type>
  static OrderedSetWrapper from(Range&& range) {
    return OrderedSetWrapper(std::from_range, std::forward<Range>(range));
  }

  OrderedSetWrapper& operator=(OrderedSetWrapper const& set)
    requires std::is_constructible_v<value_type, value_type const&>
  {
    if (this != &set) {
      auto alloc = get_allocator();
      if constexpr (std::allocator_traits<allocator_type>::propagate_on_container_copy_assignment::value)
        alloc = set.get_allocator();
      adopt(OrderedSetWrapper(set, alloc));
    }
    return *this;
  }

  OrderedSetWrapper& operator=(OrderedSetWrapper&& set) {
    if (this != &set) {
      if constexpr (std::allocator_traits<allocator_type>::propagate_on_container_move_assignment::value)
        adopt(OrderedSetWrapper(std::move(set)));
      else
        adopt(OrderedSetWrapper(std::move(set), get_allocator()));
    }
    return *this;
  }

  allocator_type get_allocator() const { return allocator_type(m_order.get_allocator()); }
  auto key_comp() const requires requires(MapType const& map) { map.key_comp(); } { return m_map.key_comp(); }
  auto hash_function() const requires requires(MapType const& map) { map.hash_function(); } { return m_map.hash_function(); }
  auto key_eq() const requires requires(MapType const& map) { map.key_eq(); } { return m_map.key_eq(); }

  // Owning, insertion-order snapshot: do not replace with a borrowed range.
  List<value_type> values() const requires std::is_constructible_v<value_type, value_type const&> {
    List<value_type> result;
    result.reserve(size());
    for (auto const& value : *this)
      result.emplace_back(value);
    return result;
  }

  auto value_view() const& { return std::ranges::subrange(begin(), end()); }
  auto value_view() && = delete;
  auto value_view() const&& = delete;

  bool contains(value_type const& value) const { return m_map.find(value) != m_map.end(); }

  std::pair<iterator, bool> insert(value_type const& value)
    requires std::is_constructible_v<value_type, value_type const&>
  { return insert_value(value, m_order.end()); }

  std::pair<iterator, bool> insert(value_type&& value)
    requires std::is_constructible_v<value_type, value_type&&>
  { return insert_value(std::move(value), m_order.end()); }

  template <typename... Parameters>
    requires std::is_constructible_v<value_type, Parameters...>
  std::pair<iterator, bool> emplace(Parameters&&... parameters) {
    auto node = m_order.emplace(m_order.end(), std::forward<Parameters>(parameters)...);
    auto [position, inserted] = m_map.insert(typename MapType::value_type(const_reference(*node), node));
    if (!inserted) {
      auto existing = position->second;
      m_order.erase(node);
      return {existing, false};
    }
    return {node, true};
  }

  bool add(value_type const& value) requires std::is_constructible_v<value_type, value_type const&> { return insert(value).second; }
  bool add(value_type&& value) requires std::is_constructible_v<value_type, value_type&&> { return insert(std::move(value)).second; }

  bool replace(value_type const& value) requires std::is_constructible_v<value_type, value_type const&> {
    // value may be a reference to the very node being erased.
    value_type owned(value);
    bool replaced = remove(value);
    emplace(transfer_reference(owned));
    return replaced;
  }

  bool replace(value_type&& value) requires std::is_constructible_v<value_type, value_type&&> {
    auto position = m_map.find(value);
    auto node = position == m_map.end() ? m_order.end() : position->second;
    value_type owned(std::move(value));
    bool replaced = position != m_map.end();
    if (replaced) {
      m_map.erase(position);
      m_order.erase(node);
    }
    emplace(std::move(owned));
    return replaced;
  }

  bool add_back(value_type const& value) requires std::is_constructible_v<value_type, value_type const&> { return add_position(value, false); }
  bool add_back(value_type&& value) requires std::is_constructible_v<value_type, value_type&&> { return add_position(std::move(value), false); }
  bool add_front(value_type const& value) requires std::is_constructible_v<value_type, value_type const&> { return add_position(value, true); }
  bool add_front(value_type&& value) requires std::is_constructible_v<value_type, value_type&&> { return add_position(std::move(value), true); }

  template <typename Range>
    requires ordered_set_source<Range, value_type>
  void insert_range(Range&& range) {
    if constexpr (std::same_as<std::remove_cvref_t<Range>, OrderedSetWrapper>) {
      if (std::addressof(range) == this)
        return;
    }
    auto it = std::ranges::begin(range);
    auto end = std::ranges::end(range);
    // As with sequence append_range, supported overlap is identified by the
    // underlying forward iterator/base, never by evaluating a projection.
    // Opaque callback-produced borrowing and single-pass sources must not
    // overlap this set.
    if constexpr (std::ranges::forward_range<Range>) {
      if (it != end && iterator_aliases(it)) {
        List<value_type> snapshot;
        if constexpr (std::ranges::sized_range<Range>)
          snapshot.reserve(std::ranges::size(range));
        for (; it != end; ++it) {
          if constexpr (std::is_rvalue_reference_v<Range&&>)
            snapshot.emplace_back(std::ranges::iter_move(it));
          else
            snapshot.emplace_back(*it);
        }
        for (auto& value : snapshot)
          insert_reference(transfer_reference(value));
        return;
      }
    }
    for (; it != end; ++it) {
      if constexpr (std::is_rvalue_reference_v<Range&&>)
        insert_reference(std::ranges::iter_move(it));
      else
        insert_reference(*it);
    }
  }

  iterator move_to_front(iterator position) {
    m_order.splice(m_order.begin(), m_order, position);
    return m_order.begin();
  }

  iterator move_to_back(iterator position) {
    if (std::next(position) != m_order.end())
      m_order.splice(m_order.end(), m_order, position);
    return position;
  }

  bool remove(value_type const& value) {
    auto position = m_map.find(value);
    if (position == m_map.end())
      return false;
    auto node = position->second;
    m_map.erase(position);
    m_order.erase(node);
    return true;
  }

  template <std::ranges::input_range Range>
    requires requires(MapType& map, std::ranges::range_reference_t<Range> value) { map.find(value); }
  void erase_all(Range&& range) {
    if constexpr (std::same_as<std::remove_cvref_t<Range>, OrderedSetWrapper>) {
      if (std::addressof(range) == this) {
        clear();
        return;
      }
    }
    // Eagerly capture unique live nodes, not lazy borrowed values. This also
    // supports move-only keys and repeated references in aliased views.
    auto selected = empty_map(m_map, get_allocator());
    List<iterator> nodes;
    for (auto&& value : range) {
      auto found = m_map.find(value);
      if (found != m_map.end()) {
        auto node = found->second;
        if (selected.insert(typename MapType::value_type(const_reference(*node), node)).second)
          nodes.emplace_back(node);
      }
    }
    selected.clear();
    for (auto node : nodes)
      remove(*node);
  }

  void clear() {
    m_map.clear();
    m_order.clear();
  }

  [[nodiscard]] std::expected<const_reference, StarException> first() const& {
    if (empty())
      return std::unexpected(SetException("first() called on empty OrderedSet"));
    return const_reference(*begin());
  }
  std::expected<const_reference, StarException> first() && = delete;
  std::expected<const_reference, StarException> first() const&& = delete;

  [[nodiscard]] std::expected<const_reference, StarException> last() const& {
    if (empty())
      return std::unexpected(SetException("last() called on empty OrderedSet"));
    return const_reference(*std::prev(end()));
  }
  std::expected<const_reference, StarException> last() && = delete;
  std::expected<const_reference, StarException> last() const&& = delete;

  [[nodiscard]] std::expected<void, StarException> remove_first() {
    if (empty())
      return std::unexpected(SetException("OrderedSet::removeFirst() called on empty OrderedSet"));
    remove(*begin());
    return {};
  }

  [[nodiscard]] std::expected<void, StarException> remove_last() {
    if (empty())
      return std::unexpected(SetException("OrderedSet::removeLast() called on empty OrderedSet"));
    remove(*std::prev(end()));
    return {};
  }

  [[nodiscard]] std::expected<value_type, StarException> take_first()
    requires (std::is_constructible_v<value_type, value_type&&> || std::is_constructible_v<value_type, value_type const&>)
  {
    if (empty())
      return std::unexpected(SetException("OrderedSet::takeFirst() called on empty OrderedSet"));
    return take_node(m_order.begin());
  }

  [[nodiscard]] std::expected<value_type, StarException> take_last()
    requires (std::is_constructible_v<value_type, value_type&&> || std::is_constructible_v<value_type, value_type const&>)
  {
    if (empty())
      return std::unexpected(SetException("OrderedSet::takeLast() called on empty OrderedSet"));
    return take_node(std::prev(m_order.end()));
  }

  template <typename Compare>
    requires requires(Compare& compare, value_type const& left, value_type const& right) {
      static_cast<bool>(std::invoke(compare, left, right));
    }
  void sort(Compare&& compare) {
    m_order.sort([&](auto const& left, auto const& right) { return static_cast<bool>(std::invoke(compare, left, right)); });
  }

  void sort() requires requires(value_type const& left, value_type const& right) { static_cast<bool>(left < right); } {
    sort([](auto const& left, auto const& right) { return static_cast<bool>(left < right); });
  }

  bool empty() const { return m_order.empty(); }
  std::size_t size() const { return m_order.size(); }
  const_iterator begin() const { return m_order.begin(); }
  const_iterator end() const { return m_order.end(); }
  const_reverse_iterator rbegin() const { return m_order.rbegin(); }
  const_reverse_iterator rend() const { return m_order.rend(); }

  std::optional<std::size_t> index_of(value_type const& value) const {
    auto position = m_map.find(value);
    if (position == m_map.end())
      return {};
    return std::distance(begin(), position->second);
  }

  [[nodiscard]] std::expected<const_reference, StarException> at(std::size_t index) const& {
    if (index >= size())
      return std::unexpected(SetException("OrderedSet::at() index out of range"));
    return const_reference(*std::next(begin(), index));
  }
  std::expected<const_reference, StarException> at(std::size_t) && = delete;
  std::expected<const_reference, StarException> at(std::size_t) const&& = delete;

  OrderedSetWrapper intersection(OrderedSetWrapper const& set) const
    requires std::is_constructible_v<value_type, value_type const&>
  {
    auto result = empty_like();
    for (auto const& value : set) {
      if (contains(value))
        result.add(value);
    }
    return result;
  }

  OrderedSetWrapper difference(OrderedSetWrapper const& set) const
    requires std::is_constructible_v<value_type, value_type const&>
  {
    auto result = empty_like();
    for (auto const& value : *this) {
      if (!set.contains(value))
        result.add(value);
    }
    return result;
  }

private:
  void reserve_index(std::size_t count) {
    if constexpr (requires { m_map.reserve(count); })
      m_map.reserve(count);
  }

  template <std::input_iterator Iterator, std::sentinel_for<Iterator> Sentinel>
  void reserve_index(Iterator const& first, Sentinel const& last) {
    if constexpr (requires { m_map.reserve(std::size_t{}); }) {
      if constexpr (std::sized_sentinel_for<Sentinel, Iterator>)
        reserve_index(static_cast<std::size_t>(last - first));
      else if constexpr (std::forward_iterator<Iterator>)
        reserve_index(static_cast<std::size_t>(std::ranges::distance(first, last)));
    }
  }

  template <std::ranges::input_range Range>
  void reserve_index(Range& range) {
    if constexpr (requires { m_map.reserve(std::size_t{}); }) {
      if constexpr (std::ranges::sized_range<Range>)
        reserve_index(std::ranges::size(range));
      else if constexpr (std::ranges::forward_range<Range>)
        reserve_index(std::ranges::begin(range), std::ranges::end(range));
    }
  }

  static OrderType make_order(allocator_type const& alloc) {
    return OrderType(std::list<value_type, typename OrderType::allocator_type>(typename OrderType::allocator_type(alloc)));
  }

  static MapType make_map(allocator_type const& alloc, Args const&... policies) {
    if constexpr (requires(MapType const& map) { map.key_comp(); })
      return MapType(policies..., typename MapType::allocator_type(alloc));
    else
      return MapType(0, policies..., typename MapType::allocator_type(alloc));
  }

  static MapType empty_map(MapType const& map, allocator_type const& alloc) {
    if constexpr (requires { map.key_comp(); })
      return make_map(alloc, map.key_comp());
    else
      return make_map(alloc, map.hash_function(), map.key_eq());
  }

  static MapType move_map(OrderedSetWrapper& set, allocator_type const& alloc) {
    if (alloc == set.get_allocator())
      return MapType(std::move(set.m_map), typename MapType::allocator_type(alloc));
    return empty_map(set.m_map, alloc);
  }

  OrderedSetWrapper empty_like() const {
    if constexpr (requires { m_map.key_comp(); })
      return OrderedSetWrapper(m_map.key_comp(), get_allocator());
    else
      return OrderedSetWrapper(m_map.hash_function(), m_map.key_eq(), get_allocator());
  }

  void adopt(OrderedSetWrapper&& replacement) {
    // Reconstruct members rather than assigning containers: allocator copy and
    // move propagation traits need not agree, even after rebinding.
    std::destroy_at(std::addressof(m_map));
    std::destroy_at(std::addressof(m_order));
    std::construct_at(std::addressof(m_order), std::move(replacement.m_order));
    std::construct_at(std::addressof(m_map), std::move(replacement.m_map));
  }

  template <typename Iterator>
  bool iterator_aliases(Iterator const& position) const {
    if constexpr (std::same_as<Iterator, iterator> || std::same_as<Iterator, typename OrderType::iterator>) {
      for (auto node = begin(); node != end(); ++node) {
        if (node == position)
          return true;
      }
      return position == end();
    } else if constexpr (requires { position.base(); }) {
      using Base = std::remove_cvref_t<decltype(position.base())>;
      if constexpr (std::input_or_output_iterator<Base> && !std::same_as<Base, Iterator>)
        return iterator_aliases(position.base());
    }
    return false;
  }

  template <typename T>
  std::pair<iterator, bool> insert_value(T&& value, iterator position) {
    auto found = m_map.find(value);
    if (found != m_map.end())
      return {found->second, false};
    auto node = m_order.emplace(position, std::forward<T>(value));
    m_map.insert(typename MapType::value_type(const_reference(*node), node));
    return {node, true};
  }

  template <typename T>
  bool add_position(T&& value, bool front) {
    auto [node, inserted] = insert_value(std::forward<T>(value), front ? m_order.begin() : m_order.end());
    if (!inserted) {
      if (front)
        move_to_front(node);
      else
        move_to_back(node);
    }
    return inserted;
  }

  static decltype(auto) transfer_reference(value_type& value) {
    if constexpr (std::is_constructible_v<value_type, value_type&&>)
      return std::move(value);
    else
      return std::as_const(value);
  }

  template <typename T>
  std::pair<iterator, bool> insert_reference(T&& value) {
    if constexpr (std::same_as<std::remove_cvref_t<T>, value_type>)
      return insert(std::forward<T>(value));
    else
      return emplace(std::forward<T>(value));
  }

  std::expected<value_type, StarException> take_node(typename OrderType::iterator node) {
    m_map.erase(const_reference(*node));
    std::expected<value_type, StarException> result(std::in_place, transfer_reference(*node));
    m_order.erase(node);
    return result;
  }

  OrderType m_order;
  MapType m_map;
};

template <typename Value, typename Compare = std::less<Value>, typename Allocator = std::allocator<Value>>
using OrderedSet = OrderedSetWrapper<std::map, Value, Allocator, Compare>;

template <typename Value, typename Hash = Star::hash<Value>, typename Equals = std::equal_to<Value>, typename Allocator = std::allocator<Value>>
using OrderedHashSet = OrderedSetWrapper<FlatHashMap, Value, Allocator, Hash, Equals>;

template <template <typename...> class Map, typename Value, typename Allocator, typename... Args>
std::ostream& operator<<(std::ostream& os, OrderedSetWrapper<Map, Value, Allocator, Args...> const& set) {
  os << "(";
  for (auto position = set.begin(); position != set.end(); ++position) {
    if (position != set.begin())
      os << ", ";
    if constexpr (requires { outputOptional(*position); })
      os << outputOptional(*position);
    else
      os << *position;
  }
  os << ")";
  return os;
}

}

export module star.ordered_set;

export template <template <typename...> class Map, typename Value, typename Allocator, typename... Args>
struct std::formatter<Star::OrderedSetWrapper<Map, Value, Allocator, Args...>> : Star::StreamFormatter {};

export namespace Star {
  using ::Star::OrderedSetWrapper;
  using ::Star::OrderedSet;
  using ::Star::OrderedHashSet;
  using ::Star::operator<<;
}
