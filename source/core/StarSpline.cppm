module;

import std;
import star.interpolation;
import star.math_common;
import star.vector;

namespace Star {
namespace SplineDetail {
  // Classify stored inputs by reference, before a fast-math float-by-value
  // boundary could attach nofpclass(nan inf). Interior samples are unchecked.
  template <finite_classifiable T>
  T unit_parameter(T const& t) {
    return is_finite(t) ? std::clamp(t, T(0), T(1)) : T(0);
  }

  template <typename T> void implicit_result(T) noexcept;

  template <typename P, typename T>
  concept interpolable = requires(P const& a, P const& b, T t) { Star::lerp(t, a, b); };

  template <typename P, typename T>
  concept reducible = requires(P& a, P const& b, T t) { a = Star::lerp(t, a, b); };

  template <typename P, typename T, std::size_t degree>
  concept evaluable = std::is_constructible_v<P, P const&> && (degree == 0 || interpolable<P, T>)
      && (degree <= 2 || reducible<P, T>);

  template <typename P, typename T, std::size_t degree>
  concept differentiable =
      (degree == 0 && requires(P const& p) { implicit_result<P>(p - p); })
      || (degree > 0 && requires(P const& a, P const& b, T scale) { implicit_result<P>((b - a) * scale); }
          && (degree == 1 || interpolable<P, T>) && (degree <= 2 || reducible<P, T>));

  template <typename P, typename T, std::size_t degree>
  concept measurable = degree == 0 || (evaluable<P, T, degree> && requires(P& p, P const& a, P const& b) {
    p = a;
    implicit_result<T>((b - a).magnitude());
  });

  // 'work' already holds the first row we need to reduce. Never copy a
  // completed row; the right neighbor has not yet been overwritten.
  template <typename P, finite_classifiable T, std::size_t control_count>
    requires (control_count >= 2 && interpolable<P, T> && (control_count == 2 || reducible<P, T>))
  P reduce(std::array<P, control_count>& work, T t) {
    if constexpr (control_count > 2) {
      for (std::size_t width = control_count; width > 2; --width)
        for (std::size_t i = 0; i + 1 < width; ++i)
          work[i] = Star::lerp(t, work[i], work[i + 1]);
    }
    return Star::lerp(t, work[0], work[1]);
  }
}

// Uniform parameter samples; only their cumulative chord lengths need storing.
// A value snapshot never borrows the spline and cannot be invalidated by edits.
template <finite_classifiable T, std::size_t subdivisions>
  requires (subdivisions > 0 && subdivisions < std::numeric_limits<std::size_t>::max())
class SplineArcLengthTable {
public:
  T total_length() const { return cumulative.back(); }

  T parameter_at_length_fraction(T const& fraction) const {
    T u = SplineDetail::unit_parameter(fraction);
    if (u == 0 || u == 1)
      return u;
    if (total_length() == 0)
      return 0;
    T target = total_length() * u;
    return parameter_in_segment(target, upper_sample(target));
  }

  // The cursor is an upper sample index, not an iterator into this object.
  // Increasing queries scan each sample at most once. A stale or backwards
  // cursor is safe as well: recover the first sample reaching the target.
  T parameter_at_length_fraction(T const& fraction, std::size_t& cursor) const {
    T u = SplineDetail::unit_parameter(fraction);
    if (u == 0 || u == 1 || total_length() == 0) {
      cursor = u == 1 ? subdivisions : 0;
      return u == 1 ? T(1) : T(0);
    }
    T target = total_length() * u;
    if (cursor > subdivisions || (cursor != 0 && cumulative[cursor - 1] >= target))
      cursor = upper_sample(target);
    else
      while (cursor < subdivisions && cumulative[cursor] < target)
        ++cursor;
    return parameter_in_segment(target, cursor);
  }

private:
  template <finite_classifiable DataT, std::size_t dimension, std::size_t order, typename PointT>
    requires (order < std::numeric_limits<std::size_t>::max())
  friend class Spline;

  std::size_t upper_sample(T target) const {
    return static_cast<std::size_t>(std::lower_bound(cumulative.begin(), cumulative.end(), target) - cumulative.begin());
  }

  T parameter_in_segment(T target, std::size_t upper) const {
    if (target == cumulative[upper])
      return T(upper) / T(subdivisions);
    // First sample >= target, and it wasn't equal: this chord is nonzero.
    T before = cumulative[upper - 1];
    T offset = (target - before) / (cumulative[upper] - before);
    return (T(upper - 1) + offset) / T(subdivisions);
  }

  std::array<T, subdivisions + 1> cumulative{};
};

template <finite_classifiable DataT, std::size_t dimension, std::size_t order, typename PointT = Vector<DataT, dimension>>
  requires (order < std::numeric_limits<std::size_t>::max())
class Spline {
public:
  Spline() requires std::default_initializable<PointT> : controls{} {}

  template <typename... Points>
    requires (sizeof...(Points) == order + 1 && (std::constructible_from<PointT, Points&&> && ...))
  explicit Spline(Points&&... points) : controls{PointT(std::forward<Points>(points))...} {}

  PointT& origin() & { return controls.front(); }
  PointT const& origin() const& { return controls.front(); }
  PointT const& origin() const&& = delete;
  PointT& dest() & { return controls.back(); }
  PointT const& dest() const& { return controls.back(); }
  PointT const& dest() const&& = delete;
  PointT& operator[](std::size_t index) & { return controls[index]; }
  PointT const& operator[](std::size_t index) const& { return controls[index]; }
  PointT const& operator[](std::size_t) const&& = delete;

  PointT point_at(DataT const& t) const requires SplineDetail::evaluable<PointT, DataT, order> {
    if constexpr (order == 0)
      return PointT(controls.front());
    else
      return point_on_unit_interval(SplineDetail::unit_parameter(t));
  }

  // The derivative is not normalized. Form the hodograph's controls directly
  // in the evaluation workspace; no lower-order spline or default PointT.
  PointT derivative_at(DataT const& parameter) const requires SplineDetail::differentiable<PointT, DataT, order> {
    if constexpr (order == 0) {
      return controls.front() - controls.front();
    } else {
      auto difference = [&](std::size_t i) -> PointT { return (controls[i + 1] - controls[i]) * DataT(order); };
      if constexpr (order == 1) {
        return difference(0);
      } else {
        DataT t = SplineDetail::unit_parameter(parameter);
        if (t == 0 || t == 1)
          return difference(t == 0 ? 0 : order - 1);
        return [&]<std::size_t... indices>(std::index_sequence<indices...>) {
          std::array<PointT, order> work{difference(indices)...};
          return SplineDetail::reduce(work, t);
        }(std::make_index_sequence<order>{});
      }
    }
  }

  // Closed interval, sampled polyline (not analytic arc length).
  DataT length(DataT const& begin = 0, DataT const& end = 1, std::size_t subdivisions = 100) const
    requires SplineDetail::measurable<PointT, DataT, order>
  {
    if constexpr (order == 0) {
      return 0;
    } else {
      auto finite = classify_finite(begin, end);
      if (!finite[0] || !finite[1] || begin < 0 || end > 1 || begin >= end || subdivisions == 0)
        return 0;
      return sample_lengths(begin, end, subdivisions, [](std::size_t, DataT) {});
    }
  }

  // Rebuild caller-owned storage in place for repeated curve updates.
  template <std::size_t subdivisions>
    requires SplineDetail::measurable<PointT, DataT, order>
  void sample_arc_lengths(SplineArcLengthTable<DataT, subdivisions>& table) const {
    if constexpr (order != 0) {
      auto record = [&](std::size_t i, DataT length) { table.cumulative[i] = length; };
      if (sample_lengths(0, 1, subdivisions, record) != 0)
        return;
    }
    table.cumulative.fill(0);
  }

  template <std::size_t subdivisions = 100>
    requires (subdivisions > 0 && subdivisions < std::numeric_limits<std::size_t>::max()
        && SplineDetail::measurable<PointT, DataT, order>)
  SplineArcLengthTable<DataT, subdivisions> arc_length_table() const {
    SplineArcLengthTable<DataT, subdivisions> table;
    sample_arc_lengths(table);
    return table;
  }

private:
  PointT point_on_unit_interval(DataT t) const requires (order > 0 && SplineDetail::evaluable<PointT, DataT, order>) {
    if (t == 0)
      return PointT(controls.front());
    if (t == 1)
      return PointT(controls.back());
    if constexpr (order == 1) {
      return Star::lerp(t, controls[0], controls[1]);
    } else {
      return [&]<std::size_t... indices>(std::index_sequence<indices...>) {
        std::array<PointT, order> work{Star::lerp(t, controls[indices], controls[indices + 1])...};
        return SplineDetail::reduce(work, t);
      }(std::make_index_sequence<order>{});
    }
  }

  template <typename Record>
    requires (order > 0 && SplineDetail::measurable<PointT, DataT, order> && std::invocable<Record&, std::size_t, DataT>)
  DataT sample_lengths(DataT begin, DataT end, std::size_t subdivisions, Record&& record) const {
    // A weighted sum of identical floating points can still round away from
    // the common value. A constant control polygon has exactly zero length.
    bool constant = true;
    for (std::size_t i = 1; i <= order; ++i) {
      if constexpr (requires { { controls[i] == controls[0] } -> std::convertible_to<bool>; })
        constant = controls[i] == controls[0];
      else
        constant = DataT((controls[i] - controls[0]).magnitude()) == 0;
      if (!constant)
        break;
    }
    if (constant)
      return 0;

    PointT previous = point_on_unit_interval(begin);
    DataT total = 0;
    for (std::size_t i = 1;; ++i) {
      DataT t = i == subdivisions ? end : begin + (end - begin) * (DataT(i) / DataT(subdivisions));
      PointT const current = point_on_unit_interval(t);
      total += DataT((current - std::as_const(previous)).magnitude());
      record(i, total);
      if (i == subdivisions)
        break;
      previous = current;
    }
    // Distances must be nonnegative. If their sum isn't representable (or an
    // input yielded NaN), never publish a partially usable inverse table.
    return is_finite(total) ? total : DataT(0);
  }

  std::array<PointT, order + 1> controls;
};

using CSplineF = Spline<float, 2, 3, Vec2F>;
}

export module star.spline;

export namespace Star {
  using ::Star::Spline;
  using ::Star::SplineArcLengthTable;
  using ::Star::CSplineF;
}
