module;
import std;
import star.format;
import star.interpolation;

import star.math_common;
import star.algorithm;
namespace Star {

// Describes a simple table from index to value, which operates on bins
// corresponding to ranges of indexes.  IndexType can be any ordered type,
// ValueType can be anything.
template <typename IndexType, typename ValueType = IndexType>
class ParametricTable {
public:
  using Index = IndexType;
  using Value = ValueType;

  ParametricTable();

  template <typename OtherIndexType, typename OtherValueType>
  explicit ParametricTable(ParametricTable<OtherIndexType, OtherValueType> const& parametricFunction);

  // Construct a ParametricTable with a list of point pairs, which does not
  // have to be sorted (it will be sorted internally). Duplicate indexes are
  // returned as a diagnostic without publishing a partial table.
  template <typename PairContainer>
  [[nodiscard]] static std::expected<ParametricTable, StarException> create(PairContainer indexValuePairs);

  // addPoint does not need to be called in order, it will insert the point in
  // the correct ordered position for the given index, and return the position.
  std::size_t addPoint(IndexType index, ValueType value);
  void clearPoints();

  std::size_t size() const;
  bool empty() const;

  [[nodiscard]] std::expected<std::reference_wrapper<IndexType const>, StarException> index(std::size_t i) const;
  [[nodiscard]] std::expected<std::reference_wrapper<ValueType const>, StarException> value(std::size_t i) const;

  // Returns true if the values of the table are also valid indexes (true when
  // the data points are monotonic increasing)
  bool isInvertible() const;

  // Invert the table, switching indexes and values. Will not generally compile
  // unless Index and Value are the same type.
  [[nodiscard]] std::expected<void, StarException> invert();

  // Find the value to the left of the given index.  If the index is lower than
  // the lowest index point, returns the first value.
  [[nodiscard]] std::expected<std::reference_wrapper<ValueType const>, StarException> get(IndexType index) const;

protected:
  using IndexList = std::vector<IndexType>;
  using ValueList = std::vector<ValueType>;

  IndexList const& indexes() const;
  ValueList const& values() const;

private:
  template <typename, typename> friend class ParametricTable;
  IndexList m_indexes;
  ValueList m_values;
};

// Extension of ParametricTable that simplifies of all the complex
// interpolation code for interpolating an ordered list of points.  Useful for
// describing a simple 2d or n-dimensional (using VectorN for value type) curve
// of one variable.  IndexType should generally be float or double, and
// ValueType can be any type that can be interpolated.
template <typename IndexType, typename ValueType = IndexType>
class ParametricFunction : public ParametricTable<IndexType, ValueType> {
public:
  using Base = ParametricTable<IndexType, ValueType>;

  ParametricFunction();
  ParametricFunction(
      InterpolationMode interpolationMode, BoundMode boundMode = BoundMode::Clamp);

  template <typename OtherIndexType, typename OtherValueType>
  explicit ParametricFunction(ParametricFunction<OtherIndexType, OtherValueType> const& parametricFunction);

  template <typename PairContainer>
  [[nodiscard]] static std::expected<ParametricFunction, StarException> create(PairContainer indexValuePairs,
      InterpolationMode interpolationMode = InterpolationMode::Linear,
      BoundMode boundMode = BoundMode::Clamp);

  InterpolationMode interpolationMode() const;
  void setInterpolationMode(InterpolationMode interpolationType);

  BoundMode boundMode() const;
  void setBoundMode(BoundMode boundMode);

  // Interpolates a value at the given index according to the interpolation and
  // bound mode.
  [[nodiscard]] std::expected<ValueType, StarException> interpolate(IndexType index) const;

  // Synonym for interpolate
  [[nodiscard]] std::expected<ValueType, StarException> operator()(IndexType index) const;

private:
  InterpolationMode m_interpolationMode;
  BoundMode m_boundMode;
};

template <typename IndexType, typename ValueType> ParametricTable<IndexType, ValueType>::ParametricTable() = default;

template <typename IndexType, typename ValueType>
template <typename OtherIndexType, typename OtherValueType>
ParametricTable<IndexType, ValueType>::ParametricTable(
    ParametricTable<OtherIndexType, OtherValueType> const& parametricTable) {
  for (std::size_t i = 0; i < parametricTable.size(); ++i) {
    m_indexes.push_back(parametricTable.m_indexes[i]);
    m_values.push_back(parametricTable.m_values[i]);
  }
}

template <typename IndexType, typename ValueType>
template <typename PairContainer>
std::expected<ParametricTable<IndexType, ValueType>, StarException> ParametricTable<IndexType, ValueType>::create(PairContainer indexValuePairs) {
  ParametricTable table;
  if (indexValuePairs.empty())
    return table;

  std::sort(indexValuePairs.begin(), indexValuePairs.end(),
      [](typename PairContainer::value_type const& a, typename PairContainer::value_type const& b) {
        return std::get<0>(a) < std::get<0>(b);
      });

  for (auto const& pair : indexValuePairs) {
    table.m_indexes.push_back(std::move(std::get<0>(pair)));
    table.m_values.push_back(std::move(std::get<1>(pair)));
  }

  for (std::size_t i = 0; i < table.size() - 1; ++i) {
    if (table.m_indexes[i] == table.m_indexes[i + 1])
      return std::unexpected(MathException("Degenerate index values given in ParametricTable constructor"));
  }
  return table;
}

template <typename IndexType, typename ValueType>
std::size_t ParametricTable<IndexType, ValueType>::addPoint(IndexType index, ValueType value) {
  std::size_t insertLocation = std::distance(m_indexes.begin(), std::upper_bound(m_indexes.begin(), m_indexes.end(), index));
  m_indexes.insert(m_indexes.begin() + insertLocation, std::move(index));
  m_values.insert(m_values.begin() + insertLocation, std::move(value));
  return insertLocation;
}

template <typename IndexType, typename ValueType>
void ParametricTable<IndexType, ValueType>::clearPoints() {
  m_indexes.clear();
  m_values.clear();
}

template <typename IndexType, typename ValueType>
std::size_t ParametricTable<IndexType, ValueType>::size() const {
  return m_indexes.size();
}

template <typename IndexType, typename ValueType>
bool ParametricTable<IndexType, ValueType>::empty() const {
  return size() == 0;
}

template <typename IndexType, typename ValueType>
std::expected<std::reference_wrapper<IndexType const>, StarException> ParametricTable<IndexType, ValueType>::index(std::size_t i) const {
  if (i >= size())
    return std::unexpected(MathException("Index out of bounds in ParametricTable::index"));
  return std::reference_wrapper<IndexType const>(m_indexes[i]);
}

template <typename IndexType, typename ValueType>
std::expected<std::reference_wrapper<ValueType const>, StarException> ParametricTable<IndexType, ValueType>::value(std::size_t i) const {
  if (i >= size())
    return std::unexpected(MathException("Index out of bounds in ParametricTable::value"));
  return std::reference_wrapper<ValueType const>(m_values[i]);
}

template <typename IndexType, typename ValueType>
bool ParametricTable<IndexType, ValueType>::isInvertible() const {
  if (empty())
    return true;

  for (std::size_t i = 0; i < size() - 1; ++i) {
    if (m_values[i] > m_values[i + 1])
      return false;
  }

  return true;
}

template <typename IndexType, typename ValueType>
std::expected<void, StarException> ParametricTable<IndexType, ValueType>::invert() {
  if (isInvertible())
    return std::unexpected(MathException("invert() called on non-invertible ParametricTable"));

  for (std::size_t i = 0; i < size(); ++i)
    std::swap(m_indexes[i], m_values[i]);
  return {};
}

template <typename IndexType, typename ValueType>
std::expected<std::reference_wrapper<ValueType const>, StarException> ParametricTable<IndexType, ValueType>::get(IndexType index) const {
  if (empty())
    return std::unexpected(MathException("get called on empty ParametricTable"));

  auto i = std::lower_bound(m_indexes.begin(), m_indexes.end(), index);
  if (i != m_indexes.begin())
    --i;

  return std::reference_wrapper<ValueType const>(m_values[std::distance(m_indexes.begin(), i)]);
}

template <typename IndexType, typename ValueType>
auto ParametricTable<IndexType, ValueType>::indexes() const -> IndexList const & {
  return m_indexes;
}

template <typename IndexType, typename ValueType>
auto ParametricTable<IndexType, ValueType>::values() const -> ValueList const & {
  return m_values;
}

template <typename IndexType, typename ValueType>
ParametricFunction<IndexType, ValueType>::ParametricFunction()
  : ParametricFunction(InterpolationMode::Linear, BoundMode::Clamp) {}

template <typename IndexType, typename ValueType>
ParametricFunction<IndexType, ValueType>::ParametricFunction(InterpolationMode interpolationMode, BoundMode boundMode)
  : m_interpolationMode(interpolationMode), m_boundMode(boundMode) {}

template <typename IndexType, typename ValueType>
template <typename OtherIndexType, typename OtherValueType>
ParametricFunction<IndexType, ValueType>::ParametricFunction(
    ParametricFunction<OtherIndexType, OtherValueType> const& parametricFunction)
  : Base(parametricFunction) {
  m_interpolationMode = parametricFunction.interpolationMode();
  m_boundMode = parametricFunction.boundMode();
}

template <typename IndexType, typename ValueType>
template <typename PairContainer>
std::expected<ParametricFunction<IndexType, ValueType>, StarException> ParametricFunction<IndexType, ValueType>::create(
    PairContainer indexValuePairs, InterpolationMode interpolationMode, BoundMode boundMode) {
  auto base = Base::create(std::move(indexValuePairs));
  if (!base)
    return std::unexpected(std::move(base.error()));
  ParametricFunction function(interpolationMode, boundMode);
  static_cast<Base&>(function) = std::move(*base);
  return function;
}

template <typename IndexType, typename ValueType>
InterpolationMode ParametricFunction<IndexType, ValueType>::interpolationMode() const {
  return m_interpolationMode;
}

template <typename IndexType, typename ValueType>
void ParametricFunction<IndexType, ValueType>::setInterpolationMode(InterpolationMode interpolationType) {
  m_interpolationMode = interpolationType;
}

template <typename IndexType, typename ValueType>
BoundMode ParametricFunction<IndexType, ValueType>::boundMode() const {
  return m_boundMode;
}

template <typename IndexType, typename ValueType>
void ParametricFunction<IndexType, ValueType>::setBoundMode(BoundMode boundMode) {
  m_boundMode = boundMode;
}

template <typename IndexType, typename ValueType>
std::expected<ValueType, StarException> ParametricFunction<IndexType, ValueType>::interpolate(IndexType index) const {
  if (Base::empty())
    return ValueType();

  if (m_interpolationMode == InterpolationMode::HalfStep) {
    return parametricInterpolate2(Base::indexes(), Base::values(), index, StepWeightOperator<IndexType>(), m_boundMode);

  } else if (m_interpolationMode == InterpolationMode::Linear) {
    return parametricInterpolate2(
        Base::indexes(), Base::values(), index, LinearWeightOperator<IndexType>(), m_boundMode);

  } else if (m_interpolationMode == InterpolationMode::Cubic) {
    // ParametricFunction uses CubicWeights with linear extrapolation (not
    // configurable atm)
    return parametricInterpolate4(
        Base::indexes(), Base::values(), index, Cubic4WeightOperator<IndexType>(true), m_boundMode);

  } else {
    return std::unexpected(MathException("Unsupported interpolation type in ParametricFunction::interpolate"));
  }
}

template <typename IndexType, typename ValueType>
std::expected<ValueType, StarException> ParametricFunction<IndexType, ValueType>::operator()(IndexType index) const {
  return interpolate(index);
}

}

export module star.parametric_function;

export namespace Star {
  using ::Star::ParametricTable;
  using ::Star::ParametricFunction;
}
