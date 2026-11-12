module;
import std;
import star.format;
import star.algorithm;
import star.memory;

namespace Star {

template <typename T>
concept small_vector_value =
  std::is_object_v<T> && !std::is_array_v<T> && !std::is_const_v<T> && !std::is_volatile_v<T>;

template <typename T, std::size_t Size>
concept small_vector_capacity = small_vector_value<T> && requires { sizeof(T); }
  && Size <= static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max()) / sizeof(T);

template <typename T, typename... Args>
concept small_vector_constructible = small_vector_value<T> && std::constructible_from<T, Args...>;

template <typename T, typename From>
concept small_vector_assignable = small_vector_constructible<T, From> && std::is_assignable_v<T&, From>;

template <typename T>
concept small_vector_relocatable = small_vector_assignable<T, T&&>;

template <typename T>
concept small_vector_equality = requires(T const& left, T const& right) { (left != right) ? true : false; };

template <typename T>
concept small_vector_order = requires(T const& left, T const& right) { (left < right) ? true : false; };

// Inline storage and a capacity-aware heap owner; no stored raw ownership or
// borrowed cursor state. Iterators and data() retain their contiguous interface.
template <small_vector_value Element, std::size_t MaxStackSize>
  requires small_vector_capacity<Element, MaxStackSize>
class SmallVector {
public:
  using value_type = Element;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using reference = value_type&;
  using const_reference = value_type const&;
  using iterator = value_type*;
  using const_iterator = value_type const*;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  SmallVector() {}
  SmallVector(SmallVector const& other)
    requires small_vector_constructible<value_type, const_reference>
      : SmallVector(other.begin(), other.end()) {}

  // Heap ownership transfers even after shrinking below inline capacity.
  SmallVector(SmallVector&& other)
    requires small_vector_constructible<value_type, value_type&&>
  {
    if (other.m_heap) {
      m_heap = std::move(other.m_heap);
      m_size = std::exchange(other.m_size, 0);
    } else {
      for (auto& value : other)
        emplace_back(std::move(value));
      other.clear();
    }
  }

  template <small_vector_value OtherElement, std::size_t OtherSize>
    requires small_vector_capacity<OtherElement, OtherSize>
    && small_vector_constructible<value_type, OtherElement const&>
  SmallVector(SmallVector<OtherElement, OtherSize> const& other) : SmallVector(other.begin(), other.end()) {}

  template <std::input_iterator Iterator, std::sentinel_for<Iterator> Sentinel>
    requires small_vector_constructible<value_type, std::iter_reference_t<Iterator>>
  SmallVector(Iterator first, Sentinel last) {
    if constexpr (std::forward_iterator<Iterator>) {
      auto count = range_size(first, last);
      reserve(count);
      if constexpr (std::contiguous_iterator<Iterator>
                    && std::is_trivially_constructible_v<value_type, std::iter_reference_t<Iterator>>) {
        std::uninitialized_copy_n(first, count, data());
        m_size = count;
        return;
      }
    }
    for (; first != last; ++first)
      emplace_back(*first);
  }

  SmallVector(size_type count, const_reference value = value_type())
    requires small_vector_constructible<value_type, const_reference>
  {
    resize(count, value);
  }

  SmallVector(std::initializer_list<value_type> values)
    requires small_vector_constructible<value_type, const_reference>
      : SmallVector(values.begin(), values.end()) {}

  ~SmallVector() { clear(); }

  SmallVector& operator=(SmallVector const& other)
    requires small_vector_assignable<value_type, const_reference>
  {
    if (this == &other)
      return *this;
    reserve(other.size());
    auto overlap = std::min(size(), other.size());
    std::copy_n(other.begin(), overlap, begin());
    while (size() < other.size())
      emplace_back(other[size()]);
    while (size() > other.size())
      discard_back();
    return *this;
  }

  SmallVector& operator=(SmallVector&& other)
    requires small_vector_relocatable<value_type>
  {
    if (this == &other)
      return *this;
    if (other.m_heap) {
      clear();
      m_heap = std::move(other.m_heap);
      m_size = std::exchange(other.m_size, 0);
    } else {
      auto overlap = std::min(size(), other.size());
      std::move(other.begin(), other.begin() + overlap, begin());
      while (size() < other.size())
        emplace_back(std::move(other[size()]));
      while (size() > other.size())
        discard_back();
      other.clear();
    }
    return *this;
  }

  SmallVector& operator=(std::initializer_list<value_type> values)
    requires small_vector_assignable<value_type, const_reference>
  {
    reserve(values.size());
    auto overlap = std::min(size(), values.size());
    std::copy_n(values.begin(), overlap, begin());
    while (size() < values.size())
      emplace_back(values.begin()[size()]);
    while (size() > values.size())
      discard_back();
    return *this;
  }

  size_type size() const noexcept { return m_size; }
  bool empty() const noexcept { return m_size == 0; }
  size_type capacity() const noexcept { return m_heap ? m_heap.get_deleter().capacity : MaxStackSize; }
  static constexpr size_type max_size() noexcept {
    return static_cast<size_type>(std::numeric_limits<difference_type>::max()) / sizeof(value_type);
  }

  void resize(size_type count, const_reference value = value_type())
    requires small_vector_constructible<value_type, const_reference>
  {
    reserve(count);
    while (size() > count)
      discard_back();
    if constexpr (std::is_trivially_copy_constructible_v<value_type>) {
      std::uninitialized_fill_n(end(), count - size(), value);
      m_size = count;
    } else {
      while (size() < count)
        emplace_back(value);
    }
  }

  void reserve(size_type requested)
    requires small_vector_constructible<value_type, value_type&&>
  {
    if (requested > max_size())
      fatal_error("SmallVector capacity exceeds max_size", false);
    auto previous_capacity = capacity();
    if (requested <= previous_capacity)
      return;
    auto grown = previous_capacity > max_size() / 2 ? max_size() : previous_capacity * 2;
    auto replacement = allocate_storage(std::max(grown, requested));
    auto previous = std::span<value_type>(data(), size());
    for (size_type i = 0; i < previous.size(); ++i)
      std::construct_at(replacement.get() + i, std::move(previous[i]));
    auto previous_owner = std::move(m_heap);
    m_heap = std::move(replacement);
    m_size = previous.size();
    std::destroy_n(previous.data(), previous.size());
  }

  [[nodiscard]] std::expected<std::reference_wrapper<value_type>, StarException> at(size_type index) {
    if (index >= size())
      return std::unexpected(OutOfRangeException::format("out of range in SmallVector::at({})", index));
    return std::ref(data()[index]);
  }
  [[nodiscard]] std::expected<std::reference_wrapper<value_type const>, StarException> at(size_type index) const {
    if (index >= size())
      return std::unexpected(OutOfRangeException::format("out of range in SmallVector::at({})", index));
    return std::cref(data()[index]);
  }
  reference operator[](size_type index) {
    if constexpr (DebugEnabled)
      star_assert_failed(index < size());
    return data()[index];
  }
  const_reference operator[](size_type index) const {
    if constexpr (DebugEnabled)
      star_assert_failed(index < size());
    return data()[index];
  }

  iterator begin() noexcept { return data(); }
  const_iterator begin() const noexcept { return data(); }
  iterator end() noexcept { return data() + size(); }
  const_iterator end() const noexcept { return data() + size(); }
  reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
  const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
  reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
  const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }

  // Inline storage has a stable non-null address, including zero capacity.
  value_type* data() noexcept { return m_heap ? m_heap.get() : reinterpret_cast<value_type*>(m_inline.data()); }
  value_type const* data() const noexcept {
    return m_heap ? m_heap.get() : reinterpret_cast<value_type const*>(m_inline.data());
  }

  void push_back(value_type value)
    requires small_vector_constructible<value_type, value_type&&>
  {
    emplace_back(std::move(value));
  }
  [[nodiscard]] std::expected<void, StarException> pop_back() {
    if (empty())
      return std::unexpected(OutOfRangeException("SmallVector::pop_back called on empty SmallVector"));
    discard_back();
    return {};
  }

  iterator insert(iterator position, value_type value)
    requires small_vector_relocatable<value_type>
  {
    auto index = static_cast<size_type>(position - begin());
    emplace(position, std::move(value));
    return begin() + index;
  }
  template <std::input_iterator Iterator, std::sentinel_for<Iterator> Sentinel>
    requires small_vector_constructible<value_type, std::iter_reference_t<Iterator>>
    && small_vector_relocatable<value_type>
  iterator insert(iterator position, Iterator first, Sentinel last) {
    auto index = static_cast<size_type>(position - begin());
    auto previous_size = size();
    if constexpr (std::forward_iterator<Iterator>)
      reserve(checked_add(previous_size, range_size(first, last)));
    for (; first != last; ++first)
      emplace_back(*first);
    std::rotate(begin() + index, begin() + previous_size, end());
    return begin() + index;
  }
  iterator insert(iterator position, std::initializer_list<value_type> values)
    requires small_vector_constructible<value_type, const_reference> && small_vector_relocatable<value_type>
  {
    return insert(position, values.begin(), values.end());
  }

  template <typename... Args>
    requires small_vector_constructible<value_type, Args&&...> && small_vector_relocatable<value_type>
  void emplace(iterator position, Args&&... args) {
    auto index = static_cast<size_type>(position - begin());
    value_type value(std::forward<Args>(args)...);
    reserve(checked_add(size(), 1));
    if (index == size()) {
      emplace_back(std::move(value));
      return;
    }
    emplace_back(std::move(*(end() - 1)));
    std::move_backward(begin() + index, end() - 2, end() - 1);
    operator[](index) = std::move(value);
  }
  template <typename... Args>
    requires small_vector_constructible<value_type, Args&&...> && small_vector_constructible<value_type, value_type&&>
  void emplace_back(Args&&... args) {
    if (size() == capacity())
      reserve(checked_add(size(), 1));
    std::construct_at(end(), std::forward<Args>(args)...);
    ++m_size;
  }

  void clear() {
    while (!empty())
      discard_back();
  }
  iterator erase(iterator position)
    requires small_vector_relocatable<value_type>
  {
    auto index = static_cast<size_type>(position - begin());
    // Assignments may append without reallocating: retain the live size bound.
    for (size_type i = index; i < size() - 1; ++i)
      operator[](i) = std::move(operator[](i + 1));
    discard_back();
    return position;
  }
  iterator erase(iterator first, iterator last)
    requires small_vector_relocatable<value_type>
  {
    auto start_index = static_cast<size_type>(first - begin());
    auto end_index = static_cast<size_type>(last - begin());
    auto count = end_index - start_index;
    if (count == 0)
      return first;
    for (size_type i = end_index; i < size(); ++i)
      operator[](start_index + i - end_index) = std::move(operator[](i));
    while (count--)
      discard_back();
    return first;
  }

  bool operator==(SmallVector const& other) const
    requires small_vector_equality<value_type>
  {
    if (this == &other)
      return true;
    if (size() != other.size())
      return false;
    if constexpr (std::is_floating_point_v<value_type>) {
      // Preserve floating comparison under production fast-math.
      for (size_type i = 0; i < size(); ++i)
        if (operator[](i) != other[i])
          return false;
      return true;
    } else {
      return std::equal(begin(), end(), other.begin(), [](const_reference left, const_reference right) -> bool {
        if (left != right)
          return false;
        return true;
      });
    }
  }
  bool operator<(SmallVector const& other) const
    requires small_vector_order<value_type>
  {
    if constexpr (std::is_floating_point_v<value_type>) {
      for (size_type i = 0; i < size(); ++i) {
        if (i >= other.size())
          return false;
        if (operator[](i) < other[i])
          return true;
        else if (other[i] < operator[](i))
          return false;
      }
      return size() < other.size();
    } else {
      return std::lexicographical_compare(begin(), end(), other.begin(), other.end(),
                                          [](const_reference left, const_reference right) -> bool {
                                            if (left < right)
                                              return true;
                                            return false;
                                          });
    }
  }

private:
  struct storage_deleter {
    size_type capacity = 0;
    void operator()(value_type* allocation) const noexcept {
      if constexpr (alignof(value_type) > alignof(std::max_align_t))
        std::allocator<value_type>{}.deallocate(allocation, capacity);
      else
        Star::free(allocation, capacity * sizeof(value_type));
    }
  };
  using storage_owner = std::unique_ptr<value_type[], storage_deleter>;

  static storage_owner allocate_storage(size_type capacity) {
    if constexpr (alignof(value_type) > alignof(std::max_align_t)) {
      return storage_owner(std::allocator<value_type>{}.allocate(capacity), storage_deleter{capacity});
    } else {
      storage_owner owner(static_cast<value_type*>(Star::malloc(capacity * sizeof(value_type))),
                          storage_deleter{capacity});
      if (!owner)
        std::terminate();
      return owner;
    }
  }
  static size_type checked_add(size_type size, size_type extra) {
    if (extra > max_size() - size)
      fatal_error("SmallVector capacity exceeds max_size", false);
    return size + extra;
  }
  template <std::forward_iterator Iterator, std::sentinel_for<Iterator> Sentinel>
  static size_type range_size(Iterator first, Sentinel last) {
    auto count = std::ranges::distance(first, last);
    if (!std::in_range<size_type>(count) || static_cast<size_type>(count) > max_size())
      fatal_error("SmallVector capacity exceeds max_size", false);
    return static_cast<size_type>(count);
  }
  void discard_back() {
    --m_size;
    std::destroy_at(end());
  }

  alignas(
    value_type) std::array<std::byte, MaxStackSize ? MaxStackSize * sizeof(value_type) : alignof(value_type)> m_inline;
  storage_owner m_heap{nullptr, storage_deleter{0}};
  size_type m_size = 0;
};

}// namespace Star

export module star.small_vector;

export namespace Star {
using ::Star::small_vector_assignable;
using ::Star::small_vector_capacity;
using ::Star::small_vector_constructible;
using ::Star::small_vector_equality;
using ::Star::small_vector_order;
using ::Star::small_vector_relocatable;
using ::Star::small_vector_value;
using ::Star::SmallVector;
}// namespace Star
