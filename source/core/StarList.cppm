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

template <typename Left, typename Right>
concept sequence_equal =
  requires(Left&& left, Right&& right) { static_cast<bool>(std::forward<Left>(left) == std::forward<Right>(right)); };

template <typename Left, typename Right>
concept sequence_less =
  requires(Left&& left, Right&& right) { static_cast<bool>(std::forward<Left>(left) < std::forward<Right>(right)); };

template <typename Function, typename... Args>
concept sequence_callable = std::invocable<Function, Args...>;

template <typename Function, typename... Args>
concept sequence_predicate = sequence_callable<Function, Args...>
  && ((!AlgorithmResultInfo<std::remove_cvref_t<std::invoke_result_t<Function, Args...>>>::Fallible
       && requires(Function function,
                   Args... args) { static_cast<bool>(std::invoke(function, std::forward<Args>(args)...)); })
      || (AlgorithmResultInfo<std::remove_cvref_t<std::invoke_result_t<Function, Args...>>>::Fallible
          && requires(Function function, Args... args) {
               static_cast<bool>(*std::invoke(function, std::forward<Args>(args)...));
             }));

template <typename Function, typename... Args>
concept sequence_comparator = sequence_predicate<Function, Args...>
  && (!AlgorithmResultInfo<std::remove_cvref_t<std::invoke_result_t<Function, Args...>>>::Fallible);

template <typename Function, typename Reference>
concept sequence_transform = sequence_callable<Function, Reference>
  && ((!AlgorithmResultInfo<std::remove_cvref_t<std::invoke_result_t<Function, Reference>>>::Fallible
       && requires(Function function,
                   Reference value) { value = std::invoke(function, std::forward<Reference>(value)); })
      || (AlgorithmResultInfo<std::remove_cvref_t<std::invoke_result_t<Function, Reference>>>::Fallible
          && requires(Function function, Reference value) {
               value = *std::invoke(function, std::forward<Reference>(value));
             }));

template <typename Function, typename Reference>
concept sequence_mapping =
  sequence_callable<Function, Reference>
  && std::is_object_v<
    typename AlgorithmResultInfo<std::remove_cvref_t<std::invoke_result_t<Function, Reference>>>::Value>
  && std::is_move_constructible_v<
    typename AlgorithmResultInfo<std::remove_cvref_t<std::invoke_result_t<Function, Reference>>>::Value>;

template <typename Range>
using sequence_source_reference =
  std::conditional_t<std::is_rvalue_reference_v<Range&&>, std::ranges::range_rvalue_reference_t<Range>,
                     std::ranges::range_reference_t<Range>>;

template <typename Range, typename Element>
concept sequence_source =
  std::ranges::input_range<Range> && std::is_constructible_v<Element, sequence_source_reference<Range>>;

template <typename Result>
concept sequence_status = std::same_as<Result, void> || std::same_as<Result, std::expected<void, StarException>>;

template <typename Sequence>
concept sequence_alias_target = SequenceContainer<Sequence> && std::ranges::forward_range<Sequence>
  && std::ranges::common_range<Sequence> && std::ranges::sized_range<Sequence> && requires(Sequence& sequence) {
       typename Sequence::value_type;
       typename Sequence::reference;
       typename Sequence::iterator;
       typename Sequence::const_iterator;
       { sequence.size() } -> std::convertible_to<std::size_t>;
       { sequence.empty() } -> std::convertible_to<bool>;
     };

template <typename Sequence>
concept sequence_append_target = sequence_alias_target<Sequence> && requires(Sequence& sequence) {
  typename Sequence::AppendResult;
  requires sequence_status<typename Sequence::AppendResult>;
  { sequence.max_size() } -> std::convertible_to<std::size_t>;
};

template <typename Sequence>
concept sequence_set_target =
  SequenceContainer<Sequence> && requires(Sequence& sequence, std::size_t position, typename Sequence::value_type&& value) {
    { sequence.size() } -> std::convertible_to<std::size_t>;
    requires sequence_status<decltype(sequence.resize(position))>;
    sequence[position] = std::move(value);
  } && (!std::is_void_v<decltype(std::declval<Sequence&>().resize(std::size_t{}))> || requires(Sequence const& sequence) {
    { sequence.max_size() } -> std::convertible_to<std::size_t>;
  });

namespace SequenceAlgorithms {

template <sequence_set_target Sequence>
[[nodiscard]] auto set(Sequence& sequence, std::size_t pos, typename Sequence::value_type&& value)
  -> std::expected<void, StarException> {
  if (pos == std::numeric_limits<std::size_t>::max())
    return std::unexpected(OutOfRangeException("sequence::set index overflows size"));
  if constexpr (std::is_void_v<decltype(sequence.resize(pos + 1))>) {
    if (pos >= sequence.max_size())
      return std::unexpected(OutOfRangeException("sequence::set index exceeds maximum size"));
  }
  if (pos >= sequence.size()) {
    if constexpr (std::is_void_v<decltype(sequence.resize(pos + 1))>) {
      sequence.resize(pos + 1);
    } else {
      auto resized = sequence.resize(pos + 1);
      if (!resized)
        return std::unexpected(std::move(resized.error()));
    }
  }
  sequence[pos] = std::move(value);
  return {};
}

template <sequence_alias_target Sequence, std::input_or_output_iterator Iterator>
bool iterator_aliases(Sequence& sequence, Iterator const& iterator) {
  if (sequence.empty())
    return false;
  if constexpr (std::ranges::contiguous_range<Sequence> && std::contiguous_iterator<Iterator>
                && std::same_as<std::iter_value_t<Iterator>, typename Sequence::value_type>) {
    std::less<typename Sequence::value_type const*> less;
    auto pointer = std::to_address(iterator);
    auto first = std::ranges::data(sequence);
    return !less(pointer, first) && !less(first + sequence.size(), pointer);
  } else if constexpr (std::same_as<Iterator, typename Sequence::iterator>
                       || std::same_as<Iterator, typename Sequence::const_iterator>) {
    for (auto position = sequence.begin(); position != sequence.end(); ++position)
      if (position == iterator)
        return true;
    return iterator == sequence.end();
  } else if constexpr (requires { iterator.base(); }) {
    using Base = std::remove_cvref_t<decltype(iterator.base())>;
    if constexpr (std::input_or_output_iterator<Base> && !std::same_as<Base, Iterator>)
      return iterator_aliases(sequence, iterator.base());
    else
      return false;
  } else {
    return false;
  }
}

template <sequence_alias_target Sequence, std::ranges::input_range Source>
bool aliases(Sequence& sequence, Source& source) {
  if (static_cast<void const*>(std::addressof(sequence)) == static_cast<void const*>(std::addressof(source)))
    return true;
  if constexpr (std::ranges::forward_range<Source>) {
    if (std::ranges::empty(source) || sequence.empty())
      return false;
    if constexpr (std::ranges::contiguous_range<Source>
                  && std::same_as<std::ranges::range_value_t<Source>, typename Sequence::value_type>
                  && std::is_lvalue_reference_v<typename Sequence::reference>) {
      // Contiguous data access does not evaluate an adaptor's payload projection.
      auto pointer = std::ranges::data(source);
      if constexpr (std::ranges::contiguous_range<Sequence>) {
        std::less<typename Sequence::value_type const*> less;
        auto first = std::ranges::data(sequence);
        return !less(pointer, first) && less(pointer, first + sequence.size());
      } else {
        for (auto& element : sequence)
          if (std::addressof(element) == pointer)
            return true;
      }
    } else {
      return iterator_aliases(sequence, std::ranges::begin(source));
    }
  }
  return false;
}

template <sequence_append_target Sequence, std::ranges::input_range Source>
void reserve_append(Sequence& sequence, Source& source) {
  if constexpr (std::ranges::sized_range<Source> && requires { sequence.reserve(std::size_t{}); }) {
    auto added = std::ranges::size(source);
    auto maximum = static_cast<std::size_t>(sequence.max_size());
    if (added > maximum - sequence.size())
      return;// Preserve the storage's ordinary first-error/partial-prefix behavior.
    auto needed = sequence.size() + static_cast<std::size_t>(added);
    if constexpr (requires { sequence.capacity(); }) {
      auto capacity = static_cast<std::size_t>(sequence.capacity());
      if (needed > capacity) {
        auto growth = capacity > maximum - capacity ? maximum : capacity + capacity;
        sequence.reserve(std::max(needed, growth));
      }
    } else {
      // SmallVector already applies geometric growth in reserve().
      sequence.reserve(needed);
    }
  }
}

template <typename Sequence, typename Source>
concept append_source = sequence_append_target<Sequence> && sequence_source<Source, typename Sequence::value_type>
  && requires(Sequence& sequence, sequence_source_reference<Source> value) {
       sequence.emplace_back(std::forward<sequence_source_reference<Source>>(value));
     };

template <typename Sequence, typename Source>
  requires append_source<Sequence, Source>
auto append_range(Sequence& sequence, Source&& source) -> typename Sequence::AppendResult;

template <typename Sequence, typename Source>
  requires append_source<Sequence, Source>
auto append_overlapping_range(Sequence& sequence, Source&& source) -> typename Sequence::AppendResult {
  using Result = typename Sequence::AppendResult;
  // Keep snapshot allocation out of the ordinary append path. Bounded storage
  // must not consume elements past its successful prefix.
  std::vector<typename Sequence::value_type> snapshot;
  std::size_t limit = sequence.max_size() - sequence.size();
  if constexpr (std::ranges::sized_range<Source>) {
    auto count = std::ranges::size(source);
    snapshot.reserve(count > limit ? limit : static_cast<std::size_t>(count));
  }
  bool overflow = false;
  for (auto&& element : source) {
    if (snapshot.size() == limit) {
      overflow = true;
      break;
    }
    if constexpr (std::is_rvalue_reference_v<Source&&>)
      snapshot.emplace_back(std::move(element));
    else
      snapshot.emplace_back(std::forward<decltype(element)>(element));
  }
  if constexpr (std::is_void_v<Result>) {
    append_range(sequence, std::move(snapshot));
  } else {
    auto appended = append_range(sequence, std::move(snapshot));
    if (!appended)
      return std::unexpected(std::move(appended.error()));
    if (overflow)
      return std::unexpected(StaticVectorSizeException("append_range exceeds sequence capacity"));
    return {};
  }
}

template <typename Sequence, typename Source>
  requires append_source<Sequence, Source>
auto append_range(Sequence& sequence, Source&& source) -> typename Sequence::AppendResult {
  using Result = typename Sequence::AppendResult;
  // Detect self, contiguous borrowed storage, and forward iterator/base overlap
  // without evaluating source payload callbacks. Opaque custom borrowing and
  // callback-produced references must not overlap destination storage unless
  // their underlying iterator/base already identifies that overlap. Single-pass
  // sources likewise must not borrow destination storage.
  if (aliases(sequence, source))
    return append_overlapping_range(sequence, std::forward<Source>(source));

  reserve_append(sequence, source);
  // Bulk construction is equivalent only when elements, conversions and the
  // allocator cannot observe the intermediate size of the destination.
  if constexpr (std::is_void_v<Result> && std::ranges::contiguous_range<Source> && std::ranges::common_range<Source>
                && std::same_as<std::ranges::range_value_t<Source>, typename Sequence::value_type>
                && std::is_trivially_copyable_v<typename Sequence::value_type>
                && std::is_move_assignable_v<typename Sequence::value_type>
                && std::is_assignable_v<typename Sequence::value_type&, sequence_source_reference<Source>> && requires {
                     typename Sequence::allocator_type;
                     requires std::same_as<typename Sequence::allocator_type,
                                           std::allocator<typename Sequence::value_type>>;
                   }) {
    if constexpr (std::is_rvalue_reference_v<Source&&>)
      sequence.insert(sequence.end(), std::make_move_iterator(std::ranges::begin(source)),
                      std::make_move_iterator(std::ranges::end(source)));
    else
      sequence.insert(sequence.end(), std::ranges::begin(source), std::ranges::end(source));
    return;
  }
  for (auto&& element : source) {
    auto add = [&]() -> decltype(auto) {
      if constexpr (std::is_rvalue_reference_v<Source&&>)
        return sequence.emplace_back(std::move(element));
      else
        return sequence.emplace_back(std::forward<decltype(element)>(element));
    };
    if constexpr (std::is_void_v<Result>) {
      add();
    } else {
      auto added = add();
      if (!added)
        return std::unexpected(std::move(added.error()));
    }
  }
  if constexpr (!std::is_void_v<Result>)
    return {};
}

template <sequence_append_target Sequence, typename Source>
  requires std::ranges::random_access_range<Sequence> && sequence_source<Source const&, typename Sequence::value_type>
  && std::ranges::common_range<Source const>
  && requires(Sequence& sequence, Source const& source, std::size_t position) {
       sequence.insert(sequence.begin() + position, std::ranges::begin(source), std::ranges::end(source));
     }
auto insert_range_at(Sequence& sequence, std::size_t position, Source const& source) ->
  typename Sequence::AppendResult {
  using Result = typename Sequence::AppendResult;
  if (aliases(sequence, source)) {
    std::vector<typename Sequence::value_type> snapshot(std::ranges::begin(source), std::ranges::end(source));
    return insert_range_at(sequence, position, snapshot);
  }
  if constexpr (std::is_void_v<Result>) {
    sequence.insert(sequence.begin() + position, std::ranges::begin(source), std::ranges::end(source));
  } else {
    auto inserted = sequence.insert(sequence.begin() + position, std::ranges::begin(source), std::ranges::end(source));
    if (!inserted)
      return std::unexpected(std::move(inserted.error()));
    return {};
  }
}

template <std::ranges::forward_range Range>
  requires sequence_equal<std::ranges::range_reference_t<Range>, std::ranges::range_value_t<Range> const&>
  && requires(Range& range, std::ranges::iterator_t<Range> position) { range.erase(position); }
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
  requires sequence_predicate<Filter&, std::ranges::range_reference_t<Range>>
  && requires(Range& range, std::ranges::iterator_t<Range> first, std::ranges::iterator_t<Range> last,
              std::ranges::range_reference_t<Range> destination, std::ranges::range_rvalue_reference_t<Range> source) {
       destination = std::forward<std::ranges::range_rvalue_reference_t<Range>>(source);
       range.erase(first, last);
     }
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
  requires sequence_callable<Function&, std::ranges::range_reference_t<Range>>
auto exec(Range& list, Function&& function) {
  using reference = std::ranges::range_reference_t<Range>;
  using Invocation = std::remove_cvref_t<std::invoke_result_t<Function&, reference>>;
  using Status = std::conditional_t<AlgorithmResultInfo<Invocation>::Fallible, std::expected<void, StarException>, void>;
  if constexpr (AlgorithmResultInfo<Invocation>::Fallible) {
    for (auto&& e : list) {
      auto result = std::invoke(function, std::forward<decltype(e)>(e));
      if (!result)
        return Status(std::unexpected(std::move(result.error())));
    }
    return Status();
  } else {
    for (auto&& value : list)
      std::invoke(function, std::forward<decltype(value)>(value));
  }
}

template <std::ranges::forward_range Range, typename Function>
  requires sequence_transform<Function&, std::ranges::range_reference_t<Range>>
auto transform(Range& list, Function&& function) {
  using reference = std::ranges::range_reference_t<Range>;
  using Invocation = std::remove_cvref_t<std::invoke_result_t<Function&, reference>>;
  using Status = std::conditional_t<AlgorithmResultInfo<Invocation>::Fallible, std::expected<void, StarException>, void>;
  if constexpr (AlgorithmResultInfo<Invocation>::Fallible) {
    for (auto&& e : list) {
      auto result = std::invoke(function, std::forward<decltype(e)>(e));
      if (!result)
        return Status(std::unexpected(std::move(result.error())));
      e = std::move(*result);
    }
    return Status();
  } else {
    for (auto&& value : list)
      value = std::invoke(function, std::forward<decltype(value)>(value));
  }
}

template <std::ranges::forward_range Range, typename Function>
  requires sequence_predicate<Function const&, std::ranges::range_reference_t<Range>>
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
  requires sequence_predicate<Function const&, std::ranges::range_reference_t<Range>>
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
  requires SequenceContainer<List const>
class ListHasher {
public:
  std::size_t operator()(List const& l) const
    requires hash_invocable<hash<typename List::value_type> const&, std::ranges::range_reference_t<List const>>;

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
  using size_type = typename Storage::size_type;
  using difference_type = typename Storage::difference_type;
  using pointer = typename Storage::pointer;
  using const_pointer = typename Storage::const_pointer;
  using allocator_type = typename Storage::allocator_type;
  using AppendResult = void;

  constexpr List() noexcept(std::is_nothrow_default_constructible_v<Storage>) = default;
  constexpr List(Storage const& storage) : m_storage(storage) {}
  constexpr List(Storage&& storage) noexcept(std::is_nothrow_move_constructible_v<Storage>)
      : m_storage(std::move(storage)) {}
  explicit List(std::size_t length, value_type const& value = value_type()) : m_storage(length, value) {}
  explicit List(std::span<value_type const> values) : m_storage(values.begin(), values.end()) {}
  template <std::input_iterator Iterator>
    requires std::sentinel_for<Iterator, Iterator>
    && std::is_constructible_v<value_type, std::iter_reference_t<Iterator>>
  List(Iterator first, Iterator last) : m_storage(std::move(first), std::move(last)) {}
  template <std::input_iterator Iterator, std::sentinel_for<Iterator> Sentinel>
    requires(!std::same_as<Iterator, Sentinel>) && std::is_constructible_v<value_type, std::iter_reference_t<Iterator>>
  List(Iterator first, Sentinel last) {
    if constexpr (std::sized_sentinel_for<Sentinel, Iterator>)
      reserve(static_cast<std::size_t>(last - first));
    for (; first != last; ++first)
      emplace_back(*first);
  }
  template <typename Range>
    requires sequence_source<Range, value_type>
  explicit List(std::from_range_t, Range&& range) {
    append_range(std::forward<Range>(range));
  }
  List(std::initializer_list<value_type> values) : m_storage(values) {}

  constexpr iterator begin() noexcept(noexcept(m_storage.begin())) { return m_storage.begin(); }
  constexpr const_iterator begin() const noexcept(noexcept(m_storage.begin())) { return m_storage.begin(); }
  constexpr const_iterator cbegin() const noexcept(noexcept(m_storage.begin())) { return m_storage.begin(); }
  constexpr iterator end() noexcept(noexcept(m_storage.end())) { return m_storage.end(); }
  constexpr const_iterator end() const noexcept(noexcept(m_storage.end())) { return m_storage.end(); }
  constexpr const_iterator cend() const noexcept(noexcept(m_storage.end())) { return m_storage.end(); }
  constexpr reverse_iterator rbegin() noexcept(noexcept(m_storage.rbegin())) { return m_storage.rbegin(); }
  constexpr const_reverse_iterator rbegin() const noexcept(noexcept(m_storage.rbegin())) { return m_storage.rbegin(); }
  constexpr const_reverse_iterator crbegin() const noexcept(noexcept(m_storage.rbegin())) { return m_storage.rbegin(); }
  constexpr reverse_iterator rend() noexcept(noexcept(m_storage.rend())) { return m_storage.rend(); }
  constexpr const_reverse_iterator rend() const noexcept(noexcept(m_storage.rend())) { return m_storage.rend(); }
  constexpr const_reverse_iterator crend() const noexcept(noexcept(m_storage.rend())) { return m_storage.rend(); }
  constexpr size_type size() const noexcept(noexcept(m_storage.size())) { return m_storage.size(); }
  constexpr bool empty() const noexcept(noexcept(m_storage.empty())) { return m_storage.empty(); }
  void clear() { m_storage.clear(); }
  constexpr reference front() noexcept(noexcept(*begin())) { return *begin(); }
  constexpr const_reference front() const noexcept(noexcept(*begin())) { return *begin(); }
  constexpr reference back() noexcept(noexcept(*std::prev(end()))) { return *std::prev(end()); }
  constexpr const_reference back() const noexcept(noexcept(*std::prev(end()))) { return *std::prev(end()); }
  void push_back(value_type const& value)
    requires std::is_constructible_v<value_type, value_type const&>
  {
    m_storage.push_back(value);
  }
  void push_back(value_type&& value)
    requires std::is_constructible_v<value_type, value_type&&>
  {
    m_storage.push_back(std::move(value));
  }
  void pop_back() noexcept(noexcept(m_storage.pop_back())) { m_storage.pop_back(); }
  template <typename... Args>
    requires std::is_constructible_v<value_type, Args...>
  decltype(auto) emplace_back(Args&&... args) noexcept(noexcept(m_storage.emplace_back(std::forward<Args>(args)...))) {
    return m_storage.emplace_back(std::forward<Args>(args)...);
  }
  decltype(auto) resize(std::size_t size)
    requires std::is_default_constructible_v<value_type>
  {
    return m_storage.resize(size);
  }
  decltype(auto) resize(std::size_t size, value_type const& value)
    requires std::is_constructible_v<value_type, value_type const&>
  {
    return m_storage.resize(size, value);
  }
  decltype(auto) insert(const_iterator pos, value_type const& value) { return m_storage.insert(pos, value); }
  decltype(auto) insert(const_iterator pos, value_type&& value) { return m_storage.insert(pos, std::move(value)); }
  decltype(auto) insert(const_iterator pos, std::initializer_list<value_type> values) {
    return m_storage.insert(pos, values);
  }
  template <std::input_iterator Iterator>
    requires std::is_constructible_v<value_type, std::iter_reference_t<Iterator>>
  decltype(auto) insert(const_iterator pos, Iterator first, Iterator last) {
    return m_storage.insert(pos, first, last);
  }
  decltype(auto) insert(const_iterator pos, std::size_t count, value_type const& value)
    requires std::is_constructible_v<value_type, value_type const&>
  {
    return m_storage.insert(pos, count, value);
  }
  decltype(auto) erase(const_iterator pos) noexcept(noexcept(m_storage.erase(pos))) { return m_storage.erase(pos); }
  decltype(auto) erase(const_iterator first, const_iterator last) noexcept(noexcept(m_storage.erase(first, last))) {
    return m_storage.erase(first, last);
  }
  template <typename... Args>
    requires std::is_constructible_v<value_type, Args...>
  decltype(auto) emplace(const_iterator pos,
                         Args&&... args) noexcept(noexcept(m_storage.emplace(pos, std::forward<Args>(args)...))) {
    return m_storage.emplace(pos, std::forward<Args>(args)...);
  }
  decltype(auto) assign(std::initializer_list<value_type> values)
    requires std::is_constructible_v<value_type, value_type const&>
  {
    return m_storage.assign(values);
  }
  template <std::input_iterator Iterator>
    requires std::is_constructible_v<value_type, std::iter_reference_t<Iterator>>
  decltype(auto) assign(Iterator first, Iterator last) {
    return m_storage.assign(first, last);
  }
  decltype(auto) assign(std::size_t count, value_type const& value)
    requires std::is_constructible_v<value_type, value_type const&>
  {
    return m_storage.assign(count, value);
  }
  constexpr void reserve(size_type capacity) { m_storage.reserve(capacity); }
  void shrink_to_fit() { m_storage.shrink_to_fit(); }
  constexpr size_type capacity() const noexcept(noexcept(m_storage.capacity())) { return m_storage.capacity(); }
  constexpr size_type max_size() const noexcept(noexcept(m_storage.max_size())) { return m_storage.max_size(); }
  constexpr allocator_type get_allocator() const noexcept(noexcept(m_storage.get_allocator())) {
    return m_storage.get_allocator();
  }
  constexpr value_type* data() noexcept
    requires requires(Storage& storage) { storage.data(); }
  {
    return m_storage.data();
  }
  constexpr value_type const* data() const noexcept
    requires requires(Storage const& storage) { storage.data(); }
  {
    return m_storage.data();
  }
  void swap(List& other) noexcept(noexcept(m_storage.swap(other.m_storage))) { m_storage.swap(other.m_storage); }
  bool operator==(List const& other) const { return m_storage == other.m_storage; }
  bool operator<(List const& other) const { return m_storage < other.m_storage; }
  bool operator>(List const& other) const { return other < *this; }
  bool operator<=(List const& other) const { return !(other < *this); }
  bool operator>=(List const& other) const { return !(*this < other); }

  template <typename Container>
    requires sequence_source<Container const&, Element>
  static List from(Container const& c);

  List slice(SliceIndex a = SliceIndex(), SliceIndex b = SliceIndex(), int i = 1) const;

  template <typename Comparator>
    requires sequence_comparator<Comparator&, reference, reference>
  List sorted(Comparator&& comparator) const
    requires std::is_constructible_v<value_type, const_reference>;
  List sorted() const
    requires sequence_less<reference, reference> && std::is_constructible_v<value_type, const_reference>;

  template <typename Function>
    requires sequence_mapping<Function&, reference>
  [[nodiscard]]
  auto transformed(Function&& function);

  template <typename Function>
    requires sequence_mapping<Function&, const_reference>
  [[nodiscard]]
  auto transformed(Function&& function) const;

  template <typename Container>
    requires sequence_source<Container, value_type>
  auto append_range(Container&& list) -> AppendResult {
    return SequenceAlgorithms::append_range(*this, std::forward<Container>(list));
  }

  auto first() & -> std::expected<std::reference_wrapper<value_type>, StarException>
    requires std::is_lvalue_reference_v<reference>
  {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("first() called on empty list"));
    return std::reference_wrapper<value_type>(*m_storage.begin());
  }

  auto first() const& -> std::expected<std::reference_wrapper<value_type const>, StarException>
    requires std::is_lvalue_reference_v<const_reference>
  {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("first() called on empty list"));
    return std::reference_wrapper<value_type const>(*m_storage.begin());
  }

  auto last() & -> std::expected<std::reference_wrapper<value_type>, StarException>
    requires std::is_lvalue_reference_v<reference>
  {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("last() called on empty list"));
    return std::reference_wrapper<value_type>(*std::prev(m_storage.end()));
  }

  auto last() const& -> std::expected<std::reference_wrapper<value_type const>, StarException>
    requires std::is_lvalue_reference_v<const_reference>
  {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("last() called on empty list"));
    return std::reference_wrapper<value_type const>(*std::prev(m_storage.end()));
  }

  auto front_value() const -> std::optional<value_type>
    requires std::is_constructible_v<value_type, const_reference>
  {
    if (m_storage.empty())
      return {};
    return std::optional<value_type>(std::in_place, *m_storage.begin());
  }

  auto back_value() const -> std::optional<value_type>
    requires std::is_constructible_v<value_type, const_reference>
  {
    if (m_storage.empty())
      return {};
    return std::optional<value_type>(std::in_place, *std::prev(m_storage.end()));
  }

  std::expected<void, StarException> pop_back_checked() {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("pop_back_checked() called on empty list"));
    m_storage.erase(std::prev(m_storage.end()));
    return {};
  }

  auto take_back() -> std::expected<value_type, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("take_back() called on empty list"));
    value_type e = std::move(*std::prev(m_storage.end()));
    m_storage.erase(std::prev(m_storage.end()));
    return e;
  }

  auto try_take_back() -> std::optional<value_type> {
    if (m_storage.empty())
      return {};
    value_type e = std::move(*std::prev(m_storage.end()));
    m_storage.erase(std::prev(m_storage.end()));
    return std::optional<value_type>(std::in_place, std::move(e));
  }

  void truncate_back(std::size_t maximumSize) {
    while (m_storage.size() > maximumSize)
      m_storage.erase(std::prev(m_storage.end()));
  }

  bool contains(value_type const& e) const
    requires sequence_equal<const_reference, value_type const&>
  {
    return std::find_if(begin(), end(), [&](auto&& value) { return static_cast<bool>(value == e); }) != end();
  }

  std::size_t remove(value_type const& e)
    requires sequence_equal<reference, value_type const&>
  {
    return SequenceAlgorithms::remove(*this, e);
  }

  template <typename Filter>
    requires sequence_predicate<Filter&, reference>
    && requires(reference slot, value_type&& value) { slot = std::move(value); }
  auto filter(Filter&& filter) {
    return SequenceAlgorithms::filter(*this, std::forward<Filter>(filter));
  }

  template <typename Self, typename Filter>
    requires std::is_copy_constructible_v<Self>
    && std::is_constructible_v<typename Self::value_type, typename Self::const_reference>
    && sequence_predicate<Filter&, typename Self::reference>
    && requires(typename Self::reference slot, typename Self::value_type&& value) { slot = std::move(value); }
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
    requires sequence_comparator<Comparator&, const_reference, const_reference>
  auto insert_sorted(value_type e, Comparator&& comparator) -> AppendResult {
    auto i = std::upper_bound(m_storage.begin(), m_storage.end(), e, [&](auto const& left, auto const& right) {
      return static_cast<bool>(std::invoke(comparator, left, right));
    });
    m_storage.insert(i, std::move(e));
  }

  auto insert_sorted(value_type e) -> AppendResult
    requires sequence_less<const_reference, const_reference>
  {
    return insert_sorted(std::move(e),
                         [](auto const& left, auto const& right) { return static_cast<bool>(left < right); });
  }

  template <typename Function>
    requires sequence_callable<Function&, reference>
  auto exec(Function&& function) {
    return SequenceAlgorithms::exec(*this, std::forward<Function>(function));
  }

  template <typename Function>
    requires sequence_callable<Function&, const_reference>
  auto exec(Function&& function) const {
    return SequenceAlgorithms::exec(*this, std::forward<Function>(function));
  }

  template <typename Function>
    requires sequence_transform<Function&, reference>
  auto transform(Function&& function) {
    return SequenceAlgorithms::transform(*this, std::forward<Function>(function));
  }

  template <typename Function>
    requires sequence_predicate<Function const&, const_reference>
  auto any(Function&& function) const {
    return SequenceAlgorithms::any(*this, std::forward<Function>(function));
  }

  auto any() const
    requires requires(const_reference value) { static_cast<bool>(value); }
  {
    return any([](auto const& value) -> auto const& { return value; });
  }

  template <typename Function>
    requires sequence_predicate<Function const&, const_reference>
  auto all(Function&& function) const {
    return SequenceAlgorithms::all(*this, std::forward<Function>(function));
  }

  auto all() const
    requires requires(const_reference value) { static_cast<bool>(value); }
  {
    return all([](auto const& value) -> auto const& { return value; });
  }

  template <typename Comparator>
    requires sequence_comparator<Comparator&, reference, reference>
  void sort(Comparator&& comparator) {
    std::sort(m_storage.begin(), m_storage.end(),
              [&](auto&& left, auto&& right) { return static_cast<bool>(std::invoke(comparator, left, right)); });
  }

  void sort()
    requires sequence_less<reference, reference>
  {
    sort([](auto const& left, auto const& right) { return static_cast<bool>(left < right); });
  }

  void reverse() {
    std::reverse(m_storage.begin(), m_storage.end());
  }

  std::size_t index_of(value_type const& e, std::size_t from = 0) const
    requires sequence_equal<const_reference, value_type const&>
  {
    if (from >= m_storage.size())
      return NPos;
    auto end = m_storage.end();
    auto found = std::find(m_storage.begin() + static_cast<difference_type>(from), end, e);
    return found == end ? NPos : static_cast<std::size_t>(std::distance(m_storage.begin(), found));
  }

  std::size_t last_index_of(value_type const& e, std::size_t til = NPos) const
    requires sequence_equal<const_reference, value_type const&>
  {
    std::size_t index = NPos;
    std::size_t end = std::min(m_storage.size(), til);
    for (std::size_t i = 0; i < end; ++i) {
      if (operator[](i) == e)
        index = i;
    }
    return index;
  }

  auto at(std::size_t n) const& -> std::expected<std::reference_wrapper<value_type const>, StarException>
    requires std::is_lvalue_reference_v<const_reference>
  {
    if (n >= m_storage.size())
      return std::unexpected(OutOfRangeException(strf("out of range list::at({})", n)));
    return std::reference_wrapper<value_type const>(operator[](n));
  }

  auto at(std::size_t n) & -> std::expected<std::reference_wrapper<value_type>, StarException>
    requires std::is_lvalue_reference_v<reference>
  {
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

  auto take_at(std::size_t i) -> std::expected<value_type, StarException> {
    if (i >= size())
      return std::unexpected(OutOfRangeException("take_at index out of range"));
    value_type e = std::move(operator[](i));
    m_storage.erase(m_storage.begin() + i);
    return e;
  }

  auto wrap(std::size_t n) const& -> std::expected<std::reference_wrapper<value_type const>, StarException>
    requires std::is_lvalue_reference_v<const_reference>
  {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("wrap() called on empty list"));
    return std::reference_wrapper<value_type const>(operator[](n % m_storage.size()));
  }

  auto wrap(std::size_t n) & -> std::expected<std::reference_wrapper<value_type>, StarException>
    requires std::is_lvalue_reference_v<reference>
  {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("wrap() called on empty list"));
    return std::reference_wrapper<value_type>(operator[](n % m_storage.size()));
  }

  void at(std::size_t) && = delete;
  void at(std::size_t) const&& = delete;
  void wrap(std::size_t) && = delete;
  void wrap(std::size_t) const&& = delete;

  auto wrap(std::size_t n, value_type def) const -> value_type {
    if (m_storage.empty())
      return def;
    else
      return operator[](n % m_storage.size());
  }

  void erase_at(std::size_t i) {
    if constexpr (Star::DebugEnabled) {
      Star::starAssertFailed((i < m_storage.size()));
    }
    m_storage.erase(m_storage.begin() + i);
  }

  void erase_at(std::size_t b, std::size_t e) {
    if constexpr (Star::DebugEnabled) {
      Star::starAssertFailed((b < m_storage.size() && e <= m_storage.size()));
    }
    m_storage.erase(m_storage.begin() + b, m_storage.begin() + e);
  }

  auto insert_at(std::size_t pos, value_type e) -> AppendResult {
    if constexpr (Star::DebugEnabled)
      Star::starAssertFailed((pos <= m_storage.size()));
    m_storage.insert(m_storage.begin() + pos, std::move(e));
  }

  template <typename Container>
    requires sequence_source<Container const&, value_type> && std::ranges::common_range<Container const>
  auto insert_range_at(std::size_t pos, Container const& list) -> AppendResult {
    if constexpr (Star::DebugEnabled)
      Star::starAssertFailed((pos <= m_storage.size()));
    return SequenceAlgorithms::insert_range_at(*this, pos, list);
  }

  [[nodiscard]] auto set(std::size_t pos, value_type e) -> std::expected<void, StarException>
    requires std::is_default_constructible_v<value_type>
    && requires(reference slot, value_type value) { slot = std::move(value); }
  {
    return SequenceAlgorithms::set(*this, pos, std::move(e));
  }

  void swap(std::size_t i, std::size_t j) { std::iter_swap(begin() + i, begin() + j); }

  std::expected<void, StarException> move(std::size_t from, std::size_t to) {
    if (from >= m_storage.size() || to >= m_storage.size())
      return std::unexpected(OutOfRangeException("List::move index out of range"));
    if (from < to)
      std::rotate(m_storage.begin() + from, m_storage.begin() + from + 1, m_storage.begin() + to + 1);
    else if (to < from)
      std::rotate(m_storage.begin() + to, m_storage.begin() + from, m_storage.begin() + from + 1);
    return {};
  }

  void first() && = delete;
  void first() const&& = delete;
  void last() && = delete;
  void last() const&& = delete;

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
  [[nodiscard]] AppendResult push_back(const_reference value)
    requires std::is_constructible_v<value_type, const_reference>
  {
    auto added = emplace_back(value);
    if (!added)
      return std::unexpected(std::move(added.error()));
    return {};
  }
  [[nodiscard]] AppendResult push_back(value_type&& value)
    requires std::is_constructible_v<value_type, value_type&&>
  {
    auto added = emplace_back(std::move(value));
    if (!added)
      return std::unexpected(std::move(added.error()));
    return {};
  }
  decltype(auto) pop_back() { return m_storage.pop_back(); }
  template <typename... Args>
    requires std::is_constructible_v<value_type, Args...>
  [[nodiscard]] auto emplace_back(Args&&... args) -> std::expected<std::reference_wrapper<value_type>, StarException> {
    auto added = m_storage.emplace_back(std::forward<Args>(args)...);
    if (!added)
      return std::unexpected(std::move(added.error()));
    return std::reference_wrapper<value_type>(back());
  }
  decltype(auto) resize(std::size_t size)
    requires std::is_default_constructible_v<value_type> && std::is_constructible_v<value_type, const_reference>
  {
    return m_storage.resize(size);
  }
  decltype(auto) resize(std::size_t size, const_reference value)
    requires std::is_constructible_v<value_type, const_reference>
  {
    return m_storage.resize(size, value);
  }
  decltype(auto) insert(iterator pos, const_reference value) { return m_storage.insert(pos, value); }
  decltype(auto) insert(iterator pos, value_type&& value) { return m_storage.insert(pos, std::move(value)); }
  decltype(auto) insert(iterator pos, std::initializer_list<value_type> values) { return m_storage.insert(pos, values); }
  template <std::input_iterator Iterator>
    requires std::is_constructible_v<value_type, std::iter_reference_t<Iterator>>
  decltype(auto) insert(iterator pos, Iterator first, Iterator last) {
    return m_storage.insert(pos, first, last);
  }
  decltype(auto) insert(iterator pos, std::size_t count, const_reference value)
    requires std::is_constructible_v<value_type, const_reference>
    && requires(Storage& storage) { storage.insert(pos, count, value); }
  {
    return m_storage.insert(pos, count, value);
  }
  decltype(auto) erase(iterator pos) noexcept(noexcept(m_storage.erase(pos))) { return m_storage.erase(pos); }
  decltype(auto) erase(iterator first, iterator last) noexcept(noexcept(m_storage.erase(first, last))) {
    return m_storage.erase(first, last);
  }
  template <typename... Args>
    requires std::is_constructible_v<value_type, Args...>
  decltype(auto) emplace(iterator pos,
                         Args&&... args) noexcept(noexcept(m_storage.emplace(pos, std::forward<Args>(args)...))) {
    return m_storage.emplace(pos, std::forward<Args>(args)...);
  }
  decltype(auto) assign(std::initializer_list<value_type> values)
    requires std::is_constructible_v<value_type, const_reference>
    && requires(Storage& storage) { storage.assign(values); }
  {
    return m_storage.assign(values);
  }
  template <std::input_iterator Iterator>
    requires std::is_constructible_v<value_type, std::iter_reference_t<Iterator>>
    && requires(Storage& storage, Iterator first, Iterator last) { storage.assign(first, last); }
  decltype(auto) assign(Iterator first, Iterator last) {
    return m_storage.assign(first, last);
  }
  decltype(auto) assign(std::size_t count, const_reference value)
    requires std::is_constructible_v<value_type, const_reference>
    && requires(Storage& storage) { storage.assign(count, value); }
  {
    return m_storage.assign(count, value);
  }
  void reserve(std::size_t capacity)
    requires requires(Storage& storage) { storage.reserve(capacity); }
  {
    m_storage.reserve(capacity);
  }
  void shrink_to_fit()
    requires requires(Storage& storage) { storage.shrink_to_fit(); }
  {
    m_storage.shrink_to_fit();
  }
  static constexpr std::size_t capacity() noexcept { return MaxSize; }
  static constexpr std::size_t max_size() noexcept { return MaxSize; }
  auto get_allocator() const requires requires(Storage const& storage) { storage.get_allocator(); } { return m_storage.get_allocator(); }
  value_type* data() noexcept(noexcept(m_storage.ptr())) { return m_storage.ptr(); }
  value_type const* data() const noexcept(noexcept(m_storage.ptr())) { return m_storage.ptr(); }
  void swap(StaticList& other) noexcept(std::is_nothrow_swappable_v<Storage>) { std::swap(m_storage, other.m_storage); }
  bool operator==(StaticList const& other) const { return m_storage == other.m_storage; }
  bool operator<(StaticList const& other) const { return m_storage < other.m_storage; }
  bool operator>(StaticList const& other) const { return other < *this; }
  bool operator<=(StaticList const& other) const { return !(other < *this); }
  bool operator>=(StaticList const& other) const { return !(*this < other); }

  template <typename Container>
    requires sequence_source<Container const&, Element>
  [[nodiscard]]
  static std::expected<StaticList, StarException> from(Container const& c);
  template <std::input_iterator Iterator, std::sentinel_for<Iterator> Sentinel>
    requires std::is_constructible_v<Element, std::iter_reference_t<Iterator>>
  [[nodiscard]]
  static std::expected<StaticList, StarException> create(Iterator first, Sentinel last);
  template <typename Range>
    requires sequence_source<Range, value_type>
  [[nodiscard]] static std::expected<StaticList, StarException> create(std::from_range_t, Range&& range) {
    if constexpr (std::ranges::sized_range<Range>) {
      if (std::ranges::size(range) > MaxSize)
        return std::unexpected(StaticVectorSizeException("StaticList::create exceeds capacity"));
    } else if constexpr (std::ranges::forward_range<Range>) {
      if (static_cast<std::size_t>(std::ranges::distance(range)) > MaxSize)
        return std::unexpected(StaticVectorSizeException("StaticList::create exceeds capacity"));
    }
    StaticList result;
    auto appended = result.append_range(std::forward<Range>(range));
    if (!appended)
      return std::unexpected(std::move(appended.error()));
    return result;
  }
  [[nodiscard]]
  static std::expected<StaticList, StarException> create(std::initializer_list<Element> list);
  [[nodiscard]]
  static std::expected<StaticList, StarException> create(std::size_t size, Element const& value = Element());



  [[nodiscard]]
  std::expected<StaticList, StarException> slice(SliceIndex a = SliceIndex(), SliceIndex b = SliceIndex(), int i = 1) const;

  template <typename Comparator>
    requires sequence_comparator<Comparator&, reference, reference>
  StaticList sorted(Comparator&& comparator) const
    requires std::is_constructible_v<value_type, const_reference>;
  StaticList sorted() const
    requires sequence_less<reference, reference> && std::is_constructible_v<value_type, const_reference>;

  template <typename Function>
    requires sequence_mapping<Function&, reference>
  [[nodiscard]]
  auto transformed(Function&& function);

  template <typename Function>
    requires sequence_mapping<Function&, const_reference>
  [[nodiscard]]
  auto transformed(Function&& function) const;

  template <typename Container>
    requires sequence_source<Container, value_type>
  auto append_range(Container&& list) -> AppendResult {
    return SequenceAlgorithms::append_range(*this, std::forward<Container>(list));
  }

  auto first() & -> std::expected<std::reference_wrapper<value_type>, StarException>
    requires std::is_lvalue_reference_v<reference>
  {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("first() called on empty list"));
    return std::reference_wrapper<value_type>(*m_storage.begin());
  }

  auto first() const& -> std::expected<std::reference_wrapper<value_type const>, StarException>
    requires std::is_lvalue_reference_v<const_reference>
  {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("first() called on empty list"));
    return std::reference_wrapper<value_type const>(*m_storage.begin());
  }

  auto last() & -> std::expected<std::reference_wrapper<value_type>, StarException>
    requires std::is_lvalue_reference_v<reference>
  {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("last() called on empty list"));
    return std::reference_wrapper<value_type>(*std::prev(m_storage.end()));
  }

  auto last() const& -> std::expected<std::reference_wrapper<value_type const>, StarException>
    requires std::is_lvalue_reference_v<const_reference>
  {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("last() called on empty list"));
    return std::reference_wrapper<value_type const>(*std::prev(m_storage.end()));
  }

  auto front_value() const -> std::optional<value_type>
    requires std::is_constructible_v<value_type, const_reference>
  {
    if (m_storage.empty())
      return {};
    return std::optional<value_type>(std::in_place, *m_storage.begin());
  }

  auto back_value() const -> std::optional<value_type>
    requires std::is_constructible_v<value_type, const_reference>
  {
    if (m_storage.empty())
      return {};
    return std::optional<value_type>(std::in_place, *std::prev(m_storage.end()));
  }

  std::expected<void, StarException> pop_back_checked() {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("pop_back_checked() called on empty list"));
    m_storage.erase(std::prev(m_storage.end()));
    return {};
  }

  auto take_back() -> std::expected<value_type, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("take_back() called on empty list"));
    value_type e = std::move(*std::prev(m_storage.end()));
    m_storage.erase(std::prev(m_storage.end()));
    return e;
  }

  auto try_take_back() -> std::optional<value_type> {
    if (m_storage.empty())
      return {};
    value_type e = std::move(*std::prev(m_storage.end()));
    m_storage.erase(std::prev(m_storage.end()));
    return std::optional<value_type>(std::in_place, std::move(e));
  }

  void truncate_back(std::size_t maximumSize) {
    while (m_storage.size() > maximumSize)
      m_storage.erase(std::prev(m_storage.end()));
  }

  bool contains(const_reference e) const
    requires sequence_equal<const_reference, const_reference>
  {
    return std::find_if(begin(), end(), [&](auto&& value) { return static_cast<bool>(value == e); }) != end();
  }

  std::size_t remove(const_reference e)
    requires sequence_equal<reference, const_reference>
  {
    return SequenceAlgorithms::remove(*this, e);
  }

  template <typename Filter>
    requires sequence_predicate<Filter&, reference>
    && requires(reference slot, value_type&& value) { slot = std::move(value); }
  auto filter(Filter&& filter) {
    return SequenceAlgorithms::filter(*this, std::forward<Filter>(filter));
  }

  template <typename Self, typename Filter>
    requires std::is_copy_constructible_v<Self>
    && std::is_constructible_v<typename Self::value_type, typename Self::const_reference>
    && sequence_predicate<Filter&, typename Self::reference>
    && requires(typename Self::reference slot, typename Self::value_type&& value) { slot = std::move(value); }
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
    requires sequence_comparator<Comparator&, const_reference, const_reference>
  auto insert_sorted(value_type e, Comparator&& comparator) -> AppendResult {
    auto i = std::upper_bound(m_storage.begin(), m_storage.end(), e, [&](auto const& left, auto const& right) {
      return static_cast<bool>(std::invoke(comparator, left, right));
    });
    auto added = m_storage.insert(i, std::move(e));
    if (!added)
      return std::unexpected(std::move(added.error()));
    return {};
  }

  auto insert_sorted(value_type e) -> AppendResult
    requires sequence_less<const_reference, const_reference>
  {
    return insert_sorted(std::move(e),
                         [](auto const& left, auto const& right) { return static_cast<bool>(left < right); });
  }

  template <typename Function>
    requires sequence_callable<Function&, reference>
  auto exec(Function&& function) {
    return SequenceAlgorithms::exec(*this, std::forward<Function>(function));
  }

  template <typename Function>
    requires sequence_callable<Function&, const_reference>
  auto exec(Function&& function) const {
    return SequenceAlgorithms::exec(*this, std::forward<Function>(function));
  }

  template <typename Function>
    requires sequence_transform<Function&, reference>
  auto transform(Function&& function) {
    return SequenceAlgorithms::transform(*this, std::forward<Function>(function));
  }

  template <typename Function>
    requires sequence_predicate<Function const&, const_reference>
  auto any(Function&& function) const {
    return SequenceAlgorithms::any(*this, std::forward<Function>(function));
  }

  auto any() const
    requires requires(const_reference value) { static_cast<bool>(value); }
  {
    return any([](auto const& value) -> auto const& { return value; });
  }

  template <typename Function>
    requires sequence_predicate<Function const&, const_reference>
  auto all(Function&& function) const {
    return SequenceAlgorithms::all(*this, std::forward<Function>(function));
  }

  auto all() const
    requires requires(const_reference value) { static_cast<bool>(value); }
  {
    return all([](auto const& value) -> auto const& { return value; });
  }

  template <typename Comparator>
    requires sequence_comparator<Comparator&, reference, reference>
  void sort(Comparator&& comparator) {
    std::sort(m_storage.begin(), m_storage.end(),
              [&](auto&& left, auto&& right) { return static_cast<bool>(std::invoke(comparator, left, right)); });
  }

  void sort()
    requires sequence_less<reference, reference>
  {
    sort([](auto const& left, auto const& right) { return static_cast<bool>(left < right); });
  }

  void reverse() {
    std::reverse(m_storage.begin(), m_storage.end());
  }

  std::size_t index_of(const_reference e, std::size_t from = 0) const
    requires sequence_equal<const_reference, const_reference>
  {
    if (from >= m_storage.size())
      return NPos;
    auto end = m_storage.end();
    auto found = std::find(m_storage.begin() + static_cast<difference_type>(from), end, e);
    return found == end ? NPos : static_cast<std::size_t>(std::distance(m_storage.begin(), found));
  }

  std::size_t last_index_of(const_reference e, std::size_t til = NPos) const
    requires sequence_equal<const_reference, const_reference>
  {
    std::size_t index = NPos;
    std::size_t end = std::min(m_storage.size(), til);
    for (std::size_t i = 0; i < end; ++i) {
      if (operator[](i) == e)
        index = i;
    }
    return index;
  }

  auto at(std::size_t n) const& -> std::expected<std::reference_wrapper<value_type const>, StarException>
    requires std::is_lvalue_reference_v<const_reference>
  {
    if (n >= m_storage.size())
      return std::unexpected(OutOfRangeException(strf("out of range list::at({})", n)));
    return std::reference_wrapper<value_type const>(operator[](n));
  }

  auto at(std::size_t n) & -> std::expected<std::reference_wrapper<value_type>, StarException>
    requires std::is_lvalue_reference_v<reference>
  {
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

  auto take_at(std::size_t i) -> std::expected<value_type, StarException> {
    if (i >= size())
      return std::unexpected(OutOfRangeException("take_at index out of range"));
    value_type e = std::move(operator[](i));
    m_storage.erase(m_storage.begin() + i);
    return e;
  }

  auto wrap(std::size_t n) const& -> std::expected<std::reference_wrapper<value_type const>, StarException>
    requires std::is_lvalue_reference_v<const_reference>
  {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("wrap() called on empty list"));
    return std::reference_wrapper<value_type const>(operator[](n % m_storage.size()));
  }

  auto wrap(std::size_t n) & -> std::expected<std::reference_wrapper<value_type>, StarException>
    requires std::is_lvalue_reference_v<reference>
  {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("wrap() called on empty list"));
    return std::reference_wrapper<value_type>(operator[](n % m_storage.size()));
  }

  void at(std::size_t) && = delete;
  void at(std::size_t) const&& = delete;
  void wrap(std::size_t) && = delete;
  void wrap(std::size_t) const&& = delete;

  auto wrap(std::size_t n, value_type def) const -> value_type {
    if (m_storage.empty())
      return def;
    else
      return operator[](n % m_storage.size());
  }

  void erase_at(std::size_t i) {
    if constexpr (Star::DebugEnabled) {
      Star::starAssertFailed((i < m_storage.size()));
    }
    m_storage.erase(m_storage.begin() + i);
  }

  void erase_at(std::size_t b, std::size_t e) {
    if constexpr (Star::DebugEnabled) {
      Star::starAssertFailed((b < m_storage.size() && e <= m_storage.size()));
    }
    m_storage.erase(m_storage.begin() + b, m_storage.begin() + e);
  }

  auto insert_at(std::size_t pos, value_type e) -> AppendResult {
    if constexpr (Star::DebugEnabled)
      Star::starAssertFailed((pos <= m_storage.size()));
    auto added = m_storage.insert(m_storage.begin() + pos, std::move(e));
    if (!added)
      return std::unexpected(std::move(added.error()));
    return {};
  }

  template <typename Container>
    requires sequence_source<Container const&, value_type> && std::ranges::common_range<Container const>
  auto insert_range_at(std::size_t pos, Container const& list) -> AppendResult {
    if constexpr (Star::DebugEnabled)
      Star::starAssertFailed((pos <= m_storage.size()));
    return SequenceAlgorithms::insert_range_at(*this, pos, list);
  }

  [[nodiscard]] auto set(std::size_t pos, value_type e) -> std::expected<void, StarException>
    requires std::is_default_constructible_v<value_type> && std::is_constructible_v<value_type, const_reference>
    && requires(reference slot, value_type value) { slot = std::move(value); }
  {
    return SequenceAlgorithms::set(*this, pos, std::move(e));
  }

  void swap(std::size_t i, std::size_t j) { std::iter_swap(begin() + i, begin() + j); }

  std::expected<void, StarException> move(std::size_t from, std::size_t to) {
    if (from >= m_storage.size() || to >= m_storage.size())
      return std::unexpected(OutOfRangeException("List::move index out of range"));
    if (from < to)
      std::rotate(m_storage.begin() + from, m_storage.begin() + from + 1, m_storage.begin() + to + 1);
    else if (to < from)
      std::rotate(m_storage.begin() + to, m_storage.begin() + from, m_storage.begin() + from + 1);
    return {};
  }

  void first() && = delete;
  void first() const&& = delete;
  void last() && = delete;
  void last() const&& = delete;

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
  template <std::input_iterator Iterator, std::sentinel_for<Iterator> Sentinel>
    requires std::is_constructible_v<value_type, std::iter_reference_t<Iterator>>
  SmallList(Iterator first, Sentinel last) {
    if constexpr (std::sized_sentinel_for<Sentinel, Iterator> && requires { reserve(std::size_t{}); })
      reserve(static_cast<std::size_t>(last - first));
    for (; first != last; ++first)
      emplace_back(*first);
  }
  template <typename Range>
    requires sequence_source<Range, value_type>
  explicit SmallList(std::from_range_t, Range&& range) {
    append_range(std::forward<Range>(range));
  }
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
  [[nodiscard]] AppendResult push_back(const_reference value)
    requires std::is_constructible_v<value_type, const_reference>
  {
    return m_storage.push_back(value);
  }
  [[nodiscard]] AppendResult push_back(value_type&& value)
    requires std::is_constructible_v<value_type, value_type&&>
  {
    return m_storage.push_back(std::move(value));
  }
  decltype(auto) pop_back() { return m_storage.pop_back(); }
  template <typename... Args>
    requires std::is_constructible_v<value_type, Args...>
  reference emplace_back(Args&&... args) noexcept(noexcept(m_storage.emplace_back(std::forward<Args>(args)...))) {
    m_storage.emplace_back(std::forward<Args>(args)...);
    return back();
  }
  decltype(auto) resize(std::size_t size)
    requires std::is_default_constructible_v<value_type> && std::is_constructible_v<value_type, const_reference>
  {
    return m_storage.resize(size);
  }
  decltype(auto) resize(std::size_t size, const_reference value)
    requires std::is_constructible_v<value_type, const_reference>
  {
    return m_storage.resize(size, value);
  }
  decltype(auto) insert(iterator pos, const_reference value) { return m_storage.insert(pos, value); }
  decltype(auto) insert(iterator pos, value_type&& value) { return m_storage.insert(pos, std::move(value)); }
  decltype(auto) insert(iterator pos, std::initializer_list<value_type> values) { return m_storage.insert(pos, values); }
  template <std::input_iterator Iterator>
    requires std::is_constructible_v<value_type, std::iter_reference_t<Iterator>>
  decltype(auto) insert(iterator pos, Iterator first, Iterator last) {
    return m_storage.insert(pos, first, last);
  }
  decltype(auto) insert(iterator pos, std::size_t count, const_reference value)
    requires std::is_constructible_v<value_type, const_reference>
    && requires(Storage& storage) { storage.insert(pos, count, value); }
  {
    return m_storage.insert(pos, count, value);
  }
  decltype(auto) erase(iterator pos) noexcept(noexcept(m_storage.erase(pos))) { return m_storage.erase(pos); }
  decltype(auto) erase(iterator first, iterator last) noexcept(noexcept(m_storage.erase(first, last))) {
    return m_storage.erase(first, last);
  }
  template <typename... Args>
    requires std::is_constructible_v<value_type, Args...>
  decltype(auto) emplace(iterator pos,
                         Args&&... args) noexcept(noexcept(m_storage.emplace(pos, std::forward<Args>(args)...))) {
    return m_storage.emplace(pos, std::forward<Args>(args)...);
  }
  void reserve(std::size_t capacity)
    requires requires(Storage& storage) { storage.reserve(capacity); }
  {
    m_storage.reserve(capacity);
  }
  void shrink_to_fit()
    requires requires(Storage& storage) { storage.shrink_to_fit(); }
  {
    m_storage.shrink_to_fit();
  }

  static constexpr std::size_t max_size() noexcept {
    return std::numeric_limits<difference_type>::max() / sizeof(value_type);
  }
  auto get_allocator() const requires requires(Storage const& storage) { storage.get_allocator(); } { return m_storage.get_allocator(); }
  value_type* data() noexcept(noexcept(m_storage.ptr())) { return m_storage.ptr(); }
  value_type const* data() const noexcept(noexcept(m_storage.ptr())) { return m_storage.ptr(); }
  void swap(SmallList& other) noexcept(std::is_nothrow_swappable_v<Storage>) { std::swap(m_storage, other.m_storage); }
  bool operator==(SmallList const& other) const { return m_storage == other.m_storage; }
  bool operator<(SmallList const& other) const { return m_storage < other.m_storage; }
  bool operator>(SmallList const& other) const { return other < *this; }
  bool operator<=(SmallList const& other) const { return !(other < *this); }
  bool operator>=(SmallList const& other) const { return !(*this < other); }

  template <typename Container>
    requires sequence_source<Container const&, Element>
  static SmallList from(Container const& c);

  SmallList slice(SliceIndex a = SliceIndex(), SliceIndex b = SliceIndex(), int i = 1) const;

  template <typename Comparator>
    requires sequence_comparator<Comparator&, reference, reference>
  SmallList sorted(Comparator&& comparator) const
    requires std::is_constructible_v<value_type, const_reference>;
  SmallList sorted() const
    requires sequence_less<reference, reference> && std::is_constructible_v<value_type, const_reference>;

  template <typename Function>
    requires sequence_mapping<Function&, reference>
  [[nodiscard]]
  auto transformed(Function&& function);

  template <typename Function>
    requires sequence_mapping<Function&, const_reference>
  [[nodiscard]]
  auto transformed(Function&& function) const;

  template <typename Container>
    requires sequence_source<Container, value_type>
  auto append_range(Container&& list) -> AppendResult {
    return SequenceAlgorithms::append_range(*this, std::forward<Container>(list));
  }

  auto first() & -> std::expected<std::reference_wrapper<value_type>, StarException>
    requires std::is_lvalue_reference_v<reference>
  {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("first() called on empty list"));
    return std::reference_wrapper<value_type>(*m_storage.begin());
  }

  auto first() const& -> std::expected<std::reference_wrapper<value_type const>, StarException>
    requires std::is_lvalue_reference_v<const_reference>
  {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("first() called on empty list"));
    return std::reference_wrapper<value_type const>(*m_storage.begin());
  }

  auto last() & -> std::expected<std::reference_wrapper<value_type>, StarException>
    requires std::is_lvalue_reference_v<reference>
  {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("last() called on empty list"));
    return std::reference_wrapper<value_type>(*std::prev(m_storage.end()));
  }

  auto last() const& -> std::expected<std::reference_wrapper<value_type const>, StarException>
    requires std::is_lvalue_reference_v<const_reference>
  {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("last() called on empty list"));
    return std::reference_wrapper<value_type const>(*std::prev(m_storage.end()));
  }

  auto front_value() const -> std::optional<value_type>
    requires std::is_constructible_v<value_type, const_reference>
  {
    if (m_storage.empty())
      return {};
    return std::optional<value_type>(std::in_place, *m_storage.begin());
  }

  auto back_value() const -> std::optional<value_type>
    requires std::is_constructible_v<value_type, const_reference>
  {
    if (m_storage.empty())
      return {};
    return std::optional<value_type>(std::in_place, *std::prev(m_storage.end()));
  }

  std::expected<void, StarException> pop_back_checked() {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("pop_back_checked() called on empty list"));
    m_storage.erase(std::prev(m_storage.end()));
    return {};
  }

  auto take_back() -> std::expected<value_type, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("take_back() called on empty list"));
    value_type e = std::move(*std::prev(m_storage.end()));
    m_storage.erase(std::prev(m_storage.end()));
    return e;
  }

  auto try_take_back() -> std::optional<value_type> {
    if (m_storage.empty())
      return {};
    value_type e = std::move(*std::prev(m_storage.end()));
    m_storage.erase(std::prev(m_storage.end()));
    return std::optional<value_type>(std::in_place, std::move(e));
  }

  void truncate_back(std::size_t maximumSize) {
    while (m_storage.size() > maximumSize)
      m_storage.erase(std::prev(m_storage.end()));
  }

  bool contains(const_reference e) const
    requires sequence_equal<const_reference, const_reference>
  {
    return std::find_if(begin(), end(), [&](auto&& value) { return static_cast<bool>(value == e); }) != end();
  }

  std::size_t remove(const_reference e)
    requires sequence_equal<reference, const_reference>
  {
    return SequenceAlgorithms::remove(*this, e);
  }

  template <typename Filter>
    requires sequence_predicate<Filter&, reference>
    && requires(reference slot, value_type&& value) { slot = std::move(value); }
  auto filter(Filter&& filter) {
    return SequenceAlgorithms::filter(*this, std::forward<Filter>(filter));
  }

  template <typename Self, typename Filter>
    requires std::is_copy_constructible_v<Self>
    && std::is_constructible_v<typename Self::value_type, typename Self::const_reference>
    && sequence_predicate<Filter&, typename Self::reference>
    && requires(typename Self::reference slot, typename Self::value_type&& value) { slot = std::move(value); }
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
    requires sequence_comparator<Comparator&, const_reference, const_reference>
  auto insert_sorted(value_type e, Comparator&& comparator) -> AppendResult {
    auto i = std::upper_bound(m_storage.begin(), m_storage.end(), e, [&](auto const& left, auto const& right) {
      return static_cast<bool>(std::invoke(comparator, left, right));
    });
    m_storage.insert(i, std::move(e));
  }

  auto insert_sorted(value_type e) -> AppendResult
    requires sequence_less<const_reference, const_reference>
  {
    return insert_sorted(std::move(e),
                         [](auto const& left, auto const& right) { return static_cast<bool>(left < right); });
  }

  template <typename Function>
    requires sequence_callable<Function&, reference>
  auto exec(Function&& function) {
    return SequenceAlgorithms::exec(*this, std::forward<Function>(function));
  }

  template <typename Function>
    requires sequence_callable<Function&, const_reference>
  auto exec(Function&& function) const {
    return SequenceAlgorithms::exec(*this, std::forward<Function>(function));
  }

  template <typename Function>
    requires sequence_transform<Function&, reference>
  auto transform(Function&& function) {
    return SequenceAlgorithms::transform(*this, std::forward<Function>(function));
  }

  template <typename Function>
    requires sequence_predicate<Function const&, const_reference>
  auto any(Function&& function) const {
    return SequenceAlgorithms::any(*this, std::forward<Function>(function));
  }

  auto any() const
    requires requires(const_reference value) { static_cast<bool>(value); }
  {
    return any([](auto const& value) -> auto const& { return value; });
  }

  template <typename Function>
    requires sequence_predicate<Function const&, const_reference>
  auto all(Function&& function) const {
    return SequenceAlgorithms::all(*this, std::forward<Function>(function));
  }

  auto all() const
    requires requires(const_reference value) { static_cast<bool>(value); }
  {
    return all([](auto const& value) -> auto const& { return value; });
  }

  template <typename Comparator>
    requires sequence_comparator<Comparator&, reference, reference>
  void sort(Comparator&& comparator) {
    std::sort(m_storage.begin(), m_storage.end(),
              [&](auto&& left, auto&& right) { return static_cast<bool>(std::invoke(comparator, left, right)); });
  }

  void sort()
    requires sequence_less<reference, reference>
  {
    sort([](auto const& left, auto const& right) { return static_cast<bool>(left < right); });
  }

  void reverse() {
    std::reverse(m_storage.begin(), m_storage.end());
  }

  std::size_t index_of(const_reference e, std::size_t from = 0) const
    requires sequence_equal<const_reference, const_reference>
  {
    if (from >= m_storage.size())
      return NPos;
    auto end = m_storage.end();
    auto found = std::find(m_storage.begin() + static_cast<difference_type>(from), end, e);
    return found == end ? NPos : static_cast<std::size_t>(std::distance(m_storage.begin(), found));
  }

  std::size_t last_index_of(const_reference e, std::size_t til = NPos) const
    requires sequence_equal<const_reference, const_reference>
  {
    std::size_t index = NPos;
    std::size_t end = std::min(m_storage.size(), til);
    for (std::size_t i = 0; i < end; ++i) {
      if (operator[](i) == e)
        index = i;
    }
    return index;
  }

  auto at(std::size_t n) const& -> std::expected<std::reference_wrapper<value_type const>, StarException>
    requires std::is_lvalue_reference_v<const_reference>
  {
    if (n >= m_storage.size())
      return std::unexpected(OutOfRangeException(strf("out of range list::at({})", n)));
    return std::reference_wrapper<value_type const>(operator[](n));
  }

  auto at(std::size_t n) & -> std::expected<std::reference_wrapper<value_type>, StarException>
    requires std::is_lvalue_reference_v<reference>
  {
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

  auto take_at(std::size_t i) -> std::expected<value_type, StarException> {
    if (i >= size())
      return std::unexpected(OutOfRangeException("take_at index out of range"));
    value_type e = std::move(operator[](i));
    m_storage.erase(m_storage.begin() + i);
    return e;
  }

  auto wrap(std::size_t n) const& -> std::expected<std::reference_wrapper<value_type const>, StarException>
    requires std::is_lvalue_reference_v<const_reference>
  {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("wrap() called on empty list"));
    return std::reference_wrapper<value_type const>(operator[](n % m_storage.size()));
  }

  auto wrap(std::size_t n) & -> std::expected<std::reference_wrapper<value_type>, StarException>
    requires std::is_lvalue_reference_v<reference>
  {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("wrap() called on empty list"));
    return std::reference_wrapper<value_type>(operator[](n % m_storage.size()));
  }

  void at(std::size_t) && = delete;
  void at(std::size_t) const&& = delete;
  void wrap(std::size_t) && = delete;
  void wrap(std::size_t) const&& = delete;

  auto wrap(std::size_t n, value_type def) const -> value_type {
    if (m_storage.empty())
      return def;
    else
      return operator[](n % m_storage.size());
  }

  void erase_at(std::size_t i) {
    if constexpr (Star::DebugEnabled) {
      Star::starAssertFailed((i < m_storage.size()));
    }
    m_storage.erase(m_storage.begin() + i);
  }

  void erase_at(std::size_t b, std::size_t e) {
    if constexpr (Star::DebugEnabled) {
      Star::starAssertFailed((b < m_storage.size() && e <= m_storage.size()));
    }
    m_storage.erase(m_storage.begin() + b, m_storage.begin() + e);
  }

  auto insert_at(std::size_t pos, value_type e) -> AppendResult {
    if constexpr (Star::DebugEnabled)
      Star::starAssertFailed((pos <= m_storage.size()));
    m_storage.insert(m_storage.begin() + pos, std::move(e));
  }

  template <typename Container>
    requires sequence_source<Container const&, value_type> && std::ranges::common_range<Container const>
  auto insert_range_at(std::size_t pos, Container const& list) -> AppendResult {
    if constexpr (Star::DebugEnabled)
      Star::starAssertFailed((pos <= m_storage.size()));
    return SequenceAlgorithms::insert_range_at(*this, pos, list);
  }

  [[nodiscard]] auto set(std::size_t pos, value_type e) -> std::expected<void, StarException>
    requires std::is_default_constructible_v<value_type> && std::is_constructible_v<value_type, const_reference>
    && requires(reference slot, value_type value) { slot = std::move(value); }
  {
    return SequenceAlgorithms::set(*this, pos, std::move(e));
  }

  void swap(std::size_t i, std::size_t j) { std::iter_swap(begin() + i, begin() + j); }

  std::expected<void, StarException> move(std::size_t from, std::size_t to) {
    if (from >= m_storage.size() || to >= m_storage.size())
      return std::unexpected(OutOfRangeException("List::move index out of range"));
    if (from < to)
      std::rotate(m_storage.begin() + from, m_storage.begin() + from + 1, m_storage.begin() + to + 1);
    else if (to < from)
      std::rotate(m_storage.begin() + to, m_storage.begin() + from, m_storage.begin() + from + 1);
    return {};
  }

  void first() && = delete;
  void first() const&& = delete;
  void last() && = delete;
  void last() const&& = delete;

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
  using size_type = typename Storage::size_type;
  using difference_type = typename Storage::difference_type;
  using pointer = typename Storage::pointer;
  using const_pointer = typename Storage::const_pointer;
  using allocator_type = typename Storage::allocator_type;
  using AppendResult = void;

  Deque() = default;
  Deque(Storage const& storage) : m_storage(storage) {}
  Deque(Storage&& storage) : m_storage(std::move(storage)) {}
  explicit Deque(std::size_t length, const_reference value = value_type()) : m_storage(length, value) {}
  explicit Deque(std::span<value_type const> values) : m_storage(values.begin(), values.end()) {}
  template <std::input_iterator Iterator, std::sentinel_for<Iterator> Sentinel>
    requires std::is_constructible_v<value_type, std::iter_reference_t<Iterator>>
  Deque(Iterator first, Sentinel last) {
    for (; first != last; ++first)
      emplace_back(*first);
  }
  template <typename Range>
    requires sequence_source<Range, value_type>
  explicit Deque(std::from_range_t, Range&& range) {
    append_range(std::forward<Range>(range));
  }
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
  size_type size() const noexcept(noexcept(m_storage.size())) { return m_storage.size(); }
  bool empty() const { return m_storage.empty(); }
  void clear() { m_storage.clear(); }
  reference front() { return *begin(); }
  const_reference front() const { return *begin(); }
  reference back() { return *std::prev(end()); }
  const_reference back() const { return *std::prev(end()); }
  void push_back(const_reference value)
    requires std::is_constructible_v<value_type, const_reference>
  {
    m_storage.push_back(value);
  }
  void push_back(value_type&& value)
    requires std::is_constructible_v<value_type, value_type&&>
  {
    m_storage.push_back(std::move(value));
  }
  void pop_back() noexcept(noexcept(m_storage.pop_back())) { m_storage.pop_back(); }
  template <typename... Args>
    requires std::is_constructible_v<value_type, Args...>
  decltype(auto) emplace_back(Args&&... args) noexcept(noexcept(m_storage.emplace_back(std::forward<Args>(args)...))) {
    return m_storage.emplace_back(std::forward<Args>(args)...);
  }
  decltype(auto) resize(std::size_t size)
    requires std::is_default_constructible_v<value_type>
  {
    return m_storage.resize(size);
  }
  decltype(auto) resize(std::size_t size, const_reference value)
    requires std::is_constructible_v<value_type, const_reference>
  {
    return m_storage.resize(size, value);
  }
  decltype(auto) insert(const_iterator pos, const_reference value) { return m_storage.insert(pos, value); }
  decltype(auto) insert(const_iterator pos, value_type&& value) { return m_storage.insert(pos, std::move(value)); }
  decltype(auto) insert(const_iterator pos, std::initializer_list<value_type> values) {
    return m_storage.insert(pos, values);
  }
  template <std::input_iterator Iterator>
    requires std::is_constructible_v<value_type, std::iter_reference_t<Iterator>>
  decltype(auto) insert(const_iterator pos, Iterator first, Iterator last) {
    return m_storage.insert(pos, first, last);
  }
  decltype(auto) insert(const_iterator pos, std::size_t count, const_reference value)
    requires std::is_constructible_v<value_type, const_reference>
  {
    return m_storage.insert(pos, count, value);
  }
  decltype(auto) erase(const_iterator pos) noexcept(noexcept(m_storage.erase(pos))) { return m_storage.erase(pos); }
  decltype(auto) erase(const_iterator first, const_iterator last) noexcept(noexcept(m_storage.erase(first, last))) {
    return m_storage.erase(first, last);
  }
  template <typename... Args>
    requires std::is_constructible_v<value_type, Args...>
  decltype(auto) emplace(const_iterator pos,
                         Args&&... args) noexcept(noexcept(m_storage.emplace(pos, std::forward<Args>(args)...))) {
    return m_storage.emplace(pos, std::forward<Args>(args)...);
  }
  decltype(auto) assign(std::initializer_list<value_type> values)
    requires std::is_constructible_v<value_type, const_reference>
  {
    return m_storage.assign(values);
  }
  template <std::input_iterator Iterator>
    requires std::is_constructible_v<value_type, std::iter_reference_t<Iterator>>
  decltype(auto) assign(Iterator first, Iterator last) {
    return m_storage.assign(first, last);
  }
  decltype(auto) assign(std::size_t count, const_reference value)
    requires std::is_constructible_v<value_type, const_reference>
  {
    return m_storage.assign(count, value);
  }
  void shrink_to_fit() { m_storage.shrink_to_fit(); }
  size_type max_size() const noexcept(noexcept(m_storage.max_size())) { return m_storage.max_size(); }
  allocator_type get_allocator() const noexcept(noexcept(m_storage.get_allocator())) {
    return m_storage.get_allocator();
  }
  void push_front(const_reference value) { m_storage.push_front(value); }
  void push_front(value_type&& value) { m_storage.push_front(std::move(value)); }
  void pop_front() { m_storage.pop_front(); }
  template <typename... Args>
    requires std::is_constructible_v<value_type, Args...>
  decltype(auto)
  emplace_front(Args&&... args) noexcept(noexcept(m_storage.emplace_front(std::forward<Args>(args)...))) {
    return m_storage.emplace_front(std::forward<Args>(args)...);
  }
  void swap(Deque& other) noexcept(noexcept(m_storage.swap(other.m_storage))) { m_storage.swap(other.m_storage); }
  bool operator==(Deque const& other) const { return m_storage == other.m_storage; }
  bool operator<(Deque const& other) const { return m_storage < other.m_storage; }
  bool operator>(Deque const& other) const { return other < *this; }
  bool operator<=(Deque const& other) const { return !(other < *this); }
  bool operator>=(Deque const& other) const { return !(*this < other); }

  template <typename Container>
    requires sequence_source<Container const&, Element>
  static Deque from(Container const& c);

  Deque slice(SliceIndex a = SliceIndex(), SliceIndex b = SliceIndex(), int i = 1) const;

  template <typename Comparator>
    requires sequence_comparator<Comparator&, reference, reference>
  Deque sorted(Comparator&& comparator) const
    requires std::is_constructible_v<value_type, const_reference>;
  Deque sorted() const
    requires sequence_less<reference, reference> && std::is_constructible_v<value_type, const_reference>;

  template <typename Function>
    requires sequence_mapping<Function&, reference>
  [[nodiscard]]
  auto transformed(Function&& function);

  template <typename Function>
    requires sequence_mapping<Function&, const_reference>
  [[nodiscard]]
  auto transformed(Function&& function) const;

  template <typename Container>
    requires sequence_source<Container, value_type>
  auto append_range(Container&& list) -> AppendResult {
    return SequenceAlgorithms::append_range(*this, std::forward<Container>(list));
  }

  auto first() & -> std::expected<std::reference_wrapper<value_type>, StarException>
    requires std::is_lvalue_reference_v<reference>
  {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("first() called on empty list"));
    return std::reference_wrapper<value_type>(*m_storage.begin());
  }

  auto first() const& -> std::expected<std::reference_wrapper<value_type const>, StarException>
    requires std::is_lvalue_reference_v<const_reference>
  {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("first() called on empty list"));
    return std::reference_wrapper<value_type const>(*m_storage.begin());
  }

  auto last() & -> std::expected<std::reference_wrapper<value_type>, StarException>
    requires std::is_lvalue_reference_v<reference>
  {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("last() called on empty list"));
    return std::reference_wrapper<value_type>(*std::prev(m_storage.end()));
  }

  auto last() const& -> std::expected<std::reference_wrapper<value_type const>, StarException>
    requires std::is_lvalue_reference_v<const_reference>
  {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("last() called on empty list"));
    return std::reference_wrapper<value_type const>(*std::prev(m_storage.end()));
  }

  auto front_value() const -> std::optional<value_type>
    requires std::is_constructible_v<value_type, const_reference>
  {
    if (m_storage.empty())
      return {};
    return std::optional<value_type>(std::in_place, *m_storage.begin());
  }

  auto back_value() const -> std::optional<value_type>
    requires std::is_constructible_v<value_type, const_reference>
  {
    if (m_storage.empty())
      return {};
    return std::optional<value_type>(std::in_place, *std::prev(m_storage.end()));
  }

  std::expected<void, StarException> pop_back_checked() {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("pop_back_checked() called on empty list"));
    m_storage.erase(std::prev(m_storage.end()));
    return {};
  }

  auto take_back() -> std::expected<value_type, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("take_back() called on empty list"));
    value_type e = std::move(*std::prev(m_storage.end()));
    m_storage.erase(std::prev(m_storage.end()));
    return e;
  }

  auto try_take_back() -> std::optional<value_type> {
    if (m_storage.empty())
      return {};
    value_type e = std::move(*std::prev(m_storage.end()));
    m_storage.erase(std::prev(m_storage.end()));
    return std::optional<value_type>(std::in_place, std::move(e));
  }

  void truncate_back(std::size_t maximumSize) {
    while (m_storage.size() > maximumSize)
      m_storage.erase(std::prev(m_storage.end()));
  }

  bool contains(const_reference e) const
    requires sequence_equal<const_reference, const_reference>
  {
    return std::find_if(begin(), end(), [&](auto&& value) { return static_cast<bool>(value == e); }) != end();
  }

  std::size_t remove(const_reference e)
    requires sequence_equal<reference, const_reference>
  {
    return SequenceAlgorithms::remove(*this, e);
  }

  template <typename Filter>
    requires sequence_predicate<Filter&, reference>
    && requires(reference slot, value_type&& value) { slot = std::move(value); }
  auto filter(Filter&& filter) {
    return SequenceAlgorithms::filter(*this, std::forward<Filter>(filter));
  }

  template <typename Self, typename Filter>
    requires std::is_copy_constructible_v<Self>
    && std::is_constructible_v<typename Self::value_type, typename Self::const_reference>
    && sequence_predicate<Filter&, typename Self::reference>
    && requires(typename Self::reference slot, typename Self::value_type&& value) { slot = std::move(value); }
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
    requires sequence_comparator<Comparator&, const_reference, const_reference>
  auto insert_sorted(value_type e, Comparator&& comparator) -> AppendResult {
    auto i = std::upper_bound(m_storage.begin(), m_storage.end(), e, [&](auto const& left, auto const& right) {
      return static_cast<bool>(std::invoke(comparator, left, right));
    });
    m_storage.insert(i, std::move(e));
  }

  auto insert_sorted(value_type e) -> AppendResult
    requires sequence_less<const_reference, const_reference>
  {
    return insert_sorted(std::move(e),
                         [](auto const& left, auto const& right) { return static_cast<bool>(left < right); });
  }

  template <typename Function>
    requires sequence_callable<Function&, reference>
  auto exec(Function&& function) {
    return SequenceAlgorithms::exec(*this, std::forward<Function>(function));
  }

  template <typename Function>
    requires sequence_callable<Function&, const_reference>
  auto exec(Function&& function) const {
    return SequenceAlgorithms::exec(*this, std::forward<Function>(function));
  }

  template <typename Function>
    requires sequence_transform<Function&, reference>
  auto transform(Function&& function) {
    return SequenceAlgorithms::transform(*this, std::forward<Function>(function));
  }

  template <typename Function>
    requires sequence_predicate<Function const&, const_reference>
  auto any(Function&& function) const {
    return SequenceAlgorithms::any(*this, std::forward<Function>(function));
  }

  auto any() const
    requires requires(const_reference value) { static_cast<bool>(value); }
  {
    return any([](auto const& value) -> auto const& { return value; });
  }

  template <typename Function>
    requires sequence_predicate<Function const&, const_reference>
  auto all(Function&& function) const {
    return SequenceAlgorithms::all(*this, std::forward<Function>(function));
  }

  auto all() const
    requires requires(const_reference value) { static_cast<bool>(value); }
  {
    return all([](auto const& value) -> auto const& { return value; });
  }

  template <typename Comparator>
    requires sequence_comparator<Comparator&, reference, reference>
  void sort(Comparator&& comparator) {
    std::sort(m_storage.begin(), m_storage.end(),
              [&](auto&& left, auto&& right) { return static_cast<bool>(std::invoke(comparator, left, right)); });
  }

  void sort()
    requires sequence_less<reference, reference>
  {
    sort([](auto const& left, auto const& right) { return static_cast<bool>(left < right); });
  }

  void reverse() {
    std::reverse(m_storage.begin(), m_storage.end());
  }

  std::size_t index_of(const_reference e, std::size_t from = 0) const
    requires sequence_equal<const_reference, const_reference>
  {
    if (from >= m_storage.size())
      return NPos;
    auto end = m_storage.end();
    auto found = std::find(m_storage.begin() + static_cast<difference_type>(from), end, e);
    return found == end ? NPos : static_cast<std::size_t>(std::distance(m_storage.begin(), found));
  }

  std::size_t last_index_of(const_reference e, std::size_t til = NPos) const
    requires sequence_equal<const_reference, const_reference>
  {
    std::size_t index = NPos;
    std::size_t end = std::min(m_storage.size(), til);
    for (std::size_t i = 0; i < end; ++i) {
      if (operator[](i) == e)
        index = i;
    }
    return index;
  }

  auto at(std::size_t n) const& -> std::expected<std::reference_wrapper<value_type const>, StarException>
    requires std::is_lvalue_reference_v<const_reference>
  {
    if (n >= m_storage.size())
      return std::unexpected(OutOfRangeException(strf("out of range list::at({})", n)));
    return std::reference_wrapper<value_type const>(operator[](n));
  }

  auto at(std::size_t n) & -> std::expected<std::reference_wrapper<value_type>, StarException>
    requires std::is_lvalue_reference_v<reference>
  {
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

  auto take_at(std::size_t i) -> std::expected<value_type, StarException> {
    if (i >= size())
      return std::unexpected(OutOfRangeException("take_at index out of range"));
    value_type e = std::move(operator[](i));
    m_storage.erase(m_storage.begin() + i);
    return e;
  }

  auto wrap(std::size_t n) const& -> std::expected<std::reference_wrapper<value_type const>, StarException>
    requires std::is_lvalue_reference_v<const_reference>
  {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("wrap() called on empty list"));
    return std::reference_wrapper<value_type const>(operator[](n % m_storage.size()));
  }

  auto wrap(std::size_t n) & -> std::expected<std::reference_wrapper<value_type>, StarException>
    requires std::is_lvalue_reference_v<reference>
  {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("wrap() called on empty list"));
    return std::reference_wrapper<value_type>(operator[](n % m_storage.size()));
  }

  void at(std::size_t) && = delete;
  void at(std::size_t) const&& = delete;
  void wrap(std::size_t) && = delete;
  void wrap(std::size_t) const&& = delete;

  auto wrap(std::size_t n, value_type def) const -> value_type {
    if (m_storage.empty())
      return def;
    else
      return operator[](n % m_storage.size());
  }

  void erase_at(std::size_t i) {
    if constexpr (Star::DebugEnabled) {
      Star::starAssertFailed((i < m_storage.size()));
    }
    m_storage.erase(m_storage.begin() + i);
  }

  void erase_at(std::size_t b, std::size_t e) {
    if constexpr (Star::DebugEnabled) {
      Star::starAssertFailed((b < m_storage.size() && e <= m_storage.size()));
    }
    m_storage.erase(m_storage.begin() + b, m_storage.begin() + e);
  }

  auto insert_at(std::size_t pos, value_type e) -> AppendResult {
    if constexpr (Star::DebugEnabled)
      Star::starAssertFailed((pos <= m_storage.size()));
    m_storage.insert(m_storage.begin() + pos, std::move(e));
  }

  template <typename Container>
    requires sequence_source<Container const&, value_type> && std::ranges::common_range<Container const>
  auto insert_range_at(std::size_t pos, Container const& list) -> AppendResult {
    if constexpr (Star::DebugEnabled)
      Star::starAssertFailed((pos <= m_storage.size()));
    return SequenceAlgorithms::insert_range_at(*this, pos, list);
  }

  [[nodiscard]] auto set(std::size_t pos, value_type e) -> std::expected<void, StarException>
    requires std::is_default_constructible_v<value_type>
    && requires(reference slot, value_type value) { slot = std::move(value); }
  {
    return SequenceAlgorithms::set(*this, pos, std::move(e));
  }

  void swap(std::size_t i, std::size_t j) { std::iter_swap(begin() + i, begin() + j); }

  std::expected<void, StarException> move(std::size_t from, std::size_t to) {
    if (from >= m_storage.size() || to >= m_storage.size())
      return std::unexpected(OutOfRangeException("List::move index out of range"));
    if (from < to)
      std::rotate(m_storage.begin() + from, m_storage.begin() + from + 1, m_storage.begin() + to + 1);
    else if (to < from)
      std::rotate(m_storage.begin() + to, m_storage.begin() + from, m_storage.begin() + from + 1);
    return {};
  }

  template <typename Container>
    requires sequence_source<Container, value_type>
  void prepend_range(Container&& list) {
    if constexpr (std::ranges::bidirectional_range<Container> && std::ranges::common_range<Container>) {
      if (!SequenceAlgorithms::aliases(*this, list)) {
        for (auto i = std::ranges::rbegin(list); i != std::ranges::rend(list); ++i) {
          if constexpr (std::is_rvalue_reference_v<Container&&>)
            m_storage.emplace_front(std::move(*i));
          else
            m_storage.emplace_front(*i);
        }
        return;
      }
    }
    std::vector<value_type> snapshot;
    if constexpr (std::ranges::sized_range<Container>)
      snapshot.reserve(std::ranges::size(list));
    for (auto&& element : list) {
      if constexpr (std::is_rvalue_reference_v<Container&&>)
        snapshot.emplace_back(std::move(element));
      else
        snapshot.emplace_back(std::forward<decltype(element)>(element));
    }
    for (auto i = snapshot.rbegin(); i != snapshot.rend(); ++i)
      m_storage.emplace_front(std::move(*i));
  }

  std::expected<void, StarException> pop_front_checked() {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("pop_front_checked() called on empty list"));
    m_storage.pop_front();
    return {};
  }

  auto take_front() -> std::expected<value_type, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("take_front() called on empty list"));
    value_type e = std::move(*m_storage.begin());
    m_storage.pop_front();
    return e;
  }

  void truncate_front(std::size_t maximumSize) {
    while (m_storage.size() > maximumSize)
      m_storage.pop_front();
  }

  void first() && = delete;
  void first() const&& = delete;
  void last() && = delete;
  void last() const&& = delete;

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
  using size_type = typename Storage::size_type;
  using difference_type = typename Storage::difference_type;
  using pointer = typename Storage::pointer;
  using const_pointer = typename Storage::const_pointer;
  using allocator_type = typename Storage::allocator_type;
  using AppendResult = void;

  LinkedList() = default;
  LinkedList(Storage const& storage) : m_storage(storage) {}
  LinkedList(Storage&& storage) : m_storage(std::move(storage)) {}
  explicit LinkedList(std::size_t length, const_reference value = value_type()) : m_storage(length, value) {}
  explicit LinkedList(std::span<value_type const> values) : m_storage(values.begin(), values.end()) {}
  template <std::input_iterator Iterator, std::sentinel_for<Iterator> Sentinel>
    requires std::is_constructible_v<value_type, std::iter_reference_t<Iterator>>
  LinkedList(Iterator first, Sentinel last) {
    for (; first != last; ++first)
      emplace_back(*first);
  }
  template <typename Range>
    requires sequence_source<Range, value_type>
  explicit LinkedList(std::from_range_t, Range&& range) {
    append_range(std::forward<Range>(range));
  }
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
  size_type size() const noexcept(noexcept(m_storage.size())) { return m_storage.size(); }
  bool empty() const { return m_storage.empty(); }
  void clear() { m_storage.clear(); }
  reference front() { return *begin(); }
  const_reference front() const { return *begin(); }
  reference back() { return *std::prev(end()); }
  const_reference back() const { return *std::prev(end()); }
  void push_back(const_reference value)
    requires std::is_constructible_v<value_type, const_reference>
  {
    m_storage.push_back(value);
  }
  void push_back(value_type&& value)
    requires std::is_constructible_v<value_type, value_type&&>
  {
    m_storage.push_back(std::move(value));
  }
  void pop_back() noexcept(noexcept(m_storage.pop_back())) { m_storage.pop_back(); }
  template <typename... Args>
    requires std::is_constructible_v<value_type, Args...>
  decltype(auto) emplace_back(Args&&... args) noexcept(noexcept(m_storage.emplace_back(std::forward<Args>(args)...))) {
    return m_storage.emplace_back(std::forward<Args>(args)...);
  }
  decltype(auto) resize(std::size_t size)
    requires std::is_default_constructible_v<value_type>
  {
    return m_storage.resize(size);
  }
  decltype(auto) resize(std::size_t size, const_reference value)
    requires std::is_constructible_v<value_type, const_reference>
  {
    return m_storage.resize(size, value);
  }
  decltype(auto) insert(const_iterator pos, const_reference value) { return m_storage.insert(pos, value); }
  decltype(auto) insert(const_iterator pos, value_type&& value) { return m_storage.insert(pos, std::move(value)); }
  decltype(auto) insert(const_iterator pos, std::initializer_list<value_type> values) {
    return m_storage.insert(pos, values);
  }
  template <std::input_iterator Iterator>
    requires std::is_constructible_v<value_type, std::iter_reference_t<Iterator>>
  decltype(auto) insert(const_iterator pos, Iterator first, Iterator last) {
    return m_storage.insert(pos, first, last);
  }
  decltype(auto) insert(const_iterator pos, std::size_t count, const_reference value)
    requires std::is_constructible_v<value_type, const_reference>
  {
    return m_storage.insert(pos, count, value);
  }
  decltype(auto) erase(const_iterator pos) noexcept(noexcept(m_storage.erase(pos))) { return m_storage.erase(pos); }
  decltype(auto) erase(const_iterator first, const_iterator last) noexcept(noexcept(m_storage.erase(first, last))) {
    return m_storage.erase(first, last);
  }
  template <typename... Args>
    requires std::is_constructible_v<value_type, Args...>
  decltype(auto) emplace(const_iterator pos,
                         Args&&... args) noexcept(noexcept(m_storage.emplace(pos, std::forward<Args>(args)...))) {
    return m_storage.emplace(pos, std::forward<Args>(args)...);
  }
  decltype(auto) assign(std::initializer_list<value_type> values)
    requires std::is_constructible_v<value_type, const_reference>
  {
    return m_storage.assign(values);
  }
  template <std::input_iterator Iterator>
    requires std::is_constructible_v<value_type, std::iter_reference_t<Iterator>>
  decltype(auto) assign(Iterator first, Iterator last) {
    return m_storage.assign(first, last);
  }
  decltype(auto) assign(std::size_t count, const_reference value)
    requires std::is_constructible_v<value_type, const_reference>
  {
    return m_storage.assign(count, value);
  }
  size_type max_size() const noexcept(noexcept(m_storage.max_size())) { return m_storage.max_size(); }
  allocator_type get_allocator() const noexcept(noexcept(m_storage.get_allocator())) {
    return m_storage.get_allocator();
  }
  void push_front(const_reference value) { m_storage.push_front(value); }
  void push_front(value_type&& value) { m_storage.push_front(std::move(value)); }
  void pop_front() { m_storage.pop_front(); }
  template <typename... Args>
    requires std::is_constructible_v<value_type, Args...>
  decltype(auto)
  emplace_front(Args&&... args) noexcept(noexcept(m_storage.emplace_front(std::forward<Args>(args)...))) {
    return m_storage.emplace_front(std::forward<Args>(args)...);
  }
  void splice(const_iterator pos, LinkedList& other) { m_storage.splice(pos, other.m_storage); }
  void splice(const_iterator pos, LinkedList& other, const_iterator element) { m_storage.splice(pos, other.m_storage, element); }
  void splice(const_iterator pos, LinkedList& other, const_iterator first, const_iterator last) { m_storage.splice(pos, other.m_storage, first, last); }
  template <typename Comparator>
    requires sequence_comparator<Comparator&, reference, reference>
  void sort(Comparator&& comparator) {
    m_storage.sort(
      [&](auto const& left, auto const& right) { return static_cast<bool>(std::invoke(comparator, left, right)); });
  }
  void sort()
    requires sequence_less<reference, reference>
  {
    sort([](auto const& left, auto const& right) { return static_cast<bool>(left < right); });
  }
  void reverse() { m_storage.reverse(); }
  template <typename Predicate = std::equal_to<value_type>>
  auto unique(Predicate predicate = {}) { return m_storage.unique(std::move(predicate)); }
  template <typename Comparator = std::less<value_type>>
  void merge(LinkedList& other, Comparator comparator = {}) { m_storage.merge(other.m_storage, std::move(comparator)); }
  void swap(LinkedList& other) noexcept(noexcept(m_storage.swap(other.m_storage))) { m_storage.swap(other.m_storage); }
  bool operator==(LinkedList const& other) const { return m_storage == other.m_storage; }
  bool operator<(LinkedList const& other) const { return m_storage < other.m_storage; }
  bool operator>(LinkedList const& other) const { return other < *this; }
  bool operator<=(LinkedList const& other) const { return !(other < *this); }
  bool operator>=(LinkedList const& other) const { return !(*this < other); }

  template <typename Container>
    requires sequence_source<Container const&, Element>
  static LinkedList from(Container const& c);

  void append_range(LinkedList list);
  void prepend_range(LinkedList list);

  template <typename Container>
    requires sequence_source<Container, value_type>
  void append_range(Container&& list);
  template <typename Container>
    requires sequence_source<Container, value_type>
  void prepend_range(Container&& list) {
    if constexpr (std::ranges::bidirectional_range<Container> && std::ranges::common_range<Container>) {
      if (!SequenceAlgorithms::aliases(*this, list)) {
        for (auto i = std::ranges::rbegin(list); i != std::ranges::rend(list); ++i) {
          if constexpr (std::is_rvalue_reference_v<Container&&>)
            m_storage.emplace_front(std::move(*i));
          else
            m_storage.emplace_front(*i);
        }
        return;
      }
    }
    std::vector<value_type> snapshot;
    if constexpr (std::ranges::sized_range<Container>)
      snapshot.reserve(std::ranges::size(list));
    for (auto&& element : list) {
      if constexpr (std::is_rvalue_reference_v<Container&&>)
        snapshot.emplace_back(std::move(element));
      else
        snapshot.emplace_back(std::forward<decltype(element)>(element));
    }
    for (auto i = snapshot.rbegin(); i != snapshot.rend(); ++i)
      m_storage.emplace_front(std::move(*i));
  }

  template <typename Comparator>
    requires sequence_comparator<Comparator&, reference, reference>
  LinkedList sorted(Comparator&& comparator) const
    requires std::is_constructible_v<value_type, const_reference>;
  LinkedList sorted() const
    requires sequence_less<reference, reference> && std::is_constructible_v<value_type, const_reference>;

  template <typename Function>
    requires sequence_mapping<Function&, reference>
  [[nodiscard]]
  auto transformed(Function&& function);

  template <typename Function>
    requires sequence_mapping<Function&, const_reference>
  [[nodiscard]]
  auto transformed(Function&& function) const;

  auto first() & -> std::expected<std::reference_wrapper<value_type>, StarException>
    requires std::is_lvalue_reference_v<reference>
  {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("first() called on empty list"));
    return std::reference_wrapper<value_type>(*m_storage.begin());
  }

  auto first() const& -> std::expected<std::reference_wrapper<value_type const>, StarException>
    requires std::is_lvalue_reference_v<const_reference>
  {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("first() called on empty list"));
    return std::reference_wrapper<value_type const>(*m_storage.begin());
  }

  auto last() & -> std::expected<std::reference_wrapper<value_type>, StarException>
    requires std::is_lvalue_reference_v<reference>
  {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("last() called on empty list"));
    return std::reference_wrapper<value_type>(*std::prev(m_storage.end()));
  }

  auto last() const& -> std::expected<std::reference_wrapper<value_type const>, StarException>
    requires std::is_lvalue_reference_v<const_reference>
  {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("last() called on empty list"));
    return std::reference_wrapper<value_type const>(*std::prev(m_storage.end()));
  }

  auto front_value() const -> std::optional<value_type>
    requires std::is_constructible_v<value_type, const_reference>
  {
    if (m_storage.empty())
      return {};
    return std::optional<value_type>(std::in_place, *m_storage.begin());
  }

  auto back_value() const -> std::optional<value_type>
    requires std::is_constructible_v<value_type, const_reference>
  {
    if (m_storage.empty())
      return {};
    return std::optional<value_type>(std::in_place, *std::prev(m_storage.end()));
  }

  std::expected<void, StarException> pop_back_checked() {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("pop_back_checked() called on empty list"));
    m_storage.erase(std::prev(m_storage.end()));
    return {};
  }

  auto take_back() -> std::expected<value_type, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("take_back() called on empty list"));
    value_type e = std::move(*std::prev(m_storage.end()));
    m_storage.erase(std::prev(m_storage.end()));
    return e;
  }

  auto try_take_back() -> std::optional<value_type> {
    if (m_storage.empty())
      return {};
    value_type e = std::move(*std::prev(m_storage.end()));
    m_storage.erase(std::prev(m_storage.end()));
    return std::optional<value_type>(std::in_place, std::move(e));
  }

  void truncate_back(std::size_t maximumSize) {
    while (m_storage.size() > maximumSize)
      m_storage.erase(std::prev(m_storage.end()));
  }

  bool contains(const_reference e) const
    requires sequence_equal<const_reference, const_reference>
  {
    return std::find_if(begin(), end(), [&](auto&& value) { return static_cast<bool>(value == e); }) != end();
  }

  std::size_t remove(const_reference e)
    requires sequence_equal<reference, const_reference>
  {
    return SequenceAlgorithms::remove(*this, e);
  }

  template <typename Filter>
    requires sequence_predicate<Filter&, reference>
    && requires(reference slot, value_type&& value) { slot = std::move(value); }
  auto filter(Filter&& filter) {
    return SequenceAlgorithms::filter(*this, std::forward<Filter>(filter));
  }

  template <typename Self, typename Filter>
    requires std::is_copy_constructible_v<Self>
    && std::is_constructible_v<typename Self::value_type, typename Self::const_reference>
    && sequence_predicate<Filter&, typename Self::reference>
    && requires(typename Self::reference slot, typename Self::value_type&& value) { slot = std::move(value); }
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
    requires sequence_comparator<Comparator&, const_reference, const_reference>
  auto insert_sorted(value_type e, Comparator&& comparator) -> AppendResult {
    auto i = std::upper_bound(m_storage.begin(), m_storage.end(), e, [&](auto const& left, auto const& right) {
      return static_cast<bool>(std::invoke(comparator, left, right));
    });
    m_storage.insert(i, std::move(e));
  }

  auto insert_sorted(value_type e) -> AppendResult
    requires sequence_less<const_reference, const_reference>
  {
    return insert_sorted(std::move(e),
                         [](auto const& left, auto const& right) { return static_cast<bool>(left < right); });
  }

  template <typename Function>
    requires sequence_callable<Function&, reference>
  auto exec(Function&& function) {
    return SequenceAlgorithms::exec(*this, std::forward<Function>(function));
  }

  template <typename Function>
    requires sequence_callable<Function&, const_reference>
  auto exec(Function&& function) const {
    return SequenceAlgorithms::exec(*this, std::forward<Function>(function));
  }

  template <typename Function>
    requires sequence_transform<Function&, reference>
  auto transform(Function&& function) {
    return SequenceAlgorithms::transform(*this, std::forward<Function>(function));
  }

  template <typename Function>
    requires sequence_predicate<Function const&, const_reference>
  auto any(Function&& function) const {
    return SequenceAlgorithms::any(*this, std::forward<Function>(function));
  }

  auto any() const
    requires requires(const_reference value) { static_cast<bool>(value); }
  {
    return any([](auto const& value) -> auto const& { return value; });
  }

  template <typename Function>
    requires sequence_predicate<Function const&, const_reference>
  auto all(Function&& function) const {
    return SequenceAlgorithms::all(*this, std::forward<Function>(function));
  }

  auto all() const
    requires requires(const_reference value) { static_cast<bool>(value); }
  {
    return all([](auto const& value) -> auto const& { return value; });
  }

  std::expected<void, StarException> pop_front_checked() {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("pop_front_checked() called on empty list"));
    m_storage.pop_front();
    return {};
  }

  auto take_front() -> std::expected<value_type, StarException> {
    if (m_storage.empty())
      return std::unexpected(OutOfRangeException("take_front() called on empty list"));
    value_type e = std::move(*m_storage.begin());
    m_storage.pop_front();
    return e;
  }

  void truncate_front(std::size_t maximumSize) {
    while (m_storage.size() > maximumSize)
      m_storage.pop_front();
  }

  void first() && = delete;
  void first() const&& = delete;
  void last() && = delete;
  void last() const&& = delete;

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
  requires sequence_source<Container const&, Element>
List<Element, Allocator> List<Element, Allocator>::from(Container const& c) {
  return List(std::from_range, c);
}

template <typename Element, typename Allocator>
auto List<Element, Allocator>::slice(SliceIndex a, SliceIndex b, int i) const -> List {
  return Star::slice(*this, a, b, i);
}

template <typename Element, typename Allocator>
template <typename Comparator>
  requires sequence_comparator<Comparator&, typename List<Element, Allocator>::reference,
                               typename List<Element, Allocator>::reference>
auto List<Element, Allocator>::sorted(Comparator&& comparator) const -> List
  requires std::is_constructible_v<value_type, const_reference>
{
  List list(*this);
  list.sort(std::forward<Comparator>(comparator));
  return list;
}

template <typename Element, typename Allocator>
List<Element, Allocator> List<Element, Allocator>::sorted() const
  requires sequence_less<reference, reference> && std::is_constructible_v<value_type, const_reference>
{
  List list(*this);
  list.sort();
  return list;
}

template <typename Element, typename Allocator>
template <typename Function>
  requires sequence_mapping<Function&, typename List<Element, Allocator>::reference>
auto List<Element, Allocator>::transformed(Function&& function) {
  using Invocation = std::remove_cvref_t<std::invoke_result_t<Function&, reference>>;
  using Output = List<typename AlgorithmResultInfo<Invocation>::Value>;
  return Star::transform<Output>(*this, std::forward<Function>(function));
}

template <typename Element, typename Allocator>
template <typename Function>
  requires sequence_mapping<Function&, typename List<Element, Allocator>::const_reference>
auto List<Element, Allocator>::transformed(Function&& function) const {
  using Invocation = std::remove_cvref_t<std::invoke_result_t<Function&, const_reference>>;
  using Output = List<typename AlgorithmResultInfo<Invocation>::Value>;
  return Star::transform<Output>(*this, std::forward<Function>(function));
}

template <typename Element, std::size_t MaxSize>
template <typename Container>
  requires sequence_source<Container const&, Element>
auto StaticList<Element, MaxSize>::from(Container const& c) -> std::expected<StaticList, StarException> {
  return create(std::from_range, c);
}

template <typename Element, std::size_t MaxSize>
template <std::input_iterator Iterator, std::sentinel_for<Iterator> Sentinel>
  requires std::is_constructible_v<Element, std::iter_reference_t<Iterator>>
auto StaticList<Element, MaxSize>::create(Iterator first, Sentinel last) -> std::expected<StaticList, StarException> {
  if constexpr (std::sized_sentinel_for<Sentinel, Iterator>) {
    if (static_cast<std::size_t>(last - first) > MaxSize)
      return std::unexpected(StaticVectorSizeException("StaticList::create exceeds capacity"));
  } else if constexpr (std::forward_iterator<Iterator>) {
    if (static_cast<std::size_t>(std::ranges::distance(first, last)) > MaxSize)
      return std::unexpected(StaticVectorSizeException("StaticList::create exceeds capacity"));
  }
  StaticList result;
  for (; first != last; ++first) {
    auto added = result.emplace_back(*first);
    if (!added)
      return std::unexpected(std::move(added.error()));
  }
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
  requires sequence_comparator<Comparator&, typename StaticList<Element, MaxSize>::reference,
                               typename StaticList<Element, MaxSize>::reference>
auto StaticList<Element, MaxSize>::sorted(Comparator&& comparator) const -> StaticList
  requires std::is_constructible_v<value_type, const_reference>
{
  StaticList list(*this);
  list.sort(std::forward<Comparator>(comparator));
  return list;
}

template <typename Element, std::size_t MaxSize>
StaticList<Element, MaxSize> StaticList<Element, MaxSize>::sorted() const
  requires sequence_less<reference, reference> && std::is_constructible_v<value_type, const_reference>
{
  StaticList list(*this);
  list.sort();
  return list;
}

template <typename Element, std::size_t MaxSize>
template <typename Function>
  requires sequence_mapping<Function&, typename StaticList<Element, MaxSize>::reference>
auto StaticList<Element, MaxSize>::transformed(Function&& function) {
  using Invocation = std::remove_cvref_t<std::invoke_result_t<Function&, reference>>;
  using Output = StaticList<typename AlgorithmResultInfo<Invocation>::Value, MaxSize>;
  return Star::transform<Output>(*this, std::forward<Function>(function));
}

template <typename Element, std::size_t MaxSize>
template <typename Function>
  requires sequence_mapping<Function&, typename StaticList<Element, MaxSize>::const_reference>
auto StaticList<Element, MaxSize>::transformed(Function&& function) const {
  using Invocation = std::remove_cvref_t<std::invoke_result_t<Function&, const_reference>>;
  using Output = StaticList<typename AlgorithmResultInfo<Invocation>::Value, MaxSize>;
  return Star::transform<Output>(*this, std::forward<Function>(function));
}

template <typename Element, std::size_t MaxStackSize>
template <typename Container>
  requires sequence_source<Container const&, Element>
SmallList<Element, MaxStackSize> SmallList<Element, MaxStackSize>::from(Container const& c) {
  return SmallList(std::from_range, c);
}

template <typename Element, std::size_t MaxStackSize>
auto SmallList<Element, MaxStackSize>::slice(SliceIndex a, SliceIndex b, int i) const -> SmallList {
  return Star::slice(*this, a, b, i);
}

template <typename Element, std::size_t MaxStackSize>
template <typename Comparator>
  requires sequence_comparator<Comparator&, typename SmallList<Element, MaxStackSize>::reference,
                               typename SmallList<Element, MaxStackSize>::reference>
auto SmallList<Element, MaxStackSize>::sorted(Comparator&& comparator) const -> SmallList
  requires std::is_constructible_v<value_type, const_reference>
{
  SmallList list(*this);
  list.sort(std::forward<Comparator>(comparator));
  return list;
}

template <typename Element, std::size_t MaxStackSize>
SmallList<Element, MaxStackSize> SmallList<Element, MaxStackSize>::sorted() const
  requires sequence_less<reference, reference> && std::is_constructible_v<value_type, const_reference>
{
  SmallList list(*this);
  list.sort();
  return list;
}

template <typename Element, std::size_t MaxStackSize>
template <typename Function>
  requires sequence_mapping<Function&, typename SmallList<Element, MaxStackSize>::reference>
auto SmallList<Element, MaxStackSize>::transformed(Function&& function) {
  using Invocation = std::remove_cvref_t<std::invoke_result_t<Function&, reference>>;
  using Output = SmallList<typename AlgorithmResultInfo<Invocation>::Value, MaxStackSize>;
  return Star::transform<Output>(*this, std::forward<Function>(function));
}

template <typename Element, std::size_t MaxStackSize>
template <typename Function>
  requires sequence_mapping<Function&, typename SmallList<Element, MaxStackSize>::const_reference>
auto SmallList<Element, MaxStackSize>::transformed(Function&& function) const {
  using Invocation = std::remove_cvref_t<std::invoke_result_t<Function&, const_reference>>;
  using Output = SmallList<typename AlgorithmResultInfo<Invocation>::Value, MaxStackSize>;
  return Star::transform<Output>(*this, std::forward<Function>(function));
}

template <typename Element, typename Allocator>
template <typename Container>
  requires sequence_source<Container const&, Element>
Deque<Element, Allocator> Deque<Element, Allocator>::from(Container const& c) {
  return Deque(std::from_range, c);
}

template <typename Element, typename Allocator>
Deque<Element, Allocator> Deque<Element, Allocator>::slice(SliceIndex a, SliceIndex b, int i) const {
  return Star::slice(*this, a, b, i);
}

template <typename Element, typename Allocator>
template <typename Comparator>
  requires sequence_comparator<Comparator&, typename Deque<Element, Allocator>::reference,
                               typename Deque<Element, Allocator>::reference>
Deque<Element, Allocator> Deque<Element, Allocator>::sorted(Comparator&& comparator) const
  requires std::is_constructible_v<value_type, const_reference>
{
  Deque l(*this);
  l.sort(std::forward<Comparator>(comparator));
  return l;
}

template <typename Element, typename Allocator>
Deque<Element, Allocator> Deque<Element, Allocator>::sorted() const
  requires sequence_less<reference, reference> && std::is_constructible_v<value_type, const_reference>
{
  Deque l(*this);
  l.sort();
  return l;
}

template <typename Element, typename Allocator>
template <typename Function>
  requires sequence_mapping<Function&, typename Deque<Element, Allocator>::reference>
auto Deque<Element, Allocator>::transformed(Function&& function) {
  using Invocation = std::remove_cvref_t<std::invoke_result_t<Function&, reference>>;
  return Star::transform<Deque<typename AlgorithmResultInfo<Invocation>::Value>>(*this, std::forward<Function>(function));
}

template <typename Element, typename Allocator>
template <typename Function>
  requires sequence_mapping<Function&, typename Deque<Element, Allocator>::const_reference>
auto Deque<Element, Allocator>::transformed(Function&& function) const {
  using Invocation = std::remove_cvref_t<std::invoke_result_t<Function&, const_reference>>;
  return Star::transform<Deque<typename AlgorithmResultInfo<Invocation>::Value>>(*this, std::forward<Function>(function));
}

template <typename Element, typename Allocator>
template <typename Container>
  requires sequence_source<Container const&, Element>
LinkedList<Element, Allocator> LinkedList<Element, Allocator>::from(Container const& c) {
  return LinkedList(std::from_range, c);
}

template <typename Element, typename Allocator> void LinkedList<Element, Allocator>::append_range(LinkedList list) {
  if (m_storage.get_allocator() == list.m_storage.get_allocator())
    m_storage.splice(m_storage.end(), list.m_storage);
  else
    SequenceAlgorithms::append_range(*this, std::move(list));
}

template <typename Element, typename Allocator> void LinkedList<Element, Allocator>::prepend_range(LinkedList list) {
  if (m_storage.get_allocator() == list.m_storage.get_allocator())
    m_storage.splice(m_storage.begin(), list.m_storage);
  else
    for (auto i = list.rbegin(); i != list.rend(); ++i)
      m_storage.emplace_front(std::move(*i));
}

template <typename Element, typename Allocator>
template <typename Container>
  requires sequence_source<Container, Element>
void LinkedList<Element, Allocator>::append_range(Container&& list) {
  SequenceAlgorithms::append_range(*this, std::forward<Container>(list));
}

template <typename Element, typename Allocator>
template <typename Comparator>
  requires sequence_comparator<Comparator&, typename LinkedList<Element, Allocator>::reference,
                               typename LinkedList<Element, Allocator>::reference>
LinkedList<Element, Allocator> LinkedList<Element, Allocator>::sorted(Comparator&& comparator) const
  requires std::is_constructible_v<value_type, const_reference>
{
  LinkedList l(*this);
  l.sort(std::forward<Comparator>(comparator));
  return l;
}

template <typename Element, typename Allocator>
LinkedList<Element, Allocator> LinkedList<Element, Allocator>::sorted() const
  requires sequence_less<reference, reference> && std::is_constructible_v<value_type, const_reference>
{
  LinkedList l(*this);
  l.sort();
  return l;
}

template <typename Element, typename Allocator>
template <typename Function>
  requires sequence_mapping<Function&, typename LinkedList<Element, Allocator>::reference>
auto LinkedList<Element, Allocator>::transformed(Function&& function) {
  using Invocation = std::remove_cvref_t<std::invoke_result_t<Function&, reference>>;
  return Star::transform<LinkedList<typename AlgorithmResultInfo<Invocation>::Value>>(*this, std::forward<Function>(function));
}

template <typename Element, typename Allocator>
template <typename Function>
  requires sequence_mapping<Function&, typename LinkedList<Element, Allocator>::const_reference>
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
  requires SequenceContainer<List const>
std::size_t ListHasher<List>::operator()(List const& l) const
  requires hash_invocable<hash<typename List::value_type> const&, std::ranges::range_reference_t<List const>>
{
  std::size_t h = 0;
  for (auto const& e : l)
    hash_combine(h, elemHasher(e));
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
