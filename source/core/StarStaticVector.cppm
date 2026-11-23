module;
import std;
import star.format;

namespace Star {

struct StaticVectorSizeExceptionTag {
  static constexpr std::string_view name() { return "StaticVectorSizeException"; }
};
using StaticVectorSizeException = StarError<StaticVectorSizeExceptionTag, StarException>;

// Stack allocated vector of elements with a dynamic size which must be less
// than a given maximum.  Acts like a vector with a built-in allocator of a
// maximum size. Capacity rejection is an explicit diagnostic.
template <typename Element, std::size_t fixed_capacity>
class StaticVector {
public:
  using iterator = typename std::span<Element>::iterator;
  using const_iterator = typename std::span<Element const>::iterator;

  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  using value_type = Element;

  using reference = Element&;
  using const_reference = Element const&;

  static constexpr std::size_t capacity() noexcept { return fixed_capacity; }
  static constexpr std::size_t max_size() noexcept { return fixed_capacity; }

  StaticVector();
  StaticVector(StaticVector const& other);
  StaticVector(StaticVector&& other);
  template <typename OtherElement, std::size_t other_capacity>
  [[nodiscard]]
  static std::expected<StaticVector, StarException> create(StaticVector<OtherElement, other_capacity> const& other);
  template <class Iterator>
  [[nodiscard]]
  static std::expected<StaticVector, StarException> create(Iterator first, Iterator last);
  [[nodiscard]]
  static std::expected<StaticVector, StarException> create(std::size_t size, Element const& value = Element());
  [[nodiscard]]
  static std::expected<StaticVector, StarException> create(std::initializer_list<Element> list);
  ~StaticVector();

  StaticVector& operator=(StaticVector const& other);
  StaticVector& operator=(StaticVector&& other);
  [[nodiscard]]
  std::expected<void, StarException> assign(std::initializer_list<Element> list);

  std::size_t size() const;
  bool empty() const;
  [[nodiscard]]
  std::expected<void, StarException> resize(std::size_t size, Element const& e = Element());

  [[nodiscard]]
  std::expected<std::reference_wrapper<Element>, StarException> at(std::size_t i);
  [[nodiscard]]
  std::expected<std::reference_wrapper<Element const>, StarException> at(std::size_t i) const;

  reference operator[](std::size_t i);
  const_reference operator[](std::size_t i) const;

  const_iterator begin() const;
  const_iterator end() const;

  iterator begin();
  iterator end();

  const_reverse_iterator rbegin() const;
  const_reverse_iterator rend() const;

  reverse_iterator rbegin();
  reverse_iterator rend();

  // Pointer to internal data, always valid even if empty.
  Element const* data() const;
  Element* data();

  [[nodiscard]]
  std::expected<void, StarException> push_back(Element e);
  [[nodiscard]]
  std::expected<void, StarException> pop_back();

  [[nodiscard]]
  std::expected<iterator, StarException> insert(iterator pos, Element e);
  template <typename Iterator>
  [[nodiscard]]
  std::expected<iterator, StarException> insert(iterator pos, Iterator begin, Iterator end);
  [[nodiscard]]
  std::expected<iterator, StarException> insert(iterator pos, std::initializer_list<Element> list);

  template <typename... Args>
  [[nodiscard]]
  std::expected<void, StarException> emplace(iterator pos, Args&&... args);

  template <typename... Args>
  [[nodiscard]]
  std::expected<void, StarException> emplace_back(Args&&... args);

  void clear();

  iterator erase(iterator pos);
  iterator erase(iterator first, iterator last);

  bool operator==(StaticVector const& other) const;
  bool operator!=(StaticVector const& other) const;
  bool operator<(StaticVector const& other) const;

private:
  template <typename... Args>
  void append_known(Args&&... args) {
    std::construct_at(data() + m_size, std::forward<Args>(args)...);
    ++m_size;
  }
  void discard_back() {
    --m_size;
    std::destroy_at(data() + m_size);
  }
  std::size_t m_size = 0;
  alignas(Element) std::byte m_elements[fixed_capacity ? fixed_capacity * sizeof(Element) : alignof(Element)];
};

template <typename Element, std::size_t fixed_capacity> StaticVector<Element, fixed_capacity>::StaticVector() {}

template <typename Element, std::size_t fixed_capacity>
StaticVector<Element, fixed_capacity>::~StaticVector() {
  clear();
}

template <typename Element, std::size_t fixed_capacity>
StaticVector<Element, fixed_capacity>::StaticVector(StaticVector const& other)
  : StaticVector() {
  for (auto const& e : other)
    append_known(e);
}

template <typename Element, std::size_t fixed_capacity>
StaticVector<Element, fixed_capacity>::StaticVector(StaticVector&& other)
  : StaticVector() {
  for (auto& e : other)
    append_known(std::move(e));
}

template <typename Element, std::size_t fixed_capacity>
template <typename OtherElement, std::size_t other_capacity>
auto StaticVector<Element, fixed_capacity>::create(StaticVector<OtherElement, other_capacity> const& other) -> std::expected<StaticVector, StarException> {
  return create(other.begin(), other.end());
}

template <typename Element, std::size_t fixed_capacity>
template <class Iterator>
auto StaticVector<Element, fixed_capacity>::create(Iterator first, Iterator last) -> std::expected<StaticVector, StarException> {
  StaticVector result;
  auto inserted = result.insert(result.begin(), first, last);
  if (!inserted)
    return std::unexpected(std::move(inserted.error()));
  return result;
}

template <typename Element, std::size_t fixed_capacity>
auto StaticVector<Element, fixed_capacity>::create(std::size_t size, Element const& value) -> std::expected<StaticVector, StarException> {
  StaticVector result;
  auto resized = result.resize(size, value);
  if (!resized)
    return std::unexpected(std::move(resized.error()));
  return result;
}

template <typename Element, std::size_t fixed_capacity>
auto StaticVector<Element, fixed_capacity>::create(std::initializer_list<Element> list) -> std::expected<StaticVector, StarException> {
  return create(list.begin(), list.end());
}

template <typename Element, std::size_t fixed_capacity>
auto StaticVector<Element, fixed_capacity>::operator=(StaticVector const& other) -> StaticVector& {
  if (this == &other)
    return *this;

  auto overlap = std::min(m_size, other.size());
  std::copy_n(other.begin(), overlap, begin());
  while (m_size < other.size())
    append_known(other[m_size]);
  while (m_size > other.size())
    discard_back();

  return *this;
}

template <typename Element, std::size_t fixed_capacity>
auto StaticVector<Element, fixed_capacity>::operator=(StaticVector&& other) -> StaticVector& {
  if (this == &other)
    return *this;

  auto overlap = std::min(m_size, other.size());
  std::move(other.begin(), other.begin() + overlap, begin());
  while (m_size < other.size())
    append_known(std::move(other[m_size]));
  while (m_size > other.size())
    discard_back();

  return *this;
}

template <typename Element, std::size_t fixed_capacity>
auto StaticVector<Element, fixed_capacity>::assign(std::initializer_list<Element> list) -> std::expected<void, StarException> {
  if (list.size() > fixed_capacity)
    return std::unexpected(StaticVectorSizeException::format("StaticVector assignment size {} out of range {}", list.size(), fixed_capacity));
  auto overlap = std::min(m_size, list.size());
  std::copy_n(list.begin(), overlap, begin());
  while (m_size < list.size())
    append_known(list.begin()[m_size]);
  while (m_size > list.size())
    discard_back();
  return {};
}

template <typename Element, std::size_t fixed_capacity>
std::size_t StaticVector<Element, fixed_capacity>::size() const {
  return m_size;
}

template <typename Element, std::size_t fixed_capacity>
bool StaticVector<Element, fixed_capacity>::empty() const {
  return m_size == 0;
}

template <typename Element, std::size_t fixed_capacity>
std::expected<void, StarException> StaticVector<Element, fixed_capacity>::resize(std::size_t size, Element const& e) {
  if (size > fixed_capacity)
    return std::unexpected(StaticVectorSizeException::format("StaticVector::resize({}) out of range {}", size, fixed_capacity));
  while (m_size > size)
    discard_back();
  while (m_size < size)
    append_known(e);
  return {};
}

template <typename Element, std::size_t fixed_capacity>
auto StaticVector<Element, fixed_capacity>::at(std::size_t i) -> std::expected<std::reference_wrapper<Element>, StarException> {
  if (i >= m_size)
    return std::unexpected(OutOfRangeException::format("out of range in StaticVector::at({})", i));
  return std::reference_wrapper<Element>(data()[i]);
}

template <typename Element, std::size_t fixed_capacity>
auto StaticVector<Element, fixed_capacity>::at(std::size_t i) const -> std::expected<std::reference_wrapper<Element const>, StarException> {
  if (i >= m_size)
    return std::unexpected(OutOfRangeException::format("out of range in StaticVector::at({})", i));
  return std::reference_wrapper<Element const>(data()[i]);
}

template <typename Element, std::size_t fixed_capacity>
auto StaticVector<Element, fixed_capacity>::operator[](std::size_t i) -> reference {
  if constexpr (Star::debug_enabled) {
    Star::star_assert_failed((i < m_size));
  }
  return data()[i];
}

template <typename Element, std::size_t fixed_capacity>
auto StaticVector<Element, fixed_capacity>::operator[](std::size_t i) const -> const_reference {
  if constexpr (Star::debug_enabled) {
    Star::star_assert_failed((i < m_size));
  }
  return data()[i];
}

template <typename Element, std::size_t fixed_capacity>
auto StaticVector<Element, fixed_capacity>::begin() const -> const_iterator {
  return std::span(data(), capacity()).begin();
}

template <typename Element, std::size_t fixed_capacity>
auto StaticVector<Element, fixed_capacity>::end() const -> const_iterator {
  return begin() + m_size;
}

template <typename Element, std::size_t fixed_capacity>
auto StaticVector<Element, fixed_capacity>::begin() -> iterator {
  return std::span(data(), capacity()).begin();
}

template <typename Element, std::size_t fixed_capacity>
auto StaticVector<Element, fixed_capacity>::end() -> iterator {
  return begin() + m_size;
}

template <typename Element, std::size_t fixed_capacity>
auto StaticVector<Element, fixed_capacity>::rbegin() const -> const_reverse_iterator {
  return const_reverse_iterator(end());
}

template <typename Element, std::size_t fixed_capacity>
auto StaticVector<Element, fixed_capacity>::rend() const -> const_reverse_iterator {
  return const_reverse_iterator(begin());
}

template <typename Element, std::size_t fixed_capacity>
auto StaticVector<Element, fixed_capacity>::rbegin() -> reverse_iterator {
  return reverse_iterator(end());
}

template <typename Element, std::size_t fixed_capacity>
auto StaticVector<Element, fixed_capacity>::rend() -> reverse_iterator {
  return reverse_iterator(begin());
}

template <typename Element, std::size_t fixed_capacity>
Element const* StaticVector<Element, fixed_capacity>::data() const {
  return reinterpret_cast<Element const*>(m_elements);
}

template <typename Element, std::size_t fixed_capacity>
Element* StaticVector<Element, fixed_capacity>::data() {
  return reinterpret_cast<Element*>(m_elements);
}

template <typename Element, std::size_t fixed_capacity>
std::expected<void, StarException> StaticVector<Element, fixed_capacity>::push_back(Element e) {
  return emplace_back(std::move(e));
}

template <typename Element, std::size_t fixed_capacity>
std::expected<void, StarException> StaticVector<Element, fixed_capacity>::pop_back() {
  if (m_size == 0)
    return std::unexpected(OutOfRangeException("StaticVector::pop_back called on empty StaticVector"));
  discard_back();
  return {};
}

template <typename Element, std::size_t fixed_capacity>
auto StaticVector<Element, fixed_capacity>::insert(iterator pos, Element e) -> std::expected<iterator, StarException> {
  auto index = static_cast<std::size_t>(pos - begin());
  auto inserted = emplace(pos, std::move(e));
  if (!inserted)
    return std::unexpected(std::move(inserted.error()));
  return begin() + index;
}

template <typename Element, std::size_t fixed_capacity>
template <typename Iterator>
auto StaticVector<Element, fixed_capacity>::insert(iterator pos, Iterator first, Iterator last) -> std::expected<iterator, StarException> {
  auto index = static_cast<std::size_t>(pos - begin());
  auto previous_size = size();
  if constexpr (std::forward_iterator<Iterator>) {
    if (static_cast<std::size_t>(std::distance(first, last)) > fixed_capacity - previous_size)
      return std::unexpected(StaticVectorSizeException::format("StaticVector::insert would extend StaticVector beyond size {}", fixed_capacity));
  }
  for (; first != last; ++first) {
    auto added = emplace_back(*first);
    if (!added) {
      while (size() > previous_size)
        discard_back();
      return std::unexpected(std::move(added.error()));
    }
  }
  std::rotate(begin() + index, begin() + previous_size, end());
  return begin() + index;
}

template <typename Element, std::size_t fixed_capacity>
auto StaticVector<Element, fixed_capacity>::insert(iterator pos, std::initializer_list<Element> list) -> std::expected<iterator, StarException> {
  return insert(pos, list.begin(), list.end());
}

template <typename Element, std::size_t fixed_capacity>
template <typename... Args>
std::expected<void, StarException> StaticVector<Element, fixed_capacity>::emplace(iterator pos, Args&&... args) {
  auto index = static_cast<std::size_t>(pos - begin());
  if (m_size == fixed_capacity)
    return std::unexpected(StaticVectorSizeException::format("StaticVector::emplace would extend StaticVector beyond size {}", fixed_capacity));
  Element value(std::forward<Args>(args)...);
  if (index == size()) {
    append_known(std::move(value));
    return {};
  }
  append_known(std::move(*(end() - 1)));
  std::move_backward(begin() + index, end() - 2, end() - 1);
  operator[](index) = std::move(value);
  return {};
}

template <typename Element, std::size_t fixed_capacity>
template <typename... Args>
std::expected<void, StarException> StaticVector<Element, fixed_capacity>::emplace_back(Args&&... args) {
  if (m_size == fixed_capacity)
    return std::unexpected(StaticVectorSizeException::format("StaticVector::emplace_back would extend StaticVector beyond size {}", fixed_capacity));
  append_known(std::forward<Args>(args)...);
  return {};
}

template <typename Element, std::size_t fixed_capacity>
void StaticVector<Element, fixed_capacity>::clear() {
  while (m_size != 0)
    discard_back();
}

template <typename Element, std::size_t fixed_capacity>
auto StaticVector<Element, fixed_capacity>::erase(iterator pos) -> iterator {
  std::size_t index = pos - begin();
  // Move assignments may change m_size; keep the live bound for each shift.
  for (std::size_t i = index; i < m_size - 1; ++i)
    operator[](i) = std::move(operator[](i + 1));
  discard_back();
  return begin() + index;
}

template <typename Element, std::size_t fixed_capacity>
auto StaticVector<Element, fixed_capacity>::erase(iterator first, iterator last) -> iterator {
  std::size_t start_index = first - begin();
  std::size_t end_index = last - begin();
  std::size_t to_remove = end_index - start_index;
  if (to_remove == 0)
    return first;
  // Move assignments may append elements that must also be shifted.
  for (std::size_t i = end_index; i < m_size; ++i)
    operator[](start_index + (i - end_index)) = std::move(operator[](i));
  while (to_remove--)
    discard_back();
  return begin() + start_index;
}

template <typename Element, std::size_t fixed_capacity>
bool StaticVector<Element, fixed_capacity>::operator==(StaticVector const& other) const {
  if (this == &other)
    return true;

  if (m_size != other.m_size)
    return false;
  // Preserve the original nonfinite comparison behavior under fast-math.
  if constexpr (std::is_floating_point_v<Element>) {
    for (std::size_t i = 0; i < m_size; ++i) {
      if (operator[](i) != other[i])
        return false;
    }
    return true;
  } else {
    return std::equal(begin(), end(), other.begin(), [](Element const& left, Element const& right) {
      if (left != right)
        return false;
      return true;
    });
  }
}

template <typename Element, std::size_t fixed_capacity>
bool StaticVector<Element, fixed_capacity>::operator!=(StaticVector const& other) const {
  return !operator==(other);
}

template <typename Element, std::size_t fixed_capacity>
bool StaticVector<Element, fixed_capacity>::operator<(StaticVector const& other) const {
  // Preserve the original nonfinite comparison behavior under fast-math.
  if constexpr (std::is_floating_point_v<Element>) {
    for (std::size_t i = 0; i < m_size; ++i) {
      if (i >= other.size())
        return false;

      Element const& a = operator[](i);
      Element const& b = other[i];

      if (a < b)
        return true;
      else if (b < a)
        return false;
    }

    return m_size < other.size();
  } else {
    return std::lexicographical_compare(begin(), end(), other.begin(), other.end(),
                                        [](Element const& left, Element const& right) {
                                          if (left < right)
                                            return true;
                                          return false;
                                        });
  }
}

}

export module star.static_vector;

export namespace Star {
  using ::Star::StaticVectorSizeExceptionTag;
  using ::Star::StaticVectorSizeException;
  using ::Star::StaticVector;
}
