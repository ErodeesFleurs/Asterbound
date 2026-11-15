module;
import std;
import star.format;
import star.list;

import star.multi_array;
import star.interpolation;
import star.multi_array_interpolator;

import star.array;
import star.math_common;
namespace Star {

// Provides a method for storing, retrieving, and interpolating uneven
// n-variate data.  Access times involve a binary search over the domain of
// each dimension, so is O(log(n)*m) where n is the size of the largest
// dimension, and m is the table_rank.
template <typename ElementT, typename PositionT, std::size_t RankN>
  requires(RankN > 0)
class MultiTable {
public:
  using Element = ElementT;
  using Position = PositionT;
  static std::size_t const Rank = RankN;

  using MultiArray = Star::MultiArray<ElementT, RankN>;

  using Interpolator2 = Star::MultiArrayInterpolator2<MultiArray, Position>;
  using Interpolator4 = Star::MultiArrayInterpolator4<MultiArray, Position>;
  using PiecewiseInterpolator = Star::MultiArrayPiecewiseInterpolator<MultiArray, Position>;

  using PositionArray = Array<Position, Rank>;
  using SizeArray = typename MultiArray::SizeArray;
  using IndexArray = typename MultiArray::IndexArray;
  using Range = List<Position>;
  using RangeArray = Array<Range, Rank>;

  MultiTable() = default;

  // Set input ranges on a particular dimension.  Will resize underlying storage
  // to fit range. Ranges must be sorted in ascending order.
  void set_range(std::size_t dim, Range const& range) {
    if constexpr (Star::DebugEnabled) {
      Star::star_assert_failed((std::is_sorted(range.begin(), range.end())));
    }
    SizeArray sizes = m_array.size();
    sizes[dim] = range.size();
    m_array.resize(sizes);

    m_ranges[dim] = range;
  }

  void set_ranges(RangeArray const& ranges) {
    SizeArray arraySize;

    for (std::size_t dim = 0; dim < Rank; ++dim) {
      if constexpr (Star::DebugEnabled) {
        Star::star_assert_failed((std::is_sorted(ranges[dim].begin(), ranges[dim].end())));
      }
      arraySize[dim] = ranges[dim].size();
      m_ranges[dim] = ranges[dim];
    }

    m_array.resize(arraySize);
  }

  // Set array element based on index.
  [[nodiscard]] std::expected<void, StarException> set(IndexArray const& index, Element const& element) {
    return m_array.set(index, element);
  }

  // Get array element based on index.
  Element const& get(IndexArray const& index) const {
    return m_array(index);
  }

  MultiArray const& array() const {
    return m_array;
  }

  MultiArray& array() {
    return m_array;
  }

  InterpolationMode interpolation_mode() const { return m_interpolation_mode; }

  void set_interpolation_mode(InterpolationMode interpolation_mode) { m_interpolation_mode = interpolation_mode; }

  BoundMode bound_mode() const { return m_bound_mode; }

  void set_bound_mode(BoundMode bound_mode) { m_bound_mode = bound_mode; }

  [[nodiscard]] std::expected<Element, StarException> interpolate(PositionArray const& coord) const
    requires requires(Interpolator2 const& linear, Interpolator4 const& cubic, PiecewiseInterpolator const& piecewise,
                      MultiArray const& array) {
      linear.interpolate(array, coord);
      cubic.interpolate(array, coord);
      piecewise.interpolate(array, coord);
    }
  {
    for (std::size_t dim = 0; dim < Rank; ++dim) {
      if (m_ranges[dim].empty() || m_array.size(dim) == 0)
        return std::unexpected(MathException("Cannot interpolate a MultiTable with an empty axis"));
    }

    if (m_interpolation_mode == InterpolationMode::HalfStep) {
      PiecewiseInterpolator piecewiseInterpolator(StepWeightOperator<Position>(), m_bound_mode);
      return piecewiseInterpolator.interpolate(m_array, to_index_space(coord));

    } else if (m_interpolation_mode == InterpolationMode::Linear) {
      Interpolator2 interpolator2(LinearWeightOperator<Position>(), m_bound_mode);
      return interpolator2.interpolate(m_array, to_index_space(coord));

    } else if (m_interpolation_mode == InterpolationMode::Cubic) {
      // MultiTable uses cubic weights with linear extrapolation (not
      // configurable atm)
      Interpolator4 interpolator4(Cubic4WeightOperator<Position>(true), m_bound_mode);
      return interpolator4.interpolate(m_array, to_index_space(coord));

    } else {
      return std::unexpected(MathException("Unsupported interpolation type in MultiTable::interpolate"));
    }
  }

  // Synonym for interpolate
  [[nodiscard]] std::expected<Element, StarException> operator()(PositionArray const& coord) const
    requires requires { interpolate(coord); }
  {
    return interpolate(coord);
  }

  // op should take a PositionArray parameter and return an element.
  template <typename OpType>
    requires requires(OpType& op, PositionArray& point, Element& element) { element = std::invoke(op, point); }
  void eval(OpType&& op) {
    m_array.forEach(EvalWrapper<OpType>(op, *this));
  }

private:
  PositionArray to_index_space(PositionArray const& coord) const {
    PositionArray indexCoord;
    for (std::size_t i = 0; i < Rank; ++i)
      indexCoord[i] = inverse_linear_interpolate_lower(m_ranges[i].begin(), m_ranges[i].end(), coord[i]);
    return indexCoord;
  }

  template <typename OpType>
  struct EvalWrapper {
    EvalWrapper(OpType& o, MultiTable const& t)
      : op(o), table(t) {}

    void operator()(IndexArray const& indexArray, Element& element) {
      PositionArray rangeArray;
      for (std::size_t i = 0; i < Rank; ++i)
        rangeArray[i] = table.m_ranges[i][indexArray[i]];

      element = std::invoke(op, rangeArray);
    }

    OpType& op;
    MultiTable const& table;
  };

  RangeArray m_ranges;
  MultiArray m_array;
  InterpolationMode m_interpolation_mode = InterpolationMode::Linear;
  BoundMode m_bound_mode = BoundMode::Clamp;
};

using MultiTable2F = MultiTable<float, float, 2>;
using MultiTable2D = MultiTable<double, double, 2>;

using MultiTable3F = MultiTable<float, float, 3>;
using MultiTable3D = MultiTable<double, double, 3>;

using MultiTable4F = MultiTable<float, float, 4>;
using MultiTable4D = MultiTable<double, double, 4>;
}

export module star.multi_table;

export namespace Star {
  using ::Star::MultiTable;
  using ::Star::MultiTable2F;
  using ::Star::MultiTable2D;
  using ::Star::MultiTable3F;
  using ::Star::MultiTable3D;
  using ::Star::MultiTable4F;
  using ::Star::MultiTable4D;
}
