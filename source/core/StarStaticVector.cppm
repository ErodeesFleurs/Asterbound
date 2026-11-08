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
template <typename Element, std::size_t MaxSize>
class StaticVector {
public:
  using iterator = Element*;
  using const_iterator = Element const*;

  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  using value_type = Element;

  using reference = Element&;
  using const_reference = Element const&;

  static constexpr std::size_t MaximumSize = MaxSize;

  StaticVector();
  StaticVector(StaticVector const& other);
  StaticVector(StaticVector&& other);
  template <typename OtherElement, std::size_t OtherMaxSize>
  [[nodiscard]]
  static std::expected<StaticVector, StarException> create(StaticVector<OtherElement, OtherMaxSize> const& other);
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
  Element const* ptr() const;
  Element* ptr();

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
  iterator erase(iterator begin, iterator end);

  bool operator==(StaticVector const& other) const;
  bool operator!=(StaticVector const& other) const;
  bool operator<(StaticVector const& other) const;

private:
  template <typename... Args>
  void appendKnown(Args&&... args) {
    std::construct_at(ptr() + m_size, std::forward<Args>(args)...);
    ++m_size;
  }
  void discardBack() {
    --m_size;
    std::destroy_at(ptr() + m_size);
  }
  std::size_t m_size = 0;
  alignas(Element) std::byte m_elements[MaxSize ? MaxSize * sizeof(Element) : alignof(Element)];
};

template <typename Element, std::size_t MaxSize> StaticVector<Element, MaxSize>::StaticVector() {}

template <typename Element, std::size_t MaxSize>
StaticVector<Element, MaxSize>::~StaticVector() {
  clear();
}

template <typename Element, std::size_t MaxSize>
StaticVector<Element, MaxSize>::StaticVector(StaticVector const& other)
  : StaticVector() {
  for (auto const& e : other)
    appendKnown(e);
}

template <typename Element, std::size_t MaxSize>
StaticVector<Element, MaxSize>::StaticVector(StaticVector&& other)
  : StaticVector() {
  for (auto& e : other)
    appendKnown(std::move(e));
}

template <typename Element, std::size_t MaxSize>
template <typename OtherElement, std::size_t OtherMaxSize>
auto StaticVector<Element, MaxSize>::create(StaticVector<OtherElement, OtherMaxSize> const& other) -> std::expected<StaticVector, StarException> {
  return create(other.begin(), other.end());
}

template <typename Element, std::size_t MaxSize>
template <class Iterator>
auto StaticVector<Element, MaxSize>::create(Iterator first, Iterator last) -> std::expected<StaticVector, StarException> {
  StaticVector result;
  auto inserted = result.insert(result.begin(), first, last);
  if (!inserted)
    return std::unexpected(std::move(inserted.error()));
  return result;
}

template <typename Element, std::size_t MaxSize>
auto StaticVector<Element, MaxSize>::create(std::size_t size, Element const& value) -> std::expected<StaticVector, StarException> {
  StaticVector result;
  auto resized = result.resize(size, value);
  if (!resized)
    return std::unexpected(std::move(resized.error()));
  return result;
}

template <typename Element, std::size_t MaxSize>
auto StaticVector<Element, MaxSize>::create(std::initializer_list<Element> list) -> std::expected<StaticVector, StarException> {
  return create(list.begin(), list.end());
}

template <typename Element, std::size_t MaxSize>
auto StaticVector<Element, MaxSize>::operator=(StaticVector const& other) -> StaticVector& {
  if (this == &other)
    return *this;

  auto overlap = std::min(m_size, other.size());
  std::copy_n(other.begin(), overlap, begin());
  while (m_size < other.size())
    appendKnown(other[m_size]);
  while (m_size > other.size())
    discardBack();

  return *this;
}

template <typename Element, std::size_t MaxSize>
auto StaticVector<Element, MaxSize>::operator=(StaticVector&& other) -> StaticVector& {
  if (this == &other)
    return *this;

  auto overlap = std::min(m_size, other.size());
  std::move(other.begin(), other.begin() + overlap, begin());
  while (m_size < other.size())
    appendKnown(std::move(other[m_size]));
  while (m_size > other.size())
    discardBack();

  return *this;
}

template <typename Element, std::size_t MaxSize>
auto StaticVector<Element, MaxSize>::assign(std::initializer_list<Element> list) -> std::expected<void, StarException> {
  if (list.size() > MaxSize)
    return std::unexpected(StaticVectorSizeException::format("StaticVector assignment size {} out of range {}", list.size(), MaxSize));
  auto overlap = std::min(m_size, list.size());
  std::copy_n(list.begin(), overlap, begin());
  while (m_size < list.size())
    appendKnown(list.begin()[m_size]);
  while (m_size > list.size())
    discardBack();
  return {};
}

template <typename Element, std::size_t MaxSize>
std::size_t StaticVector<Element, MaxSize>::size() const {
  return m_size;
}

template <typename Element, std::size_t MaxSize>
bool StaticVector<Element, MaxSize>::empty() const {
  return m_size == 0;
}

template <typename Element, std::size_t MaxSize>
std::expected<void, StarException> StaticVector<Element, MaxSize>::resize(std::size_t size, Element const& e) {
  if (size > MaxSize)
    return std::unexpected(StaticVectorSizeException::format("StaticVector::resize({}) out of range {}", size, MaxSize));
  while (m_size > size)
    discardBack();
  while (m_size < size)
    appendKnown(e);
  return {};
}

template <typename Element, std::size_t MaxSize>
auto StaticVector<Element, MaxSize>::at(std::size_t i) -> std::expected<std::reference_wrapper<Element>, StarException> {
  if (i >= m_size)
    return std::unexpected(OutOfRangeException::format("out of range in StaticVector::at({})", i));
  return std::reference_wrapper<Element>(ptr()[i]);
}

template <typename Element, std::size_t MaxSize>
auto StaticVector<Element, MaxSize>::at(std::size_t i) const -> std::expected<std::reference_wrapper<Element const>, StarException> {
  if (i >= m_size)
    return std::unexpected(OutOfRangeException::format("out of range in StaticVector::at({})", i));
  return std::reference_wrapper<Element const>(ptr()[i]);
}

template <typename Element, std::size_t MaxSize>
auto StaticVector<Element, MaxSize>::operator[](std::size_t i) -> reference {
  if constexpr (Star::DebugEnabled) {
    Star::star_assert_failed((i < m_size));
  }
  return ptr()[i];
}

template <typename Element, std::size_t MaxSize>
auto StaticVector<Element, MaxSize>::operator[](std::size_t i) const -> const_reference {
  if constexpr (Star::DebugEnabled) {
    Star::star_assert_failed((i < m_size));
  }
  return ptr()[i];
}

template <typename Element, std::size_t MaxSize>
auto StaticVector<Element, MaxSize>::begin() const -> const_iterator {
  return ptr();
}

template <typename Element, std::size_t MaxSize>
auto StaticVector<Element, MaxSize>::end() const -> const_iterator {
  return ptr() + m_size;
}

template <typename Element, std::size_t MaxSize>
auto StaticVector<Element, MaxSize>::begin() -> iterator {
  return ptr();
}

template <typename Element, std::size_t MaxSize>
auto StaticVector<Element, MaxSize>::end() -> iterator {
  return ptr() + m_size;
}

template <typename Element, std::size_t MaxSize>
auto StaticVector<Element, MaxSize>::rbegin() const -> const_reverse_iterator {
  return const_reverse_iterator(end());
}

template <typename Element, std::size_t MaxSize>
auto StaticVector<Element, MaxSize>::rend() const -> const_reverse_iterator {
  return const_reverse_iterator(begin());
}

template <typename Element, std::size_t MaxSize>
auto StaticVector<Element, MaxSize>::rbegin() -> reverse_iterator {
  return reverse_iterator(end());
}

template <typename Element, std::size_t MaxSize>
auto StaticVector<Element, MaxSize>::rend() -> reverse_iterator {
  return reverse_iterator(begin());
}

template <typename Element, std::size_t MaxSize>
Element const* StaticVector<Element, MaxSize>::ptr() const {
  return reinterpret_cast<Element const*>(m_elements);
}

template <typename Element, std::size_t MaxSize>
Element* StaticVector<Element, MaxSize>::ptr() {
  return reinterpret_cast<Element*>(m_elements);
}

template <typename Element, std::size_t MaxSize>
std::expected<void, StarException> StaticVector<Element, MaxSize>::push_back(Element e) {
  return emplace_back(std::move(e));
}

template <typename Element, std::size_t MaxSize>
std::expected<void, StarException> StaticVector<Element, MaxSize>::pop_back() {
  if (m_size == 0)
    return std::unexpected(OutOfRangeException("StaticVector::pop_back called on empty StaticVector"));
  discardBack();
  return {};
}

template <typename Element, std::size_t MaxSize>
auto StaticVector<Element, MaxSize>::insert(iterator pos, Element e) -> std::expected<iterator, StarException> {
  auto index = static_cast<std::size_t>(pos - ptr());
  auto inserted = emplace(pos, std::move(e));
  if (!inserted)
    return std::unexpected(std::move(inserted.error()));
  return begin() + index;
}

template <typename Element, std::size_t MaxSize>
template <typename Iterator>
auto StaticVector<Element, MaxSize>::insert(iterator pos, Iterator first, Iterator last) -> std::expected<iterator, StarException> {
  auto index = static_cast<std::size_t>(pos - ptr());
  auto previousSize = size();
  if constexpr (std::forward_iterator<Iterator>) {
    if (static_cast<std::size_t>(std::distance(first, last)) > MaxSize - previousSize)
      return std::unexpected(StaticVectorSizeException::format("StaticVector::insert would extend StaticVector beyond size {}", MaxSize));
  }
  for (; first != last; ++first) {
    auto added = emplace_back(*first);
    if (!added) {
      while (size() > previousSize)
        discardBack();
      return std::unexpected(std::move(added.error()));
    }
  }
  std::rotate(begin() + index, begin() + previousSize, end());
  return begin() + index;
}

template <typename Element, std::size_t MaxSize>
auto StaticVector<Element, MaxSize>::insert(iterator pos, std::initializer_list<Element> list) -> std::expected<iterator, StarException> {
  return insert(pos, list.begin(), list.end());
}

template <typename Element, std::size_t MaxSize>
template <typename... Args>
std::expected<void, StarException> StaticVector<Element, MaxSize>::emplace(iterator pos, Args&&... args) {
  auto index = static_cast<std::size_t>(pos - ptr());
  if (m_size == MaxSize)
    return std::unexpected(StaticVectorSizeException::format("StaticVector::emplace would extend StaticVector beyond size {}", MaxSize));
  Element value(std::forward<Args>(args)...);
  if (index == size()) {
    appendKnown(std::move(value));
    return {};
  }
  appendKnown(std::move(*(end() - 1)));
  std::move_backward(begin() + index, end() - 2, end() - 1);
  operator[](index) = std::move(value);
  return {};
}

template <typename Element, std::size_t MaxSize>
template <typename... Args>
std::expected<void, StarException> StaticVector<Element, MaxSize>::emplace_back(Args&&... args) {
  if (m_size == MaxSize)
    return std::unexpected(StaticVectorSizeException::format("StaticVector::emplace_back would extend StaticVector beyond size {}", MaxSize));
  appendKnown(std::forward<Args>(args)...);
  return {};
}

template <typename Element, std::size_t MaxSize>
void StaticVector<Element, MaxSize>::clear() {
  while (m_size != 0)
    discardBack();
}

template <typename Element, std::size_t MaxSize>
auto StaticVector<Element, MaxSize>::erase(iterator pos) -> iterator {
  std::size_t index = pos - ptr();
  // Move assignments may change m_size; keep the live bound for each shift.
  for (std::size_t i = index; i < m_size - 1; ++i)
    operator[](i) = std::move(operator[](i + 1));
  discardBack();
  return pos;
}

template <typename Element, std::size_t MaxSize>
auto StaticVector<Element, MaxSize>::erase(iterator begin, iterator end) -> iterator {
  std::size_t startIndex = begin - ptr();
  std::size_t endIndex = end - ptr();
  std::size_t toRemove = endIndex - startIndex;
  if (toRemove == 0)
    return begin;
  // Move assignments may append elements that must also be shifted.
  for (std::size_t i = endIndex; i < m_size; ++i)
    operator[](startIndex + (i - endIndex)) = std::move(operator[](i));
  while (toRemove--)
    discardBack();
  return begin;
}

template <typename Element, std::size_t MaxSize>
bool StaticVector<Element, MaxSize>::operator==(StaticVector const& other) const {
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

template <typename Element, std::size_t MaxSize>
bool StaticVector<Element, MaxSize>::operator!=(StaticVector const& other) const {
  return !operator==(other);
}

template <typename Element, std::size_t MaxSize>
bool StaticVector<Element, MaxSize>::operator<(StaticVector const& other) const {
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
