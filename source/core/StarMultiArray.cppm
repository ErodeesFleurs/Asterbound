module;
import std;
import star.format;
import star.text_formatter;
import star.list;
import star.array;
import star.optional;

namespace Star {

struct MultiArrayExceptionTag {
  static constexpr std::string_view name() { return "MultiArrayException"; }
};
using MultiArrayException = StarError<MultiArrayExceptionTag, StarException>;

// Multidimensional array class that wraps a vector as a simple contiguous N
// dimensional array. The last index is contiguous (stride 1), matching
// std::layout_right; the first index has the largest stride.
//
// Due to usage of std::vector, ElementT = bool means that the user must use
// set() and get() rather than operator()
template <typename ElementT, std::size_t RankN>
class MultiArray {
public:
  using Storage = List<ElementT>;

  using Element = ElementT;
  static std::size_t const Rank = RankN;

  using IndexArray = Array<std::size_t, Rank>;
  using SizeArray = Array<std::size_t, Rank>;

  using Extents = std::dextents<std::size_t, Rank>;
  using View = std::mdspan<Element, Extents, std::layout_right>;
  using ConstView = std::mdspan<Element const, Extents, std::layout_right>;

  using iterator = typename Storage::iterator;
  using const_iterator = typename Storage::const_iterator;
  using value_type = Element;

  MultiArray();
  template <typename... T>
  explicit MultiArray(std::size_t i, T... rest);
  explicit MultiArray(SizeArray const& shape);
  explicit MultiArray(SizeArray const& shape, Element const& c);

  SizeArray const& size() const;
  std::size_t size(std::size_t dimension) const;

  void clear();

  void resize(SizeArray const& shape);
  void resize(SizeArray const& shape, Element const& c);

  template <typename... T>
  void resize(std::size_t i, T... rest);

  void fill(Element const& element);

  // Does not preserve previous element position, array contents will be
  // invalid.
  void setSize(SizeArray const& shape);
  void setSize(SizeArray const& shape, Element const& c);

  template <typename... T>
  void setSize(std::size_t i, T... rest);

  Element& operator()(IndexArray const& index);
  Element const& operator()(IndexArray const& index) const;

  template <typename... T>
  Element& operator()(std::size_t i1, T... rest);
  template <typename... T>
  Element const& operator()(std::size_t i1, T... rest) const;

  // Returns an error if out of bounds.
  [[nodiscard]] std::expected<std::reference_wrapper<Element>, StarException> at(IndexArray const& index);
  [[nodiscard]] std::expected<std::reference_wrapper<Element const>, StarException> at(IndexArray const& index) const;

  template <typename... T>
  [[nodiscard]] std::expected<std::reference_wrapper<Element>, StarException> at(std::size_t i1, T... rest);
  template <typename... T>
  [[nodiscard]] std::expected<std::reference_wrapper<Element const>, StarException> at(std::size_t i1, T... rest) const;

  // Returns an error if out of bounds.
  [[nodiscard]] std::expected<void, StarException> set(IndexArray const& index, Element element);

  // Returns default element if out of bounds.
  Element get(IndexArray const& index, Element def = Element());

  // Auto-resizes array if out of bounds
  void setResize(IndexArray const& index, Element element);

  // Copy the given array element for element into this array.  The shape of
  // this array must be at least as large in every dimension as the source
  // array
  void copy(MultiArray const& source);
  void copy(MultiArray const& source, IndexArray const& sourceMin, IndexArray const& sourceMax, IndexArray const& targetMin);

  // op will be called with IndexArray and Element parameters.
  template <typename OpType>
  void forEach(IndexArray const& min, SizeArray const& size, OpType&& op);
  template <typename OpType>
  void forEach(IndexArray const& min, SizeArray const& size, OpType&& op) const;

  // Shortcut for calling forEach on the entire array
  template <typename OpType>
  void forEach(OpType&& op);
  template <typename OpType>
  void forEach(OpType&& op) const;

  void print(TextBuffer& output) const
      requires std::same_as<optional_diagnostic_result<Element>, void>;

  // Api for more direct access to elements.

  std::size_t count() const;

  Element const& atIndex(std::size_t index) const;
  Element& atIndex(std::size_t index);

  Element const* data() const;
  Element* data();

  // Unchecked, non-owning views of ordinary contiguous storage (not bool proxies).
  // Keep the owner alive and do not resize, clear, move or replace it while borrowed.
  // Reacquire the view after any operation that changes storage or shape.
  View view() & requires std::same_as<typename Storage::reference, Element&>;
  ConstView view() const & requires std::same_as<typename Storage::reference, Element&>;
  View view() && = delete;
  ConstView view() const && = delete;

private:
  std::size_t storageIndex(IndexArray const& index) const;

  void sub_print(TextBuffer& output, IndexArray index, std::size_t dim) const
      requires std::same_as<optional_diagnostic_result<Element>, void>;

  template <typename OpType>
  void subForEach(IndexArray const& min, SizeArray const& size, OpType&& op, IndexArray& index, std::size_t offset, std::size_t dim) const;

  template <typename OpType>
  void subForEach(IndexArray const& min, SizeArray const& size, OpType&& op, IndexArray& index, std::size_t offset, std::size_t dim);

  void subCopy(MultiArray const& source, IndexArray const& sourceMin, IndexArray const& sourceMax,
      IndexArray const& targetMin, IndexArray& sourceIndex, IndexArray& targetIndex, std::size_t dim);

  Storage m_data;
  SizeArray m_shape;
};

using MultiArray2I = MultiArray<int, 2>;
using MultiArray2S = MultiArray<std::size_t, 2>;
using MultiArray2U = MultiArray<unsigned, 2>;
using MultiArray2F = MultiArray<float, 2>;
using MultiArray2D = MultiArray<double, 2>;

using MultiArray3I = MultiArray<int, 3>;
using MultiArray3S = MultiArray<std::size_t, 3>;
using MultiArray3U = MultiArray<unsigned, 3>;
using MultiArray3F = MultiArray<float, 3>;
using MultiArray3D = MultiArray<double, 3>;

using MultiArray4I = MultiArray<int, 4>;
using MultiArray4S = MultiArray<std::size_t, 4>;
using MultiArray4U = MultiArray<unsigned, 4>;
using MultiArray4F = MultiArray<float, 4>;
using MultiArray4D = MultiArray<double, 4>;

template <typename Element, std::size_t Rank>
  requires std::same_as<optional_diagnostic_result<Element>, void>
void write_text(TextBuffer& output, MultiArray<Element, Rank> const& array);

template <typename Element, std::size_t Rank>
MultiArray<Element, Rank>::MultiArray() {
  m_shape = SizeArray::filled(0);
}

template <typename Element, std::size_t Rank>
MultiArray<Element, Rank>::MultiArray(SizeArray const& shape) {
  setSize(shape);
}

template <typename Element, std::size_t Rank>
MultiArray<Element, Rank>::MultiArray(SizeArray const& shape, Element const& c) {
  setSize(shape, c);
}

template <typename Element, std::size_t Rank>
template <typename... T>
MultiArray<Element, Rank>::MultiArray(std::size_t i, T... rest) {
  setSize(SizeArray{i, rest...});
}

template <typename Element, std::size_t Rank>
typename MultiArray<Element, Rank>::SizeArray const& MultiArray<Element, Rank>::size() const {
  return m_shape;
}

template <typename Element, std::size_t Rank>
std::size_t MultiArray<Element, Rank>::size(std::size_t dimension) const {
  return m_shape[dimension];
}

template <typename Element, std::size_t Rank>
void MultiArray<Element, Rank>::clear() {
  setSize(SizeArray::filled(0));
}

template <typename Element, std::size_t Rank>
void MultiArray<Element, Rank>::resize(SizeArray const& shape) {
  if (m_data.empty()) {
    setSize(shape);
    return;
  }

  bool equal = true;
  for (std::size_t i = 0; i < Rank; ++i)
    equal = equal && (m_shape[i] == shape[i]);

  if (equal)
    return;

  MultiArray newArray(shape);
  newArray.copy(*this);
  std::swap(*this, newArray);
}

template <typename Element, std::size_t Rank>
void MultiArray<Element, Rank>::resize(SizeArray const& shape, Element const& c) {
  if (m_data.empty()) {
    setSize(shape, c);
    return;
  }

  bool equal = true;
  for (std::size_t i = 0; i < Rank; ++i)
    equal = equal && (m_shape[i] == shape[i]);

  if (equal)
    return;

  MultiArray newArray(shape, c);
  newArray.copy(*this);
  *this = std::move(newArray);
}

template <typename Element, std::size_t Rank>
template <typename... T>
void MultiArray<Element, Rank>::resize(std::size_t i, T... rest) {
  resize(SizeArray{i, rest...});
}

template <typename Element, std::size_t Rank>
void MultiArray<Element, Rank>::fill(Element const& element) {
  std::fill(m_data.begin(), m_data.end(), element);
}

template <typename Element, std::size_t Rank>
void MultiArray<Element, Rank>::setSize(SizeArray const& shape) {
  std::size_t storageSize = 1;
  for (std::size_t i = 0; i < Rank; ++i) {
    m_shape[i] = shape[i];
    storageSize *= shape[i];
  }

  m_data.resize(storageSize);
}

template <typename Element, std::size_t Rank>
void MultiArray<Element, Rank>::setSize(SizeArray const& shape, Element const& c) {
  std::size_t storageSize = 1;
  for (std::size_t i = 0; i < Rank; ++i) {
    m_shape[i] = shape[i];
    storageSize *= shape[i];
  }
  m_data.resize(storageSize, c);
}

template <typename Element, std::size_t Rank>
template <typename... T>
void MultiArray<Element, Rank>::setSize(std::size_t i, T... rest) {
  setSize({i, rest...});
}

template <typename Element, std::size_t Rank>
Element& MultiArray<Element, Rank>::operator()(IndexArray const& index) {
  return m_data[storageIndex(index)];
}

template <typename Element, std::size_t Rank>
Element const& MultiArray<Element, Rank>::operator()(IndexArray const& index) const {
  return m_data[storageIndex(index)];
}

template <typename Element, std::size_t Rank>
template <typename... T>
Element& MultiArray<Element, Rank>::operator()(std::size_t i1, T... rest) {
  return m_data[storageIndex(IndexArray(i1, rest...))];
}

template <typename Element, std::size_t Rank>
template <typename... T>
Element const& MultiArray<Element, Rank>::operator()(std::size_t i1, T... rest) const {
  return m_data[storageIndex(IndexArray(i1, rest...))];
}

template <typename Element, std::size_t Rank>
std::expected<std::reference_wrapper<Element const>, StarException> MultiArray<Element, Rank>::at(IndexArray const& index) const {
  for (std::size_t i = Rank; i != 0; --i) {
    if (index[i - 1] >= m_shape[i - 1])
      return std::unexpected(MultiArrayException(Star::format("Out of bounds on MultiArray::at({})", index)));
  }

  return std::reference_wrapper<Element const>(m_data[storageIndex(index)]);
}

template <typename Element, std::size_t Rank>
std::expected<std::reference_wrapper<Element>, StarException> MultiArray<Element, Rank>::at(IndexArray const& index) {
  for (std::size_t i = Rank; i != 0; --i) {
    if (index[i - 1] >= m_shape[i - 1])
      return std::unexpected(MultiArrayException(Star::format("Out of bounds on MultiArray::at({})", index)));
  }

  return std::reference_wrapper<Element>(m_data[storageIndex(index)]);
}

template <typename Element, std::size_t Rank>
template <typename... T>
std::expected<std::reference_wrapper<Element>, StarException> MultiArray<Element, Rank>::at(std::size_t i1, T... rest) {
  return at(IndexArray(i1, rest...));
}

template <typename Element, std::size_t Rank>
template <typename... T>
std::expected<std::reference_wrapper<Element const>, StarException> MultiArray<Element, Rank>::at(std::size_t i1, T... rest) const {
  return at(IndexArray(i1, rest...));
}

template <typename Element, std::size_t Rank>
std::expected<void, StarException> MultiArray<Element, Rank>::set(IndexArray const& index, Element element) {
  for (std::size_t i = Rank; i != 0; --i) {
    if (index[i - 1] >= m_shape[i - 1])
      return std::unexpected(MultiArrayException(Star::format("Out of bounds on MultiArray::set({})", index)));
  }

  m_data[storageIndex(index)] = std::move(element);
  return {};
}

template <typename Element, std::size_t Rank>
Element MultiArray<Element, Rank>::get(IndexArray const& index, Element def) {
  for (std::size_t i = Rank; i != 0; --i) {
    if (index[i - 1] >= m_shape[i - 1])
      return std::move(def);
  }

  return m_data[storageIndex(index)];
}

template <typename Element, std::size_t Rank>
void MultiArray<Element, Rank>::setResize(IndexArray const& index, Element element) {
  SizeArray newShape;
  for (std::size_t i = 0; i < Rank; ++i)
    newShape[i] = std::max(m_shape[i], index[i] + 1);
  resize(newShape);

  m_data[storageIndex(index)] = std::move(element);
}

template <typename Element, std::size_t Rank>
void MultiArray<Element, Rank>::copy(MultiArray const& source) {
  IndexArray max;
  for (std::size_t i = 0; i < Rank; ++i)
    max[i] = std::min(size(i), source.size(i));

  copy(source, IndexArray::filled(0), max, IndexArray::filled(0));
}

template <typename Element, std::size_t Rank>
void MultiArray<Element, Rank>::copy(MultiArray const& source, IndexArray const& sourceMin, IndexArray const& sourceMax, IndexArray const& targetMin) {
  IndexArray sourceIndex;
  IndexArray targetIndex;
  subCopy(source, sourceMin, sourceMax, targetMin, sourceIndex, targetIndex, 0);
}

template <typename Element, std::size_t Rank>
template <typename OpType>
void MultiArray<Element, Rank>::forEach(IndexArray const& min, SizeArray const& size, OpType&& op) {
  IndexArray index;
  subForEach(min, size, std::forward<OpType>(op), index, 0, 0);
}

template <typename Element, std::size_t Rank>
template <typename OpType>
void MultiArray<Element, Rank>::forEach(IndexArray const& min, SizeArray const& size, OpType&& op) const {
  IndexArray index;
  subForEach(min, size, std::forward<OpType>(op), index, 0, 0);
}

template <typename Element, std::size_t Rank>
template <typename OpType>
void MultiArray<Element, Rank>::forEach(OpType&& op) {
  forEach(IndexArray::filled(0), size(), std::forward<OpType>(op));
}

template <typename Element, std::size_t Rank>
template <typename OpType>
void MultiArray<Element, Rank>::forEach(OpType&& op) const {
  forEach(IndexArray::filled(0), size(), std::forward<OpType>(op));
}

template <typename Element, std::size_t Rank>
void MultiArray<Element, Rank>::print(TextBuffer& output) const
    requires std::same_as<optional_diagnostic_result<Element>, void> {
  sub_print(output, IndexArray(), 0);
}

template <typename Element, std::size_t Rank>
std::size_t MultiArray<Element, Rank>::count() const {
  return m_data.size();
}

template <typename Element, std::size_t Rank>
Element const& MultiArray<Element, Rank>::atIndex(std::size_t index) const {
  return m_data[index];
}

template <typename Element, std::size_t Rank>
Element& MultiArray<Element, Rank>::atIndex(std::size_t index) {
  return m_data[index];
}

template <typename Element, std::size_t Rank>
Element const* MultiArray<Element, Rank>::data() const {
  return m_data.data();
}

template <typename Element, std::size_t Rank> Element* MultiArray<Element, Rank>::data() { return m_data.data(); }

template <typename Element, std::size_t Rank>
typename MultiArray<Element, Rank>::View MultiArray<Element, Rank>::view() &
    requires std::same_as<typename Storage::reference, Element&> {
  return View(data(), Extents(std::span<std::size_t const, Rank>(m_shape)));
}

template <typename Element, std::size_t Rank>
typename MultiArray<Element, Rank>::ConstView MultiArray<Element, Rank>::view() const &
    requires std::same_as<typename Storage::reference, Element&> {
  return ConstView(data(), Extents(std::span<std::size_t const, Rank>(m_shape)));
}

template <typename Element, std::size_t Rank>
std::size_t MultiArray<Element, Rank>::storageIndex(IndexArray const& index) const {
  std::size_t loc = index[0];
  if constexpr (Star::DebugEnabled) {
    Star::star_assert_failed((index[0] < m_shape[0]));
  }
  for (std::size_t i = 1; i < Rank; ++i) {
    loc = loc * m_shape[i] + index[i];
    if constexpr (Star::DebugEnabled) {
      Star::star_assert_failed((index[i] < m_shape[i]));
    }
  }
  return loc;
}

template <typename Element, std::size_t Rank>
void MultiArray<Element, Rank>::sub_print(TextBuffer& output, IndexArray index, std::size_t dim) const
    requires std::same_as<optional_diagnostic_result<Element>, void> {
  if (dim == Rank - 1) {
    for (std::size_t i = 0; i < m_shape[dim]; ++i) {
      index[dim] = i;
      append_text(output, optional_diagnostic_value(m_data[storageIndex(index)]));
      output.push_back(' ');
    }
    output.push_back('\n');
  } else {
    for (std::size_t i = 0; i < m_shape[dim]; ++i) {
      index[dim] = i;
      sub_print(output, index, dim + 1);
    }
    output.push_back('\n');
  }
}

template <typename Element, std::size_t Rank>
template <typename OpType>
void MultiArray<Element, Rank>::subForEach(IndexArray const& min, SizeArray const& size, OpType&& op, IndexArray& index, std::size_t offset, std::size_t dim) {
  std::size_t minIndex = min[dim];
  std::size_t maxIndex = minIndex + size[dim];
  for (std::size_t i = minIndex; i < maxIndex; ++i) {
    index[dim] = i;
    if (dim == Rank - 1)
      op(index, m_data[offset + i]);
    else
      subForEach(min, size, std::forward<OpType>(op), index, (offset + i) * m_shape[dim + 1], dim + 1);
  }
}

template <typename Element, std::size_t Rank>
template <typename OpType>
void MultiArray<Element, Rank>::subForEach(IndexArray const& min, SizeArray const& size, OpType&& op, IndexArray& index, std::size_t offset, std::size_t dim) const {
  std::size_t minIndex = min[dim];
  std::size_t maxIndex = minIndex + size[dim];
  for (std::size_t i = minIndex; i < maxIndex; ++i) {
    index[dim] = i;
    if (dim == Rank - 1)
      op(index, m_data[offset + i]);
    else
      subForEach(min, size, std::forward<OpType>(op), index, (offset + i) * m_shape[dim + 1], dim + 1);
  }
}

template <typename Element, std::size_t Rank>
void MultiArray<Element, Rank>::subCopy(MultiArray const& source, IndexArray const& sourceMin, IndexArray const& sourceMax,
    IndexArray const& targetMin, IndexArray& sourceIndex, IndexArray& targetIndex, std::size_t dim) {
  std::size_t w = sourceMax[dim] - sourceMin[dim];
  if (dim < Rank - 1) {
    for (std::size_t i = 0; i < w; ++i) {
      sourceIndex[dim] = i + sourceMin[dim];
      targetIndex[dim] = i + targetMin[dim];
      subCopy(source, sourceMin, sourceMax, targetMin, sourceIndex, targetIndex, dim + 1);
    }
  } else {
    sourceIndex[dim] = sourceMin[dim];
    targetIndex[dim] = targetMin[dim];
    std::size_t sourceStorageStart = source.storageIndex(sourceIndex);
    std::size_t targetStorageStart = storageIndex(targetIndex);
    for (std::size_t i = 0; i < w; ++i)
      m_data[targetStorageStart + i] = source.m_data[sourceStorageStart + i];
  }
}

template <typename Element, std::size_t Rank>
  requires std::same_as<optional_diagnostic_result<Element>, void>
void write_text(TextBuffer& output, MultiArray<Element, Rank> const& array) {
  array.print(output);
}

}

export module star.multi_array;

export template <typename Element, std::size_t Rank>
  requires std::same_as<Star::optional_diagnostic_result<Element>, void>
struct std::formatter<Star::MultiArray<Element, Rank>> : Star::TextFormatter {};

export namespace Star {
  using ::Star::MultiArrayExceptionTag;
  using ::Star::MultiArrayException;
  using ::Star::MultiArray;
  using ::Star::MultiArray2I;
  using ::Star::MultiArray2S;
  using ::Star::MultiArray2U;
  using ::Star::MultiArray2F;
  using ::Star::MultiArray2D;
  using ::Star::MultiArray3I;
  using ::Star::MultiArray3S;
  using ::Star::MultiArray3U;
  using ::Star::MultiArray3F;
  using ::Star::MultiArray3D;
  using ::Star::MultiArray4I;
  using ::Star::MultiArray4S;
  using ::Star::MultiArray4U;
  using ::Star::MultiArray4F;
  using ::Star::MultiArray4D;
  using ::Star::write_text;
}
