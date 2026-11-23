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

template <typename Op, typename Point, typename Reference>
concept multi_table_evaluator = requires(Op& op, Point& point, Reference element) {
  element = std::invoke(op, point);
};

template <typename Range>
concept multi_table_ordered_range = requires(typename Range::value_type const& left, typename Range::value_type const& right) {
  (left < right) ? true : false;
};

// Provides a method for storing, retrieving, and interpolating uneven
// n-variate data.  Access times involve a binary search over the domain of
// each dimension, so is O(log(n)*m) where n is the size of the largest
// dimension, and m is the table_rank.
template <typename ElementT, typename PositionT, std::size_t table_rank>
  requires(table_rank > 0)
class MultiTable {
public:
  using Element = ElementT;
  using Position = PositionT;
  static constexpr std::size_t rank = table_rank;

  using MultiArray = Star::MultiArray<ElementT, table_rank>;

  using Interpolator2 = Star::MultiArrayInterpolator2<MultiArray, Position>;
  using Interpolator4 = Star::MultiArrayInterpolator4<MultiArray, Position>;
  using PiecewiseInterpolator = Star::MultiArrayPiecewiseInterpolator<MultiArray, Position>;

  using PositionArray = Array<Position, rank>;
  using SizeArray = typename MultiArray::SizeArray;
  using IndexArray = typename MultiArray::IndexArray;
  using Range = List<Position>;
  using RangeArray = Array<Range, rank>;

  MultiTable() = default;
  MultiTable(MultiTable const&) = default;
  MultiTable& operator=(MultiTable const&) = default;

  MultiTable(MultiTable&&) = default;

  MultiTable& operator=(MultiTable&& other)
      noexcept(std::is_nothrow_move_assignable_v<RangeArray> && std::is_nothrow_move_assignable_v<MultiArray>) {
    if (this != std::addressof(other)) {
      m_ranges = std::move(other.m_ranges);
      m_array = std::move(other.m_array);
      m_interpolation_mode = other.m_interpolation_mode;
      m_bound_mode = other.m_bound_mode;
    }
    return *this;
  }

  // Axes are nondecreasing: duplicates and empty ranges are supported.
  // Validate before changing either the axes or the coordinate-preserving grid.
  [[nodiscard]] std::expected<void, StarException> set_range(std::size_t dim, Range range)
    requires (multi_table_ordered_range<Range>
        && requires(MultiArray& array, SizeArray const& shape, Range& target, Range&& source) {
          array.resize(shape);
          target = std::move(source);
        })
  {
    if (dim >= rank)
      return std::unexpected(MathException("MultiTable range dimension is out of bounds"));
    if (!std::is_sorted(range.begin(), range.end()))
      return std::unexpected(MathException("MultiTable ranges must be nondecreasing"));
    SizeArray sizes = m_array.extents();
    sizes[dim] = range.size();
    auto resized = m_array.resize(sizes);
    if (!resized)
      return std::unexpected(std::move(resized.error()));
    m_ranges[dim] = std::move(range);
    return {};
  }

  [[nodiscard]] std::expected<void, StarException> set_ranges(RangeArray ranges)
    requires (multi_table_ordered_range<Range>
        && requires(MultiArray& array, SizeArray const& shape, RangeArray& target, RangeArray&& source) {
          array.resize(shape);
          target = std::move(source);
        })
  {
    SizeArray sizes;
    for (std::size_t dim = 0; dim < rank; ++dim) {
      if (!std::is_sorted(ranges[dim].begin(), ranges[dim].end()))
        return std::unexpected(MathException("MultiTable ranges must be nondecreasing"));
      sizes[dim] = ranges[dim].size();
    }
    auto resized = m_array.resize(sizes);
    if (!resized)
      return std::unexpected(std::move(resized.error()));
    m_ranges = std::move(ranges);
    return {};
  }

  // Adopt an already populated grid without allocating another one.
  [[nodiscard]] std::expected<void, StarException> assign(RangeArray ranges, MultiArray data)
    requires (multi_table_ordered_range<Range>
        && requires(RangeArray& target_ranges, RangeArray&& source_ranges, MultiArray& target_data, MultiArray&& source_data) {
          target_ranges = std::move(source_ranges);
          target_data = std::move(source_data);
        })
  {
    for (std::size_t dim = 0; dim < rank; ++dim) {
      if (!std::is_sorted(ranges[dim].begin(), ranges[dim].end()))
        return std::unexpected(MathException("MultiTable ranges must be nondecreasing"));
      if (ranges[dim].size() != data.extent(dim))
        return std::unexpected(MathException("MultiTable ranges do not match the data extents"));
    }
    m_ranges = std::move(ranges);
    m_array = std::move(data);
    return {};
  }

  void clear() {
    for (auto& range : m_ranges)
      range.clear();
    m_array.clear();
  }

  [[nodiscard]] std::expected<void, StarException> set(IndexArray const& index, Element element)
    requires requires(MultiArray& array, Element&& value) { array.set(index, std::move(value)); }
  {
    return m_array.set(index, std::move(element));
  }

  typename MultiArray::Storage::reference operator[](IndexArray const& index) {
    return m_array(index);
  }

  typename MultiArray::Storage::const_reference operator[](IndexArray const& index) const {
    return m_array(index);
  }

  [[nodiscard]] auto at(IndexArray const& index) &
    requires requires(MultiArray& array) { array.at(index); }
  {
    return m_array.at(index);
  }

  [[nodiscard]] auto at(IndexArray const& index) const &
    requires requires(MultiArray const& array) { array.at(index); }
  {
    return m_array.at(index);
  }

  void at(IndexArray const&) && = delete;
  void at(IndexArray const&) const && = delete;

  MultiArray const& array() const & {
    return m_array;
  }
  MultiArray const& array() const && = delete;

  auto view() & requires requires(MultiArray& array) { array.view(); } {
    return m_array.view();
  }
  auto view() const & requires requires(MultiArray const& array) { array.view(); } {
    return m_array.view();
  }
  void view() && = delete;
  void view() const && = delete;

  auto elements() & requires requires(MultiArray& array) { array.elements(); } {
    return m_array.elements();
  }
  auto elements() const & requires requires(MultiArray const& array) { array.elements(); } {
    return m_array.elements();
  }
  void elements() && = delete;
  void elements() const && = delete;

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
    for (std::size_t dim = 0; dim < rank; ++dim) {
      if (m_ranges[dim].empty() || m_array.extent(dim) == 0)
        return std::unexpected(MathException("Cannot interpolate a MultiTable with an empty axis"));
    }

    if (m_interpolation_mode == InterpolationMode::HalfStep) {
      PiecewiseInterpolator piecewise_interpolator(StepWeightOperator<Position>(), m_bound_mode);
      return piecewise_interpolator.interpolate(m_array, to_index_space(coord));

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
    requires (multi_table_evaluator<OpType, PositionArray, typename MultiArray::Storage::reference>
        && std::is_default_constructible_v<PositionArray>
        && requires(Position& target, typename Range::reference source) { target = source; })
  void eval(OpType&& op) {
    m_array.for_each([&](IndexArray const& index, auto&& element) {
      PositionArray point;
      for (std::size_t dim = 0; dim < rank; ++dim)
        point[dim] = m_ranges[dim][index[dim]];
      element = std::invoke(op, point);
    });
  }

private:
  PositionArray to_index_space(PositionArray const& coord) const {
    PositionArray index_coord;
    for (std::size_t i = 0; i < rank; ++i)
      index_coord[i] = inverse_linear_interpolate_lower(m_ranges[i].begin(), m_ranges[i].end(), coord[i]);
    return index_coord;
  }


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
  using ::Star::multi_table_evaluator;
  using ::Star::multi_table_ordered_range;
  using ::Star::MultiTable;
  using ::Star::MultiTable2F;
  using ::Star::MultiTable2D;
  using ::Star::MultiTable3F;
  using ::Star::MultiTable3D;
  using ::Star::MultiTable4F;
  using ::Star::MultiTable4D;
}
