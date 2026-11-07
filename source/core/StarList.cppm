module;
import star.memory;
import star.stream_formatter;
import star.hash;

import std;

import star.static_vector;
import star.small_vector;
import star.pythonic;
import star.optional;
import star.format;

import star.algorithm;
namespace Star {

// Only explicitly opted-in owning sequences participate in generic codecs.
template <typename T>
concept SequenceContainer = requires { typename std::remove_cvref_t<T>::SequenceContainerTag; }
    && std::ranges::range<T>;

namespace SequenceAlgorithms {

template <std::ranges::input_range Range, std::ranges::input_range Container>
auto appendAll(Range& list, Container&& source)
    -> decltype(list.push_back(std::declval<std::ranges::range_value_t<Range>>())) {
  using AppendResult = decltype(list.push_back(std::declval<std::ranges::range_value_t<Range>>()));
  for (auto& e : source) {
    if constexpr (std::is_void_v<AppendResult>) {
      if constexpr (std::is_rvalue_reference_v<Container&&>)
        list.push_back(std::move(e));
      else
        list.push_back(e);
    } else {
      auto added = [&] {
        if constexpr (std::is_rvalue_reference_v<Container&&>)
          return list.push_back(std::move(e));
        else
          return list.push_back(e);
      }();
      if (!added)
        return std::unexpected(std::move(added.error()));
    }
  }
  if constexpr (!std::is_void_v<AppendResult>)
    return {};
}

template <std::ranges::forward_range Range>
std::size_t remove(Range& list, std::ranges::range_value_t<Range> const& e) {
  // Erase immediately: e may alias an element, and subsequent comparisons can
  // observe each erase's moves and destructions.
  std::size_t removed = 0;
  auto i = list.begin();
  while (i != list.end()) {
    if (*i == e) {
      ++removed;
      i = list.erase(i);
    } else {
      ++i;
    }
  }
  return removed;
}

template <std::ranges::forward_range Range, typename Filter>
auto filter(Range& list, Filter&& filter) {
  using reference = std::ranges::range_reference_t<Range>;
  using Invocation = std::remove_cvref_t<std::invoke_result_t<Filter&, reference>>;
  using Status = std::conditional_t<AlgorithmResultInfo<Invocation>::Fallible, std::expected<void, StarException>, void>;
  if constexpr (AlgorithmResultInfo<Invocation>::Fallible) {
    auto destination = list.begin();
    for (auto source = list.begin(); source != list.end(); ++source) {
      auto accepted = std::invoke(filter, *source);
      if (!accepted)
        return Status(std::unexpected(std::move(accepted.error())));
      if (static_cast<bool>(*accepted)) {
        if (destination != source)
          *destination = std::move(*source);
        ++destination;
      }
    }
    list.erase(destination, list.end());
    return Status();
  } else {
    auto tail = std::remove_if(list.begin(), list.end(), [&](auto&& value) {
      return !static_cast<bool>(std::invoke(filter, std::forward<decltype(value)>(value)));
    });
    list.erase(tail, list.end());
  }
}

template <std::ranges::forward_range Range, typename Function>
auto exec(Range& list, Function&& function) {
  using reference = std::ranges::range_reference_t<Range>;
  using Invocation = std::remove_cvref_t<std::invoke_result_t<Function&, reference>>;
  using Status = std::conditional_t<AlgorithmResultInfo<Invocation>::Fallible, std::expected<void, StarException>, void>;
  if constexpr (AlgorithmResultInfo<Invocation>::Fallible) {
    for (auto& e : list) {
      auto result = std::invoke(function, e);
      if (!result)
        return Status(std::unexpected(std::move(result.error())));
    }
    return Status();
  } else {
    std::ranges::for_each(list, std::ref(function));
  }
}

template <std::ranges::forward_range Range, typename Function>
auto transform(Range& list, Function&& function) {
  using reference = std::ranges::range_reference_t<Range>;
  using Invocation = std::remove_cvref_t<std::invoke_result_t<Function&, reference>>;
  using Status = std::conditional_t<AlgorithmResultInfo<Invocation>::Fallible, std::expected<void, StarException>, void>;
  if constexpr (AlgorithmResultInfo<Invocation>::Fallible) {
    for (auto& e : list) {
      auto result = std::invoke(function, e);
      if (!result)
        return Status(std::unexpected(std::move(result.error())));
      e = std::move(*result);
    }
    return Status();
  } else {
    std::ranges::for_each(list, [&](auto& value) {
      value = std::invoke(function, value);
    });
  }
}

template <std::ranges::forward_range Range, typename Function>
auto any(Range& list, Function&& function) {
  using const_reference = std::ranges::range_reference_t<Range>;
  using Invocation = std::remove_cvref_t<std::invoke_result_t<Function const&, const_reference>>;
  using Result = std::conditional_t<AlgorithmResultInfo<Invocation>::Fallible, std::expected<bool, StarException>, bool>;
  if constexpr (AlgorithmResultInfo<Invocation>::Fallible) {
    for (auto const& e : list) {
      decltype(auto) accepted = std::invoke(std::as_const(function), e);
      if (!accepted)
        return Result(std::unexpected(std::move(accepted.error())));
      if (static_cast<bool>(*accepted))
        return Result(true);
    }
    return Result(false);
  } else {
    return std::ranges::any_of(list, [&](auto const& value) {
      return static_cast<bool>(std::invoke(std::as_const(function), value));
    });
  }
}

template <std::ranges::forward_range Range, typename Function>
auto all(Range& list, Function&& function) {
  using const_reference = std::ranges::range_reference_t<Range>;
  using Invocation = std::remove_cvref_t<std::invoke_result_t<Function const&, const_reference>>;
  using Result = std::conditional_t<AlgorithmResultInfo<Invocation>::Fallible, std::expected<bool, StarException>, bool>;
  if constexpr (AlgorithmResultInfo<Invocation>::Fallible) {
    for (auto const& e : list) {
      decltype(auto) accepted = std::invoke(std::as_const(function), e);
      if (!accepted)
        return Result(std::unexpected(std::move(accepted.error())));
      if (!static_cast<bool>(*accepted))
        return Result(false);
    }
    return Result(true);
  } else {
    return std::ranges::all_of(list, [&](auto const& value) {
      return static_cast<bool>(std::invoke(std::as_const(function), value));
    });
  }
}

}

template <typename List>
class ListHasher {
public:
  std::size_t operator()(List const& l) const;

private:
  hash<typename List::value_type> elemHasher;
};

template <typename Element, typename Allocator = std::allocator<Element>>
class List {
  using Storage = std::vector<Element, Allocator>;

public:
  using SequenceContainerTag = void;
  using value_type = Element;
  using reference = typename Storage::reference;
  using const_reference = typename Storage::const_reference;
  using iterator = typename Storage::iterator;
  using const_iterator = typename Storage::const_iterator;
  using reverse_iterator = typename Storage::reverse_iterator;
  using const_reverse_iterator = typename Storage::const_reverse_iterator;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using AppendResult = decltype(std::declval<Storage&>().push_back(std::declval<value_type>()));

  List() = default;
  List(Storage const& storage) : m_storage(storage) {}
  List(Storage&& storage) : m_storage(std::move(storage)) {}
  explicit List(std::size_t length, const_reference value = value_type()) : m_storage(length, value) {}
  explicit List(std::span<value_type const> values) : m_storage(values.begin(), values.end()) {}
  template <std::input_iterator Iterator>
  List(Iterator first, Iterator last) : m_storage(first, last) {}
  List(std::initializer_list<value_type> values) : m_storage(values) {}

  iterator begin() { return m_storage.begin(); }
  const_iterator begin() const { return m_storage.begin(); }
  const_iterator cbegin() const { return m_storage.begin(); }
  iterator end() { return m_storage.end(); }
  const_iterator end() const { return m_storage.end(); }
  const_iterator cend() const { return m_storage.end(); }
  reverse_iterator rbegin() { return m_storage.rbegin(); }
  const_reverse_iterator rbegin() const { return m_storage.rbegin(); }
  const_reverse_iterator crbegin() const { return m_storage.rbegin(); }
  reverse_iterator rend() { return m_storage.rend(); }
  const_reverse_iterator rend() const { return m_storage.rend(); }
  const_reverse_iterator crend() const { return m_storage.rend(); }
  std::size_t size() const { return m_storage.size(); }
  bool empty() const { return m_storage.empty(); }
  void clear() { m_storage.clear(); }
  reference front() { return *begin(); }
  const_reference front() const { return *begin(); }
  reference back() { return *std::prev(end()); }
  const_reference back() const { return *std::prev(end()); }
  [[nodiscard]] AppendResult push_back(const_reference value) { return m_storage.push_back(value); }
  [[nodiscard]] AppendResult push_back(value_type&& value) { return m_storage.push_back(std::move(value)); }
  decltype(auto) pop_back() { return m_storage.pop_back(); }
  template <typename... Args>
  decltype(auto) emplace_back(Args&&... args) { return m_storage.emplace_back(std::forward<Args>(args)...); }
  decltype(auto) resize(std::size_t size) { return m_storage.resize(size); }
  decltype(auto) resize(std::size_t size, const_reference value) { return m_storage.resize(size, value); }
  decltype(auto) insert(iterator pos, const_reference value) { return m_storage.insert(pos, value); }
  decltype(auto) insert(iterator pos, value_type&& value) { return m_storage.insert(pos, std::move(value)); }
  decltype(auto) insert(iterator pos, std::initializer_list<value_type> values) { return m_storage.insert(pos, values); }
  template <typename Position, typename... Args>
    requires requires(Storage& storage, Position pos, Args&&... args) { storage.insert(pos, std::forward<Args>(args)...); }
  decltype(auto) insert(Position pos, Args&&... args) { return m_storage.insert(pos, std::forward<Args>(args)...); }
  template <typename... Args>
  decltype(auto) erase(Args&&... args) { return m_storage.erase(std::forward<Args>(args)...); }
  template <typename... Args>
  decltype(auto) emplace(iterator pos, Args&&... args) { return m_storage.emplace(pos, std::forward<Args>(args)...); }
  decltype(auto) assign(std::initializer_list<value_type> values) { return m_storage.assign(values); }
  template <typename... Args>
    requires requires(Storage& storage, Args&&... args) { storage.assign(std::forward<Args>(args)...); }
  decltype(auto) assign(Args&&... args) { return m_storage.assign(std::forward<Args>(args)...); }
  template <typename... Args>
    requires requires(Storage& storage, Args&&... args) { storage.reserve(std::forward<Args>(args)...); }
  decltype(auto) reserve(Args&&... args) { return m_storage.reserve(std::forward<Args>(args)...); }
  template <typename... Args>
    requires requires(Storage& storage, Args&&... args) { storage.shrink_to_fit(std::forward<Args>(args)...); }
  decltype(auto) shrink_to_fit(Args&&... args) { return m_storage.shrink_to_fit(std::forward<Args>(args)...); }
  auto capacity() const requires requires(Storage const& storage) { storage.capacity(); } { return m_storage.capacity(); }
  auto max_size() const requires requires(Storage const& storage) { storage.max_size(); } { return m_storage.max_size(); }
  auto get_allocator() const requires requires(Storage const& storage) { storage.get_allocator(); } { return m_storage.get_allocator(); }
  value_type* data() { return m_storage.data(); }
  value_type const* data() const { return m_storage.data(); }
  void swap(List& other) { std::swap(m_storage, other.m_storage); }
  bool operator==(List const& other) const { return m_storage == other.m_storage; }
  bool operator<(List const& other) const { return m_storage < other.m_storage; }
  bool operator>(List const& other) const { return other < *this; }
  bool operator<=(List const& other) const { return !(other < *this); }
  bool operator>=(List const& other) const { return !(*this < other); }

  template <typename Container>
  static List from(Container const& c);



  // Pointer to contiguous storage, returns nullptr if empty
  value_type* ptr();
  value_type const* ptr() const;

  List slice(SliceIndex a = SliceIndex(), SliceIndex b = SliceIndex(), int i = 1) const;

  template <typename Comparator>
  List sorted(Comparator&& comparator) const;
  List sorted() const;

  template <typename Function>
  [[nodiscard]]
  auto transformed(Function&& function);

  template <typename Function>
  [[nodiscard]]
  auto transformed(Function&& function) const;

  auto append(value_type e) -> AppendResult {
    return m_storage.push_back(std::move(e));
  }

  template <typename Container>
  auto appendAll(Container&& list) -> AppendResult {
    return SequenceAlgorithms::appendAll(*this, std::forward<Container>(list));
  }

  template <class... Args>
  auto emplaceAppend(Args&&... args) -> reference
      requires std::is_void_v<AppendResult> {
    m_storage.emplace_back(std::forward<Args>(args)...);
    return *std::prev(m_storage.end());
  }

  template <class... Args>
  auto emplaceAppend(Args&&... args) -> std::expected<std::reference_wrapper<value_type>, StarException>
      requires (!std::is_void_v<AppendResult>) {
    auto added = m_storage.emplace_back(std::forward<Args>(args)...);
    if (!added)
      return std::unexpected(std::move(added.error()));
    return std::reference_wrapper<value_type>(*std::prev(m_storage.end()));
  }

  auto first() -> std::expected<std::reference_wrapper<value_type>, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("first() called on empty list"));
    return std::reference_wrapper<value_type>(*m_storage.begin());
  }

  auto first() const -> std::expected<std::reference_wrapper<value_type const>, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("first() called on empty list"));
    return std::reference_wrapper<value_type const>(*m_storage.begin());
  }

  auto last() -> std::expected<std::reference_wrapper<value_type>, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("last() called on empty list"));
    return std::reference_wrapper<value_type>(*std::prev(m_storage.end()));
  }

  auto last() const -> std::expected<std::reference_wrapper<value_type const>, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("last() called on empty list"));
    return std::reference_wrapper<value_type const>(*std::prev(m_storage.end()));
  }

  auto maybeFirst() -> std::optional<value_type> {
    if (m_storage.empty())
      return {};
    return std::optional<value_type>(std::in_place, *m_storage.begin());
  }

  auto maybeLast() -> std::optional<value_type> {
    if (m_storage.empty())
      return {};
    return std::optional<value_type>(std::in_place, *std::prev(m_storage.end()));
  }

  std::expected<void, StarException> removeLast() {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("removeLast() called on empty list"));
    m_storage.erase(std::prev(m_storage.end()));
    return {};
  }

  auto takeLast() -> std::expected<value_type, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("takeLast() called on empty list"));
    value_type e = std::move(*std::prev(m_storage.end()));
    m_storage.erase(std::prev(m_storage.end()));
    return e;
  }

  auto maybeTakeLast() -> std::optional<value_type> {
    if (m_storage.empty())
      return {};
    value_type e = std::move(*std::prev(m_storage.end()));
    m_storage.erase(std::prev(m_storage.end()));
    return std::optional<value_type>(std::in_place, std::move(e));
  }

  void limitSizeBack(std::size_t maximumSize) {
    while (m_storage.size() > maximumSize)
      m_storage.erase(std::prev(m_storage.end()));
  }

  std::size_t count() const {
    return m_storage.size();
  }

  bool contains(const_reference e) const {
    return std::ranges::contains(*this, e);
  }

  std::size_t remove(const_reference e) {
    return SequenceAlgorithms::remove(*this, e);
  }

  template <typename Filter>
  auto filter(Filter&& filter) {
    return SequenceAlgorithms::filter(*this, std::forward<Filter>(filter));
  }

  template <typename Self, typename Filter>
  auto filtered(this Self const& self, Filter&& filter) {
    Self list(self);
    if constexpr (std::is_void_v<decltype(list.filter(std::forward<Filter>(filter)))>) {
      list.filter(std::forward<Filter>(filter));
      return list;
    } else {
      auto filtered = list.filter(std::forward<Filter>(filter));
      if (!filtered)
        return std::expected<Self, StarException>(std::unexpected(std::move(filtered.error())));
      return std::expected<Self, StarException>(std::move(list));
    }
  }

  template <typename Comparator>
  auto insertSorted(value_type e, Comparator&& comparator) -> AppendResult {
    static_assert(std::is_invocable_r_v<bool, Comparator&, const_reference, const_reference>,
        "Sorted insertion requires a nonfallible bool comparator");
    auto i = std::upper_bound(m_storage.begin(), m_storage.end(), e, std::forward<Comparator>(comparator));
    if constexpr (std::is_void_v<AppendResult>) {
      m_storage.insert(i, std::move(e));
    } else {
      auto added = m_storage.insert(i, std::move(e));
      if (!added)
        return std::unexpected(std::move(added.error()));
      return {};
    }
  }

  auto insertSorted(value_type e) -> AppendResult {
    return insertSorted(std::move(e), std::less<value_type>{});
  }

  template <typename Function>
  auto exec(Function&& function) {
    return SequenceAlgorithms::exec(*this, std::forward<Function>(function));
  }

  template <typename Function>
  auto exec(Function&& function) const {
    return SequenceAlgorithms::exec(*this, std::forward<Function>(function));
  }

  template <typename Function>
  auto transform(Function&& function) {
    return SequenceAlgorithms::transform(*this, std::forward<Function>(function));
  }

  template <typename Function>
  auto any(Function&& function) const {
    return SequenceAlgorithms::any(*this, std::forward<Function>(function));
  }

  auto any() const {
    return any([](auto const& value) -> auto const& { return value; });
  }

  template <typename Function>
  auto all(Function&& function) const {
    return SequenceAlgorithms::all(*this, std::forward<Function>(function));
  }

  auto all() const {
    return all([](auto const& value) -> auto const& { return value; });
  }

  template <typename Comparator>
  void sort(Comparator&& comparator) {
    static_assert(std::is_invocable_r_v<bool, Comparator&, reference, reference>,
        "Sorting requires a nonfallible bool comparator; precompute fallible keys first");
    std::sort(m_storage.begin(), m_storage.end(), std::forward<Comparator>(comparator));
  }

  void sort() {
    std::sort(m_storage.begin(), m_storage.end(), std::less<value_type>());
  }

  void reverse() {
    std::reverse(m_storage.begin(), m_storage.end());
  }

  std::size_t indexOf(const_reference e, std::size_t from = 0) const {
    if (from >= m_storage.size())
      return NPos;
    auto end = m_storage.end();
    auto found = std::find(m_storage.begin() + static_cast<difference_type>(from), end, e);
    return found == end ? NPos : static_cast<std::size_t>(std::distance(m_storage.begin(), found));
  }

  std::size_t lastIndexOf(const_reference e, std::size_t til = NPos) const {
    std::size_t index = NPos;
    std::size_t end = std::min(m_storage.size(), til);
    for (std::size_t i = 0; i < end; ++i) {
      if (operator[](i) == e)
        index = i;
    }
    return index;
  }

  auto at(std::size_t n) const -> std::expected<std::reference_wrapper<value_type const>, StarException> {
    if (n >= m_storage.size())
      return std::unexpected(OutOfRangeException(strf("out of range list::at({})", n)));
    return std::reference_wrapper<value_type const>(operator[](n));
  }

  auto at(std::size_t n) -> std::expected<std::reference_wrapper<value_type>, StarException> {
    if (n >= m_storage.size())
      return std::unexpected(OutOfRangeException(strf("out of range list::at({})", n)));
    return std::reference_wrapper<value_type>(operator[](n));
  }

  auto operator[](std::size_t n) const -> const_reference {
    if constexpr (Star::DebugEnabled) {
      Star::starAssertFailed((n < m_storage.size()));
    }
    return m_storage.operator[](n);
  }

  auto operator[](std::size_t n) -> reference {
    if constexpr (Star::DebugEnabled) {
      Star::starAssertFailed((n < m_storage.size()));
    }
    return m_storage.operator[](n);
  }

  auto get(std::size_t n, value_type def = value_type()) const -> value_type {
    if (n >= m_storage.size())
      return def;
    return operator[](n);
  }

  auto takeAt(std::size_t i) -> std::expected<value_type, StarException> {
    auto element = at(i);
    if (!element)
      return std::unexpected(std::move(element.error()));
    value_type e = std::move(element->get());
    m_storage.erase(m_storage.begin() + i);
    return e;
  }

  auto wrap(std::size_t n) const -> std::expected<std::reference_wrapper<value_type const>, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("wrap() called on empty list"));
    return std::reference_wrapper<value_type const>(operator[](n % m_storage.size()));
  }

  auto wrap(std::size_t n) -> std::expected<std::reference_wrapper<value_type>, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("wrap() called on empty list"));
    return std::reference_wrapper<value_type>(operator[](n % m_storage.size()));
  }

  auto wrap(std::size_t n, value_type def) const -> value_type {
    if (m_storage.empty())
      return def;
    else
      return operator[](n % m_storage.size());
  }

  void eraseAt(std::size_t i) {
    if constexpr (Star::DebugEnabled) {
      Star::starAssertFailed((i < m_storage.size()));
    }
    m_storage.erase(m_storage.begin() + i);
  }

  void eraseAt(std::size_t b, std::size_t e) {
    if constexpr (Star::DebugEnabled) {
      Star::starAssertFailed((b < m_storage.size() && e <= m_storage.size()));
    }
    m_storage.erase(m_storage.begin() + b, m_storage.begin() + e);
  }

  auto insertAt(std::size_t pos, value_type e) -> AppendResult {
    if constexpr (Star::DebugEnabled)
      Star::starAssertFailed((pos <= m_storage.size()));
    if constexpr (std::is_void_v<AppendResult>) {
      m_storage.insert(m_storage.begin() + pos, std::move(e));
    } else {
      auto added = m_storage.insert(m_storage.begin() + pos, std::move(e));
      if (!added)
        return std::unexpected(std::move(added.error()));
      return {};
    }
  }

  template <typename Container>
  auto insertAllAt(std::size_t pos, Container const& l) -> AppendResult {
    if constexpr (Star::DebugEnabled)
      Star::starAssertFailed((pos <= m_storage.size()));
    if constexpr (std::is_void_v<AppendResult>) {
      m_storage.insert(m_storage.begin() + pos, l.begin(), l.end());
    } else {
      auto added = m_storage.insert(m_storage.begin() + pos, l.begin(), l.end());
      if (!added)
        return std::unexpected(std::move(added.error()));
      return {};
    }
  }

  auto set(std::size_t pos, value_type e) -> AppendResult {
    if constexpr (std::is_void_v<AppendResult>) {
      if (pos >= m_storage.size())
        m_storage.resize(pos + 1);
      operator[](pos) = std::move(e);
    } else {
      if (pos == std::numeric_limits<std::size_t>::max())
        return std::unexpected(OutOfRangeException("List::set index overflows size"));
      if (pos >= m_storage.size()) {
        auto resized = m_storage.resize(pos + 1);
        if (!resized)
          return std::unexpected(std::move(resized.error()));
      }
      operator[](pos) = std::move(e);
      return {};
    }
  }

  void swap(std::size_t i, std::size_t j) {
    std::swap(operator[](i), operator[](j));
  }

  std::expected<void, StarException> move(std::size_t from, std::size_t to) {
    if (from >= m_storage.size() || to >= m_storage.size())
      return std::unexpected(OutOfRangeException("List::move index out of range"));
    if (from < to)
      std::rotate(m_storage.begin() + from, m_storage.begin() + from + 1, m_storage.begin() + to + 1);
    else if (to < from)
      std::rotate(m_storage.begin() + to, m_storage.begin() + from, m_storage.begin() + from + 1);
    return {};
  }

private:
  Storage m_storage;
};

template <typename Element, std::size_t MaxSize>
class StaticList {
  using Storage = StaticVector<Element, MaxSize>;

public:
  using SequenceContainerTag = void;
  using value_type = Element;
  using reference = typename Storage::reference;
  using const_reference = typename Storage::const_reference;
  using iterator = typename Storage::iterator;
  using const_iterator = typename Storage::const_iterator;
  using reverse_iterator = typename Storage::reverse_iterator;
  using const_reverse_iterator = typename Storage::const_reverse_iterator;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using AppendResult = decltype(std::declval<Storage&>().push_back(std::declval<value_type>()));

  StaticList() = default;
  StaticList(Storage const& storage) : m_storage(storage) {}
  StaticList(Storage&& storage) : m_storage(std::move(storage)) {}
  static constexpr std::size_t MaximumSize = MaxSize;

  iterator begin() { return m_storage.begin(); }
  const_iterator begin() const { return m_storage.begin(); }
  const_iterator cbegin() const { return m_storage.begin(); }
  iterator end() { return m_storage.end(); }
  const_iterator end() const { return m_storage.end(); }
  const_iterator cend() const { return m_storage.end(); }
  reverse_iterator rbegin() { return m_storage.rbegin(); }
  const_reverse_iterator rbegin() const { return m_storage.rbegin(); }
  const_reverse_iterator crbegin() const { return m_storage.rbegin(); }
  reverse_iterator rend() { return m_storage.rend(); }
  const_reverse_iterator rend() const { return m_storage.rend(); }
  const_reverse_iterator crend() const { return m_storage.rend(); }
  std::size_t size() const { return m_storage.size(); }
  bool empty() const { return m_storage.empty(); }
  void clear() { m_storage.clear(); }
  reference front() { return *begin(); }
  const_reference front() const { return *begin(); }
  reference back() { return *std::prev(end()); }
  const_reference back() const { return *std::prev(end()); }
  [[nodiscard]] AppendResult push_back(const_reference value) { return m_storage.push_back(value); }
  [[nodiscard]] AppendResult push_back(value_type&& value) { return m_storage.push_back(std::move(value)); }
  decltype(auto) pop_back() { return m_storage.pop_back(); }
  template <typename... Args>
  decltype(auto) emplace_back(Args&&... args) { return m_storage.emplace_back(std::forward<Args>(args)...); }
  decltype(auto) resize(std::size_t size) { return m_storage.resize(size); }
  decltype(auto) resize(std::size_t size, const_reference value) { return m_storage.resize(size, value); }
  decltype(auto) insert(iterator pos, const_reference value) { return m_storage.insert(pos, value); }
  decltype(auto) insert(iterator pos, value_type&& value) { return m_storage.insert(pos, std::move(value)); }
  decltype(auto) insert(iterator pos, std::initializer_list<value_type> values) { return m_storage.insert(pos, values); }
  template <typename Position, typename... Args>
    requires requires(Storage& storage, Position pos, Args&&... args) { storage.insert(pos, std::forward<Args>(args)...); }
  decltype(auto) insert(Position pos, Args&&... args) { return m_storage.insert(pos, std::forward<Args>(args)...); }
  template <typename... Args>
  decltype(auto) erase(Args&&... args) { return m_storage.erase(std::forward<Args>(args)...); }
  template <typename... Args>
  decltype(auto) emplace(iterator pos, Args&&... args) { return m_storage.emplace(pos, std::forward<Args>(args)...); }
  decltype(auto) assign(std::initializer_list<value_type> values) { return m_storage.assign(values); }
  template <typename... Args>
    requires requires(Storage& storage, Args&&... args) { storage.assign(std::forward<Args>(args)...); }
  decltype(auto) assign(Args&&... args) { return m_storage.assign(std::forward<Args>(args)...); }
  template <typename... Args>
    requires requires(Storage& storage, Args&&... args) { storage.reserve(std::forward<Args>(args)...); }
  decltype(auto) reserve(Args&&... args) { return m_storage.reserve(std::forward<Args>(args)...); }
  template <typename... Args>
    requires requires(Storage& storage, Args&&... args) { storage.shrink_to_fit(std::forward<Args>(args)...); }
  decltype(auto) shrink_to_fit(Args&&... args) { return m_storage.shrink_to_fit(std::forward<Args>(args)...); }
  auto capacity() const requires requires(Storage const& storage) { storage.capacity(); } { return m_storage.capacity(); }
  auto max_size() const requires requires(Storage const& storage) { storage.max_size(); } { return m_storage.max_size(); }
  auto get_allocator() const requires requires(Storage const& storage) { storage.get_allocator(); } { return m_storage.get_allocator(); }
  value_type* ptr() { return m_storage.ptr(); }
  value_type const* ptr() const { return m_storage.ptr(); }
  void swap(StaticList& other) { std::swap(m_storage, other.m_storage); }
  bool operator==(StaticList const& other) const { return m_storage == other.m_storage; }
  bool operator<(StaticList const& other) const { return m_storage < other.m_storage; }
  bool operator>(StaticList const& other) const { return other < *this; }
  bool operator<=(StaticList const& other) const { return !(other < *this); }
  bool operator>=(StaticList const& other) const { return !(*this < other); }

  template <typename Container>
  [[nodiscard]]
  static std::expected<StaticList, StarException> from(Container const& c);
  template <typename Iterator>
  [[nodiscard]]
  static std::expected<StaticList, StarException> create(Iterator first, Iterator last);
  [[nodiscard]]
  static std::expected<StaticList, StarException> create(std::initializer_list<Element> list);
  [[nodiscard]]
  static std::expected<StaticList, StarException> create(std::size_t size, Element const& value = Element());



  [[nodiscard]]
  std::expected<StaticList, StarException> slice(SliceIndex a = SliceIndex(), SliceIndex b = SliceIndex(), int i = 1) const;

  template <typename Comparator>
  StaticList sorted(Comparator&& comparator) const;
  StaticList sorted() const;

  template <typename Function>
  [[nodiscard]]
  auto transformed(Function&& function);

  template <typename Function>
  [[nodiscard]]
  auto transformed(Function&& function) const;

  auto append(value_type e) -> AppendResult {
    return m_storage.push_back(std::move(e));
  }

  template <typename Container>
  auto appendAll(Container&& list) -> AppendResult {
    return SequenceAlgorithms::appendAll(*this, std::forward<Container>(list));
  }

  template <class... Args>
  auto emplaceAppend(Args&&... args) -> reference
      requires std::is_void_v<AppendResult> {
    m_storage.emplace_back(std::forward<Args>(args)...);
    return *std::prev(m_storage.end());
  }

  template <class... Args>
  auto emplaceAppend(Args&&... args) -> std::expected<std::reference_wrapper<value_type>, StarException>
      requires (!std::is_void_v<AppendResult>) {
    auto added = m_storage.emplace_back(std::forward<Args>(args)...);
    if (!added)
      return std::unexpected(std::move(added.error()));
    return std::reference_wrapper<value_type>(*std::prev(m_storage.end()));
  }

  auto first() -> std::expected<std::reference_wrapper<value_type>, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("first() called on empty list"));
    return std::reference_wrapper<value_type>(*m_storage.begin());
  }

  auto first() const -> std::expected<std::reference_wrapper<value_type const>, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("first() called on empty list"));
    return std::reference_wrapper<value_type const>(*m_storage.begin());
  }

  auto last() -> std::expected<std::reference_wrapper<value_type>, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("last() called on empty list"));
    return std::reference_wrapper<value_type>(*std::prev(m_storage.end()));
  }

  auto last() const -> std::expected<std::reference_wrapper<value_type const>, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("last() called on empty list"));
    return std::reference_wrapper<value_type const>(*std::prev(m_storage.end()));
  }

  auto maybeFirst() -> std::optional<value_type> {
    if (m_storage.empty())
      return {};
    return std::optional<value_type>(std::in_place, *m_storage.begin());
  }

  auto maybeLast() -> std::optional<value_type> {
    if (m_storage.empty())
      return {};
    return std::optional<value_type>(std::in_place, *std::prev(m_storage.end()));
  }

  std::expected<void, StarException> removeLast() {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("removeLast() called on empty list"));
    m_storage.erase(std::prev(m_storage.end()));
    return {};
  }

  auto takeLast() -> std::expected<value_type, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("takeLast() called on empty list"));
    value_type e = std::move(*std::prev(m_storage.end()));
    m_storage.erase(std::prev(m_storage.end()));
    return e;
  }

  auto maybeTakeLast() -> std::optional<value_type> {
    if (m_storage.empty())
      return {};
    value_type e = std::move(*std::prev(m_storage.end()));
    m_storage.erase(std::prev(m_storage.end()));
    return std::optional<value_type>(std::in_place, std::move(e));
  }

  void limitSizeBack(std::size_t maximumSize) {
    while (m_storage.size() > maximumSize)
      m_storage.erase(std::prev(m_storage.end()));
  }

  std::size_t count() const {
    return m_storage.size();
  }

  bool contains(const_reference e) const {
    return std::ranges::contains(*this, e);
  }

  std::size_t remove(const_reference e) {
    return SequenceAlgorithms::remove(*this, e);
  }

  template <typename Filter>
  auto filter(Filter&& filter) {
    return SequenceAlgorithms::filter(*this, std::forward<Filter>(filter));
  }

  template <typename Self, typename Filter>
  auto filtered(this Self const& self, Filter&& filter) {
    Self list(self);
    if constexpr (std::is_void_v<decltype(list.filter(std::forward<Filter>(filter)))>) {
      list.filter(std::forward<Filter>(filter));
      return list;
    } else {
      auto filtered = list.filter(std::forward<Filter>(filter));
      if (!filtered)
        return std::expected<Self, StarException>(std::unexpected(std::move(filtered.error())));
      return std::expected<Self, StarException>(std::move(list));
    }
  }

  template <typename Comparator>
  auto insertSorted(value_type e, Comparator&& comparator) -> AppendResult {
    static_assert(std::is_invocable_r_v<bool, Comparator&, const_reference, const_reference>,
        "Sorted insertion requires a nonfallible bool comparator");
    auto i = std::upper_bound(m_storage.begin(), m_storage.end(), e, std::forward<Comparator>(comparator));
    if constexpr (std::is_void_v<AppendResult>) {
      m_storage.insert(i, std::move(e));
    } else {
      auto added = m_storage.insert(i, std::move(e));
      if (!added)
        return std::unexpected(std::move(added.error()));
      return {};
    }
  }

  auto insertSorted(value_type e) -> AppendResult {
    return insertSorted(std::move(e), std::less<value_type>{});
  }

  template <typename Function>
  auto exec(Function&& function) {
    return SequenceAlgorithms::exec(*this, std::forward<Function>(function));
  }

  template <typename Function>
  auto exec(Function&& function) const {
    return SequenceAlgorithms::exec(*this, std::forward<Function>(function));
  }

  template <typename Function>
  auto transform(Function&& function) {
    return SequenceAlgorithms::transform(*this, std::forward<Function>(function));
  }

  template <typename Function>
  auto any(Function&& function) const {
    return SequenceAlgorithms::any(*this, std::forward<Function>(function));
  }

  auto any() const {
    return any([](auto const& value) -> auto const& { return value; });
  }

  template <typename Function>
  auto all(Function&& function) const {
    return SequenceAlgorithms::all(*this, std::forward<Function>(function));
  }

  auto all() const {
    return all([](auto const& value) -> auto const& { return value; });
  }

  template <typename Comparator>
  void sort(Comparator&& comparator) {
    static_assert(std::is_invocable_r_v<bool, Comparator&, reference, reference>,
        "Sorting requires a nonfallible bool comparator; precompute fallible keys first");
    std::sort(m_storage.begin(), m_storage.end(), std::forward<Comparator>(comparator));
  }

  void sort() {
    std::sort(m_storage.begin(), m_storage.end(), std::less<value_type>());
  }

  void reverse() {
    std::reverse(m_storage.begin(), m_storage.end());
  }

  std::size_t indexOf(const_reference e, std::size_t from = 0) const {
    if (from >= m_storage.size())
      return NPos;
    auto end = m_storage.end();
    auto found = std::find(m_storage.begin() + static_cast<difference_type>(from), end, e);
    return found == end ? NPos : static_cast<std::size_t>(std::distance(m_storage.begin(), found));
  }

  std::size_t lastIndexOf(const_reference e, std::size_t til = NPos) const {
    std::size_t index = NPos;
    std::size_t end = std::min(m_storage.size(), til);
    for (std::size_t i = 0; i < end; ++i) {
      if (operator[](i) == e)
        index = i;
    }
    return index;
  }

  auto at(std::size_t n) const -> std::expected<std::reference_wrapper<value_type const>, StarException> {
    if (n >= m_storage.size())
      return std::unexpected(OutOfRangeException(strf("out of range list::at({})", n)));
    return std::reference_wrapper<value_type const>(operator[](n));
  }

  auto at(std::size_t n) -> std::expected<std::reference_wrapper<value_type>, StarException> {
    if (n >= m_storage.size())
      return std::unexpected(OutOfRangeException(strf("out of range list::at({})", n)));
    return std::reference_wrapper<value_type>(operator[](n));
  }

  auto operator[](std::size_t n) const -> const_reference {
    if constexpr (Star::DebugEnabled) {
      Star::starAssertFailed((n < m_storage.size()));
    }
    return m_storage.operator[](n);
  }

  auto operator[](std::size_t n) -> reference {
    if constexpr (Star::DebugEnabled) {
      Star::starAssertFailed((n < m_storage.size()));
    }
    return m_storage.operator[](n);
  }

  auto get(std::size_t n, value_type def = value_type()) const -> value_type {
    if (n >= m_storage.size())
      return def;
    return operator[](n);
  }

  auto takeAt(std::size_t i) -> std::expected<value_type, StarException> {
    auto element = at(i);
    if (!element)
      return std::unexpected(std::move(element.error()));
    value_type e = std::move(element->get());
    m_storage.erase(m_storage.begin() + i);
    return e;
  }

  auto wrap(std::size_t n) const -> std::expected<std::reference_wrapper<value_type const>, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("wrap() called on empty list"));
    return std::reference_wrapper<value_type const>(operator[](n % m_storage.size()));
  }

  auto wrap(std::size_t n) -> std::expected<std::reference_wrapper<value_type>, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("wrap() called on empty list"));
    return std::reference_wrapper<value_type>(operator[](n % m_storage.size()));
  }

  auto wrap(std::size_t n, value_type def) const -> value_type {
    if (m_storage.empty())
      return def;
    else
      return operator[](n % m_storage.size());
  }

  void eraseAt(std::size_t i) {
    if constexpr (Star::DebugEnabled) {
      Star::starAssertFailed((i < m_storage.size()));
    }
    m_storage.erase(m_storage.begin() + i);
  }

  void eraseAt(std::size_t b, std::size_t e) {
    if constexpr (Star::DebugEnabled) {
      Star::starAssertFailed((b < m_storage.size() && e <= m_storage.size()));
    }
    m_storage.erase(m_storage.begin() + b, m_storage.begin() + e);
  }

  auto insertAt(std::size_t pos, value_type e) -> AppendResult {
    if constexpr (Star::DebugEnabled)
      Star::starAssertFailed((pos <= m_storage.size()));
    if constexpr (std::is_void_v<AppendResult>) {
      m_storage.insert(m_storage.begin() + pos, std::move(e));
    } else {
      auto added = m_storage.insert(m_storage.begin() + pos, std::move(e));
      if (!added)
        return std::unexpected(std::move(added.error()));
      return {};
    }
  }

  template <typename Container>
  auto insertAllAt(std::size_t pos, Container const& l) -> AppendResult {
    if constexpr (Star::DebugEnabled)
      Star::starAssertFailed((pos <= m_storage.size()));
    if constexpr (std::is_void_v<AppendResult>) {
      m_storage.insert(m_storage.begin() + pos, l.begin(), l.end());
    } else {
      auto added = m_storage.insert(m_storage.begin() + pos, l.begin(), l.end());
      if (!added)
        return std::unexpected(std::move(added.error()));
      return {};
    }
  }

  auto set(std::size_t pos, value_type e) -> AppendResult {
    if constexpr (std::is_void_v<AppendResult>) {
      if (pos >= m_storage.size())
        m_storage.resize(pos + 1);
      operator[](pos) = std::move(e);
    } else {
      if (pos == std::numeric_limits<std::size_t>::max())
        return std::unexpected(OutOfRangeException("List::set index overflows size"));
      if (pos >= m_storage.size()) {
        auto resized = m_storage.resize(pos + 1);
        if (!resized)
          return std::unexpected(std::move(resized.error()));
      }
      operator[](pos) = std::move(e);
      return {};
    }
  }

  void swap(std::size_t i, std::size_t j) {
    std::swap(operator[](i), operator[](j));
  }

  std::expected<void, StarException> move(std::size_t from, std::size_t to) {
    if (from >= m_storage.size() || to >= m_storage.size())
      return std::unexpected(OutOfRangeException("List::move index out of range"));
    if (from < to)
      std::rotate(m_storage.begin() + from, m_storage.begin() + from + 1, m_storage.begin() + to + 1);
    else if (to < from)
      std::rotate(m_storage.begin() + to, m_storage.begin() + from, m_storage.begin() + from + 1);
    return {};
  }

private:
  Storage m_storage;
};

template <typename Element, std::size_t MaxStackSize>
class SmallList {
  using Storage = SmallVector<Element, MaxStackSize>;

public:
  using SequenceContainerTag = void;
  using value_type = Element;
  using reference = typename Storage::reference;
  using const_reference = typename Storage::const_reference;
  using iterator = typename Storage::iterator;
  using const_iterator = typename Storage::const_iterator;
  using reverse_iterator = typename Storage::reverse_iterator;
  using const_reverse_iterator = typename Storage::const_reverse_iterator;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using AppendResult = decltype(std::declval<Storage&>().push_back(std::declval<value_type>()));

  SmallList() = default;
  SmallList(Storage const& storage) : m_storage(storage) {}
  SmallList(Storage&& storage) : m_storage(std::move(storage)) {}
  explicit SmallList(std::size_t length, const_reference value = value_type()) : m_storage(length, value) {}
  explicit SmallList(std::span<value_type const> values) : m_storage(values.begin(), values.end()) {}
  template <std::input_iterator Iterator>
  SmallList(Iterator first, Iterator last) : m_storage(first, last) {}
  SmallList(std::initializer_list<value_type> values) {
    for (auto const& value : values)
      m_storage.push_back(value);
  }

  iterator begin() { return m_storage.begin(); }
  const_iterator begin() const { return m_storage.begin(); }
  const_iterator cbegin() const { return m_storage.begin(); }
  iterator end() { return m_storage.end(); }
  const_iterator end() const { return m_storage.end(); }
  const_iterator cend() const { return m_storage.end(); }
  reverse_iterator rbegin() { return m_storage.rbegin(); }
  const_reverse_iterator rbegin() const { return m_storage.rbegin(); }
  const_reverse_iterator crbegin() const { return m_storage.rbegin(); }
  reverse_iterator rend() { return m_storage.rend(); }
  const_reverse_iterator rend() const { return m_storage.rend(); }
  const_reverse_iterator crend() const { return m_storage.rend(); }
  std::size_t size() const { return m_storage.size(); }
  bool empty() const { return m_storage.empty(); }
  void clear() { m_storage.clear(); }
  reference front() { return *begin(); }
  const_reference front() const { return *begin(); }
  reference back() { return *std::prev(end()); }
  const_reference back() const { return *std::prev(end()); }
  [[nodiscard]] AppendResult push_back(const_reference value) { return m_storage.push_back(value); }
  [[nodiscard]] AppendResult push_back(value_type&& value) { return m_storage.push_back(std::move(value)); }
  decltype(auto) pop_back() { return m_storage.pop_back(); }
  template <typename... Args>
  decltype(auto) emplace_back(Args&&... args) { return m_storage.emplace_back(std::forward<Args>(args)...); }
  decltype(auto) resize(std::size_t size) { return m_storage.resize(size); }
  decltype(auto) resize(std::size_t size, const_reference value) { return m_storage.resize(size, value); }
  decltype(auto) insert(iterator pos, const_reference value) { return m_storage.insert(pos, value); }
  decltype(auto) insert(iterator pos, value_type&& value) { return m_storage.insert(pos, std::move(value)); }
  decltype(auto) insert(iterator pos, std::initializer_list<value_type> values) { return m_storage.insert(pos, values); }
  template <typename Position, typename... Args>
    requires requires(Storage& storage, Position pos, Args&&... args) { storage.insert(pos, std::forward<Args>(args)...); }
  decltype(auto) insert(Position pos, Args&&... args) { return m_storage.insert(pos, std::forward<Args>(args)...); }
  template <typename... Args>
  decltype(auto) erase(Args&&... args) { return m_storage.erase(std::forward<Args>(args)...); }
  template <typename... Args>
  decltype(auto) emplace(iterator pos, Args&&... args) { return m_storage.emplace(pos, std::forward<Args>(args)...); }
  template <typename... Args>
    requires requires(Storage& storage, Args&&... args) { storage.reserve(std::forward<Args>(args)...); }
  decltype(auto) reserve(Args&&... args) { return m_storage.reserve(std::forward<Args>(args)...); }
  template <typename... Args>
    requires requires(Storage& storage, Args&&... args) { storage.shrink_to_fit(std::forward<Args>(args)...); }
  decltype(auto) shrink_to_fit(Args&&... args) { return m_storage.shrink_to_fit(std::forward<Args>(args)...); }
  auto capacity() const requires requires(Storage const& storage) { storage.capacity(); } { return m_storage.capacity(); }
  auto max_size() const requires requires(Storage const& storage) { storage.max_size(); } { return m_storage.max_size(); }
  auto get_allocator() const requires requires(Storage const& storage) { storage.get_allocator(); } { return m_storage.get_allocator(); }
  value_type* ptr() { return m_storage.ptr(); }
  value_type const* ptr() const { return m_storage.ptr(); }
  void swap(SmallList& other) { std::swap(m_storage, other.m_storage); }
  bool operator==(SmallList const& other) const { return m_storage == other.m_storage; }
  bool operator<(SmallList const& other) const { return m_storage < other.m_storage; }
  bool operator>(SmallList const& other) const { return other < *this; }
  bool operator<=(SmallList const& other) const { return !(other < *this); }
  bool operator>=(SmallList const& other) const { return !(*this < other); }

  template <typename Container>
  static SmallList from(Container const& c);



  SmallList slice(SliceIndex a = SliceIndex(), SliceIndex b = SliceIndex(), int i = 1) const;

  template <typename Comparator>
  SmallList sorted(Comparator&& comparator) const;
  SmallList sorted() const;

  template <typename Function>
  [[nodiscard]]
  auto transformed(Function&& function);

  template <typename Function>
  [[nodiscard]]
  auto transformed(Function&& function) const;

  auto append(value_type e) -> AppendResult {
    return m_storage.push_back(std::move(e));
  }

  template <typename Container>
  auto appendAll(Container&& list) -> AppendResult {
    return SequenceAlgorithms::appendAll(*this, std::forward<Container>(list));
  }

  template <class... Args>
  auto emplaceAppend(Args&&... args) -> reference
      requires std::is_void_v<AppendResult> {
    m_storage.emplace_back(std::forward<Args>(args)...);
    return *std::prev(m_storage.end());
  }

  template <class... Args>
  auto emplaceAppend(Args&&... args) -> std::expected<std::reference_wrapper<value_type>, StarException>
      requires (!std::is_void_v<AppendResult>) {
    auto added = m_storage.emplace_back(std::forward<Args>(args)...);
    if (!added)
      return std::unexpected(std::move(added.error()));
    return std::reference_wrapper<value_type>(*std::prev(m_storage.end()));
  }

  auto first() -> std::expected<std::reference_wrapper<value_type>, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("first() called on empty list"));
    return std::reference_wrapper<value_type>(*m_storage.begin());
  }

  auto first() const -> std::expected<std::reference_wrapper<value_type const>, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("first() called on empty list"));
    return std::reference_wrapper<value_type const>(*m_storage.begin());
  }

  auto last() -> std::expected<std::reference_wrapper<value_type>, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("last() called on empty list"));
    return std::reference_wrapper<value_type>(*std::prev(m_storage.end()));
  }

  auto last() const -> std::expected<std::reference_wrapper<value_type const>, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("last() called on empty list"));
    return std::reference_wrapper<value_type const>(*std::prev(m_storage.end()));
  }

  auto maybeFirst() -> std::optional<value_type> {
    if (m_storage.empty())
      return {};
    return std::optional<value_type>(std::in_place, *m_storage.begin());
  }

  auto maybeLast() -> std::optional<value_type> {
    if (m_storage.empty())
      return {};
    return std::optional<value_type>(std::in_place, *std::prev(m_storage.end()));
  }

  std::expected<void, StarException> removeLast() {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("removeLast() called on empty list"));
    m_storage.erase(std::prev(m_storage.end()));
    return {};
  }

  auto takeLast() -> std::expected<value_type, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("takeLast() called on empty list"));
    value_type e = std::move(*std::prev(m_storage.end()));
    m_storage.erase(std::prev(m_storage.end()));
    return e;
  }

  auto maybeTakeLast() -> std::optional<value_type> {
    if (m_storage.empty())
      return {};
    value_type e = std::move(*std::prev(m_storage.end()));
    m_storage.erase(std::prev(m_storage.end()));
    return std::optional<value_type>(std::in_place, std::move(e));
  }

  void limitSizeBack(std::size_t maximumSize) {
    while (m_storage.size() > maximumSize)
      m_storage.erase(std::prev(m_storage.end()));
  }

  std::size_t count() const {
    return m_storage.size();
  }

  bool contains(const_reference e) const {
    return std::ranges::contains(*this, e);
  }

  std::size_t remove(const_reference e) {
    return SequenceAlgorithms::remove(*this, e);
  }

  template <typename Filter>
  auto filter(Filter&& filter) {
    return SequenceAlgorithms::filter(*this, std::forward<Filter>(filter));
  }

  template <typename Self, typename Filter>
  auto filtered(this Self const& self, Filter&& filter) {
    Self list(self);
    if constexpr (std::is_void_v<decltype(list.filter(std::forward<Filter>(filter)))>) {
      list.filter(std::forward<Filter>(filter));
      return list;
    } else {
      auto filtered = list.filter(std::forward<Filter>(filter));
      if (!filtered)
        return std::expected<Self, StarException>(std::unexpected(std::move(filtered.error())));
      return std::expected<Self, StarException>(std::move(list));
    }
  }

  template <typename Comparator>
  auto insertSorted(value_type e, Comparator&& comparator) -> AppendResult {
    static_assert(std::is_invocable_r_v<bool, Comparator&, const_reference, const_reference>,
        "Sorted insertion requires a nonfallible bool comparator");
    auto i = std::upper_bound(m_storage.begin(), m_storage.end(), e, std::forward<Comparator>(comparator));
    if constexpr (std::is_void_v<AppendResult>) {
      m_storage.insert(i, std::move(e));
    } else {
      auto added = m_storage.insert(i, std::move(e));
      if (!added)
        return std::unexpected(std::move(added.error()));
      return {};
    }
  }

  auto insertSorted(value_type e) -> AppendResult {
    return insertSorted(std::move(e), std::less<value_type>{});
  }

  template <typename Function>
  auto exec(Function&& function) {
    return SequenceAlgorithms::exec(*this, std::forward<Function>(function));
  }

  template <typename Function>
  auto exec(Function&& function) const {
    return SequenceAlgorithms::exec(*this, std::forward<Function>(function));
  }

  template <typename Function>
  auto transform(Function&& function) {
    return SequenceAlgorithms::transform(*this, std::forward<Function>(function));
  }

  template <typename Function>
  auto any(Function&& function) const {
    return SequenceAlgorithms::any(*this, std::forward<Function>(function));
  }

  auto any() const {
    return any([](auto const& value) -> auto const& { return value; });
  }

  template <typename Function>
  auto all(Function&& function) const {
    return SequenceAlgorithms::all(*this, std::forward<Function>(function));
  }

  auto all() const {
    return all([](auto const& value) -> auto const& { return value; });
  }

  template <typename Comparator>
  void sort(Comparator&& comparator) {
    static_assert(std::is_invocable_r_v<bool, Comparator&, reference, reference>,
        "Sorting requires a nonfallible bool comparator; precompute fallible keys first");
    std::sort(m_storage.begin(), m_storage.end(), std::forward<Comparator>(comparator));
  }

  void sort() {
    std::sort(m_storage.begin(), m_storage.end(), std::less<value_type>());
  }

  void reverse() {
    std::reverse(m_storage.begin(), m_storage.end());
  }

  std::size_t indexOf(const_reference e, std::size_t from = 0) const {
    if (from >= m_storage.size())
      return NPos;
    auto end = m_storage.end();
    auto found = std::find(m_storage.begin() + static_cast<difference_type>(from), end, e);
    return found == end ? NPos : static_cast<std::size_t>(std::distance(m_storage.begin(), found));
  }

  std::size_t lastIndexOf(const_reference e, std::size_t til = NPos) const {
    std::size_t index = NPos;
    std::size_t end = std::min(m_storage.size(), til);
    for (std::size_t i = 0; i < end; ++i) {
      if (operator[](i) == e)
        index = i;
    }
    return index;
  }

  auto at(std::size_t n) const -> std::expected<std::reference_wrapper<value_type const>, StarException> {
    if (n >= m_storage.size())
      return std::unexpected(OutOfRangeException(strf("out of range list::at({})", n)));
    return std::reference_wrapper<value_type const>(operator[](n));
  }

  auto at(std::size_t n) -> std::expected<std::reference_wrapper<value_type>, StarException> {
    if (n >= m_storage.size())
      return std::unexpected(OutOfRangeException(strf("out of range list::at({})", n)));
    return std::reference_wrapper<value_type>(operator[](n));
  }

  auto operator[](std::size_t n) const -> const_reference {
    if constexpr (Star::DebugEnabled) {
      Star::starAssertFailed((n < m_storage.size()));
    }
    return m_storage.operator[](n);
  }

  auto operator[](std::size_t n) -> reference {
    if constexpr (Star::DebugEnabled) {
      Star::starAssertFailed((n < m_storage.size()));
    }
    return m_storage.operator[](n);
  }

  auto get(std::size_t n, value_type def = value_type()) const -> value_type {
    if (n >= m_storage.size())
      return def;
    return operator[](n);
  }

  auto takeAt(std::size_t i) -> std::expected<value_type, StarException> {
    auto element = at(i);
    if (!element)
      return std::unexpected(std::move(element.error()));
    value_type e = std::move(element->get());
    m_storage.erase(m_storage.begin() + i);
    return e;
  }

  auto wrap(std::size_t n) const -> std::expected<std::reference_wrapper<value_type const>, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("wrap() called on empty list"));
    return std::reference_wrapper<value_type const>(operator[](n % m_storage.size()));
  }

  auto wrap(std::size_t n) -> std::expected<std::reference_wrapper<value_type>, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("wrap() called on empty list"));
    return std::reference_wrapper<value_type>(operator[](n % m_storage.size()));
  }

  auto wrap(std::size_t n, value_type def) const -> value_type {
    if (m_storage.empty())
      return def;
    else
      return operator[](n % m_storage.size());
  }

  void eraseAt(std::size_t i) {
    if constexpr (Star::DebugEnabled) {
      Star::starAssertFailed((i < m_storage.size()));
    }
    m_storage.erase(m_storage.begin() + i);
  }

  void eraseAt(std::size_t b, std::size_t e) {
    if constexpr (Star::DebugEnabled) {
      Star::starAssertFailed((b < m_storage.size() && e <= m_storage.size()));
    }
    m_storage.erase(m_storage.begin() + b, m_storage.begin() + e);
  }

  auto insertAt(std::size_t pos, value_type e) -> AppendResult {
    if constexpr (Star::DebugEnabled)
      Star::starAssertFailed((pos <= m_storage.size()));
    if constexpr (std::is_void_v<AppendResult>) {
      m_storage.insert(m_storage.begin() + pos, std::move(e));
    } else {
      auto added = m_storage.insert(m_storage.begin() + pos, std::move(e));
      if (!added)
        return std::unexpected(std::move(added.error()));
      return {};
    }
  }

  template <typename Container>
  auto insertAllAt(std::size_t pos, Container const& l) -> AppendResult {
    if constexpr (Star::DebugEnabled)
      Star::starAssertFailed((pos <= m_storage.size()));
    if constexpr (std::is_void_v<AppendResult>) {
      m_storage.insert(m_storage.begin() + pos, l.begin(), l.end());
    } else {
      auto added = m_storage.insert(m_storage.begin() + pos, l.begin(), l.end());
      if (!added)
        return std::unexpected(std::move(added.error()));
      return {};
    }
  }

  auto set(std::size_t pos, value_type e) -> AppendResult {
    if constexpr (std::is_void_v<AppendResult>) {
      if (pos >= m_storage.size())
        m_storage.resize(pos + 1);
      operator[](pos) = std::move(e);
    } else {
      if (pos == std::numeric_limits<std::size_t>::max())
        return std::unexpected(OutOfRangeException("List::set index overflows size"));
      if (pos >= m_storage.size()) {
        auto resized = m_storage.resize(pos + 1);
        if (!resized)
          return std::unexpected(std::move(resized.error()));
      }
      operator[](pos) = std::move(e);
      return {};
    }
  }

  void swap(std::size_t i, std::size_t j) {
    std::swap(operator[](i), operator[](j));
  }

  std::expected<void, StarException> move(std::size_t from, std::size_t to) {
    if (from >= m_storage.size() || to >= m_storage.size())
      return std::unexpected(OutOfRangeException("List::move index out of range"));
    if (from < to)
      std::rotate(m_storage.begin() + from, m_storage.begin() + from + 1, m_storage.begin() + to + 1);
    else if (to < from)
      std::rotate(m_storage.begin() + to, m_storage.begin() + from, m_storage.begin() + from + 1);
    return {};
  }

private:
  Storage m_storage;
};

template <typename Element, typename Allocator = std::allocator<Element>>
class Deque {
  using Storage = std::deque<Element, Allocator>;

public:
  using SequenceContainerTag = void;
  using value_type = Element;
  using reference = typename Storage::reference;
  using const_reference = typename Storage::const_reference;
  using iterator = typename Storage::iterator;
  using const_iterator = typename Storage::const_iterator;
  using reverse_iterator = typename Storage::reverse_iterator;
  using const_reverse_iterator = typename Storage::const_reverse_iterator;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using AppendResult = decltype(std::declval<Storage&>().push_back(std::declval<value_type>()));

  Deque() = default;
  Deque(Storage const& storage) : m_storage(storage) {}
  Deque(Storage&& storage) : m_storage(std::move(storage)) {}
  explicit Deque(std::size_t length, const_reference value = value_type()) : m_storage(length, value) {}
  explicit Deque(std::span<value_type const> values) : m_storage(values.begin(), values.end()) {}
  template <std::input_iterator Iterator>
  Deque(Iterator first, Iterator last) : m_storage(first, last) {}
  Deque(std::initializer_list<value_type> values) : m_storage(values) {}

  iterator begin() { return m_storage.begin(); }
  const_iterator begin() const { return m_storage.begin(); }
  const_iterator cbegin() const { return m_storage.begin(); }
  iterator end() { return m_storage.end(); }
  const_iterator end() const { return m_storage.end(); }
  const_iterator cend() const { return m_storage.end(); }
  reverse_iterator rbegin() { return m_storage.rbegin(); }
  const_reverse_iterator rbegin() const { return m_storage.rbegin(); }
  const_reverse_iterator crbegin() const { return m_storage.rbegin(); }
  reverse_iterator rend() { return m_storage.rend(); }
  const_reverse_iterator rend() const { return m_storage.rend(); }
  const_reverse_iterator crend() const { return m_storage.rend(); }
  std::size_t size() const { return m_storage.size(); }
  bool empty() const { return m_storage.empty(); }
  void clear() { m_storage.clear(); }
  reference front() { return *begin(); }
  const_reference front() const { return *begin(); }
  reference back() { return *std::prev(end()); }
  const_reference back() const { return *std::prev(end()); }
  [[nodiscard]] AppendResult push_back(const_reference value) { return m_storage.push_back(value); }
  [[nodiscard]] AppendResult push_back(value_type&& value) { return m_storage.push_back(std::move(value)); }
  decltype(auto) pop_back() { return m_storage.pop_back(); }
  template <typename... Args>
  decltype(auto) emplace_back(Args&&... args) { return m_storage.emplace_back(std::forward<Args>(args)...); }
  decltype(auto) resize(std::size_t size) { return m_storage.resize(size); }
  decltype(auto) resize(std::size_t size, const_reference value) { return m_storage.resize(size, value); }
  decltype(auto) insert(iterator pos, const_reference value) { return m_storage.insert(pos, value); }
  decltype(auto) insert(iterator pos, value_type&& value) { return m_storage.insert(pos, std::move(value)); }
  decltype(auto) insert(iterator pos, std::initializer_list<value_type> values) { return m_storage.insert(pos, values); }
  template <typename Position, typename... Args>
    requires requires(Storage& storage, Position pos, Args&&... args) { storage.insert(pos, std::forward<Args>(args)...); }
  decltype(auto) insert(Position pos, Args&&... args) { return m_storage.insert(pos, std::forward<Args>(args)...); }
  template <typename... Args>
  decltype(auto) erase(Args&&... args) { return m_storage.erase(std::forward<Args>(args)...); }
  template <typename... Args>
  decltype(auto) emplace(iterator pos, Args&&... args) { return m_storage.emplace(pos, std::forward<Args>(args)...); }
  decltype(auto) assign(std::initializer_list<value_type> values) { return m_storage.assign(values); }
  template <typename... Args>
    requires requires(Storage& storage, Args&&... args) { storage.assign(std::forward<Args>(args)...); }
  decltype(auto) assign(Args&&... args) { return m_storage.assign(std::forward<Args>(args)...); }
  template <typename... Args>
    requires requires(Storage& storage, Args&&... args) { storage.reserve(std::forward<Args>(args)...); }
  decltype(auto) reserve(Args&&... args) { return m_storage.reserve(std::forward<Args>(args)...); }
  template <typename... Args>
    requires requires(Storage& storage, Args&&... args) { storage.shrink_to_fit(std::forward<Args>(args)...); }
  decltype(auto) shrink_to_fit(Args&&... args) { return m_storage.shrink_to_fit(std::forward<Args>(args)...); }
  auto capacity() const requires requires(Storage const& storage) { storage.capacity(); } { return m_storage.capacity(); }
  auto max_size() const requires requires(Storage const& storage) { storage.max_size(); } { return m_storage.max_size(); }
  auto get_allocator() const requires requires(Storage const& storage) { storage.get_allocator(); } { return m_storage.get_allocator(); }
  void push_front(const_reference value) { m_storage.push_front(value); }
  void push_front(value_type&& value) { m_storage.push_front(std::move(value)); }
  void pop_front() { m_storage.pop_front(); }
  template <typename... Args>
  decltype(auto) emplace_front(Args&&... args) { return m_storage.emplace_front(std::forward<Args>(args)...); }
  void swap(Deque& other) { std::swap(m_storage, other.m_storage); }
  bool operator==(Deque const& other) const { return m_storage == other.m_storage; }
  bool operator<(Deque const& other) const { return m_storage < other.m_storage; }
  bool operator>(Deque const& other) const { return other < *this; }
  bool operator<=(Deque const& other) const { return !(other < *this); }
  bool operator>=(Deque const& other) const { return !(*this < other); }

  template <typename Container>
  static Deque from(Container const& c);



  Deque slice(SliceIndex a = SliceIndex(), SliceIndex b = SliceIndex(), int i = 1) const;

  template <typename Comparator>
  Deque sorted(Comparator&& comparator) const;
  Deque sorted() const;

  template <typename Function>
  [[nodiscard]]
  auto transformed(Function&& function);

  template <typename Function>
  [[nodiscard]]
  auto transformed(Function&& function) const;

  auto append(value_type e) -> AppendResult {
    return m_storage.push_back(std::move(e));
  }

  template <typename Container>
  auto appendAll(Container&& list) -> AppendResult {
    return SequenceAlgorithms::appendAll(*this, std::forward<Container>(list));
  }

  template <class... Args>
  auto emplaceAppend(Args&&... args) -> reference
      requires std::is_void_v<AppendResult> {
    m_storage.emplace_back(std::forward<Args>(args)...);
    return *std::prev(m_storage.end());
  }

  template <class... Args>
  auto emplaceAppend(Args&&... args) -> std::expected<std::reference_wrapper<value_type>, StarException>
      requires (!std::is_void_v<AppendResult>) {
    auto added = m_storage.emplace_back(std::forward<Args>(args)...);
    if (!added)
      return std::unexpected(std::move(added.error()));
    return std::reference_wrapper<value_type>(*std::prev(m_storage.end()));
  }

  auto first() -> std::expected<std::reference_wrapper<value_type>, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("first() called on empty list"));
    return std::reference_wrapper<value_type>(*m_storage.begin());
  }

  auto first() const -> std::expected<std::reference_wrapper<value_type const>, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("first() called on empty list"));
    return std::reference_wrapper<value_type const>(*m_storage.begin());
  }

  auto last() -> std::expected<std::reference_wrapper<value_type>, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("last() called on empty list"));
    return std::reference_wrapper<value_type>(*std::prev(m_storage.end()));
  }

  auto last() const -> std::expected<std::reference_wrapper<value_type const>, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("last() called on empty list"));
    return std::reference_wrapper<value_type const>(*std::prev(m_storage.end()));
  }

  auto maybeFirst() -> std::optional<value_type> {
    if (m_storage.empty())
      return {};
    return std::optional<value_type>(std::in_place, *m_storage.begin());
  }

  auto maybeLast() -> std::optional<value_type> {
    if (m_storage.empty())
      return {};
    return std::optional<value_type>(std::in_place, *std::prev(m_storage.end()));
  }

  std::expected<void, StarException> removeLast() {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("removeLast() called on empty list"));
    m_storage.erase(std::prev(m_storage.end()));
    return {};
  }

  auto takeLast() -> std::expected<value_type, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("takeLast() called on empty list"));
    value_type e = std::move(*std::prev(m_storage.end()));
    m_storage.erase(std::prev(m_storage.end()));
    return e;
  }

  auto maybeTakeLast() -> std::optional<value_type> {
    if (m_storage.empty())
      return {};
    value_type e = std::move(*std::prev(m_storage.end()));
    m_storage.erase(std::prev(m_storage.end()));
    return std::optional<value_type>(std::in_place, std::move(e));
  }

  void limitSizeBack(std::size_t maximumSize) {
    while (m_storage.size() > maximumSize)
      m_storage.erase(std::prev(m_storage.end()));
  }

  std::size_t count() const {
    return m_storage.size();
  }

  bool contains(const_reference e) const {
    return std::ranges::contains(*this, e);
  }

  std::size_t remove(const_reference e) {
    return SequenceAlgorithms::remove(*this, e);
  }

  template <typename Filter>
  auto filter(Filter&& filter) {
    return SequenceAlgorithms::filter(*this, std::forward<Filter>(filter));
  }

  template <typename Self, typename Filter>
  auto filtered(this Self const& self, Filter&& filter) {
    Self list(self);
    if constexpr (std::is_void_v<decltype(list.filter(std::forward<Filter>(filter)))>) {
      list.filter(std::forward<Filter>(filter));
      return list;
    } else {
      auto filtered = list.filter(std::forward<Filter>(filter));
      if (!filtered)
        return std::expected<Self, StarException>(std::unexpected(std::move(filtered.error())));
      return std::expected<Self, StarException>(std::move(list));
    }
  }

  template <typename Comparator>
  auto insertSorted(value_type e, Comparator&& comparator) -> AppendResult {
    static_assert(std::is_invocable_r_v<bool, Comparator&, const_reference, const_reference>,
        "Sorted insertion requires a nonfallible bool comparator");
    auto i = std::upper_bound(m_storage.begin(), m_storage.end(), e, std::forward<Comparator>(comparator));
    if constexpr (std::is_void_v<AppendResult>) {
      m_storage.insert(i, std::move(e));
    } else {
      auto added = m_storage.insert(i, std::move(e));
      if (!added)
        return std::unexpected(std::move(added.error()));
      return {};
    }
  }

  auto insertSorted(value_type e) -> AppendResult {
    return insertSorted(std::move(e), std::less<value_type>{});
  }

  template <typename Function>
  auto exec(Function&& function) {
    return SequenceAlgorithms::exec(*this, std::forward<Function>(function));
  }

  template <typename Function>
  auto exec(Function&& function) const {
    return SequenceAlgorithms::exec(*this, std::forward<Function>(function));
  }

  template <typename Function>
  auto transform(Function&& function) {
    return SequenceAlgorithms::transform(*this, std::forward<Function>(function));
  }

  template <typename Function>
  auto any(Function&& function) const {
    return SequenceAlgorithms::any(*this, std::forward<Function>(function));
  }

  auto any() const {
    return any([](auto const& value) -> auto const& { return value; });
  }

  template <typename Function>
  auto all(Function&& function) const {
    return SequenceAlgorithms::all(*this, std::forward<Function>(function));
  }

  auto all() const {
    return all([](auto const& value) -> auto const& { return value; });
  }

  template <typename Comparator>
  void sort(Comparator&& comparator) {
    static_assert(std::is_invocable_r_v<bool, Comparator&, reference, reference>,
        "Sorting requires a nonfallible bool comparator; precompute fallible keys first");
    std::sort(m_storage.begin(), m_storage.end(), std::forward<Comparator>(comparator));
  }

  void sort() {
    std::sort(m_storage.begin(), m_storage.end(), std::less<value_type>());
  }

  void reverse() {
    std::reverse(m_storage.begin(), m_storage.end());
  }

  std::size_t indexOf(const_reference e, std::size_t from = 0) const {
    if (from >= m_storage.size())
      return NPos;
    auto end = m_storage.end();
    auto found = std::find(m_storage.begin() + static_cast<difference_type>(from), end, e);
    return found == end ? NPos : static_cast<std::size_t>(std::distance(m_storage.begin(), found));
  }

  std::size_t lastIndexOf(const_reference e, std::size_t til = NPos) const {
    std::size_t index = NPos;
    std::size_t end = std::min(m_storage.size(), til);
    for (std::size_t i = 0; i < end; ++i) {
      if (operator[](i) == e)
        index = i;
    }
    return index;
  }

  auto at(std::size_t n) const -> std::expected<std::reference_wrapper<value_type const>, StarException> {
    if (n >= m_storage.size())
      return std::unexpected(OutOfRangeException(strf("out of range list::at({})", n)));
    return std::reference_wrapper<value_type const>(operator[](n));
  }

  auto at(std::size_t n) -> std::expected<std::reference_wrapper<value_type>, StarException> {
    if (n >= m_storage.size())
      return std::unexpected(OutOfRangeException(strf("out of range list::at({})", n)));
    return std::reference_wrapper<value_type>(operator[](n));
  }

  auto operator[](std::size_t n) const -> const_reference {
    if constexpr (Star::DebugEnabled) {
      Star::starAssertFailed((n < m_storage.size()));
    }
    return m_storage.operator[](n);
  }

  auto operator[](std::size_t n) -> reference {
    if constexpr (Star::DebugEnabled) {
      Star::starAssertFailed((n < m_storage.size()));
    }
    return m_storage.operator[](n);
  }

  auto get(std::size_t n, value_type def = value_type()) const -> value_type {
    if (n >= m_storage.size())
      return def;
    return operator[](n);
  }

  auto takeAt(std::size_t i) -> std::expected<value_type, StarException> {
    auto element = at(i);
    if (!element)
      return std::unexpected(std::move(element.error()));
    value_type e = std::move(element->get());
    m_storage.erase(m_storage.begin() + i);
    return e;
  }

  auto wrap(std::size_t n) const -> std::expected<std::reference_wrapper<value_type const>, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("wrap() called on empty list"));
    return std::reference_wrapper<value_type const>(operator[](n % m_storage.size()));
  }

  auto wrap(std::size_t n) -> std::expected<std::reference_wrapper<value_type>, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("wrap() called on empty list"));
    return std::reference_wrapper<value_type>(operator[](n % m_storage.size()));
  }

  auto wrap(std::size_t n, value_type def) const -> value_type {
    if (m_storage.empty())
      return def;
    else
      return operator[](n % m_storage.size());
  }

  void eraseAt(std::size_t i) {
    if constexpr (Star::DebugEnabled) {
      Star::starAssertFailed((i < m_storage.size()));
    }
    m_storage.erase(m_storage.begin() + i);
  }

  void eraseAt(std::size_t b, std::size_t e) {
    if constexpr (Star::DebugEnabled) {
      Star::starAssertFailed((b < m_storage.size() && e <= m_storage.size()));
    }
    m_storage.erase(m_storage.begin() + b, m_storage.begin() + e);
  }

  auto insertAt(std::size_t pos, value_type e) -> AppendResult {
    if constexpr (Star::DebugEnabled)
      Star::starAssertFailed((pos <= m_storage.size()));
    if constexpr (std::is_void_v<AppendResult>) {
      m_storage.insert(m_storage.begin() + pos, std::move(e));
    } else {
      auto added = m_storage.insert(m_storage.begin() + pos, std::move(e));
      if (!added)
        return std::unexpected(std::move(added.error()));
      return {};
    }
  }

  template <typename Container>
  auto insertAllAt(std::size_t pos, Container const& l) -> AppendResult {
    if constexpr (Star::DebugEnabled)
      Star::starAssertFailed((pos <= m_storage.size()));
    if constexpr (std::is_void_v<AppendResult>) {
      m_storage.insert(m_storage.begin() + pos, l.begin(), l.end());
    } else {
      auto added = m_storage.insert(m_storage.begin() + pos, l.begin(), l.end());
      if (!added)
        return std::unexpected(std::move(added.error()));
      return {};
    }
  }

  auto set(std::size_t pos, value_type e) -> AppendResult {
    if constexpr (std::is_void_v<AppendResult>) {
      if (pos >= m_storage.size())
        m_storage.resize(pos + 1);
      operator[](pos) = std::move(e);
    } else {
      if (pos == std::numeric_limits<std::size_t>::max())
        return std::unexpected(OutOfRangeException("List::set index overflows size"));
      if (pos >= m_storage.size()) {
        auto resized = m_storage.resize(pos + 1);
        if (!resized)
          return std::unexpected(std::move(resized.error()));
      }
      operator[](pos) = std::move(e);
      return {};
    }
  }

  void swap(std::size_t i, std::size_t j) {
    std::swap(operator[](i), operator[](j));
  }

  std::expected<void, StarException> move(std::size_t from, std::size_t to) {
    if (from >= m_storage.size() || to >= m_storage.size())
      return std::unexpected(OutOfRangeException("List::move index out of range"));
    if (from < to)
      std::rotate(m_storage.begin() + from, m_storage.begin() + from + 1, m_storage.begin() + to + 1);
    else if (to < from)
      std::rotate(m_storage.begin() + to, m_storage.begin() + from, m_storage.begin() + from + 1);
    return {};
  }

  void prepend(value_type e) {
    m_storage.push_front(std::move(e));
  }

  template <typename Container>
  void prependAll(Container&& list) {
    for (auto i = std::rbegin(list); i != std::rend(list); ++i) {
      if (std::is_rvalue_reference_v<Container&&>)
        m_storage.push_front(std::move(*i));
      else
        m_storage.push_front(*i);
    }
  }

  template <class... Args>
  auto emplacePrepend(Args&&... args) -> reference {
    m_storage.emplace_front(std::forward<Args>(args)...);
    return *m_storage.begin();
  }

  std::expected<void, StarException> removeFirst() {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("removeFirst() called on empty list"));
    m_storage.pop_front();
    return {};
  }

  auto takeFirst() -> std::expected<value_type, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("takeFirst() called on empty list"));
    value_type e = std::move(*m_storage.begin());
    m_storage.pop_front();
    return e;
  }

  void limitSizeFront(std::size_t maximumSize) {
    while (m_storage.size() > maximumSize)
      m_storage.pop_front();
  }

private:
  Storage m_storage;
};

template <typename Element, typename Allocator = std::allocator<Element>>
class LinkedList {
  using Storage = std::list<Element, Allocator>;

public:
  using SequenceContainerTag = void;
  using value_type = Element;
  using reference = typename Storage::reference;
  using const_reference = typename Storage::const_reference;
  using iterator = typename Storage::iterator;
  using const_iterator = typename Storage::const_iterator;
  using reverse_iterator = typename Storage::reverse_iterator;
  using const_reverse_iterator = typename Storage::const_reverse_iterator;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using AppendResult = decltype(std::declval<Storage&>().push_back(std::declval<value_type>()));

  LinkedList() = default;
  LinkedList(Storage const& storage) : m_storage(storage) {}
  LinkedList(Storage&& storage) : m_storage(std::move(storage)) {}
  explicit LinkedList(std::size_t length, const_reference value = value_type()) : m_storage(length, value) {}
  explicit LinkedList(std::span<value_type const> values) : m_storage(values.begin(), values.end()) {}
  template <std::input_iterator Iterator>
  LinkedList(Iterator first, Iterator last) : m_storage(first, last) {}
  LinkedList(std::initializer_list<value_type> values) : m_storage(values) {}

  iterator begin() { return m_storage.begin(); }
  const_iterator begin() const { return m_storage.begin(); }
  const_iterator cbegin() const { return m_storage.begin(); }
  iterator end() { return m_storage.end(); }
  const_iterator end() const { return m_storage.end(); }
  const_iterator cend() const { return m_storage.end(); }
  reverse_iterator rbegin() { return m_storage.rbegin(); }
  const_reverse_iterator rbegin() const { return m_storage.rbegin(); }
  const_reverse_iterator crbegin() const { return m_storage.rbegin(); }
  reverse_iterator rend() { return m_storage.rend(); }
  const_reverse_iterator rend() const { return m_storage.rend(); }
  const_reverse_iterator crend() const { return m_storage.rend(); }
  std::size_t size() const { return m_storage.size(); }
  bool empty() const { return m_storage.empty(); }
  void clear() { m_storage.clear(); }
  reference front() { return *begin(); }
  const_reference front() const { return *begin(); }
  reference back() { return *std::prev(end()); }
  const_reference back() const { return *std::prev(end()); }
  [[nodiscard]] AppendResult push_back(const_reference value) { return m_storage.push_back(value); }
  [[nodiscard]] AppendResult push_back(value_type&& value) { return m_storage.push_back(std::move(value)); }
  decltype(auto) pop_back() { return m_storage.pop_back(); }
  template <typename... Args>
  decltype(auto) emplace_back(Args&&... args) { return m_storage.emplace_back(std::forward<Args>(args)...); }
  decltype(auto) resize(std::size_t size) { return m_storage.resize(size); }
  decltype(auto) resize(std::size_t size, const_reference value) { return m_storage.resize(size, value); }
  decltype(auto) insert(iterator pos, const_reference value) { return m_storage.insert(pos, value); }
  decltype(auto) insert(iterator pos, value_type&& value) { return m_storage.insert(pos, std::move(value)); }
  decltype(auto) insert(iterator pos, std::initializer_list<value_type> values) { return m_storage.insert(pos, values); }
  template <typename Position, typename... Args>
    requires requires(Storage& storage, Position pos, Args&&... args) { storage.insert(pos, std::forward<Args>(args)...); }
  decltype(auto) insert(Position pos, Args&&... args) { return m_storage.insert(pos, std::forward<Args>(args)...); }
  template <typename... Args>
  decltype(auto) erase(Args&&... args) { return m_storage.erase(std::forward<Args>(args)...); }
  template <typename... Args>
  decltype(auto) emplace(iterator pos, Args&&... args) { return m_storage.emplace(pos, std::forward<Args>(args)...); }
  decltype(auto) assign(std::initializer_list<value_type> values) { return m_storage.assign(values); }
  template <typename... Args>
    requires requires(Storage& storage, Args&&... args) { storage.assign(std::forward<Args>(args)...); }
  decltype(auto) assign(Args&&... args) { return m_storage.assign(std::forward<Args>(args)...); }
  template <typename... Args>
    requires requires(Storage& storage, Args&&... args) { storage.reserve(std::forward<Args>(args)...); }
  decltype(auto) reserve(Args&&... args) { return m_storage.reserve(std::forward<Args>(args)...); }
  template <typename... Args>
    requires requires(Storage& storage, Args&&... args) { storage.shrink_to_fit(std::forward<Args>(args)...); }
  decltype(auto) shrink_to_fit(Args&&... args) { return m_storage.shrink_to_fit(std::forward<Args>(args)...); }
  auto capacity() const requires requires(Storage const& storage) { storage.capacity(); } { return m_storage.capacity(); }
  auto max_size() const requires requires(Storage const& storage) { storage.max_size(); } { return m_storage.max_size(); }
  auto get_allocator() const requires requires(Storage const& storage) { storage.get_allocator(); } { return m_storage.get_allocator(); }
  void push_front(const_reference value) { m_storage.push_front(value); }
  void push_front(value_type&& value) { m_storage.push_front(std::move(value)); }
  void pop_front() { m_storage.pop_front(); }
  template <typename... Args>
  decltype(auto) emplace_front(Args&&... args) { return m_storage.emplace_front(std::forward<Args>(args)...); }
  void splice(const_iterator pos, LinkedList& other) { m_storage.splice(pos, other.m_storage); }
  void splice(const_iterator pos, LinkedList& other, const_iterator element) { m_storage.splice(pos, other.m_storage, element); }
  void splice(const_iterator pos, LinkedList& other, const_iterator first, const_iterator last) { m_storage.splice(pos, other.m_storage, first, last); }
  template <typename Comparator = std::less<value_type>>
  void sort(Comparator comparator = {}) { m_storage.sort(std::move(comparator)); }
  void reverse() { m_storage.reverse(); }
  template <typename Predicate = std::equal_to<value_type>>
  auto unique(Predicate predicate = {}) { return m_storage.unique(std::move(predicate)); }
  template <typename Comparator = std::less<value_type>>
  void merge(LinkedList& other, Comparator comparator = {}) { m_storage.merge(other.m_storage, std::move(comparator)); }
  void swap(LinkedList& other) { std::swap(m_storage, other.m_storage); }
  bool operator==(LinkedList const& other) const { return m_storage == other.m_storage; }
  bool operator<(LinkedList const& other) const { return m_storage < other.m_storage; }
  bool operator>(LinkedList const& other) const { return other < *this; }
  bool operator<=(LinkedList const& other) const { return !(other < *this); }
  bool operator>=(LinkedList const& other) const { return !(*this < other); }

  template <typename Container>
  static LinkedList from(Container const& c);



  void appendAll(LinkedList list);
  void prependAll(LinkedList list);

  template <typename Container>
  void appendAll(Container&& list);
  template <typename Container>
  void prependAll(Container&& list);

  template <typename Comparator>
  LinkedList sorted(Comparator&& comparator) const;
  LinkedList sorted() const;

  template <typename Function>
  [[nodiscard]]
  auto transformed(Function&& function);

  template <typename Function>
  [[nodiscard]]
  auto transformed(Function&& function) const;

  auto append(value_type e) -> AppendResult {
    return m_storage.push_back(std::move(e));
  }

  template <class... Args>
  auto emplaceAppend(Args&&... args) -> reference
      requires std::is_void_v<AppendResult> {
    m_storage.emplace_back(std::forward<Args>(args)...);
    return *std::prev(m_storage.end());
  }

  template <class... Args>
  auto emplaceAppend(Args&&... args) -> std::expected<std::reference_wrapper<value_type>, StarException>
      requires (!std::is_void_v<AppendResult>) {
    auto added = m_storage.emplace_back(std::forward<Args>(args)...);
    if (!added)
      return std::unexpected(std::move(added.error()));
    return std::reference_wrapper<value_type>(*std::prev(m_storage.end()));
  }

  auto first() -> std::expected<std::reference_wrapper<value_type>, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("first() called on empty list"));
    return std::reference_wrapper<value_type>(*m_storage.begin());
  }

  auto first() const -> std::expected<std::reference_wrapper<value_type const>, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("first() called on empty list"));
    return std::reference_wrapper<value_type const>(*m_storage.begin());
  }

  auto last() -> std::expected<std::reference_wrapper<value_type>, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("last() called on empty list"));
    return std::reference_wrapper<value_type>(*std::prev(m_storage.end()));
  }

  auto last() const -> std::expected<std::reference_wrapper<value_type const>, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("last() called on empty list"));
    return std::reference_wrapper<value_type const>(*std::prev(m_storage.end()));
  }

  auto maybeFirst() -> std::optional<value_type> {
    if (m_storage.empty())
      return {};
    return std::optional<value_type>(std::in_place, *m_storage.begin());
  }

  auto maybeLast() -> std::optional<value_type> {
    if (m_storage.empty())
      return {};
    return std::optional<value_type>(std::in_place, *std::prev(m_storage.end()));
  }

  std::expected<void, StarException> removeLast() {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("removeLast() called on empty list"));
    m_storage.erase(std::prev(m_storage.end()));
    return {};
  }

  auto takeLast() -> std::expected<value_type, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("takeLast() called on empty list"));
    value_type e = std::move(*std::prev(m_storage.end()));
    m_storage.erase(std::prev(m_storage.end()));
    return e;
  }

  auto maybeTakeLast() -> std::optional<value_type> {
    if (m_storage.empty())
      return {};
    value_type e = std::move(*std::prev(m_storage.end()));
    m_storage.erase(std::prev(m_storage.end()));
    return std::optional<value_type>(std::in_place, std::move(e));
  }

  void limitSizeBack(std::size_t maximumSize) {
    while (m_storage.size() > maximumSize)
      m_storage.erase(std::prev(m_storage.end()));
  }

  std::size_t count() const {
    return m_storage.size();
  }

  bool contains(const_reference e) const {
    return std::ranges::contains(*this, e);
  }

  std::size_t remove(const_reference e) {
    return SequenceAlgorithms::remove(*this, e);
  }

  template <typename Filter>
  auto filter(Filter&& filter) {
    return SequenceAlgorithms::filter(*this, std::forward<Filter>(filter));
  }

  template <typename Self, typename Filter>
  auto filtered(this Self const& self, Filter&& filter) {
    Self list(self);
    if constexpr (std::is_void_v<decltype(list.filter(std::forward<Filter>(filter)))>) {
      list.filter(std::forward<Filter>(filter));
      return list;
    } else {
      auto filtered = list.filter(std::forward<Filter>(filter));
      if (!filtered)
        return std::expected<Self, StarException>(std::unexpected(std::move(filtered.error())));
      return std::expected<Self, StarException>(std::move(list));
    }
  }

  template <typename Comparator>
  auto insertSorted(value_type e, Comparator&& comparator) -> AppendResult {
    static_assert(std::is_invocable_r_v<bool, Comparator&, const_reference, const_reference>,
        "Sorted insertion requires a nonfallible bool comparator");
    auto i = std::upper_bound(m_storage.begin(), m_storage.end(), e, std::forward<Comparator>(comparator));
    if constexpr (std::is_void_v<AppendResult>) {
      m_storage.insert(i, std::move(e));
    } else {
      auto added = m_storage.insert(i, std::move(e));
      if (!added)
        return std::unexpected(std::move(added.error()));
      return {};
    }
  }

  auto insertSorted(value_type e) -> AppendResult {
    return insertSorted(std::move(e), std::less<value_type>{});
  }

  template <typename Function>
  auto exec(Function&& function) {
    return SequenceAlgorithms::exec(*this, std::forward<Function>(function));
  }

  template <typename Function>
  auto exec(Function&& function) const {
    return SequenceAlgorithms::exec(*this, std::forward<Function>(function));
  }

  template <typename Function>
  auto transform(Function&& function) {
    return SequenceAlgorithms::transform(*this, std::forward<Function>(function));
  }

  template <typename Function>
  auto any(Function&& function) const {
    return SequenceAlgorithms::any(*this, std::forward<Function>(function));
  }

  auto any() const {
    return any([](auto const& value) -> auto const& { return value; });
  }

  template <typename Function>
  auto all(Function&& function) const {
    return SequenceAlgorithms::all(*this, std::forward<Function>(function));
  }

  auto all() const {
    return all([](auto const& value) -> auto const& { return value; });
  }

  void prepend(value_type e) {
    m_storage.push_front(std::move(e));
  }

  template <class... Args>
  auto emplacePrepend(Args&&... args) -> reference {
    m_storage.emplace_front(std::forward<Args>(args)...);
    return *m_storage.begin();
  }

  std::expected<void, StarException> removeFirst() {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("removeFirst() called on empty list"));
    m_storage.pop_front();
    return {};
  }

  auto takeFirst() -> std::expected<value_type, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("takeFirst() called on empty list"));
    value_type e = std::move(*m_storage.begin());
    m_storage.pop_front();
    return e;
  }

  void limitSizeFront(std::size_t maximumSize) {
    while (m_storage.size() > maximumSize)
      m_storage.pop_front();
  }

private:
  Storage m_storage;
};

template <typename Element, typename Allocator>
struct hash<List<Element, Allocator>> : public ListHasher<List<Element, Allocator>> {};

template <typename Element, std::size_t MaxSize>
struct hash<StaticList<Element, MaxSize>> : public ListHasher<StaticList<Element, MaxSize>> {};

template <typename Element, std::size_t MaxStackSize>
struct hash<SmallList<Element, MaxStackSize>> : public ListHasher<SmallList<Element, MaxStackSize>> {};

template <typename Element, typename Allocator>
struct hash<Deque<Element, Allocator>> : public ListHasher<Deque<Element, Allocator>> {};

template <typename Element, typename Allocator>
struct hash<LinkedList<Element, Allocator>> : public ListHasher<LinkedList<Element, Allocator>> {};

template <typename Element, typename Allocator>
template <typename Container>
List<Element, Allocator> List<Element, Allocator>::from(Container const& c) {
  return List(c.begin(), c.end());
}

template <typename Element, typename Allocator>
auto List<Element, Allocator>::ptr() -> value_type * {
  return m_storage.data();
}

template <typename Element, typename Allocator>
auto List<Element, Allocator>::ptr() const -> value_type const * {
  return m_storage.data();
}

template <typename Element, typename Allocator>
auto List<Element, Allocator>::slice(SliceIndex a, SliceIndex b, int i) const -> List {
  return Star::slice(*this, a, b, i);
}

template <typename Element, typename Allocator>
template <typename Comparator>
auto List<Element, Allocator>::sorted(Comparator&& comparator) const -> List {
  static_assert(std::is_invocable_r_v<bool, Comparator&, reference, reference>,
      "Sorting requires a nonfallible bool comparator; precompute fallible keys first");
  List list(*this);
  list.sort(std::forward<Comparator>(comparator));
  return list;
}

template <typename Element, typename Allocator>
List<Element, Allocator> List<Element, Allocator>::sorted() const {
  List list(*this);
  list.sort();
  return list;
}

template <typename Element, typename Allocator>
template <typename Function>
auto List<Element, Allocator>::transformed(Function&& function) {
  using Invocation = std::remove_cvref_t<std::invoke_result_t<Function&, reference>>;
  using Output = List<typename AlgorithmResultInfo<Invocation>::Value>;
  return Star::transform<Output>(*this, std::forward<Function>(function));
}

template <typename Element, typename Allocator>
template <typename Function>
auto List<Element, Allocator>::transformed(Function&& function) const {
  using Invocation = std::remove_cvref_t<std::invoke_result_t<Function&, const_reference>>;
  using Output = List<typename AlgorithmResultInfo<Invocation>::Value>;
  return Star::transform<Output>(*this, std::forward<Function>(function));
}

template <typename Element, std::size_t MaxSize>
template <typename Container>
auto StaticList<Element, MaxSize>::from(Container const& c) -> std::expected<StaticList, StarException> {
  return create(c.begin(), c.end());
}

template <typename Element, std::size_t MaxSize>
template <typename Iterator>
auto StaticList<Element, MaxSize>::create(Iterator first, Iterator last) -> std::expected<StaticList, StarException> {
  StaticList result;
  auto added = result.insert(result.end(), first, last);
  if (!added)
    return std::unexpected(std::move(added.error()));
  return result;
}

template <typename Element, std::size_t MaxSize>
auto StaticList<Element, MaxSize>::create(std::initializer_list<Element> list) -> std::expected<StaticList, StarException> {
  return create(list.begin(), list.end());
}

template <typename Element, std::size_t MaxSize>
auto StaticList<Element, MaxSize>::create(std::size_t size, Element const& value) -> std::expected<StaticList, StarException> {
  StaticList result;
  auto resized = result.resize(size, value);
  if (!resized)
    return std::unexpected(std::move(resized.error()));
  return result;
}

template <typename Element, std::size_t MaxSize>
auto StaticList<Element, MaxSize>::slice(SliceIndex a, SliceIndex b, int i) const -> std::expected<StaticList, StarException> {
  return Star::slice(*this, a, b, i);
}

template <typename Element, std::size_t MaxSize>
template <typename Comparator>
auto StaticList<Element, MaxSize>::sorted(Comparator&& comparator) const -> StaticList {
  static_assert(std::is_invocable_r_v<bool, Comparator&, reference, reference>,
      "Sorting requires a nonfallible bool comparator; precompute fallible keys first");
  StaticList list(*this);
  list.sort(std::forward<Comparator>(comparator));
  return list;
}

template <typename Element, std::size_t MaxSize>
StaticList<Element, MaxSize> StaticList<Element, MaxSize>::sorted() const {
  StaticList list(*this);
  list.sort();
  return list;
}

template <typename Element, std::size_t MaxSize>
template <typename Function>
auto StaticList<Element, MaxSize>::transformed(Function&& function) {
  using Invocation = std::remove_cvref_t<std::invoke_result_t<Function&, reference>>;
  using Output = StaticList<typename AlgorithmResultInfo<Invocation>::Value, MaxSize>;
  return Star::transform<Output>(*this, std::forward<Function>(function));
}

template <typename Element, std::size_t MaxSize>
template <typename Function>
auto StaticList<Element, MaxSize>::transformed(Function&& function) const {
  using Invocation = std::remove_cvref_t<std::invoke_result_t<Function&, const_reference>>;
  using Output = StaticList<typename AlgorithmResultInfo<Invocation>::Value, MaxSize>;
  return Star::transform<Output>(*this, std::forward<Function>(function));
}

template <typename Element, std::size_t MaxStackSize>
template <typename Container>
SmallList<Element, MaxStackSize> SmallList<Element, MaxStackSize>::from(Container const& c) {
  return SmallList(c.begin(), c.end());
}

template <typename Element, std::size_t MaxStackSize>
auto SmallList<Element, MaxStackSize>::slice(SliceIndex a, SliceIndex b, int i) const -> SmallList {
  return Star::slice(*this, a, b, i);
}

template <typename Element, std::size_t MaxStackSize>
template <typename Comparator>
auto SmallList<Element, MaxStackSize>::sorted(Comparator&& comparator) const -> SmallList {
  static_assert(std::is_invocable_r_v<bool, Comparator&, reference, reference>,
      "Sorting requires a nonfallible bool comparator; precompute fallible keys first");
  SmallList list(*this);
  list.sort(std::forward<Comparator>(comparator));
  return list;
}

template <typename Element, std::size_t MaxStackSize>
SmallList<Element, MaxStackSize> SmallList<Element, MaxStackSize>::sorted() const {
  SmallList list(*this);
  list.sort();
  return list;
}

template <typename Element, std::size_t MaxStackSize>
template <typename Function>
auto SmallList<Element, MaxStackSize>::transformed(Function&& function) {
  using Invocation = std::remove_cvref_t<std::invoke_result_t<Function&, reference>>;
  using Output = SmallList<typename AlgorithmResultInfo<Invocation>::Value, MaxStackSize>;
  return Star::transform<Output>(*this, std::forward<Function>(function));
}

template <typename Element, std::size_t MaxStackSize>
template <typename Function>
auto SmallList<Element, MaxStackSize>::transformed(Function&& function) const {
  using Invocation = std::remove_cvref_t<std::invoke_result_t<Function&, const_reference>>;
  using Output = SmallList<typename AlgorithmResultInfo<Invocation>::Value, MaxStackSize>;
  return Star::transform<Output>(*this, std::forward<Function>(function));
}

template <typename Element, typename Allocator>
template <typename Container>
Deque<Element, Allocator> Deque<Element, Allocator>::from(Container const& c) {
  return Deque(c.begin(), c.end());
}

template <typename Element, typename Allocator>
Deque<Element, Allocator> Deque<Element, Allocator>::slice(SliceIndex a, SliceIndex b, int i) const {
  return Star::slice(*this, a, b, i);
}

template <typename Element, typename Allocator>
template <typename Comparator>
Deque<Element, Allocator> Deque<Element, Allocator>::sorted(Comparator&& comparator) const {
  static_assert(std::is_invocable_r_v<bool, Comparator&, reference, reference>,
      "Sorting requires a nonfallible bool comparator; precompute fallible keys first");
  Deque l(*this);
  l.sort(std::forward<Comparator>(comparator));
  return l;
}

template <typename Element, typename Allocator>
Deque<Element, Allocator> Deque<Element, Allocator>::sorted() const {
  Deque l(*this);
  l.sort();
  return l;
}

template <typename Element, typename Allocator>
template <typename Function>
auto Deque<Element, Allocator>::transformed(Function&& function) {
  using Invocation = std::remove_cvref_t<std::invoke_result_t<Function&, reference>>;
  return Star::transform<Deque<typename AlgorithmResultInfo<Invocation>::Value>>(*this, std::forward<Function>(function));
}

template <typename Element, typename Allocator>
template <typename Function>
auto Deque<Element, Allocator>::transformed(Function&& function) const {
  using Invocation = std::remove_cvref_t<std::invoke_result_t<Function&, const_reference>>;
  return Star::transform<Deque<typename AlgorithmResultInfo<Invocation>::Value>>(*this, std::forward<Function>(function));
}

template <typename Element, typename Allocator>
template <typename Container>
LinkedList<Element, Allocator> LinkedList<Element, Allocator>::from(Container const& c) {
  return LinkedList(c.begin(), c.end());
}

template <typename Element, typename Allocator>
void LinkedList<Element, Allocator>::appendAll(LinkedList list) {
  m_storage.splice(m_storage.end(), list.m_storage);
}

template <typename Element, typename Allocator>
void LinkedList<Element, Allocator>::prependAll(LinkedList list) {
  m_storage.splice(m_storage.begin(), list.m_storage);
}

template <typename Element, typename Allocator>
template <typename Container>
void LinkedList<Element, Allocator>::appendAll(Container&& list) {
  for (auto& e : list) {
    if constexpr (std::is_rvalue_reference_v<Container&&>)
      m_storage.push_back(std::move(e));
    else
      m_storage.push_back(e);
  }
}

template <typename Element, typename Allocator>
template <typename Container>
void LinkedList<Element, Allocator>::prependAll(Container&& list) {
  for (auto i = std::rbegin(list); i != std::rend(list); ++i) {
    if constexpr (std::is_rvalue_reference_v<Container&&>)
      m_storage.push_front(std::move(*i));
    else
      m_storage.push_front(*i);
  }
}

template <typename Element, typename Allocator>
template <typename Comparator>
LinkedList<Element, Allocator> LinkedList<Element, Allocator>::sorted(Comparator&& comparator) const {
  static_assert(std::is_invocable_r_v<bool, Comparator&, reference, reference>,
      "Sorting requires a nonfallible bool comparator; precompute fallible keys first");
  LinkedList l(*this);
  l.sort(std::forward<Comparator>(comparator));
  return l;
}

template <typename Element, typename Allocator>
LinkedList<Element, Allocator> LinkedList<Element, Allocator>::sorted() const {
  LinkedList l(*this);
  l.sort();
  return l;
}

template <typename Element, typename Allocator>
template <typename Function>
auto LinkedList<Element, Allocator>::transformed(Function&& function) {
  using Invocation = std::remove_cvref_t<std::invoke_result_t<Function&, reference>>;
  return Star::transform<LinkedList<typename AlgorithmResultInfo<Invocation>::Value>>(*this, std::forward<Function>(function));
}

template <typename Element, typename Allocator>
template <typename Function>
auto LinkedList<Element, Allocator>::transformed(Function&& function) const {
  using Invocation = std::remove_cvref_t<std::invoke_result_t<Function&, const_reference>>;
  return Star::transform<LinkedList<typename AlgorithmResultInfo<Invocation>::Value>>(*this, std::forward<Function>(function));
}

template <typename T>
decltype(auto) listDiagnosticValue(T const& value) {
  if constexpr (requires { outputOptional(value); })
    return outputOptional(value);
  else
    return (value);
}

template <typename Sequence>
using ListDiagnosticResult = decltype(std::declval<std::ostream&>() << listDiagnosticValue(std::declval<typename Sequence::value_type const&>()));

template <SequenceContainer Sequence>
std::ostream& operator<<(std::ostream& os, Sequence const& list)
    requires std::same_as<ListDiagnosticResult<Sequence>, std::ostream&>;

template <SequenceContainer Sequence>
[[nodiscard]]
std::expected<std::reference_wrapper<std::ostream>, StarException> operator<<(std::ostream& os, Sequence const& list)
    requires std::same_as<ListDiagnosticResult<Sequence>, std::expected<std::reference_wrapper<std::ostream>, StarException>>;

template <SequenceContainer Sequence>
std::ostream& operator<<(std::ostream& os, Sequence const& list)
    requires std::same_as<ListDiagnosticResult<Sequence>, std::ostream&> {
  os << "(";
  for (auto i = list.begin(); i != list.end(); ++i) {
    if (i != list.begin())
      os << ", ";
    os << listDiagnosticValue(*i);
  }
  return os << ")";
}

template <SequenceContainer Sequence>
[[nodiscard]]
std::expected<std::reference_wrapper<std::ostream>, StarException> operator<<(std::ostream& os, Sequence const& list)
    requires std::same_as<ListDiagnosticResult<Sequence>, std::expected<std::reference_wrapper<std::ostream>, StarException>> {
  os << "(";
  for (auto i = list.begin(); i != list.end(); ++i) {
    if (i != list.begin())
      os << ", ";
    auto written = os << listDiagnosticValue(*i);
    if (!written)
      return std::unexpected(std::move(written.error()));
  }
  os << ")";
  return std::ref(os);
}

template <typename List>
std::size_t ListHasher<List>::operator()(List const& l) const {
  std::size_t h = 0;
  for (auto const& e : l)
    hashCombine(h, elemHasher(e));
  return h;
}

}

export module star.list;

export namespace Star {
  template <typename Element, typename Allocator>
  struct hash<List<Element, Allocator>>;
  template <typename Element, std::size_t MaxStackSize>
  struct hash<StaticList<Element, MaxStackSize>>;
  template <typename Element, std::size_t MaxStackSize>
  struct hash<SmallList<Element, MaxStackSize>>;
  template <typename Element, typename Allocator>
  struct hash<Deque<Element, Allocator>>;
  template <typename Element, typename Allocator>
  struct hash<LinkedList<Element, Allocator>>;
}

export template <typename Element, typename Allocator>
  requires std::same_as<decltype(std::declval<std::ostream&>() << std::declval<Star::List<Element, Allocator> const&>()), std::ostream&>
struct std::formatter<Star::List<Element, Allocator>> : Star::StreamFormatter {};

export template <typename Element, std::size_t Capacity>
  requires std::same_as<decltype(std::declval<std::ostream&>() << std::declval<Star::StaticList<Element, Capacity> const&>()), std::ostream&>
struct std::formatter<Star::StaticList<Element, Capacity>> : Star::StreamFormatter {};

export template <typename Element, std::size_t Capacity>
  requires std::same_as<decltype(std::declval<std::ostream&>() << std::declval<Star::SmallList<Element, Capacity> const&>()), std::ostream&>
struct std::formatter<Star::SmallList<Element, Capacity>> : Star::StreamFormatter {};

export template <typename Element, typename Allocator>
  requires std::same_as<decltype(std::declval<std::ostream&>() << std::declval<Star::Deque<Element, Allocator> const&>()), std::ostream&>
struct std::formatter<Star::Deque<Element, Allocator>> : Star::StreamFormatter {};

export template <typename Element, typename Allocator>
  requires std::same_as<decltype(std::declval<std::ostream&>() << std::declval<Star::LinkedList<Element, Allocator> const&>()), std::ostream&>
struct std::formatter<Star::LinkedList<Element, Allocator>> : Star::StreamFormatter {};

export namespace Star {
  using ::Star::SequenceContainer;
  using ::Star::ListHasher;
  using ::Star::List;
  using ::Star::StaticList;
  using ::Star::SmallList;
  using ::Star::Deque;
  using ::Star::LinkedList;
  using ::Star::operator<<;
  using ::Star::hash;
}
