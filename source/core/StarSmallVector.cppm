module;
import std;
import star.format;
import star.algorithm;
import star.memory;

namespace Star {

// A vector that is stack allocated up to a maximum size, becoming heap
// allocated when it grows beyond that size.  Always takes up stack space of
// MaxStackSize * sizeof(Element).
template <typename Element, std::size_t MaxStackSize>
class SmallVector {
public:
  typedef Element* iterator;
  typedef Element const* const_iterator;

  typedef std::reverse_iterator<iterator> reverse_iterator;
  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

  typedef Element value_type;

  typedef Element& reference;
  typedef Element const& const_reference;

  SmallVector();
  SmallVector(SmallVector const& other);
  // Successful moves leave the source empty and reusable. Heap storage is
  // transferred even when its current size fits on the stack.
  SmallVector(SmallVector&& other);
  template <typename OtherElement, std::size_t OtherMaxStackSize>
  SmallVector(SmallVector<OtherElement, OtherMaxStackSize> const& other);
  template <class Iterator>
  SmallVector(Iterator first, Iterator last);
  SmallVector(std::size_t size, Element const& value = Element());
  SmallVector(std::initializer_list<Element> list);
  ~SmallVector();

  SmallVector& operator=(SmallVector const& other);
  SmallVector& operator=(SmallVector&& other);
  SmallVector& operator=(std::initializer_list<Element> list);

  std::size_t size() const;
  bool empty() const;
  void resize(std::size_t size, Element const& e = Element());
  void reserve(std::size_t capacity);

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

  void push_back(Element e);
  [[nodiscard]]
  std::expected<void, StarException> pop_back();

  iterator insert(iterator pos, Element e);
  template <typename Iterator>
  iterator insert(iterator pos, Iterator begin, Iterator end);
  iterator insert(iterator pos, std::initializer_list<Element> list);

  template <typename... Args>
  void emplace(iterator pos, Args&&... args);

  template <typename... Args>
  void emplace_back(Args&&... args);

  void clear();

  iterator erase(iterator pos);
  iterator erase(iterator begin, iterator end);

  bool operator==(SmallVector const& other) const;
  bool operator!=(SmallVector const& other) const;
  bool operator<(SmallVector const& other) const;

private:
  void discardBack() {
    --m_end;
    std::destroy_at(m_end);
  }
  alignas(Element) std::byte m_stackElements[MaxStackSize ? MaxStackSize * sizeof(Element) : alignof(Element)];

  bool isHeapAllocated() const;
  static void freeHeapElements(Element* elements, std::size_t capacity);

  Element* m_begin;
  Element* m_end;
  Element* m_capacity;
};

template <typename Element, std::size_t MaxStackSize>
SmallVector<Element, MaxStackSize>::SmallVector() {
  m_begin = reinterpret_cast<Element*>(m_stackElements);
  m_end = m_begin;
  m_capacity = m_begin + MaxStackSize;
}

template <typename Element, std::size_t MaxStackSize>
SmallVector<Element, MaxStackSize>::~SmallVector() {
  clear();
  if (isHeapAllocated()) {
    freeHeapElements(m_begin, m_capacity - m_begin);
  }
}

template <typename Element, std::size_t MaxStackSize>
SmallVector<Element, MaxStackSize>::SmallVector(SmallVector const& other)
  : SmallVector() {
  insert(begin(), other.begin(), other.end());
}

template <typename Element, std::size_t MaxStackSize>
SmallVector<Element, MaxStackSize>::SmallVector(SmallVector&& other)
  : SmallVector() {
  if (other.isHeapAllocated()) {
    m_begin = other.m_begin;
    m_end = other.m_end;
    m_capacity = other.m_capacity;
    other.m_begin = reinterpret_cast<Element*>(other.m_stackElements);
    other.m_end = other.m_begin;
    other.m_capacity = other.m_begin + MaxStackSize;
  } else {
    for (auto& e : other)
      emplace_back(std::move(e));
    other.clear();
  }
}

template <typename Element, std::size_t MaxStackSize>
template <typename OtherElement, std::size_t OtherMaxStackSize>
SmallVector<Element, MaxStackSize>::SmallVector(SmallVector<OtherElement, OtherMaxStackSize> const& other)
  : SmallVector() {
    for (auto const& e : other)
      emplace_back(e);
}

template <typename Element, std::size_t MaxStackSize>
template <class Iterator>
SmallVector<Element, MaxStackSize>::SmallVector(Iterator first, Iterator last)
  : SmallVector() {
    insert(begin(), first, last);
}

template <typename Element, std::size_t MaxStackSize>
SmallVector<Element, MaxStackSize>::SmallVector(std::size_t size, Element const& value)
  : SmallVector() {
    resize(size, value);
}

template <typename Element, std::size_t MaxStackSize>
SmallVector<Element, MaxStackSize>::SmallVector(std::initializer_list<Element> list)
  : SmallVector() {
    for (auto const& e : list)
      emplace_back(e);
}

template <typename Element, std::size_t MaxStackSize>
auto SmallVector<Element, MaxStackSize>::operator=(SmallVector const& other) -> SmallVector& {
  if (this == &other)
    return *this;

  reserve(other.size());
  auto overlap = std::min(size(), other.size());
  std::copy_n(other.begin(), overlap, begin());
  while (size() < other.size())
    emplace_back(other[size()]);
  while (size() > other.size())
    discardBack();

  return *this;
}

template <typename Element, std::size_t MaxStackSize>
auto SmallVector<Element, MaxStackSize>::operator=(SmallVector&& other) -> SmallVector& {
  if (this == &other)
    return *this;

  if (other.isHeapAllocated()) {
    clear();
    if (isHeapAllocated())
      freeHeapElements(m_begin, m_capacity - m_begin);
    m_begin = other.m_begin;
    m_end = other.m_end;
    m_capacity = other.m_capacity;
    other.m_begin = reinterpret_cast<Element*>(other.m_stackElements);
    other.m_end = other.m_begin;
    other.m_capacity = other.m_begin + MaxStackSize;
  } else {
    auto overlap = std::min(size(), other.size());
    std::move(other.begin(), other.begin() + overlap, begin());
    while (size() < other.size())
      emplace_back(std::move(other[size()]));
    while (size() > other.size())
      discardBack();
    other.clear();
  }

  return *this;
}

template <typename Element, std::size_t MaxStackSize>
auto SmallVector<Element, MaxStackSize>::operator=(std::initializer_list<Element> list) -> SmallVector& {
  reserve(list.size());
  auto overlap = std::min(size(), list.size());
  std::copy_n(list.begin(), overlap, begin());
  while (size() < list.size())
    emplace_back(list.begin()[size()]);
  while (size() > list.size())
    discardBack();
  return *this;
}

template <typename Element, std::size_t MaxStackSize>
std::size_t SmallVector<Element, MaxStackSize>::size() const {
  return m_end - m_begin;
}

template <typename Element, std::size_t MaxStackSize>
bool SmallVector<Element, MaxStackSize>::empty() const {
  return m_begin == m_end;
}

template <typename Element, std::size_t MaxStackSize>
void SmallVector<Element, MaxStackSize>::resize(std::size_t size, Element const& e) {
  reserve(size);

  for (std::size_t i = this->size(); i > size; --i)
    discardBack();
  for (std::size_t i = this->size(); i < size; ++i)
    emplace_back(e);
}

template <typename Element, std::size_t MaxStackSize>
void SmallVector<Element, MaxStackSize>::reserve(std::size_t newCapacity) {
  std::size_t oldCapacity = m_capacity - m_begin;
  if (newCapacity > oldCapacity) {
    newCapacity = std::max(oldCapacity * 2, newCapacity);
    Element* newMem;
    if constexpr (alignof(Element) > alignof(std::max_align_t)) {
      newMem = std::allocator<Element>{}.allocate(newCapacity);
    } else {
      newMem = static_cast<Element*>(Star::malloc(newCapacity * sizeof(Element)));
      if (!newMem)
        std::terminate();
    }

    std::size_t size = m_end - m_begin;
    auto oldMem = m_begin;
    auto oldHeapAllocated = isHeapAllocated();

    for (std::size_t constructed = 0; constructed < size; ++constructed)
      std::construct_at(newMem + constructed, std::move(oldMem[constructed]));

    m_begin = newMem;
    m_end = m_begin + size;
    m_capacity = m_begin + newCapacity;

    std::destroy_n(oldMem, size);
    if (oldHeapAllocated)
      freeHeapElements(oldMem, oldCapacity);
  }
}

template <typename Element, std::size_t MaxStackSize>
auto SmallVector<Element, MaxStackSize>::at(std::size_t i) -> std::expected<std::reference_wrapper<Element>, StarException> {
  if (i >= size())
    return std::unexpected(OutOfRangeException::format("out of range in SmallVector::at({})", i));
  return std::reference_wrapper<Element>(m_begin[i]);
}

template <typename Element, std::size_t MaxStackSize>
auto SmallVector<Element, MaxStackSize>::at(std::size_t i) const -> std::expected<std::reference_wrapper<Element const>, StarException> {
  if (i >= size())
    return std::unexpected(OutOfRangeException::format("out of range in SmallVector::at({})", i));
  return std::reference_wrapper<Element const>(m_begin[i]);
}

template <typename Element, std::size_t MaxStackSize>
auto SmallVector<Element, MaxStackSize>::operator[](std::size_t i) -> reference {
  if constexpr (Star::DebugEnabled) {
    Star::starAssertFailed((i < size()));
  }
  return m_begin[i];
}

template <typename Element, std::size_t MaxStackSize>
auto SmallVector<Element, MaxStackSize>::operator[](std::size_t i) const -> const_reference {
  if constexpr (Star::DebugEnabled) {
    Star::starAssertFailed((i < size()));
  }
  return m_begin[i];
}

template <typename Element, std::size_t MaxStackSize>
auto SmallVector<Element, MaxStackSize>::begin() const -> const_iterator {
  return m_begin;
}

template <typename Element, std::size_t MaxStackSize>
auto SmallVector<Element, MaxStackSize>::end() const -> const_iterator {
  return m_end;
}

template <typename Element, std::size_t MaxStackSize>
auto SmallVector<Element, MaxStackSize>::begin() -> iterator {
  return m_begin;
}

template <typename Element, std::size_t MaxStackSize>
auto SmallVector<Element, MaxStackSize>::end() -> iterator {
  return m_end;
}

template <typename Element, std::size_t MaxStackSize>
auto SmallVector<Element, MaxStackSize>::rbegin() const -> const_reverse_iterator {
    return const_reverse_iterator(end());
}

template <typename Element, std::size_t MaxStackSize>
auto SmallVector<Element, MaxStackSize>::rend() const -> const_reverse_iterator {
    return const_reverse_iterator(begin());
}

template <typename Element, std::size_t MaxStackSize>
auto SmallVector<Element, MaxStackSize>::rbegin() -> reverse_iterator {
    return reverse_iterator(end());
}

template <typename Element, std::size_t MaxStackSize>
auto SmallVector<Element, MaxStackSize>::rend() -> reverse_iterator {
    return reverse_iterator(begin());
}

template <typename Element, std::size_t MaxStackSize>
Element const* SmallVector<Element, MaxStackSize>::ptr() const {
  return m_begin;
}

template <typename Element, std::size_t MaxStackSize>
Element* SmallVector<Element, MaxStackSize>::ptr() {
  return m_begin;
}

template <typename Element, std::size_t MaxStackSize>
void SmallVector<Element, MaxStackSize>::push_back(Element e) {
  emplace_back(std::move(e));
}

template <typename Element, std::size_t MaxStackSize>
std::expected<void, StarException> SmallVector<Element, MaxStackSize>::pop_back() {
  if (m_begin == m_end)
    return std::unexpected(OutOfRangeException("SmallVector::pop_back called on empty SmallVector"));
  discardBack();
  return {};
}

template <typename Element, std::size_t MaxStackSize>
auto SmallVector<Element, MaxStackSize>::insert(iterator pos, Element e) -> iterator {
  auto index = static_cast<std::size_t>(pos - m_begin);
  emplace(pos, std::move(e));
  return begin() + index;
}

template <typename Element, std::size_t MaxStackSize>
template <typename Iterator>
auto SmallVector<Element, MaxStackSize>::insert(iterator pos, Iterator first, Iterator last) -> iterator {
  auto index = static_cast<std::size_t>(pos - m_begin);
  auto previousSize = size();
  if constexpr (std::forward_iterator<Iterator>)
    reserve(previousSize + std::distance(first, last));
  for (; first != last; ++first)
    emplace_back(*first);
  std::rotate(begin() + index, begin() + previousSize, end());
  return begin() + index;
}

template <typename Element, std::size_t MaxStackSize>
auto SmallVector<Element, MaxStackSize>::insert(iterator pos, std::initializer_list<Element> list) -> iterator {
  return insert(pos, list.begin(), list.end());
}

template <typename Element, std::size_t MaxStackSize>
template <typename... Args>
void SmallVector<Element, MaxStackSize>::emplace(iterator pos, Args&&... args) {
  auto index = static_cast<std::size_t>(pos - m_begin);
  Element value(std::forward<Args>(args)...);
  reserve(size() + 1);
  if (index == size()) {
    emplace_back(std::move(value));
    return;
  }
  emplace_back(std::move(*(m_end - 1)));
  std::move_backward(begin() + index, end() - 2, end() - 1);
  operator[](index) = std::move(value);
}

template <typename Element, std::size_t MaxStackSize>
template <typename... Args>
void SmallVector<Element, MaxStackSize>::emplace_back(Args&&... args) {
  if (m_end == m_capacity)
    reserve(size() + 1);
  std::construct_at(m_end, std::forward<Args>(args)...);
  ++m_end;
}

template <typename Element, std::size_t MaxStackSize>
void SmallVector<Element, MaxStackSize>::clear() {
  while (m_begin != m_end)
    discardBack();
}

template <typename Element, std::size_t MaxStackSize>
auto SmallVector<Element, MaxStackSize>::erase(iterator pos) -> iterator {
  std::size_t index = pos - ptr();
  // Assignments can append without reallocating, so keep the live size bound.
  for (std::size_t i = index; i < size() - 1; ++i)
    operator[](i) = std::move(operator[](i + 1));
  discardBack();
  return pos;
}

template <typename Element, std::size_t MaxStackSize>
auto SmallVector<Element, MaxStackSize>::erase(iterator begin, iterator end) -> iterator {
  std::size_t startIndex = begin - ptr();
  std::size_t endIndex = end - ptr();
  std::size_t toRemove = endIndex - startIndex;
  if (toRemove == 0)
    return begin;
  // As above, an assignment can extend the suffix that still needs shifting.
  for (std::size_t i = endIndex; i < size(); ++i)
    operator[](startIndex + (i - endIndex)) = std::move(operator[](i));
  while (toRemove--)
    discardBack();
  return begin;
}

template <typename Element, std::size_t MaxStackSize>
bool SmallVector<Element, MaxStackSize>::operator==(SmallVector const& other) const {
  if (this == &other)
    return true;

  if (size() != other.size())
    return false;

  if constexpr (std::is_floating_point_v<Element>) {
    // Preserve the original floating comparison under production fast-math.
    for (std::size_t i = 0; i < size(); ++i) {
      if (operator[](i) != other[i])
        return false;
    }
    return true;
  } else {
    return std::equal(begin(), end(), other.begin(), [](Element const& left, Element const& right) -> bool {
      if (left != right)
        return false;
      return true;
    });
  }
}

template <typename Element, std::size_t MaxStackSize>
bool SmallVector<Element, MaxStackSize>::operator!=(SmallVector const& other) const {
  return !operator==(other);
}

template <typename Element, std::size_t MaxStackSize>
bool SmallVector<Element, MaxStackSize>::operator<(SmallVector const& other) const {
  if constexpr (std::is_floating_point_v<Element>) {
    // Preserve the original floating comparison under production fast-math.
    for (std::size_t i = 0; i < size(); ++i) {
      if (i >= other.size())
        return false;

      Element const& a = operator[](i);
      Element const& b = other[i];

      if (a < b)
        return true;
      else if (b < a)
        return false;
    }

    return size() < other.size();
  } else {
    return std::lexicographical_compare(begin(), end(), other.begin(), other.end(),
                                        [](Element const& left, Element const& right) -> bool {
                                          if (left < right)
                                            return true;
                                          return false;
                                        });
  }
}

template <typename Element, std::size_t MaxStackSize>
bool SmallVector<Element, MaxStackSize>::isHeapAllocated() const {
  return m_begin != reinterpret_cast<Element const*>(m_stackElements);
}

template <typename Element, std::size_t MaxStackSize>
void SmallVector<Element, MaxStackSize>::freeHeapElements(Element* elements, std::size_t capacity) {
  if constexpr (alignof(Element) > alignof(std::max_align_t))
    std::allocator<Element>{}.deallocate(elements, capacity);
  else
    Star::free(elements, capacity * sizeof(Element));
}

}

export module star.small_vector;

export namespace Star {
  using ::Star::SmallVector;
}
